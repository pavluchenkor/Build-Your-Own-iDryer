---
title: "Assembly and Pre-Startup Check"
description: "Final assembly of a custom heated cabinet on ESP32: mounting into the enclosure, first controlled heating, temperature calibration, and safety checklist before continuous operation."
---

# Assembly and Check

On this page you will assemble the device into the enclosure, perform the first controlled heating cycle, and verify that the cabinet operates safely. Complete the checks in order and do not leave the device unattended during the first power-up.

## Assembly Order

1. Secure the ESP32 and power section inside the enclosure so that the low-voltage and high-voltage zones are separated.
2. Position the SHT31 sensor inside the cabinet away from the direct airflow of the heater — otherwise it will measure the heater jet temperature rather than the air temperature in the volume.
3. Secure the thermistor in thermal contact with the heater.
4. Verify that wires do not touch the heater and do not interfere with the fan.
5. For version B (`220V`) ensure that the mains wires are secured in the terminals, insulation is intact, and the enclosure is grounded.

For enclosure design and component placement requirements — [Enclosure Design](../07-3d-printing/05-enclosure-design.md).

!!! warning "3D-printed parts near heat"
    PLA softens at temperatures easily found near the heater. Parts near heat must be printed from heat-resistant material: ABS, ASA or PA (nylon). See [Heat-Resistant Materials](../07-3d-printing/04-heat-resistant-materials.md) and [Why PLA is Risky](../07-3d-printing/06-why-pla-is-risky.md).

## Pre-Power Checks

Use a multimeter to verify before the first power-up:

- No short circuit between power and ground.
- Sensor power is `3.3V`, not `5V`.
- Common ground between controller and power supply.
- Thermistor and voltage divider resistor are assembled correctly.
- For version B — enclosure grounding and fuse are in place.

How to use a multimeter — [Multimeter](../05-tools/02-multimeter.md).

## First Startup

1. Apply power only to the controller and sensors. If a single power supply feeds everything, power the controller separately: from USB or through a step-down converter, and leave the heater wire disconnected for now.
2. Verify the device is Online on the portal and displays temperature and humidity readings.
3. Connect the heater and fan.
4. Start the heat maintenance mode from the portal and observe.

!!! danger "Do not leave the first heating unattended"
    During first power-up, monitor the device. Verify that the heater switches off when reaching the target and by thermistor protection, not heating continuously.

What to observe in the first minutes:

- Air temperature rises and stabilizes near the target.
- Heater temperature does not exceed the set limit.
- Heating turns off when reaching the target and resumes after cooling by the hysteresis value.
- Fan operates and does not strike wires.
- Controller does not reboot when the load switches on.

## Calibration

After the first heating cycle, cross-check readings with a separate thermometer inside the cabinet:

- If the air temperature in the cabinet differs from the target — check the SHT31 placement (it must not be in a jet or against a wall).
- If the heater temperature looks implausible — check the thermistor type and voltage divider resistor value.
- If needed, adjust the target temperature and hysteresis in the [menu](06-menu.md).

## If Something Does Not Work

| Symptom | Where to Look |
|---------|---------------|
| Controller reboots under load | [Power Issues](../08-common-mistakes/02-power-mistakes.md) |
| Sensor shows garbage | [Wiring Issues](../08-common-mistakes/03-wiring-mistakes.md), [Checking Thermistor](../06-practical-guides/02-checking-thermistor.md) |
| Device does not connect to Wi-Fi | [Controller Issues](../08-common-mistakes/04-controller-mistakes.md) |
| Heater/SSR runs hot | [Heater and SSR Issues](../08-common-mistakes/05-heater-ssr-mistakes.md) |

General troubleshooting sequence — [Diagnostic Checklist](../08-common-mistakes/06-diagnostic-checklist.md).

## Pre-Operation Checklist

- [ ] Device maintains target temperature and does not heat continuously.
- [ ] Heater protection by thermistor activates.
- [ ] Wires do not touch heater and fan.
- [ ] 3D-printed parts near heat are heat-resistant.
- [ ] For version B: enclosure is grounded, fuse is installed, insulation is intact.
- [ ] Portal readings match the actual temperature inside the cabinet.

## Summary

You have built a heated storage cabinet on ESP32 and `idryer-core`: the device reads climate and heater temperature, maintains the set temperature, protects the heater from overheating, and is controlled via the portal. This is a complete foundation that you can build on with custom ecosystem modules.

Further components — lighting, scales, RFID — are also supported by the core; they can be added using the same approach: sensor or peripheral → telemetry or command → portal display.
