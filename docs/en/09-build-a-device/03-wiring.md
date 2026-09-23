---
title: "Wiring Diagram for Heated Cabinet on ESP32"
description: "ESP32 pinout map for DIY cabinet: SHT31 over I2C, thermistor on ADC, heater and fan via switch. Signal and power isolation."
---

# Wiring Diagram

This page explains how to connect components around ESP32. First, a general pinout map, then connection of each node and rules for power section layout.

!!! warning "Check your board pinout first"
    The pin numbers below are examples. Different ESP32-C3 and ESP32-S3 boards have different numbering and pin layouts. Before assembly, verify the pinout of your specific development board: for DevKit and Super Mini it is published by the manufacturer and included in the board description. Not all pins can be used freely: some are occupied by boot, flash, or USB.

## Pinout Map (Example)

| Node | Line | ESP32 Pin (example) |
|------|-------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Thermistor | ADC signal | GPIO2 |
| Heater (switch) | control | GPIO4 |
| Fan (switch/PWM) | control | GPIO5 |

Sensor power is `3.3V` and `GND` from the board. The power section is powered separately.

## SHT31 over I2C

SHT31 connects with four wires:

1. Sensor `VCC` — to board `3.3V`.
2. Sensor `GND` — to board `GND`.
3. Sensor `SDA` — to `SDA` pin (example: GPIO8).
4. Sensor `SCL` — to `SCL` pin (example: GPIO9).

I2C lines are short. If the sensor is far from the board, keep the wires as short as possible and twisted together. Most SHT31 modules have pull-up resistors already on the module board.

!!! note "SHT31 Address"
    SHT31 typically has address `0x44` (sometimes `0x45`). If the sensor does not respond, check the address and `SDA`/`SCL` lines.

## Thermistor on ADC

The thermistor is connected in a voltage divider with a pull-up resistor:

1. One thermistor pin — to `3.3V`.
2. The other thermistor pin — to the junction point with the `4.7 kΩ` resistor and the ADC pin (example: GPIO2).
3. The other end of the `4.7 kΩ` resistor — to `GND`.

The controller measures the voltage at the midpoint of the divider and calculates the thermistor resistance from it, then the temperature. The thermistor type is set in the firmware (see [Heating Control](07-heating-control.md)).

Details on thermistor verification and assembly — [Checking Thermistor](../06-practical-guides/02-checking-thermistor.md).

## Heater and Fan via Switch

ESP32 does not control the load directly, but through a switch. Which switch depends on the version from [System Bill of Materials](02-bom.md).

### Version A (24V/12V) — MOSFET Module

1. The module signal input (`PWM`/`SIG`) — to the ESP32 control pin (example: GPIO4 for heater, GPIO5 for fan).
2. Module `GND` — to common `GND` with ESP32.
3. Module power input and load — to the `24V` power supply.

!!! warning "Common Ground"
    The controller `GND` and the power supply `GND` must be connected together. Without a common ground, the control signal has no reference level, and the switch works unpredictably.

Fan connection with control is detailed in [Connecting Fan](../06-practical-guides/01-connecting-fan.md). Switch logic — [MOSFET Module](../01-electronics-basics/02-mosfet-module.md).

### Version B (220V) — SSR/Relay

!!! danger "Before assembly of the mains section"
    All connections to the mains must be made with the device completely de-energized. The case with the mains section must have protective grounding and a fuse. Use mains wires of sufficient cross-section and secure them in the terminals reliably.

SSR has two sides. **Control** — a low-voltage input controlled by the controller. **Power** — terminals through which the mains voltage of the load passes. The sides are isolated from each other by an optocoupler inside the SSR, so the mains can be controlled with a weak `3.3V` signal.

1. The control input is usually marked `DC+` and `DC-` (sometimes `+` and `-`) and is rated for `3–32V` DC. Connect `DC+` to the ESP32 control pin (example: GPIO4), and `DC-` to the controller `GND`. The `3.3V` voltage from the ESP32 pin is sufficient to open the SSR.
2. The power pins (often marked as mains/`AC` and load/`LOAD`) are inserted in the break of one of the heater mains wires — just like a switch in the wire.
3. The fan is switched with a separate SSR or relay in the same way.

!!! note "Why SSR needs a heatsink"
    When the SSR switches, it heats up slightly, and the higher the load current, the greater the heating. Therefore, the SSR is screwed to a heatsink (a metal plate for heat dissipation), and the SSR is chosen with a current margin — notably higher than the load current. What margin and heatsink you need for your current — [Solid-State Relay (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Layout: Signal and Power Sections

- Keep signal wires (sensors, control) separate from power wires.
- Do not run thermistor and I2C wires along power wires of the heater — this is a source of noise.
- In version B, physically separate the mains and low-voltage zones inside the case.
- Bring all low-voltage grounds to a single point.

Fan noise and poor grounding are a common cause of "floating" readings and reboots. See [Wiring Mistakes](../08-common-mistakes/03-wiring-mistakes.md).

## Checklist Before Powering On

- Sensor power is `3.3V`, not `5V`.
- Thermistor and voltage divider resistor are assembled correctly, ADC pin at the midpoint.
- Common ground between controller and power supply.
- In version B — case grounding, fuse, reliable terminals, insulation.
- No short circuits between power and ground (check with multimeter).

Multimeter checking — [Multimeter](../05-tools/02-multimeter.md).

## What's Next

Hardware assembly is complete. Move to [Firmware Start on Core](04-firmware-start.md): create a project and bring the device to Online status on the portal.
