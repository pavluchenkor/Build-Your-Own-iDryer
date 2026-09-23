---
title: "Intelligenter Filter: Systemzusammensetzung (BOM)"
description: "Komponentenliste für den Luftfilter: ESP32-C3, VOC-Sensor SGP40, Lüfter 120 mm, HEPA- und Aktivkohle-Filter, MOSFET-Schalter, Stromversorgung."
---

# Systemzusammensetzung

Vollständige Komponentenliste. Die Preise sind ungefähr, alles ist auf jedem Marktplatz erhältlich.

## Elektronik

| Komponente | Beispiel | Preis | Wofür |
|---|---|---|---|
| ESP32-C3 Platine | ESP32-C3 Super Mini oder ähnlich | ~$3 | Gehirn des Geräts, Wi-Fi |
| VOC-Sensor | SGP40 (Modul, I2C) | ~$4 | Luftqualitäts-Index |
| Lüfter 120 mm, 12 V | jeder Standard-Lüfter, besser mit Gleitlager | ~$5 | Luft durch Filter drücken |
| MOSFET-Schalter | Modul auf AO3400/IRLZ44N oder fertiges „MOSFET switch module" | ~$1 | Lüfter von 3,3-V GPIO schalten |
| Stromversorgung 12 V / 1 A | beliebig hochwertig | ~$4 | Lüfter-Stromversorgung |
| Step-Down-Modul 12→5 V | mini-360 (buck) | ~$1 | ESP32-Stromversorgung von gleicher Quelle |

Zu Platinen-Auswahl – [Controller](../02-controllers/00-how-to-choose-controller.md), zu Stromversorgung und Step-Down-Modulen – [Elektronik-Grundlagen](../01-electronics-basics/01-load-calculation-24v.md).

## Filter-Teil

| Komponente | Beispiel | Wofür |
|---|---|---|
| HEPA-Filter | runde Kassette von Auto-/Raum-Luftreiniger | Partikel abfangen |
| Aktivierter Kohle | Granulate in Kassette oder Kohlematte | VOC und Gerüche absorbieren |
| Gehäuse | 3D-gedruckt (STL nach eigenem Filter entwerfen) oder beliebige passende Box | alles zusammenhalten |

!!! note "Reihenfolge der Schichten"
    Luft muss gehen: Eingang → HEPA → Kohle → Lüfter → Ausgang. Der Lüfter wird „auf Druck", hinter den Filtern, eingebaut: dann wird durch Gehäuse-Risse keine schmutzige Luft am Filter vorbei angesaugt. Grundsätzlich ist das nicht so wichtig – es wirkt die Umwälzrate des Luftvolumens über die Zeit: je höher die Leistung des Lüfters (CFM), desto kürzer diese Zeit.

## Warum SGP40

- I2C, Stromversorgung 3,3 V – verbindet sich mit ESP32 mit zwei Signal-Drähten;
- gibt **VOC-Index** 0..500 aus (100 – „normale Luft", mehr – schmutziger), braucht keine Kalibrierung;
- es gibt eine fertige Adafruit-Bibliothek.

Alternativen:

- **ENS160** – VOC-Index + eCO2-Schätzung, auch I2C. Gute Variante „zwei in eins";
- **MH-Z19B/C** – echter NDIR CO2-Sensor (ppm), UART, ~$20. Für einen Filter übertrieben.

## Werkzeuge

Lötkolben, Flussmittel, Lötdraht, Multimeter, Schrumpfschlauch. Ausführlich – [Werkzeuge](../05-tools/02-multimeter.md).
