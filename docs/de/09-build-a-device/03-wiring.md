---
title: "Schaltplan eines beheizbaren Schranks auf ESP32"
description: "ESP32-Pin-Belegung für einen selbstgebauten Schrank: SHT31 über I2C, Thermistor auf ADC, Heizer und Lüfter über Schalter. Trennung von Niederspannungs- und Hochspannungsteil."
---

# Schaltplan

Diese Seite zeigt, wie Sie die Komponenten um den ESP32 verbinden. Zunächst eine allgemeine Pin-Belegungstabelle, dann die Verbindung jedes Knotens und die Regeln für die Hochspannungsverdrahtung.

!!! warning "Überprüfen Sie zunächst die Pin-Belegung Ihrer Platine"
    Die unten angegebenen Pin-Nummern sind ein Beispiel. Bei verschiedenen ESP32-C3- und ESP32-S3-Platinen sind die Nummerierung und Anordnung der Pins unterschiedlich. Überprüfen Sie vor dem Zusammenbau die Pin-Belegung genau Ihres Entwicklungsboards: bei DevKit und Super Mini ist sie vom Hersteller veröffentlicht und in der Platinenbeschreibung zu finden. Nicht alle Pins können frei verwendet werden: Einige sind für das Booten, Flash oder USB reserviert.

## Pin-Belegungstabelle (Beispiel)

| Knoten | Leitung | ESP32-Pin (Beispiel) |
|--------|---------|----------------------|
| SHT31 | `SDA` | GPIO8 |
| SHT31 | `SCL` | GPIO9 |
| Thermistor | ADC-Signal | GPIO2 |
| Heizer (Schalter) | Steuerung | GPIO4 |
| Lüfter (Schalter/PWM) | Steuerung | GPIO5 |

Die Sensorversorgung erfolgt über `3.3V` und `GND` der Platine. Der Hochspannungsteil wird separat versorgt.

## SHT31 über I2C

Der SHT31 wird mit vier Drähten verbunden:

1. `VCC` des Sensors — zum `3.3V` der Platine.
2. `GND` des Sensors — zum `GND` der Platine.
3. `SDA` des Sensors — zum `SDA`-Pin (Beispiel: GPIO8).
4. `SCL` des Sensors — zum `SCL`-Pin (Beispiel: GPIO9).

Die I2C-Leitungen sind kurz. Wenn der Sensor weit entfernt von der Platine angebracht ist, halten Sie die Drähte so kurz wie möglich und verdrehen Sie sie. Die meisten SHT31-Module haben bereits Pull-up-Widerstände auf der Modulplatine.

!!! note "SHT31-Adresse"
    Der SHT31 hat normalerweise die Adresse `0x44` (manchmal `0x45`). Wenn der Sensor nicht antwortet, überprüfen Sie die Adresse und die Leitungen `SDA`/`SCL`.

## Thermistor auf ADC

Der Thermistor wird in einen Spannungsteiler zusammen mit einem Pull-up-Widerstand eingebaut:

1. Ein Pin des Thermistors — zum `3.3V`.
2. Der andere Pin des Thermistors — zum Verbindungspunkt mit dem `4.7 kΩ`-Widerstand und zum ADC-Pin (Beispiel: GPIO2).
3. Der andere Pin des `4.7 kΩ`-Widerstands — zum `GND`.

Der Controller misst die Spannung am mittleren Punkt des Teilers und berechnet daraus den Widerstand des Thermistors und dann die Temperatur. Der Thermistor-Typ wird in der Firmware angegeben (siehe [Heizungssteuerung](07-heating-control.md)).

Ausführliche Informationen zur Prüfung und zum Zusammenbau finden Sie unter [Thermistor-Prüfung](../06-practical-guides/02-checking-thermistor.md).

## Heizer und Lüfter über Schalter

Der ESP32 steuert die Last nicht direkt, sondern über einen Schalter. Welcher Schalter verwendet wird, hängt von der Version aus [Systemzusammensetzung](02-bom.md) ab.

### Version A (24V/12V) — MOSFET-Modul

1. Der Signalanschluss des Moduls (`PWM`/`SIG`) — zum Steuereingabe-Pin des ESP32 (Beispiel: GPIO4 für den Heizer, GPIO5 für den Lüfter).
2. `GND` des Moduls — zum gemeinsamen `GND` mit dem ESP32.
3. Der Stromversorgungseingabe des Moduls und die Last — zur `24V` Stromversorgung.

