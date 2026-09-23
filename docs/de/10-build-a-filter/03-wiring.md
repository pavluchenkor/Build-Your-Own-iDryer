---
title: "Intelligenter Filter: Schaltschema"
description: "SGP40 an I2C und Lüfter über MOSFET-Schalter an ESP32-C3 anschließen: Pins, Stromversorgung, typische Fehler."
---

# Schaltschema

Das Schema ist einfach: Sensor auf I2C, Lüfter über Schalter, gemeinsame 12 V Stromversorgung.

```text
Stromvers. 12 V ──┬────────────────────────► Lüfter (+)
                  │                          Lüfter (−) ◄── MOSFET (Drain)
                  │                                         MOSFET (Source) ─► GND
                  │                                         MOSFET (Gate) ◄─ GPIO4 ESP32
                  │
                  └─► buck 12→5 V ─► ESP32-C3 (5V)

SGP40:  VCC ─► 3V3 ESP32    SDA ─► GPIO8    SCL ─► GPIO9    GND ─► GND
```

## Pins

| Signal | Pin ESP32-C3 Super Mini |
|---|---|
| I2C SDA (SGP40) | GPIO8 |
| I2C SCL (SGP40) | GPIO9 |
| MOSFET Gate (Lüfter) | GPIO4 |

Pins können anders gewählt werden – ändern Sie dann die Nummern im Code ([Kapitel 5](05-sensor-and-telemetry.md)).

## Anschlussregeln

1. **Gemeinsame Masse.** GND der Stromversorgung, ESP32, MOSFET-Modul und Sensor müssen verbunden sein. Die Hälfte von „funktioniert nicht" in DIY-Projekten – vergessene gemeinsame Masse.
2. **Sensor – nur auf 3,3 V.** SGP40 verträgt 5 V an der Stromversorgung nicht.
3. **Lüfter – nur über Schalter.** GPIO gibt Milliampere ab; ein Lüfter zieht hundert. Direkter Anschluss brennt den Anschluss durch. Wie ein MOSFET-Schalter funktioniert – [Transistoren und Schalter](../01-electronics-basics/02-mosfet-module.md).
4. **Externe Schutzdiode** für Computer-Lüfter ist normalerweise nicht erforderlich: Der Lüfter hat innen seine eigene Schalt-Elektronik und sieht von außen wie elektronische Last, nicht wie reine Induktivität. Aber beim Schalten der Stromversorgungsleitung mit Schalter (besonders mit PWM) ist eine Shunt-Diode parallel zum Lüfter nützlich als Schalter-Schutz vor induktiver Spannungsspitze – und falls sie bereits im Schalter-Modul vorhanden ist, ist das nur ein Plus.

!!! warning "Überprüfen Sie die Polarität vor dem Einschalten"
    Vertauschte + und − auf der 12-Volt-Leitung zerstören das buck-Modul und oft auch die Platine. Durchmessen Sie mit Multimeter vor der ersten Stromzufuhr.

## Überprüfung ohne Firmware

Nach dem Zusammenbau, vor dem Aufspielen der Hauptfirmware:

1. 12 V anlegen – ESP32 sollte sich als USB-Gerät im System identifizieren, wenn Kabel angesteckt ist (oder eine Strom-LED leuchtet).
2. Gate des MOSFET kurz über 1-kΩ-Widerstand auf 3,3 V bringen – Lüfter sollte anspringen.
3. I2C-Sensor prüfen wir direkt aus der Firmware mit dem Bus-Scanner in [Kapitel 5](05-sensor-and-telemetry.md).
