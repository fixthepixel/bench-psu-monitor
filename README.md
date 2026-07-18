# Bench PSU Monitor

A 3-channel bench power supply monitor PCB built around an **ESP32 DevKit V1** and a **Texas Instruments INA3221** triple-channel current/voltage sense IC.

## Features

- 3 independent measurement channels (CH1, CH2, CH3) via screw terminals
- Voltage and current monitoring over I²C using the INA3221
- 10 mΩ shunt resistors for current sensing
- I²C address selection via solder jumpers JP1/JP2
- Compact 2-layer PCB with gerbers ready for fabrication

## Hardware

| Component | Value / Part |
|-----------|-------------|
| MCU | ESP32 DevKit V1 (30-pin) |
| Current/Voltage monitor | INA3221 (3-channel, I²C) |
| Shunt resistors | 10 mΩ (RS1, RS2, RS3) |
| Bypass capacitor | 100 nF |
| Bulk capacitor | 10 µF |

## Repository Contents

```
bench-psu-monitor.kicad_sch   Schematic
bench-psu-monitor.kicad_pcb   PCB layout
bom/ibom.html                 Interactive BOM
gerber/                       Gerber files for fabrication
```

## Fabrication

Gerber files are in [gerber/](gerber/) and can be sent directly to any PCB manufacturer. The board is 2-layer (F_Cu / B_Cu).