!!! warning "Gemeinsame Erde"
    Der `GND` des Controllers und der `GND` der Hochspannungsstromversorgung müssen verbunden sein. Ohne gemeinsame Erde hat das Steuersignal kein Bezugspotenzial, und der Schalter funktioniert unvorhersehbar.

Die Verbindung des Lüfters mit Steuerung wird detailliert in [Lüfterverbindung](../06-practical-guides/01-connecting-fan.md) behandelt. Die Logik des Schalters — [MOSFET-Modul](../01-electronics-basics/02-mosfet-module.md).

### Version B (220V) — SSR/Relais

!!! danger "Vor der Montage des Netzteils"
    Alle Anschlüsse zum Stromnetz müssen erfolgen, wenn das Gerät vollständig stromlos ist. Das Gehäuse mit dem Netzteil muss Schutzerdung und Sicherung haben. Netzkabel müssen ausreichend dimensioniert sein und sicher in den Klemmen befestigt sein.

Das SSR hat zwei Seiten. **Steuerseite** — Niederspannungseingabe, die vom Controller gesteuert wird. **Leistungsseite** — Anschlüsse, durch die die Netzspannung der Last fließt. Die Seiten sind durch einen Optokoppler im SSR isoliert, daher kann das Netz mit einem schwachen `3.3V`-Signal gesteuert werden.

1. Der Steuereingabe ist normalerweise mit `DC+` und `DC-` gekennzeichnet (manchmal `+` und `-`) und ist für `3–32V` Gleichstrom ausgelegt. Verbinden Sie `DC+` mit dem Steuereingabe-Pin des ESP32 (Beispiel: GPIO4) und `DC-` mit dem `GND` des Controllers. Die Spannung von `3.3V` vom ESP32-Pin reicht aus, um das SSR zu öffnen.
2. Die Hochspannungsanschlüsse (oft als Netz/`AC` und Last/`LOAD` gekennzeichnet) werden in die Unterbrechung einer der Netzzuleitungen des Heizers eingebaut — genau wie ein Schalter im Draht.
3. Der Lüfter wird von einem separaten SSR oder Relais auf die gleiche Weise geschaltet.

!!! note "Warum SSR-Kühlkörper"
    Beim Schalten wird das SSR leicht warm, und je höher der Laststrom ist, desto stärker die Wärmeerzeugung. Daher wird das SSR an einen Kühlkörper (Metallplatte zur Wärmeableitung) geschraubt, und das SSR wird mit einer Stromreserve ausgelegt — deutlich höher als der Laststrom. Welche Reserve und Kühlkörper Sie für Ihren Strom benötigen — [Halbleiterrelais (SSR)](../01-electronics-basics/04-solid-state-relay-ssr.md).

## Verdrahtung: Niederspannungs- und Hochspannungsteil

- Halten Sie Signaldrähte (Sensoren, Steuerung) separat von Hochspannungsleitungen.
- Verlegen Sie die Thermistor- und I2C-Drähte nicht entlang der Hochspannungsleitungen des Heizers — dies ist eine Quelle von Störungen.
- Trennen Sie in Version B die Netzspannungs- und Niederspannungszonen physisch voneinander im Gehäuse.
- Verbinden Sie alle Massen des Niederspannungsteils an einem Punkt.

Störungen vom Lüfter und schlechte Erdung sind häufige Ursachen für „schwankende" Messwerte und Neustarts. Siehe [Verdrahtungsfehler](../08-common-mistakes/03-wiring-mistakes.md).

## Was vor der Stromversorgung überprüft werden sollte

- Sensorversorgung `3.3V`, nicht `5V`.
- Thermistor und Teiler-Widerstand sind richtig montiert, ADC-Pin am mittleren Punkt.
- Gemeinsame Erde des Controllers und der Hochspannungsstromversorgung.
- In Version B — Gehäuseerdung, Sicherung, sichere Klemmen, Isolierung.
- Keine Kurzschlüsse zwischen Stromversorgung und Erde (mit einem Multimeter durchmessen).

Überprüfung mit Multimeter — [Multimeter](../05-tools/02-multimeter.md).

## Was kommt als nächstes

Die Hardware ist zusammengebaut. Fahren Sie mit [Firmware-Start im Kern](04-firmware-start.md) fort: Erstellen Sie ein Projekt und bringen Sie das Gerät in den Online-Status auf dem Portal.
