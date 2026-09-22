---
title: "Intelligenter Luftfilter: Konzept und Vorteil des Portals"
description: "Durchgängiges Beispiel Nr. 2: Luftfilter für 3D-Druck-Bereiche auf ESP32 und idryer-core — eigener VOC-Sensor, eigene Steuerung und automatische Gerätekarte im Portal über ein Card-Manifest."
---

# Intelligenter Luftfilter: Konzept

Dies ist das zweite durchgängige Beispiel des Abschnitts „Selbst bauen“. Im [ersten Beispiel](../09-build-a-device/01-concept.md) haben Sie ein beheizbares Gehäuse aus „Wörterbuch"-Bausteinen des Ökosystems zusammengebaut: Temperatur, Luftfeuchtigkeit, Heizer. Hier machen wir einen Schritt weiter – wir bauen ein Gerät, das **es im iDryer-Ökosystem überhaupt nicht gibt**: einen Luftfilter mit Sensor für flüchtige organische Stoffe (VOC).

## Hauptidee: Das ist ein Konzept, kein Rezept für ein einzelnes Gerät

Bitte lesen Sie diesen Absatz sorgfältig – er ist wichtiger als der Rest des Kapitels.

Der Filter hier ist nur ein Beispiel. Der gezeigte Ansatz funktioniert für **jedes Gerät, das Sie erfinden**: Befeuchter, Gebläsestation, Abluftregler, Filamentlager-Monitor — was auch immer. Sie geben in der Firmware an, welche Sensoren und Steuerelemente das Gerät hat – mit einer oder zwei Codezeilen pro Sensor – und das Gerät **erscheint automatisch im Portal und in der mobilen App** mit einer fertigen Karte: Live-Messwerte, Tasten, Eingabefelder. Keine Codezeile im Portal, keine Absprache mit dem iDryer-Team, keine Pull-Requests.

Dies funktioniert dank des Mechanismus der **dynamischen Karten** (Entity Manifest): Das Gerät veröffentlicht eine maschinell lesbare Beschreibung „was anzeigen und steuern", und Portal und App bauen die Benutzeroberfläche nach dieser Beschreibung. Wie das im Code aussieht – [siehe das Kapitel über die Karte](06-card.md).

!!! note "Was das praktisch bedeutet"
    Gerät erfunden → auf ESP32 gebaut → Sensoren und Tasten in der Firmware beschrieben → in der App ans Konto gekoppelt. Fertig: Das Gerät hat eine Benutzeroberfläche im Portal und in der App. Die Distanz von der Idee bis „ich steuere vom Smartphone" – ein Abend.

## Was genau wir bauen

**Luftfilter für den 3D-Druck-Bereich**: Eine Box mit Lüfter, HEPA-Filter und Aktivkohle-Schicht, die:

- die Luftqualität mit einem VOC-Sensor (SGP40) misst;
- selbständig den Lüfter einschaltet, wenn die Luft schmutzig ist, und ausschaltet, wenn sie gereinigt wurde;
- den VOC-Index und den Lüfter-Status im Portal anzeigt;
- es ermöglicht, den Modus vom Portal zu wählen (`auto` / `on` / `off`) und die Auslöseschwelle anzupassen.

ABS und ASA riechen beim Drucken nach Styrol, Harze nach ihrem eigenen Aroma. Ein Filter beim Drucker – das ist nicht Luxus, das ist Hygiene.

## Warum das das ideale Anfänger-Projekt ist

Falls das Gehäuse aus Abschnitt 09 zu kompliziert erschien – beginnen Sie mit dem Filter:

- **kein Heizer** – also kein Leistungsteil, keine Thermosicherungen und keine Risiken;
- minimale Komponenten: Platine, Sensor, Lüfter, Transistor;
- Budget etwa `$15` ohne Gehäuse;
- bei jedem Fehler im Code ist das Schlimmste, das passiert, dass der Lüfter nicht angeht.

## Grenzen der Aufgabe

Seien wir ehrlich, was dieser Filter **nicht** ist:

- das ist keine Absauganlage: Die Luft wird durch den Filter im Kreis gepumpt, nicht nach draußen geblasen;
- das ist kein medizinisches Gerät: SGP40 zeigt einen relativen **Index** der Luftqualität, nicht die Konzentration eines bestimmten Gases in ppm;
- dieser Filter ersetzt das Lüften nicht.

!!! note "VOC oder CO2?"
    Für Druckdämpfe ist der richtige Sensor – VOC: er reagiert auf organische Stoffe (Styrol, Lösungsmittel). CO2-Sensoren (z. B. NDIR-Sensor MH-Z19) messen Kohlendioxid – das ist ein Indikator für Stickigkeit, nicht für Verschmutzung durch Druck. Wenn Sie beides wollen, gibt ENS160 sowohl VOC-Index als auch eCO2-Schätzung auf einmal; der Ansatz aus diesem Abschnitt ändert sich nicht – einfach noch eine Zeile im Card-Manifest.

## Route durch den Abschnitt

1. [Systemzusammensetzung](02-bom.md) – was kaufen.
2. [Schaltschema](03-wiring.md) – wie anschließen.
3. [Firmware-Start](04-firmware-start.md) – Gerüst auf `idryer-core`, Bindung an Portal.
4. [Sensor und Telemetrie](05-sensor-and-telemetry.md) – VOC lesen und in Cloud senden.
5. [Gerätekarte](06-card.md) – Sensoren und Steuerung deklarieren, Benutzeroberfläche abrufen.
6. [Automatik-Logik](07-auto-logic.md) – Schwelle, Hysterese, manuelle Steuerung vom Portal.
7. [Zusammenbau und Überprüfung](08-assembly-and-check.md) – finale Checkliste.
