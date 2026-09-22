<div align="center">

<img src="docs/img/iDryer_logo_small.png" width="220" alt="iDryer">

# Build Your Own iDryer

**Your own device on ESP32. Portal, app, push: all of it is ready, you write only the logic.**

[![Documentation](https://img.shields.io/badge/docs-idryer.org-e7352c)](https://docs.idryer.org/en/development/byod/) [![Telegram](https://img.shields.io/badge/Telegram-iDryer-2ca5e0)](https://t.me/iDryer) [![Discord](https://img.shields.io/badge/Discord-join-5865f2)](https://discord.gg/jGce5eeHHz) [![License](https://img.shields.io/badge/license-Apache--2.0%20%2F%20CC%20BY%204.0-blue)](LICENSE.txt)

</div>

---

## What this is

A set of end-to-end examples and the course that goes with them. Each example is a complete device built on the `idryer-core` library, brought to a working state and connected to the [iDryer portal](https://portal.idryer.org/).

Build a device on the core and you get the whole infrastructure at once: portal, authentication, device binding, secured communication, telemetry, charts, over-the-air updates. None of that has to be written.

## Who this is for

- **You are a developer and you need a portal, not a backend.** You have a device idea and an ESP32, and no interest in writing a server, an auth system and a mobile app. The core hands you all of it, finished.
- **You are building your first device.** Electronics is still unfamiliar, but you want to understand what connects to what and why, instead of copying someone else's schematic. The course takes you from a current calculation to working firmware.
- **You need a device that does not exist yet.** A humidifier, a fume extractor, a warehouse monitor. The ecosystem does not keep a list of allowed devices: declare your sensors and the device is in the portal.

## What you write and what the core does

You write only what is specific to your device: reading sensors, driving the load, the operating logic. That is hundreds of lines, not thousands. In the finished storage cabinet example the main file is **125 lines**.

Everything else is handled by `idryer-core`: Wi-Fi connection, binding to an account, the secured MQTT session, telemetry publishing, command handling, updates. You never write the network stack.

## The device shows up in the portal by itself

This is the key difference from ordinary DIY.

In the firmware you declare which sensors and controls the device has, one or two lines each. The portal builds the device card from that description on its own: live readings, buttons, input fields.

Not a line of code on the portal side. No approvals, no pull requests. A device the iDryer ecosystem has never seen before, a humidifier, a part cooling station, a fume extractor controller, a warehouse monitor, gets a working interface.

> The automatic card currently works in the web portal. The same rendering is planned for the mobile app but is not implemented yet.

→ [How this is done in code](https://docs.idryer.org/en/development/byod/10-build-a-filter/06-card/)

## Ready examples

### Filament storage cabinet

A closed cabinet for 10-40 spools with gentle heat at 40-45 °C that keeps filament dry. A single ESP32 does everything: reads the climate, drives the heater and the fan, keeps the link to the portal.

The example covers the full path: concept, parts list, wiring diagram, first firmware, sensors, menu, heater control, assembly and checkout.

→ [Build the cabinet](https://docs.idryer.org/en/development/byod/09-build-a-device/01-concept/)

### Smart air filter

A box with a fan, a HEPA filter and a carbon layer. It measures air quality with a VOC sensor, turns itself on when the air is dirty and off once it is clean. The mode and the trigger threshold are set from the portal.

ABS and ASA give off styrene while printing, resins have a bouquet of their own. A filter next to the printer is hygiene, not luxury.

This example shows the main point: a device that does not exist in the ecosystem at all gets a full device card in the portal.

→ [Build the filter](https://docs.idryer.org/en/development/byod/10-build-a-filter/01-concept/)

## If electronics is still unfamiliar

The examples are preceded by a full course, from the first current calculation to a working device. It does not replace an electrician and it does not teach circuit design, but it walks you through everything the build requires.

| Section | Topics |
|---|---|
| Electronics basics | Power, current, load, MOSFET, triac, solid state relay |
| Controllers | ESP32, Arduino, RP2040, STM32, interfaces, flashing |
| Components | Heaters, fans, thermistors, servos, load cells, displays, RFID |
| Thermal physics and materials | Thermal conductivity, convection, material safety |
| Tools | Multimeter, USB-UART, soldering iron, crimping, ST-Link, oscilloscope |
| Practice | Connecting a fan, testing a thermistor and the other procedures step by step |
| 3D printing | Which parts survive next to a heater |
| Common mistakes | Where to look when it will not turn on, overheats or behaves oddly |

Read it in order, or open it as a reference.

→ [Start here](https://docs.idryer.org/en/development/byod/00-start-here/01-introduction/) · [Common mistakes](https://docs.idryer.org/en/development/byod/08-common-mistakes/01-overview/)

## More examples are coming

This is an open collection. If you built your own device on the core, send in the example and it will sit alongside the rest.

Write-ups of failures are just as useful: what did not start, where you got burned, which component turned out to be the wrong one. Stories like that save other people weeks.

## Status

The course is under active development: chapters are being written, examples are being added. The storage cabinet is built and lives in the portal, the air filter is documented step by step.

## Safety

> **Devices in this section get hot and run from mains power.** Working with `110-230 V` takes its own discipline and does not forgive haste. Before you power up anything you built yourself, read the safety sections in full.

The documentation does not replace an electrician and it does not grant permission to build dangerous devices without understanding what you are doing.

## Place in the ecosystem

| Layer | What it does | Repository |
|---|---|---|
| Course and examples | How to build your own device: **this repository** | Build-Your-Own-iDryer |
| Core | Library: network, portal, protocol, telemetry | [idryer-core](https://github.com/pavluchenkor/idryer-core) |
| Production controller | Ready solution for dryers | [iDryerControllerV2](https://github.com/pavluchenkor/iDryerControllerV2) |
| Cloud | Portal, app, authentication | [portal.idryer.org](https://portal.idryer.org/) |

## What is in the repository

| Path | What it is |
|---|---|
| `example/` | Working example projects, ready to build |

## License

Documentation is under [CC BY 4.0](LICENSE-Documentation.txt). Source code, including the examples, is under the [Apache License 2.0](LICENSE-Software.txt). Details in [LICENSE.txt](LICENSE.txt) and [NOTICE](NOTICE).

The iDryer name is not covered by these licenses: see [TRADEMARKS.md](https://github.com/pavluchenkor/idryer-core/blob/main/TRADEMARKS.md).

## Help

- [Telegram](https://t.me/iDryer)
- [Discord](https://discord.gg/jGce5eeHHz)
- [Documentation](https://docs.idryer.org/en/development/byod/)

Guides and teardowns on the channels: [YouTube](https://www.youtube.com/@iDryerProject) · [Rutube](https://rutube.ru/channel/34401569/)

## Contributing

Send in your examples, extend the course, fix mistakes: open an issue or a pull request.

## Next

[Build the air filter](https://docs.idryer.org/en/development/byod/10-build-a-filter/01-concept/): the shortest path from an idea to a working device.
