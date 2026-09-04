# Build Your Own iDryer

## Purpose

This repository contains practical documentation for people who want to build their own device using the iDryer ecosystem.

The main goal is simple:

Help a person without an engineering background understand enough to safely build, connect, and maintain a working device.

This is **not** an academic electronics course.

The documentation should be:

- practical;
- clear without special training;
- usable immediately;
- written for a regular user, not only for an engineer.

---

## Who This Is For

This section is for users who:

- have little or no electronics experience;
- want to build their own device;
- are not confident with programming or hardware;
- prefer step-by-step instructions.

---

## Core Principles

All pages in this section follow these principles:

1. Explain only what is needed.

- no unnecessary theory;
- only what is useful for the build.

2. Always show practical use.

- practical wiring examples;
- real connections;
- common user mistakes.

3. Safety comes first.

This is especially important for:

- mains voltage;
- heaters;
- SSR modules;
- power supplies;
- wiring and insulation.

4. The text should be clear on the first read.

- no complicated wording;
- no long explanations that do not help the build.

5. One page covers one topic.

---

## Documentation Structure

The documentation is stored in language folders:

```text
docs/
├── ru/
├── en/
├── de/
├── fr/
├── es/
├── cs/
├── ja/
├── pt/
├── pt-BR/
├── zh/
└── zh-Hant/
```

Russian (`docs/ru`) is the source language. Other languages are translated from it.

The main content sections are:

```text
00-start-here/
01-electronics-basics/
02-controllers/
03-common-components/
04-thermal-physics-and-materials/
05-tools/
06-practical-guides/
07-3d-printing/
08-common-mistakes/
```

There is also a local planning file:

```text
00-карта-раздела.md
```

It is a working map of the Russian section. It is useful for planning and review, but it is not part of the published documentation site.

---

## Required Topics

### Electronics Basics

The basics should cover:

- voltage;
- current;
- power;
- resistance;
- AC and DC;
- why 24V and mains voltage must not be confused;
- how to choose a power supply;
- why power margin matters;
- what inrush current is.

---

### Relays, SSR, and MOSFETs

The documentation should explain:

- how they differ;
- when to use each one;
- why an SSR is not always needed;
- why a MOSFET does not replace an SSR;
- why a relay can stick;
- why an SSR can heat up.

---

### Mains Voltage Safety

This is a critical topic.

It should cover:

- why mains voltage is dangerous;
- how to connect loads safely;
- grounding;
- fuses;
- circuit breakers;
- wire cross-section;
- terminals;
- insulation;
- mistakes that can destroy equipment or create real danger.

---

### Controllers

#### ESP32

Espressif Systems ESP32:

- what it is used for;
- Wi-Fi;
- GPIO;
- PWM;
- ADC;
- why it is convenient.

#### Arduino

- why it is widely known;
- how it differs from ESP32;
- when it is useful;
- when ESP32 is a better choice.

#### STM32

STMicroelectronics STM32:

- where it is used;
- why it is more advanced;
- why a bootloader matters;
- what DFU, Boot, and ST-Link mean.

---

### USB-UART Adapters

The documentation should explain:

- what a USB-UART adapter is;
- why it is needed;
- how to connect it;
- common RX/TX mistakes;
- 3.3V and 5V logic levels;
- how an ESP can be damaged.

---

### Components

Each component page should answer:

- what it is;
- why it is needed;
- how it connects;
- what users most often break.

Required components:

- heaters;
- fans;
- servos;
- thermistors;
- LED strips;
- OLED displays;
- TFT / touch screens;
- load cells;
- RFID/NFC.

---

### Practical Guides

Step-by-step guides should cover:

- connecting an SSR to ESP32;
- connecting a heater safely;
- using 24V and mains voltage in one device;
- connecting a fan;
- choosing a power supply;
- correct grounding;
- handling EMI;
- diagnosing unstable USB;
- checking a thermistor;
- connecting a servo;
- connecting a load cell;
- connecting an RFID reader.

---

### Tools

#### Multimeter

- how to measure voltage;
- how to use continuity mode;
- how not to damage the multimeter.

#### Oscilloscope

- practical use only;
- how to check PWM;
- how to check UART;
- how to inspect noise.

#### ST-Link

- why it is needed;
- when it is needed;
- how to flash STM32.

#### USB-TTL

- how to use it;
- how not to swap RX and TX.

#### Soldering

- soldering wires;
- soldering JST connectors;
- soldering thermistors;
- common mistakes.

---

### 3D Printing

This section should cover:

- what STL is;
- PETG, ABS, and ASA;
- heat-resistant materials;
- enclosure design;
- why PLA is a poor choice for heated enclosures.

---

### Common Mistakes

This section should explain:

- why USB does not work;
- why ESP32 restarts;
- why an SSR gets very hot;
- why a fan creates interference;
- why a thermistor shows nonsense;
- why a display does not turn on;
- why a servo breaks the power supply;
- why the device behaves unstably.

---

## Useful External Sources

- **Alex Gyver** — Russian-language educational videos about Arduino, ESP32, PWM, relays, servos, OLED displays, and sensors.
- **GreatScott!** — English-language videos with a practical engineering view of electronics.
- **STMicroelectronics** — official STM32 documentation.
- **Espressif Systems** — official ESP32 documentation.
- **Arduino** — documentation and beginner examples for microcontrollers.

---

## Main Goal

After reading this section, the user should understand what they are doing and why, instead of becoming more confused.

## License

This repository combines documentation and example source code, licensed
separately — see [LICENSE.txt](LICENSE.txt) and [NOTICE](NOTICE).

- **Documentation** (all prose, guides, images): [CC BY 4.0](LICENSE-Documentation.txt)
- **Source code** (including the example firmware under `example/`): [Apache License 2.0](LICENSE-Software.txt)

Both licenses permit commercial use.

The licenses do not grant rights to the iDryer name. Community projects are
welcome and the naming policy is permissive — see
[TRADEMARKS.md](https://github.com/pavluchenkor/idryer-core/blob/main/TRADEMARKS.md).

Hardware design, CAD files and PCB sources are licensed separately and are not
covered by this repository.
