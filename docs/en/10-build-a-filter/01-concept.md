---
title: "Smart air filter: concept and what the portal gives you"
description: "End-to-end example #2: air filter for a 3D printing zone on ESP32 and idryer-core — your own VOC sensor, your own control, and automatic device card on the portal through a card manifest."
---

# Smart air filter: concept

This is the second end-to-end example in the "build it yourself" section. In the [first example](../09-build-a-device/01-concept.md), you assembled a heated cabinet from the "vocabulary" building blocks of the ecosystem: temperature, humidity, heater. Here we take a step further — assembling a device that **does not exist in the iDryer ecosystem at all**: an air filter with a volatile organic compound (VOC) sensor.

## Main idea: this is a concept, not a recipe for one device

Read this paragraph carefully — it is more important than the rest of the chapter.

The filter here is just an example. The approach shown here works for **any device you invent**: a humidifier, a blower station, an exhaust controller, a filament warehouse monitor, anything. You declare in the firmware what sensors and controls your device has — one or two lines of code per item — and the device **automatically appears on the portal and in the mobile app** with a ready-made card: live readings, buttons, input fields. Not a single line of code on the portal side, no agreements with iDryer, no pull requests.

This works thanks to the **dynamic card** mechanism (entity manifest): the device publishes a machine-readable description of "what to show and what to control," and the portal and app build the interface from this description. How this looks in the code — [chapter on the card](06-card.md).

!!! note "What this means in practice"
    Invent a device → assemble it on ESP32 → describe the sensors and buttons in the firmware → link it to your account in the app. Done: the device has an interface on the portal and in the app. From idea to "I control it from my phone" is one evening of work.

## What exactly are we building

**Air filter for a 3D printing zone**: a box with a fan, HEPA filter, and a carbon layer, which:

- measures air quality with a VOC sensor (SGP40);
- automatically turns the fan on when the air is dirty and off when it's cleaned;
- displays the VOC index and fan status on the portal;
- allows you to select a mode from the portal (`auto` / `on` / `off`) and set the triggering threshold.

ABS and ASA smell of styrene when printing, resins have their own bouquet. A filter at the printer is not a luxury but hygiene.

## Why this is the ideal first project

If the cabinet from section 09 seemed complicated — start with the filter:

- **no heater** — meaning no power section, thermal fuses, or risks;
- minimal components: board, sensor, fan, transistor;
- budget around `$15` without a case;
- in any code error, the worst that happens is the fan won't turn on.

## Problem boundaries

Let us be honest about what this filter **is not**:

- it is not an exhaust hood: air is circulated through the filter, not blown outside;
- it is not a medical device: SGP40 shows a relative air quality **index**, not the concentration of a specific gas in ppm;
- the filter does not replace ventilation.

!!! note "VOC or CO2?"
    For printing vapor, the right sensor is VOC: it reacts to organic compounds (styrene, solvents). CO2 sensors (e.g., NDIR sensor MH-Z19) measure carbon dioxide — this is a measure of stuffiness, not print contamination. If you want both, ENS160 gives both VOC index and eCO2 estimate; the approach from this section will not change — just one more line in the card manifest.

## Section roadmap

1. [System composition](02-bom.md) — what to buy.
2. [Wiring diagram](03-wiring.md) — how to connect.
3. [Firmware startup](04-firmware-start.md) — skeleton on `idryer-core`, linking to the portal.
4. [Sensor and telemetry](05-sensor-and-telemetry.md) — reading VOC and sending to the cloud.
5. [Device card](06-card.md) — declaring sensors and controls, getting the interface.
6. [Automation logic](07-auto-logic.md) — threshold, hysteresis, manual mode from the portal.
7. [Assembly and check](08-assembly-and-check.md) — final checklist.
