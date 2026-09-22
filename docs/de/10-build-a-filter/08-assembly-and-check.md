---
title: "Intelligenter Filter: Zusammenbau und Endabnahme"
description: "Filter-Zusammenbau in Gehäuse, Reihenfolge der Filter-Schichten und komplette Checkliste: Sensor, Telemetrie, Karte, Befehle, Automatik."
---

# Zusammenbau und Prüfung

## Zusammenbau

1. **Gehäuse.** Box mit zwei Öffnungen: Lufteingang und Ausgang. Sie drucken nach Ihrer Filterkartusche oder bearbeiten ein Fertig-Gehäuse.
2. **Schichten im Luftstrom:** Eingang → HEPA → Kohle → Lüfter (auf Druck) → Ausgang. Fugen – ohne Spalten: Luft ist faul und geht an Filterrundgang vorbei, falls sie kann.
3. **Sensor** – im Einluftstrom, vor den Filtern: Er riecht die schmutzige Zimmerluft, nicht gereinigte.
4. **Elektronik** – in separatem Fach oder an Wand, weg vom Luftstrom. Platine – auf Abstandshalter, nicht „haufenweise".
5. Drähte sichern: Lüfter-Vibration lockert mit Zeit alles, was nicht befestigt ist.

## Komplette Checkliste

Überprüfen Sie der Reihe nach – jeder Punkt baut auf vorigen auf.

| # | Prüfung | Wie |
|---|---|---|
| 1 | Stromversorgung | 12 V an Lüfter-Leitung, 5 V nach buck, 3,3 V am Sensor |
| 2 | Sensor lebt | im Serial-Log Index ~100 in reiner Luft, wächst von Atemluft |
| 3 | Gerät Online | Status im Portal nach der Kopplung in der App |
| 4 | Telemetrie | `vocIndex` und `fanStatus` im Gerätestrom |
| 5 | Karte | VOC- und Lüfter-Zellen, Liste Mode, Feld Threshold |
| 6 | Befehl vom Portal | Mode → `on`: Lüfter lief an, Karte zeigte „An" |
| 7 | Automatik | Mode → `auto`, Atemluft: sprang an auf Schwelle, ausgeschaltet darunter |
| 8 | Neustart | Modus und Schwelle gespeichert, Karte lebte wieder auf |

## Was dann

Filter ist bereit. Dann – nach Geschmack:

- **Mehr Entities**: Button „Produktblase 5 Minuten" (`card().button(...)`), zweiter Sensor, Lüfter-Betriebsstunden-Zähler mit Austausch-Erinnerung;
- **Schönes Layout**: Fabrik-`layoutRow` Sie schon sahen;
- **Ihre Geräte**: Dieser ganze Abschnitt – Schablone. Ersetzen Sie Sensor, Stellglied und Logik – und bauen Sie nach gleicher Schablone Befeuchter, Abluft-Controller, Steuerung von Allem auf. Das Manifest macht Interface selbst.

Falls etwas nicht anspringt – [Typische Fehler](../08-common-mistakes/01-power-mistakes.md).
