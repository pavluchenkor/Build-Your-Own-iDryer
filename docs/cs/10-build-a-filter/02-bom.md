---
title: "Chytrý filtr: složení systému (BOM)"
description: "Seznam součástek filtru vzduchu: ESP32-C3, VOC senzor SGP40, ventilátor 120 mm, HEPA a uhlíkové filtry, MOSFET klíč, napájení."
---

# Složení systému

Úplný seznam součástek. Ceny jsou orientační, všechno se koupí na kterémkoliv tržišti.

## Elektronika

| Součástka | Příklad | Cena | Proč |
|---|---|---|---|
| Deska ESP32-C3 | ESP32-C3 Super Mini nebo ekvivalent | ~$3 | mozek zařízení, Wi-Fi |
| Senzor VOC | SGP40 (modul, I2C) | ~$4 | index kvality vzduchu |
| Ventilátor 120 mm, 12 V | libovolný skříňový, ideálně s hydrodynamickým ložiskem | ~$5 | průtok vzduchu filtrem |
| MOSFET spínač | modul na AO3400/IRLZ44N nebo hotový „MOSFET switch module" | ~$1 | spínání ventilátoru z 3,3V GPIO |
| Zdroj napájení 12 V / 1 A | kterýkoliv kvalitní | ~$4 | napájení ventilátoru |
| Regulátor napětí 12→5 V | mini-360 (buck) | ~$1 | napájení ESP32 ze stejného zdroje |

Výběr desek — [Řadiče](../02-controllers/00-how-to-choose-controller.md), o napájení a regulátorech — [Základy elektroniky](../01-electronics-basics/01-load-calculation-24v.md).

## Filtrační část

| Součástka | Příklad | Proč |
|---|---|---|
| HEPA filtr | kulatá kazeta z automobilové/domácí čističky vzduchu | zachytává částice |
| Aktivní uhlí | granule v kazetě nebo uhlíkový mat | pohlcuje VOC a pachy |
| Kryt | vytisknete (STL navrhnete pro svůj filtr) nebo použijete libovolnou vhodnou krabici | drží vše pohromadě |

!!! note "Pořadí vrstev"
    Vzduch musí proudit: vstup → HEPA → uhlí → ventilátor → výstup. Ventilátor se umísťuje „na výfuk", za filtry: pak se skulinami krytu nenasává špinavý vzduch mimo filtr. Principiálně to není tak důležité — rozhoduje obměna objemu vzduchu za čas: čím vyšší výkon ventilátoru (CFM), tím je tento čas kratší.

## Proč SGP40

- I2C, napájení 3,3 V — připojí se k ESP32 dvěma signálovými vodiči;
- vydává **VOC index** 0..500 (100 — „normální vzduch", více — špinavěji), nevyžaduje kalibraci;
- existuje hotová knihovna Adafruit.

Alternativy:

- **ENS160** — VOC index + odhad eCO2, také I2C. Dobrá volba „dva v jednom";
- **MH-Z19B/C** — pravý NDIR senzor CO2 (ppm), UART, ~$20. Pro filtr je nadměrný.

## Nástroje

Pájecí stanice, tavidlo, cín, multimetr, teplem smrštitelná bužírka. Podrobně — [Nástroje](../05-tools/02-multimeter.md).
