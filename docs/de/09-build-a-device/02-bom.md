---
title: "Zusammensetzung des beheizten Schranksystems: Komponenten und zwei Versionen der Leistungsstufe"
description: "Komponentenliste für einen selbstgebauten beheizten Schrank auf ESP32: SHT31-Sensor, Thermistor, Heizelement und Lüfter in Niedrigspannungsversion (24V) und Netzversion (220V)."
---

# Zusammensetzung des Systems

Auf dieser Seite finden Sie die Komponentenliste des Geräts und zwei Varianten der Leistungsstufe. Der Steuerkreis (Regler und Sensoren) ist in beiden Versionen identisch. Unterschiedlich ist nur die Schaltung des Heizelements und des Lüfters.

## Steuerkreis (gemeinsam für beide Versionen)

| Knoten | Zweck | Anmerkung |
|------|------------|------------|
| ESP32-C3 oder ESP32-S3 | Regler: Logik, Wi-Fi, Portal | DevKit oder Super Mini geeignet |
| Sensor SHT31 | Temperatur und Luftfeuchtigkeit im Schrank | I2C-Schnittstelle |
| Thermistor NTC 100K | Temperaturkontrolle des Heizelements | Beispiel: Generic 3950 |
| Pullup-Widerstand für Thermistor | Spannungsteiler für ADC | Typischerweise `4,7 kΩ` |
| Stromversorgung | Stromversorgung für Regler und Niedrigspannungsperipherie | Spannung gemäß gewählter Version |

C3 und S3 sind für dieses Beispiel gleichwertig: Code, Anschluss und Montagereihenfolge sind identisch. Der S3 ist leistungsfähiger und hat mehr Speicher — nehmen Sie ihn, wenn Sie später ein Display, einen adressierbaren LED-Streifen oder eigene aufwendige Logik hinzufügen möchten.

ESP32 wurde ausgewählt, weil er Wi-Fi enthält, über die erforderlichen Schnittstellen verfügt (I2C für SHT31, ADC für Thermistor, PWM zur Laststeuerung) und direkt von `idryer-core` unterstützt wird. Weitere Details — [ESP32-Regler](../02-controllers/01-esp32-controller.md).

!!! warning "ESP32-Logik — 3.3V"
    ESP32 arbeitet mit `3.3V`. Geben Sie nicht `5V` auf seine Anschlüsse. Dies gilt für Sensoren, Module und Adapter. Weitere Details — [Regelfehler](../08-common-mistakes/04-controller-mistakes.md).

## Sensoren

**SHT31** misst Temperatur und Luftfeuchtigkeit der Luft im Schrank. Dies ist die wichtigste Rückmeldung: Damit können Sie sehen, ob das eingestellte Klima eingehalten wird. Verbindung über I2C (zwei Leitungen: `SDA`, `SCL`). Weitere Details — [Thermistoren und Klimasensoren](../03-common-components/04-thermistors.md).

**Thermistor** misst die Temperatur des Heizelements selbst, nicht der Luft. Es ist erforderlich, um zu verhindern, dass das Heizelement überhitzt: Die Luft wärmt sich langsam auf, das Heizelement schnell. Der Thermistor wird als Spannungsteiler an einen ADC-Anschluss angeschlossen. [Thermistor-Überprüfung](../06-practical-guides/02-checking-thermistor.md).

!!! note "Warum zwei Wärmesensoren"
    SHT31 zeigt „wie warm ist es im Schrank", Thermistor — „ist das Heizelement überhitzt". Der erste legt das Ziel fest, der zweite schützt vor Notfällen.

## Leistungsstufe: Wählen Sie eine Version

Heizelement und Lüfter sind Last, die vom Regler gesteuert werden. ESP32 kann eine solche Last nicht direkt schalten: Sein Anschluss liefert nur ein schwaches Signal `3.3V`. Zwischen dem Regler und der Last ist ein Schalter erforderlich.

Es gibt zwei grundlegend unterschiedliche Versionen. Wählen Sie eine, je nachdem welches Heizelement und welchen Lüfter Sie verwenden.

