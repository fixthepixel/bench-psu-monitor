"""
ina3221_bridge.py — ESP32/INA3221 WebSocket relay for OBS browser source overlay.

Usage:
    python ina3221_bridge.py --esp-host 192.168.1.50 --ws-port 8766

Dependencies:
    pip install websockets

Point overlay.html's WS_URL at ws://localhost:8766 (or whatever --ws-port you use).
"""

import argparse
import asyncio
import json
import logging
import time
import websockets
from websockets.server import serve
from websockets.exceptions import ConnectionClosed

CLIENTS: set = set()


async def esp_reader(esp_host: str, esp_port: int, queue: asyncio.Queue):
    """Connects to the ESP32's WebSocket server and feeds parsed dicts into queue."""
    log = logging.getLogger("esp")
    uri = f"ws://{esp_host}:{esp_port}/ws"
    while True:
        try:
            log.info(f"Connecting to {uri} ...")
            async with websockets.connect(uri, ping_interval=5, ping_timeout=5) as ws:
                log.info("Connected to ESP32")
                async for msg in ws:
                    try:
                        data = json.loads(msg)
                        data["ts"] = time.time()
                        await queue.put(data)
                    except json.JSONDecodeError:
                        continue
        except (OSError, ConnectionClosed) as e:
            log.warning(f"ESP32 connection error: {e} — retrying in 2s")
            await queue.put({"error": str(e), "ts": time.time()})
            await asyncio.sleep(2)


async def ws_handler(websocket):
    CLIENTS.add(websocket)
    log = logging.getLogger("ws")
    log.info(f"Overlay client connected: {websocket.remote_address}")
    try:
        await websocket.wait_closed()
    finally:
        CLIENTS.discard(websocket)
        log.info("Overlay client disconnected")


async def broadcaster(queue: asyncio.Queue):
    while True:
        data = await queue.get()
        if CLIENTS:
            msg = json.dumps(data)
            await asyncio.gather(
                *[c.send(msg) for c in list(CLIENTS)],
                return_exceptions=True,
            )


async def main(esp_host: str, esp_port: int, ws_port: int):
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    )
    log = logging.getLogger("main")
    log.info(f"INA3221 bridge starting — esp={esp_host}:{esp_port}, ws=ws://localhost:{ws_port}")

    queue: asyncio.Queue = asyncio.Queue(maxsize=4)

    async with serve(ws_handler, "localhost", ws_port):
        log.info(f"WebSocket listening on ws://localhost:{ws_port}")
        await asyncio.gather(
            esp_reader(esp_host, esp_port, queue),
            broadcaster(queue),
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32 INA3221 → WebSocket bridge for OBS")
    parser.add_argument("--esp-host", required=True,
                        help="ESP32 IP address (e.g. 192.168.1.50)")
    parser.add_argument("--esp-port", default=80, type=int,
                        help="ESP32 WebSocket port (default: 80)")
    parser.add_argument("--ws-port", default=8766, type=int,
                        help="Local WebSocket port for overlay.html (default: 8766)")
    args = parser.parse_args()
    asyncio.run(main(args.esp_host, args.esp_port, args.ws_port))