# Firmware — Bench PSU Monitor

Arduino sketch for the ESP32-based bench power supply monitor. Reads voltage, current, and power from an INA3221 and streams the data over WebSocket at 10 Hz.

## Requirements

### Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32 DevKit V1 |
| Sensor | INA3221 at I²C address `0x40` |
| Shunt (CH1) | 10 mΩ (`SHUNT_R_CH1 = 0.01`) |
| Calibration button | GPIO0 (BOOT button) |
| WiFi button | GPIO23 |

### Libraries

Install via Arduino Library Manager or the links below:

| Library | Purpose |
|---------|---------|
| [Adafruit INA3221](https://github.com/adafruit/Adafruit_INA3221) | INA3221 driver |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | Captive-portal WiFi setup |
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) | Async HTTP + WebSocket server |
| [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) | Async TCP layer (ESPAsyncWebServer dependency) |

`Wire.h`, `WiFi.h`, and `Preferences.h` are bundled with the ESP32 Arduino core.

### Arduino IDE Setup

1. Add the ESP32 board package URL in **File → Preferences → Additional Boards Manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Install **esp32 by Espressif** via **Tools → Board → Boards Manager**.
3. Select **ESP32 Dev Module** as the target board.

## Flashing

1. Open `bench-psu-monitor.ino` in Arduino IDE.
2. Connect the ESP32 via USB.
3. Select the correct COM port under **Tools → Port**.
4. Click **Upload**.

## First-Time WiFi Configuration

On first boot (or after a reset), the device will not be connected to WiFi. Hold **GPIO23** for **3 seconds** to launch the captive-portal access point (`INA3221-Setup`). Connect to that AP from a phone or laptop, enter your WiFi credentials, and the device will save them to flash and reboot.

## Calibration

Zero-offset calibration compensates for shunt resistor and board offsets at no-load:

1. Disconnect all load from CH1.
2. Hold the **BOOT button (GPIO0)** for **2 seconds**.
3. The offset is measured and stored in NVS (survives reboots).

## WebSocket Data

Once connected to WiFi, the device hosts a WebSocket endpoint:

```
ws://<device-ip>/ws
```

JSON frames are broadcast at **100 ms intervals** (10 Hz):

```json
{"v": 12.345, "i": 123.45, "p": 1523.4, "ts": 12345678}
```

| Key | Unit | Description |
|-----|------|-------------|
| `v` | V | Bus voltage |
| `i` | mA | Current (zero-offset corrected) |
| `p` | mW | Power |
| `ts` | ms | `millis()` timestamp |

## Pin Summary

| GPIO | Function |
|------|----------|
| 21 | I²C SDA |
| 22 | I²C SCL |
| 0 | Zero calibration (hold 2 s) |
| 23 | WiFi config portal (hold 3 s) |