### Version A — Niedrigspannung (24V oder 12V)

Heizelement und Lüfter werden mit `24V` (oder `12V`) Gleichstrom versorgt. Dies ist ein einfacherer und sicherer Weg für die Eigenentwicklung.

| Knoten | Komponente |
|------|-----------|
| Heizelement | Heizelement `12V` oder `24V` (PTC-Heizer) |
| Lüfter | Lüfter `24V` oder `12V` (2-Pin oder 4-Pin) |
| Heizelement-Schalter | MOSFET-Modul |
| Lüfter-Schalter | MOSFET-Modul (oder 4-Pin PWM direkt) |
| Stromversorgung | `24V DC` mit Leistungsreserve |

Der Regler steuert das MOSFET-Modul mit einem Signal vom ESP32-Anschluss. Das Modul schaltet die Niedrigspannungslast. Dies ist die gleiche Logik wie bei einem fertigen Regler. Weitere Details — [MOSFET-Modul](../01-electronics-basics/02-mosfet-module.md).

Die Stromversorgungsleistung wird für die Gesamtlast mit Reserve berechnet — siehe [Berechnung des Laststroms 24V](../01-electronics-basics/01-load-calculation-24v.md).

!!! note "Empfohlene Version für das erste Gerät"
    Wenn Sie das Gerät zum ersten Mal zusammenbauen, beginnen Sie mit Version A. Hier gibt es keine Netzspannung auf der Last, und Montagefehler sind weniger gefährlich.

### Version B — Netzspannung (110–230V AC)

Heizelement und Lüfter werden über das Netzwerk `110–230V` versorgt. Dies wird gemacht, wenn ein leistungsstarkes Netzheizsystem benötigt wird — z.B. ein vorgefertigtes Heizelement mit Lüfter für einen Schrank. Hier werden anstelle von MOSFET-Modulen Module zur Schaltung von Wechselstrom verwendet.

| Knoten | Komponente |
|------|-----------|
| Heizelement | Netzheizer `110–230V AC` |
| Lüfter | Netzlüfter `110–230V AC` |
| Heizelement-Schalter | Halbleiterrelais (SSR) für AC |
| Lüfter-Schalter | SSR oder gewöhnliches Relais für AC |
| Stromversorgung | Separate `24V`/`5V DC` Stromversorgung für Regler und Sensoren |
| Schutz | Sicherung, Schutzerdung des Gehäuses |

!!! danger "Netzspannung ist lebensgefährlich"
    Version B arbeitet mit Spannung `110–230V`. Ein Montagefehler kann zu Stromschlag oder Brand führen. Lesen Sie vor dem Zusammenbau unbedingt die Sicherheitsmaterialien: [Triac](../01-electronics-basics/03-triac.md), [Halbleiterrelais (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md), [Fehler bei Heizelementen und SSR](../08-common-mistakes/05-heater-ssr-mistakes.md). Wenn Sie keine Erfahrung mit Netzspannung haben, wählen Sie Version A.

Der Regler und die Sensoren in Version B werden immer noch von einer separaten Niedrigspannungsquelle (`5V`/`24V`) versorgt. Die Netzseite und die Schwachstromseite müssen physisch und elektrisch getrennt sein.

## Optionale Module

Diese Knoten sind für den Schrank nicht erforderlich, werden aber vom Core unterstützt und können später hinzugefügt werden:

- adressierbare LED-Beleuchtung (`hasLed`);
- Gewichtssensor für Filamentverbrauch (`hasWeight`);
- RFID-Tag der Spule (`hasRfid`).

Der Basic-Schrank nutzt diese nicht — beginnen Sie mit dem Nötigsten.

## Was kommt als Nächstes

Wenn die Komponenten ausgewählt sind, gehen Sie zu [Schaltplan](03-wiring.md): welcher ESP32-Anschluss für was zuständig ist und wie man die Schwachstrom- und Leistungsteile aufteilt.
