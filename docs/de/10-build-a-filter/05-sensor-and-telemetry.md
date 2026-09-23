---
title: "Intelligenter Filter: VOC-Sensor und Telemetrie"
description: "SGP40 über I2C lesen und eigenes Feld vocIndex in iDryer-Telemetrie über onTelemetryPublish-Callback veröffentlichen."
---

# Sensor und Telemetrie

In diesem Kapitel beginnt der Filter, die Luft zu messen und Daten in die Cloud zu senden. Die Schlüsseltechnik ist das **eigene Feld in der Telemetrie**: Das Ökosystem-Wörterbuch weiß nichts über VOC, aber der Kern erlaubt, beliebige Felder zur Telemetrie hinzuzufügen.

## 1. Sensor-Bibliothek

Fügen Sie in `platformio.ini` zu `lib_deps`:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. SGP40 lesen

In `src/main.cpp` (Pins – aus dem [Schema](03-wiring.md)):

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = keine Daten noch

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() führt interne Sensor-Kompensation durch.
    // Index: ~100 = normale Luft, höher = schmutziger (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Rufen Sie `initVocSensor()` in `setup()` nach `s_link.begin()` auf und `readVocSensor()` – in `loop()` einmal pro Sekunde (mit millis-Timer, nicht mit `delay`!):

```cpp
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();
    }
}
```

!!! warning "Keine delay() in loop"
    `s_link.loop()` muss ständig aufgerufen werden – darauf hängen Wi-Fi, MQTT und Befehle vom Portal. `delay(1000)` einfrieren lässt all das. Nur millis-Timer.

## 3. Eigenes Feld in der Telemetrie

Alle `telemetryPeriodMs` Millisekunden sammelt der Kern selbst eine JSON-Telemetrie-Nachricht und sendet sie in die Cloud. Für unser Gerät (eine Unit, vom Wörterbuch nur Lüfter) sammelt der Kern diese Nachricht:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Zerlegen wir die Struktur:

- `units` – Array von **Units** (Kammern) des Geräts. Ein Serien-iDryer kann bis zu vier unabhängige Kammern haben, daher ist Telemetrie – immer ein Array, auch wenn die Kammer eine;
- `units[0]` – erste (und für uns einzige) Unit: Wir gaben `unitsCount = 1` in `Config` an;
- `fanStatus` – Wörterbuch-Feld, erschien wegen `hasFan = true`;
- `rssi`, `uptime` – Wi-Fi-Level und Betriebszeit, der Kern fügt immer hinzu.

VOC ist in dieser Nachricht nicht – der Kern kennt unseren Sensor nicht. Aber direkt vor dem Senden gibt der Kern Ihrem Code die Möglichkeit, eigene Felder zur Nachricht hinzuzufügen. Dazu registrieren Sie einen **Callback** (Rückruf) – eine Funktion, die Sie dem Kern übergeben, und der Kern ruft sie selbst bei jeder Veröffentlichung auf und übergibt die gesammelte JSON (Parameter `doc` – das ist sie).

In `setup()`:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc – die Telemetrie-Nachricht, die der Kern gesammelt hat (siehe JSON oben).
    // Wir schreiben das vocIndex-Feld zu unserer ersten Unit.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

Die Zeile `doc["units"][0]["vocIndex"] = g_vocIndex;` wird so gelesen: „in der Nachricht `doc` nimm das Array `units`, darin Element `0` (unsere einzige Unit) und schreib das Feld `vocIndex` hinein". Der Feldname ist Ihre Wahl – im [nächsten Kapitel](06-card.md) verweisen Sie auf ihn, um den Wert auf der Karte zu zeigen.

!!! note "Falls Sie das Wort Hook treffen"
    In den Quellen des Kerns heißt dieser Callback `PublishHook` – „Hook" (Haken) bedeutet das gleiche: ein Punkt, wo die Bibliothek Ihre Funktion „anhängen" lässt. Die Begriffe sind austauschbar; in dieser Dokumentation sagen wir „Callback".

!!! note "Lambda und warum sie „leer" ist"
    Die Konstruktion `[](JsonObject doc) { ... }` heißt **Lambda** – das ist eine Funktion ohne Namen, geschrieben direkt am Einsatzort, um sie nicht separat auszulagern und einen Namen zu erfinden.

    Die eckigen Klammern am Anfang – „Capture-Liste": Darin werden lokale Variablen aufgelistet, die die Funktion mitnimmt. Kernregel: **Klammern sind immer leer** (`[]`) – die Lambda erfasst nichts und trägt keinen Zustand mit sich (das heißt auf Englisch *stateless*, „ohne Zustand").

    Der Grund ist technisch: Lambdas mit Erfassung brauchen dynamischen Speicher, und häufige Zuordnungen auf ESP32 fragmentieren den Heap und können Wi-Fi zum Absturz bringen. Darum akzeptiert der Kern nur einfache Funktionen.

    Praktisches Ergebnis: Alles, das der Callback braucht, speichern Sie in **globalen** Variablen – wie unser `g_vocIndex`. Diese Regel gilt für alle Callbacks des `idryer-core`.

Der Lüfter-Status wird über das Wörterbuch veröffentlicht – schreiben Sie ihn einfach ins Kernfeld, wenn Sie ihn ein-/ausschalten (Logik – in [Kapitel 7](07-auto-logic.md)):

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. Kein Sensor zur Hand? Demo-Modus

Den Weg bis zur Karte kann man auch ohne SGP40 gehen: den Index berechnet ein Luftmodell. Kopieren Sie die Datei `demo_voc.h` aus dem Beispiel:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

und fügen Sie in `platformio.ini` ein Build-Flag hinzu:

```ini
build_flags =
    -DDEMO_VOC=1
```

Beide Zweige sind hinter einer Funktion versteckt, und `loop()` weiß nicht, woher der Wert kam:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

Das Modell verhält sich wie ein Raum, in dem ein Drucker druckt: solange der Lüfter steht, kriecht der Index vom Hintergrundwert um `100` nach oben, und bei laufendem Lüfter fällt er. Den Lüfter-Status nimmt das Modell aus Ihrer eigenen Logik, deshalb sind Schwelle und Hysterese aus [Kapitel 7](07-auto-logic.md) in Aktion zu sehen.

Für ein Arbeitsgerät wird das Flag nicht gesetzt: dann wird der Zweig mit dem echten Sensor gebaut.

## 5. Überprüfung

Nach der Programmierung sieht die Telemetrie im MQTT-Stream des Geräts (oder im Serial-Log der Veröffentlichungen) so aus:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` – Ihr eigenes Feld, das in die Cloud geht neben dem Wörterbuch-`fanStatus`. Das Portal erhält es bereits, weiß aber noch nicht, was damit anfangen: Zeigen Sie das im nächsten Kapitel.

!!! note "Das eigene Feld lebt in Echtzeit"
    Das Portal verteilt die Telemetrie an die Clients so, wie sie ist, deshalb zeigt die Karte `vocIndex` sofort. In die Historie für die Graphen schreibt der Server aber nur Wörterbuch-Größen – Temperatur, Luftfeuchtigkeit, Heizung. Ihr eigenes Feld wird im Telemetrie-Graphen nicht auftauchen; wenn Sie Historie brauchen, sammeln Sie sie bei sich (zum Beispiel Home Assistant oder eine eigene Datenbank).

Atmen Sie auf den Sensor oder bringen Sie einen Marker heran – der Index sollte sich innerhalb von Sekunden merklich erhöhen.
