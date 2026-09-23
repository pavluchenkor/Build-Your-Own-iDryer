---
title: "Smart filter: wiring diagram"
description: "Connecting SGP40 via I2C and fan via MOSFET switch to ESP32-C3: pins, power supply, common mistakes."
---

# Wiring diagram

The circuit is simple: sensor on I2C, fan through a switch, common 12V power.

```text
12V PSU ──┬────────────────────────► Fan (+)
          │                          Fan (−) ◄── MOSFET (drain)
          │                                       MOSFET (source) ─► GND
          │                                       MOSFET (gate) ◄─ GPIO4 ESP32
          │
          └─► buck 12→5V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Pins

| Signal | ESP32-C3 Super Mini pin |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| MOSFET gate (fan) | GPIO4 |

You can choose different pins — then change the numbers in the code ([chapter 5](05-sensor-and-telemetry.md)).

## Connection rules

1. **Common ground.** GND of the power supply, ESP32, MOSFET module, and sensor must be connected. Half of "doesn't work" in DIY projects is forgotten common ground.
2. **Sensor only on 3.3V.** SGP40 cannot handle 5V on power.
3. **Fan only through a switch.** GPIO outputs milliamps; a fan takes hundreds. Direct connection will burn the pin. How a MOSFET switch works — [Transistors and switches](../01-electronics-basics/02-mosfet-module.md).
4. **External protective diode** for a computer fan is usually not required: the fan has its own switching electronics inside, and from the outside it looks like an electronic load, not a pure inductance. But when switching the power line with a key (especially with PWM), a shunting diode across the fan is useful as protection of the key from inductive spikes — and if it is already in the key module, that is a bonus.

!!! warning "Check polarity before power-on"
    Mixed + and − on the 12-volt line will kill the buck module and often the board. Beep with a multimeter before the first power-up.

## Verification without firmware

After assembly, before loading the main code:

1. Apply 12V — ESP32 should show up in the system as a USB device when the cable is connected (or a power LED lights up).
2. Briefly short the MOSFET gate to 3.3V through a 1kΩ resistor — the fan should turn on.
3. We will check the I2C sensor from the firmware with a bus scanner in [chapter 5](05-sensor-and-telemetry.md).
