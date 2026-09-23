---
title: "Chytrý filtr: schéma zapojení"
description: "Připojení SGP40 přes I2C a ventilátoru skrz MOSFET klíč k ESP32-C3: piny, napájení, typické chyby."
---

# Schéma zapojení

Schéma je jednoduché: senzor na I2C, ventilátor skrz klíč, společné napájení 12 V.

```text
Zdroj 12 V ──┬────────────────────────► Ventilátor (+)
          │                          Ventilátor (−) ◄── MOSFET (drain)
          │                                             MOSFET (source) ─► GND
          │                                             MOSFET (gate) ◄─ GPIO4 ESP32
          │
          └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Piny

| Signál | Pin ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| Hradlo MOSFET (ventilátor) | GPIO4 |

Piny lze zvolit jinak — pak změňte čísla v kódu ([kapitola 5](05-sensor-and-telemetry.md)).

## Pravidla zapojení

1. **Společná zem.** GND zdroje, ESP32, MOSFET modulu a senzoru musí být vzájemně propojeny. Polovina „nefunguje" u domácích projektů způsobuje zapomenutá společná zem.
2. **Senzor — pouze na 3,3 V.** SGP40 netoleruje 5 V na napájení.
3. **Ventilátor — pouze skrz klíč.** GPIO vydává miliampéry; ventilátor potřebuje stovky. Přímé připojení spálí pin. Jak funguje MOSFET klíč — [Tranzistory a MOSFET klíče](../01-electronics-basics/02-mosfet-module.md).
4. **Externí ochranná dioda** pro počítačový ventilátor obvykle není nutná: ventilátor má uvnitř vlastní spínací elektroniku a zvenku se chová jako elektronická zátěž, nikoli jako čistá indukčnost. Ale při spínání napájecí větve klíčem (zejména s PWM) je ochranná dioda paralelně k ventilátoru užitečná jako ochrana tranzistoru před indukčním napěťovým špičkem — a je-li již součástí modulu spínače, jen dobře.

!!! warning "Zkontrolujte polaritu před zapnutím"
    Zaměnění + a − na 12V linky zabije buck modul a často i desku. Proveďte měření multimetrem dříve, než poprvé připojíte napájení.

## Ověření bez firmwaru

Po montáži, před nahráním hlavního kódu:

1. Připojte 12 V — ESP32 by se měl při připojení kabelu hlásit jako USB zařízení (nebo se rozsvítí LED napájení).
2. Krátce propojte hradlo MOSFET přes rezistor 1 kΩ na 3,3 V — ventilátor by se měl rozběhnout.
3. I2C senzor otestujeme přímo z firmwaru pomocí skeneru sběrnice v [kapitole 5](05-sensor-and-telemetry.md).
