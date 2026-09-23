---
title: "Bill of Materials for a heated cabinet system: components and two versions of the power section"
description: "Component list for a self-made heated cabinet on ESP32: SHT31 sensor, thermistor, heater and fan in low-voltage (24V) and mains (220V) versions."
---

# Bill of Materials

This page lists the components of the device and two options for the power section. The low-voltage part (controller and sensors) is the same in both versions. Only the way the heater and fan are switched differs.

## Low-voltage part (common to both versions)

| Node | Purpose | Notes |
|------|---------|-------|
| ESP32-C3 or ESP32-S3 | Controller: logic, Wi-Fi, portal | DevKit or Super Mini will work |
| SHT31 sensor | Temperature and humidity in the cabinet | I2C interface |
| NTC 100K thermistor | Heater temperature control | For example, Generic 3950 |
| Thermistor pull-up resistor | Voltage divider for ADC | Usually `4.7 kΩ` |
| Power supply | Power for controller and low-voltage peripherals | Voltage depends on chosen version |

C3 and S3 are equivalent for this example: the code, the wiring and the assembly order are the same. S3 is more powerful and has more memory — take it if you later want to add a display, an addressable LED strip or your own heavy logic.

ESP32 was chosen because it has Wi-Fi, the required interfaces (I2C for SHT31, ADC for thermistor, PWM for load control) and is directly supported by `idryer-core`. For more details, see [ESP32 Controller](../02-controllers/01-esp32-controller.md).

!!! warning "ESP32 logic — 3.3V"
    ESP32 operates at `3.3V`. Do not apply `5V` to its pins. This applies to sensors, modules, and adapters. For more details, see [Controller mistakes](../08-common-mistakes/04-controller-mistakes.md).

## Sensors

**SHT31** measures the temperature and humidity of the air inside the cabinet. This is the main feedback: through it you can see whether the set climate is being maintained. It connects via I2C (two lines: `SDA`, `SCL`). For more details, see [Thermistors and climate sensors](../03-common-components/04-thermistors.md).

**Thermistor** measures the temperature of the heater itself, not the air. It is needed to prevent the heater from overheating: the air warms up slowly, but the heater warms up quickly. The thermistor is connected as a voltage divider to an ADC pin. [Checking the thermistor](../06-practical-guides/02-checking-thermistor.md).

!!! note "Why two heat sensors"
    SHT31 tells you "what is the temperature in the cabinet", the thermistor tells you "has the heater overheated". The first sets the goal, the second protects from an emergency.

## Power section: choose a version

The heater and fan are loads controlled by the controller. ESP32 cannot switch such a load directly: its pin outputs a weak `3.3V` signal. A switch is needed between the controller and the load.

There are two fundamentally different versions. Choose one depending on what heater and fan you are using.

### Version A — low-voltage (24V or 12V)

The heater and fan are powered by `24V` (or `12V`) direct current. This is a simpler and safer way for self-assembly.

| Node | Component |
|------|-----------|
| Heater | Heating element `12V` or `24V` (PTC heater) |
| Fan | Fan `24V` or `12V` (2-pin or 4-pin) |
| Heater switch | MOSFET module |
| Fan switch | MOSFET module (or 4-pin PWM directly) |
| Power supply | `24V DC` with power reserve |

The controller controls the MOSFET module with a signal from the ESP32 pin. The module switches the low-voltage load. This is the same logic as in a ready-made controller. For more details, see [MOSFET module](../01-electronics-basics/02-mosfet-module.md).

Power supply capacity is calculated for the total load with reserve — see [Calculating load current for 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Recommended version for your first device"
    If you are assembling the device for the first time, start with version A. There is no mains voltage on the load here, and a wiring error is less dangerous.

### Version B — mains (110–230V AC)

The heater and fan are powered by the mains `110–230V`. This is done when you need a powerful mains heater — for example, a ready-made heater with fan for a cabinet. Here, mains switching modules are used instead of MOSFET modules.

| Node | Component |
|------|-----------|
| Heater | Mains heater `110–230V AC` |
| Fan | Mains fan `110–230V AC` |
| Heater switch | Solid-state relay (SSR) for AC |
| Fan switch | SSR or conventional relay for AC |
| Power supply | Separate `24V`/`5V DC` for controller and sensors |
| Protection | Fuse, protective grounding of the housing |

!!! danger "Mains voltage is dangerous to life"
    Version B works with voltage `110–230V`. A wiring error can lead to electric shock or fire. Before assembly, be sure to read the safety materials: [Triac](../01-electronics-basics/03-triac.md), [Solid-state relay (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Heater and SSR mistakes](../08-common-mistakes/05-heater-ssr-mistakes.md). If you have no experience working with mains voltage, choose version A.

The controller and sensors in version B are still powered by a separate low-voltage source (`5V`/`24V`). The mains part and low-voltage part must be physically and electrically separated.

## Optional modules

These nodes are not required for the cabinet, but are supported by the core and can be added later:

- addressable LED lighting (`hasLed`);
- filament consumption weight sensor (`hasWeight`);
- spool RFID tag (`hasRfid`).

The basic cabinet does not use them — we start with the minimum.

## What's next

When the components are selected, move on to [Wiring diagram](03-wiring.md): which ESP32 pin is responsible for what and how to separate the low-voltage and power sections.
