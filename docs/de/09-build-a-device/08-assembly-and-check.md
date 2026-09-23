---
title: "Montage des beheizten Schranks und Überprüfung vor dem Start"
description: "Endmontage des DIY-Schranks auf ESP32: Einbau in das Gehäuse, erste Aufwärmphase, Temperaturkalibrierung und Sicherheitsprüfung vor dem regulären Betrieb."
---

# Montage und Überprüfung

Auf dieser Seite bauen Sie das Gerät in das Gehäuse ein, führen die erste kontrollierte Aufwärmphase durch und überprüfen, dass der Schrank sicher funktioniert. Führen Sie die Überprüfungen der Reihe nach durch und lassen Sie das Gerät beim ersten Einschalten nicht unbeaufsichtigt.

## Montagereihenfolge

1. Befestigen Sie den ESP32 und die Leistungsteile im Gehäuse so, dass die Schwach- und Hochspannungszonen getrennt sind.
2. Platzieren Sie den SHT31-Sensor im Schrank abseits des direkten Stroms vom Heizer – sonst misst er die Temperatur des Strahls statt der Luft im Volumen.
3. Befestigen Sie den Thermistor in thermischem Kontakt mit dem Heizer.
4. Überprüfen Sie, dass die Leitungen den Heizer nicht berühren und nicht in den Lüfter geraten.
5. In Version B (`220V`) stellen Sie sicher, dass die Netzleitungen in den Klemmen befestigt sind, die Isolierung intakt ist und das Gehäuse geerdet ist.

Anforderungen an Gehäuse und Anordnung der Baugruppen – [Gehäusedesign](../07-3d-printing/05-enclosure-design.md).

!!! warning "Gedruckte Teile in der Nähe von Wärmequellen"
    PLA wird bei Temperaturen weich, die leicht in der Nähe des Heizers auftreten. Teile in der Nähe von Wärme sollten aus hitzebeständigem Material gedruckt werden: ABS, ASA oder PA (Nylon). Siehe [Hitzebeständige Materialien](../07-3d-printing/04-heat-resistant-materials.md) und [Warum PLA riskant ist](../07-3d-printing/06-why-pla-is-risky.md).

## Überprüfung vor Stromversorgung

Überprüfen Sie mit einem Multimeter vor dem ersten Einschalten:

- kein Kurzschluss zwischen Stromversorgung und Masse;
- Sensorversorgung `3.3V`, nicht `5V`;
- gemeinsame Masse von Regler und Stromversorgungseinheit;
- Thermistor und Spannungsteiler-Widerstand sind richtig zusammengebaut;
- in Version B – Gehäuseerdung und Sicherung vorhanden.

Zur Verwendung eines Multimeters – [Multimeter](../05-tools/02-multimeter.md).

## Erster Start

1. Versorgen Sie zunächst nur den Regler und die Sensoren mit Strom. Wenn ein einziges Netzteil alles versorgt, versorgen Sie den Regler separat: über USB oder einen Abwärtswandler, und schließen Sie die Leitung des Heizers noch nicht an.
2. Stellen Sie sicher, dass das Gerät online auf dem Portal angezeigt wird und Temperatur und Luftfeuchtigkeit anzeigt.
3. Verbinden Sie den Heizer und den Lüfter.
4. Starten Sie den Wärmehaltmodus vom Portal aus und beobachten Sie.

!!! danger "Lassen Sie die erste Aufwärmphase nicht unbeaufsichtigt"
    Überwachen Sie das Gerät beim ersten Einschalten. Stellen Sie sicher, dass der Heizer beim Erreichen des Ziels und durch den Thermistor-Schutz ausgeschaltet wird, nicht kontinuierlich heizt.

Was in den ersten Minuten zu beobachten ist:

- die Lufttemperatur steigt und stabilisiert sich um das Ziel;
- die Heizer-Temperatur übersteigt die eingestellte Grenze nicht;
- die Heizung schaltet sich beim Erreichen des Ziels aus und schaltet sich nach dem Abkühlen um den Hysteresewert wieder ein;
- der Lüfter läuft und berührt keine Leitungen;
- der Regler startet nicht neu, wenn die Last eingeschaltet wird.

## Kalibrierung

Nach der ersten Aufwärmphase vergleichen Sie die Messwerte mit einem separaten Thermometer im Schrank:

- wenn die Lufttemperatur im Schrank vom Ziel abweicht – überprüfen Sie die Platzierung des SHT31 (er sollte nicht im Strahl oder an der Wand stehen);
- wenn die Heizer-Temperatur unrealistisch aussieht – überprüfen Sie den Thermistor-Typ und den Widerstand des Spannungsteilers;
- passen Sie bei Bedarf die Zieltemperatur und Hysterese im [Menü](06-menu.md) an.

## Wenn etwas nicht funktioniert

| Symptom | Überprüfen Sie |
|---------|---|
| Regler startet bei Lastbetrieb neu | [Stromversorgungsfehler](../08-common-mistakes/02-power-mistakes.md) |
| Sensor zeigt Unsinn | [Verdrahtungsfehler](../08-common-mistakes/03-wiring-mistakes.md), [Thermistor-Überprüfung](../06-practical-guides/02-checking-thermistor.md) |
| Gerät verbindet sich nicht mit Wi-Fi | [Reglerfehler](../08-common-mistakes/04-controller-mistakes.md) |
| Heizer/SSR wird sehr heiß | [Fehler bei Heizer und SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

Allgemeine Diagnoseabfolge – [Diagnose-Checkliste](../08-common-mistakes/06-diagnostic-checklist.md).

## Checkliste vor regulärem Betrieb

- [ ] Das Gerät hält die Zieltemperatur und heizt nicht kontinuierlich.
- [ ] Der Schutz des Heizers durch den Thermistor funktioniert.
- [ ] Die Leitungen berühren den Heizer und den Lüfter nicht.
- [ ] Gedruckte Teile in der Nähe von Wärme sind hitzebeständig.
- [ ] In Version B: Gehäuse ist geerdet, Sicherung installiert, Isolierung intakt.
- [ ] Die Daten auf dem Portal stimmen mit der tatsächlichen Temperatur im Schrank überein.

## Fazit

Sie haben einen beheizten Speicherschrank auf ESP32 und `idryer-core` gebaut: Das Gerät liest Klima und Heizer-Temperatur, hält die eingestellte Temperatur, schützt den Heizer vor Überhitzung und wird vom Portal aus gesteuert. Dies ist eine vollständige Grundlage, auf der Sie Ihre eigenen Ökosystem-Module aufbauen können.

Weitere Komponenten – Beleuchtung, Waagen, RFID – werden auch vom Kern unterstützt; sie können nach dem gleichen Schema hinzugefügt werden: Sensor oder Peripherie → Telemetrie oder Befehl → Anzeige auf dem Portal.
