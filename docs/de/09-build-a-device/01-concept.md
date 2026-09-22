---
title: "Bauen Sie Ihr eigenes Gerät auf dem iDryer-Kern: Konzept"
description: "Durchgehendes Beispiel: wie Sie von Grund auf einen beheizten Aufbewahrungsschrank für Filament auf ESP32 und der idryer-core-Bibliothek mit Verbindung zum iDryer-Portal bauen."
---

# Bauen Sie Ihr eigenes Gerät: Konzept

Dieser Abschnitt ist ein durchgehendes Beispiel. In den vorherigen Abschnitten wurden einzelne Bausteine erklärt: Stromversorgung, Steuerungen, Sensoren, Heizer, Sicherheit. Hier bauen Sie diese Bausteine zu einem fertigen Gerät zusammen und bringen es in einen betriebsbereiten Zustand mit Verbindung zum [iDryer-Portal](https://portal.idryer.org/).

Das Beispiel basiert auf der `idryer-core`-Bibliothek. Die Bibliothek übernimmt die gesamte Netzwerk-Integration: Wi-Fi-Verbindung, Kontobindung, sichere MQTT-Sitzung, periodische Telemetrie-Veröffentlichung. Sie schreiben nur das, das für Ihr Gerät spezifisch ist: Sensorablesung, Heizer- und Lüftersteuerung, Logik zur Temperaturregelung.

## Was bauen wir genau

Wir bauen einen **beheizten Aufbewahrungsschrank für Filament**. Das ist ein geschlossener Schrank für 10–40 Spulen, in dem eine Temperatur von etwa `40–45 °C` aufrechterhalten wird.

Es ist wichtig, die Grenzen der Aufgabe von Anfang an festzulegen.

!!! note "Das ist kein Hochtemperatur-Trockner"
    Wir behaupten nicht, schnelles Trocknen bei hoher Temperatur zu bieten. Das Ziel des Geräts ist es, im Schrank sanfte Wärme aufrechtzuerhalten, die das Filament bei der Lagerung trocken hält.

Eine Temperatur von `40–45 °C` reicht aus, um die meisten anspruchslosen Kunststoffe – von PLA bis ABS – in trockenem Zustand zu lagern. Für aktives Trocknen anspruchsvoller Materialien (Nylon, Polycarbonat, PA-CF) sind höhere Temperaturen und eine andere Konstruktion erforderlich – solche Trockner werden separat nach den Prinzipien aus den anderen Abschnitten gebaut.

## Warum das selbst tun

Der fertige iDryer-Steuerung kann bereits alles, was unten beschrieben ist. Dieses Beispiel ist nicht als Ersatz gedacht, sondern um zu zeigen, **wie das Gerät von innen funktioniert**, und eine Grundlage für eigene Module zu geben.

Eigenständiger Aufbau ist sinnvoll, wenn:

- Sie einen Schrank in nicht standardisierter Größe oder Form benötigen;
- Sie verstehen möchten, wie die Steuerung die Heizung kontrolliert und mit dem Portal kommuniziert;
- Sie Ihr eigenes Modul im Ökosystem erstellen möchten und dieses Beispiel als Ausgangspunkt nehmen.

## Wie unterscheidet sich das vom V2-Steuerung

Der Serien-iDryer-V2-Steuerung ist zweiprozessor-basiert: Die Hauptlogik läuft auf einem separaten Mikrocontroller, und das ESP32-Modul fungiert nur als Brücke zu Wi-Fi und dem Portal. Das ist für ein Serienprodukt mit Bildschirm, Waagen, RFID und mehreren Kameras gerechtfertigt.

Für einen selbstgebauten Schrank ist diese Komplexität nicht erforderlich. Wir vereinfachen die Architektur auf einen **einzelnen ESP32**, der alles selbst macht:

- liest Sensoren;
- steuert Heizer und Lüfter;
- verbindet sich mit Wi-Fi und dem Portal über `idryer-core`.

Funktional wiederholen wir das Verhalten einer Kammer des V2-Steuerung (Klimasensor, Heizer mit Rückkopplung durch Thermistor, Lüfter), aber in ehrlicher DIY-Ausführung auf einer einzigen Platine.

!!! note "Servo wird nicht verwendet"
    In der V2-Steuerung steuert ein Servomotor die Luftklappe der Kammer. Für einen Aufbewahrungsschrank mit gleichmäßiger sanfter Heizung ist die Klappe nicht erforderlich, daher gibt es in diesem Beispiel keinen Servomotor.

## Was die Verbindung mit dem Kern bietet

Wenn das Gerät auf `idryer-core` gebaut und an ein Konto gebunden ist, erhalten Sie ohne zusätzlichen Code:

- Steuerung und Überwachung über das [Portal](https://portal.idryer.org/) und mobile App;
- Diagramme für Temperatur und Luftfeuchtigkeit im Schrank;
- Fernstart und -stop des Heizmodus;
- Parametereinstellung (Zieltemperatur, Hysterese) über das Geräte-Menü.

!!! note "Eigene Sensoren und Steuerelemente auf der Gerätekarte"
    Die Gerätekarte ist nicht auf die Wörterbuchfähigkeiten (`has*`) beschränkt. Über das Card-Manifest kann die Firmware beliebige eigene Sensoren und Steuerelemente deklarieren — sie erscheinen automatisch im Portal und in der App. Wie das funktioniert, zeigt das Durchgangsbeispiel [„Intelligenter Luftfilter"](../10-build-a-filter/01-concept.md), insbesondere [das Kapitel über die Karte](../10-build-a-filter/06-card.md). Start und Stopp der Wärmehaltung werden in [Kapitel 7](07-heating-control.md) als Aktionen der Karte deklariert.

## Was ist in diesem Abschnitt enthalten

Im Folgenden geht der Pfad Schritt für Schritt von der leeren Platine zum funktionsfähigen Schrank:

1. [Systemzusammensetzung](02-bom.md) – welche Komponenten wir nehmen und zwei Versionen des Stromteils (Niedrigspannung und Netzspannung).
2. [Schaltplan](03-wiring.md) – ESP32-Pin-Zuordnung, Trennung von Signalspannung und Stromteil, Sicherheit.
3. [Firmware-Start auf dem Kern](04-firmware-start.md) – PlatformIO-Projekt, erster Start, Portalbindung.
4. [Sensoren](05-sensors.md) – verbinden Sie SHT31 und Thermistor, lesen Sie Daten von ihnen.
5. [Menü aus YAML](06-menu.md) – beschreiben Sie Geräteeinstellungen, sie landen in NVS und auf dem Portal.
6. [Heizsteuerung](07-heating-control.md) – Logik zur Temperaturregelung, Lüfter, Start und Stopp über die Gerätekarte.
7. [Montage und Überprüfung](08-assembly-and-check.md) – Endmontage, erste Erwärmung, Sicherheits-Checkliste.

!!! tip "Fertiges Beispiel"
    Wenn Sie sofort das Ergebnis sehen möchten – das fertige Projekt liegt im Ordner `example/09-cabinet/` des Repositorys und wird mit dem Befehl `pio run -e cabinet` gebaut. Die folgenden Kapitel analysieren diesen Code Schritt für Schritt.

## Siehe auch

- [Wo Sie anfangen sollen](../00-start-here/01-introduction.md) – Gesamtlesereihenfolge des Abschnitts.
- [ESP32-Steuerung](../02-controllers/01-esp32-controller.md) – warum ESP32 für ein Gerät mit Wi-Fi praktisch ist.
- [Häufige Komponenten](../03-common-components/01-overview.md) – Komponentenkarte des Geräts.
