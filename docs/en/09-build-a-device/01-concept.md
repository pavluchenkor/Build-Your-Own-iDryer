---
title: "Building Your Own Device on iDryer Core: Concept"
description: "End-to-end example: how to build a heated filament storage cabinet from scratch using ESP32 and the idryer-core library with connection to the iDryer portal."
---

# Building Your Own Device: Concept

This section is an end-to-end example. Previous sections explained individual components: power supply, controllers, sensors, heaters, safety. Here you assemble these components into one complete device and bring it to working condition with connection to the [iDryer portal](https://portal.idryer.org/).

The example is built on the `idryer-core` library. The library handles all network integration: Wi-Fi connection, account binding, secure MQTT session, periodic telemetry publication. You write only what is specific to your device: reading sensors, controlling heater and fan, maintaining temperature logic.

## What Exactly We Are Building

We are building a **heated filament storage cabinet**. This is an enclosed cabinet for 10–40 spools, maintaining a temperature around `40–45 °C`.

It is important to define the scope from the start.

!!! note "This is not a high-temperature dryer"
    We do not claim rapid drying at high temperature. The goal of this device is to maintain gentle heat in the cabinet that keeps filament dry during storage.

A temperature of `40–45 °C` is sufficient to keep most undemanding plastics — from PLA to ABS — in dry condition. For active drying of demanding materials (nylon, polycarbonate, PA-CF) higher temperatures and different design are needed — such dryers are built separately, following principles from other sections.

## Why Build It Yourself

The finished iDryer controller already knows how to do everything described below. This example is not instead of it, but to show **how a device works internally** and provide a basis for your own modules.

Self-assembly makes sense when:

- you need a cabinet of non-standard size or shape;
- you want to understand how the controller manages heating and communicates with the portal;
- you plan to build your own ecosystem module and use this example as a starting point.

## How This Differs from the V2 Controller

The production iDryer V2 controller is dual-processor: main logic runs on a separate microcontroller, while the ESP32 module acts only as a bridge to Wi-Fi and the portal. This is justified for a production device with display, scales, RFID, and multiple cameras.

For a homemade cabinet, such complexity is unnecessary. We simplify the architecture to a **single ESP32** that does everything:

- reads sensors;
- controls heater and fan;
- connects to Wi-Fi and portal via `idryer-core`.

Functionally we replicate the behavior of one camera of the V2 controller (climate sensor, heater with thermistor feedback, fan), but in honest DIY form on a single board.

!!! note "Servo is not used"
    In the V2 controller, a servo controls the air damper of the camera. For a storage cabinet with uniform gentle heating, the damper is not needed, so this example has no servo.

## What Connecting to the Core Provides

When a device is built on `idryer-core` and bound to an account, you get without additional code:

- control and monitoring via [portal](https://portal.idryer.org/) and mobile app;
- temperature and humidity graph in the cabinet;
- remote start and stop of heat maintenance mode;
- parameter configuration (target temperature, hysteresis) via device menu.

!!! note "Custom sensors and controls on the card"
    The device card is not limited to vocabulary capabilities (`has*`). Through a card manifest, the firmware can declare any custom sensors and controls — they will automatically appear on the portal and in the app. How to do this is shown in the end-to-end example ["Smart Air Filter"](../10-build-a-filter/01-concept.md), especially in [the card chapter](../10-build-a-filter/06-card.md). Start and stop of heat maintenance are declared as card actions in [chapter 7](07-heating-control.md).

## What This Section Includes

Below is a step-by-step path from an empty board to a working cabinet:

1. [System Composition](02-bom.md) — which components to use and two versions of the power section (low-voltage and mains).
2. [Wiring Diagram](03-wiring.md) — ESP32 pin map, separation of signal and power circuits, safety.
3. [Firmware Start on Core](04-firmware-start.md) — PlatformIO project, first run, binding to portal.
4. [Sensors](05-sensors.md) — connect SHT31 and thermistor, read data from them.
5. [Menu from YAML](06-menu.md) — describe device settings, they go into NVS and to the portal.
6. [Heating Control](07-heating-control.md) — temperature maintenance logic, fan, start and stop from the device card.
7. [Assembly and Check](08-assembly-and-check.md) — final assembly, first heat run, safety checklist.

!!! tip "Ready-made example"
    If you want to see the result immediately — the finished project is in the `example/09-cabinet/` folder of the repository and builds with the command `pio run -e cabinet`. The chapters below break down this same code step by step.

## See Also

- [Getting Started](../00-start-here/01-introduction.md) — general reading order for the section.
- [ESP32 Controller](../02-controllers/01-esp32-controller.md) — why ESP32 is convenient for a Wi-Fi device.
- [Common Components](../03-common-components/01-overview.md) — device component map.
