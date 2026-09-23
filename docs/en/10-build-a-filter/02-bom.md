---
title: "Smart filter: system composition (BOM)"
description: "List of air filter components: ESP32-C3, VOC sensor SGP40, 120mm fan, HEPA and carbon filters, MOSFET switch, power supply."
---

# System composition

Complete list of components. Prices are approximate; everything is available on any marketplace.

## Electronics

| Component | Example | Price | Purpose |
|---|---|---|---|
| ESP32-C3 board | ESP32-C3 Super Mini or similar | ~$3 | device brain, Wi-Fi |
| VOC sensor | SGP40 (module, I2C) | ~$4 | air quality index |
| 120mm fan, 12V | any case fan, preferably with ball bearing | ~$5 | air flow through filter |
| MOSFET switch | AO3400/IRLZ44N module or ready-made "MOSFET switch module" | ~$1 | turning on fan from 3.3V GPIO |
| 12V / 1A power supply | any quality one | ~$4 | fan power |
| 12→5V step-down module | mini-360 (buck) | ~$1 | powering ESP32 from the same PSU |

For board selection — [Controllers](../02-controllers/00-how-to-choose-controller.md), for power and step-down modules — [Electronics Basics](../01-electronics-basics/01-load-calculation-24v.md).

## Filter section

| Component | Example | Purpose |
|---|---|---|
| HEPA filter | round cartridge from an automotive/household air purifier | catches particles |
| Activated carbon | granules in a cassette or carbon mat | absorbs VOC and odors |
| Case | 3D printed (design STL for your filter) or any suitable box | holds everything together |

!!! note "Layer order"
    Air flow should be: inlet → HEPA → carbon → fan → outlet. The fan is installed "as exhaust", after the filters: then dirty air is not drawn in through the case gaps, bypassing the filter. This is not that critical — what matters is air volume turnover over time: the higher the fan performance (CFM), the shorter this time.

## Why SGP40

- I2C, 3.3V power — connects to ESP32 with two signal wires;
- outputs **VOC index** 0..500 (100 = "normal air", higher = dirtier), no calibration needed;
- Adafruit has a ready-made library.

Alternatives:

- **ENS160** — VOC index + eCO2 estimate, also I2C. A good "two-in-one" option;
- **MH-Z19B/C** — real NDIR CO2 sensor (ppm), UART, ~$20. Overkill for a filter.

## Tools

Soldering iron, flux, solder, multimeter, heat shrink. Details — [Tools](../05-tools/02-multimeter.md).
