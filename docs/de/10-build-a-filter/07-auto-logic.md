---
title: "Intelligenter Filter: Automatik-Logik"
description: "VOC-Index-Schwelle und Hysterese, auto/on/off-Modi vom Portal, Einstellungen-Speicherung in NVS und Lüfter-Status-Veröffentlichung."
---

# Automatik-Logik

Verbinden wir alles: Der Sensor entscheidet, der Lüfter dreht, das Portal steuert.

## 1. Status und Einstellungen

Am Anfang von `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC-Index zum Einschalten
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) – nicht-flüchtiger Speicher des ESP32: Modus und Schwelle überleben Neustart.

## 2. Lüfter-Steuerung

Alles Ein- und Ausschalten fassen wir in eine Funktion `setFan`. Sie nimmt ein Argument `on` – gewünschter Status: `true` = einschalten, `false` = ausschalten. Im Rest des Codes rufen wir immer `setFan(true)` / `setFan(false)` auf, sie macht ganze Routine: dreht Pin, merkt sich Status und meldet Portal.

```cpp
static void setFan(bool on) {      // on – Argument: true = einschalten, false = ausschalten
    if (g_fanOn == on) return;     // schon in gewünschtem Status – nichts tun
    g_fanOn = on;                  // neuen Status in globaler Variablen merken
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // physisch Lüfter-Schalter ein-/ausschalten

    // Status dem Kern melden: fanOn[0] – Wörterbuch-Telemetrie-Feld
    // (erschien aus hasFan = true; [0] – unsere einzige Unit, wie in Kapitel 5).
    // Daher geht es in die Cloud und auf die „Lüfter"-Zelle der Karte.
    s_link.telemetry.fanOn[0] = on;

    // Statusänderung – Grund, Telemetrie sofort zu senden, nicht auf Periode zu warten.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` macht Antwort sofortig: Knopf im Portal gedrückt – innerhalb einer Sekunde zeigt Karte bestätigten Status. Genau bestätigten: iDryer-Portal „rät" nie Status, es zeigt, was Gerät wirklich gesendet hat.

## 3. Automatik mit Hysterese

Wenn Lüfter genau auf Schwelle schaltet, daran flattert er ein/aus. Abhilfe – Lücke:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: schalten an Schwelle ein, 20 Punkte darunter aus.
    if (g_vocIndex < 0) return;                      // Sensor noch stumm
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

Aufrufen `tickAutoLogic()` dort, wo wir Sensor lesen – in `loop()` per Sekunden-Timer. Das ist eben der `loop()` aus Kapitel 5, dazu eine Zeile. Ganz sieht es jetzt so aus:

```cpp
void loop() {
    s_link.loop();                        // Netz, Telemetrie, Befehle – immer zuerst

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // pro Sekunde:
        s_lastReadMs = now;
        readVocSensor();                  //   VOC lesen (Kapitel 5)
        tickAutoLogic();                  //   und sofort über Lüfter entscheiden
    }
}
```

Reihenfolge innerhalb Sekunden-Block nicht zufällig: erst frischer Sensor-Messwert, dann Entscheidung.

## 4. Callbacks vom Portal

Jene Funktionen, die wir in [Kapitel 6](06-card.md) versprachen:

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // sofort anwenden, nicht auf nächsten Tick warten
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Diese Funktionen **ersetzen** Stubs aus Kapitel 6 – leere Versionen löschen.

Achten Sie, was in diesem Code **nicht** ist: MQTT-Parsing, Topics, JSON-Befehle. Nutzer wählte `on` in der Portal-Liste → Kern fing Befehl auf, überprüfte und rief `onModeSelected("on")` auf. Ganze Transport-Mechanik – Kernverantwortung.

## 5. Endgültiges setup()

Noch zwei Dinge hinzufügen zu `setup()`: Laden gespeicherter Einstellungen aus NVS (am Anfang, damit Logik gleich damit arbeitet) und Einstellen des Lüfter-Pins. Ganz `setup()` nach diesem Kapitel so:

```cpp
void setup() {
    Serial.begin(115200);

    // Einstellungen aus NVS: was Nutzer in früheren Male wählte.
    s_prefs.begin("filter");   // „filter"-Namespace in NVS öffnen
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Zweite Argumente getUChar/getInt – Standard-Werte: rückgeben bei
    // erstem Start, wenn in NVS noch nichts gespeichert.

    pinMode(FAN_PIN, OUTPUT);  // Lüfter-Schlüssel-Pin – auf Ausgang

    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrie: eigenes Feld vocIndex (Kapitel 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karte: Sensor + Steuerelemente (Kapitel 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Layout (Kapitel 6, optional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Szenarien-Überprüfung

| Aktion | Erwartet |
|---|---|
| Modus `auto`, auf Sensor pusten | VOC wächst, auf Schwelle Lüfter anspringt, Karte zeigt „An" |
| Luft gereinigt | Unter Schwelle−20 Lüfter schaltet selbst aus |
| Modus `on` vom Portal | Lüfter dreht unabhängig von VOC |
| Modus `off` vom Portal | Lüfter still, VOC zeigt weiter |
| Platte neu gestartet | Modus und Schwelle gespeichert |

## 7. Kompletter Code: src/main.cpp ganz

Ganzer Code Kapitel 4–7, in einer Datei gesammelt. Falls etwas mit Ihrem nicht stimmt – vergleichen Sie mit diesem Listing.

```cpp
// ============================================================
// Intelligenter Luftfilter auf idryer-core.
// SGP40 (VOC) + Lüfter per MOSFET, auto/Handbetrieb,
// Steuerung und Karte im Portal durch Card-Manifest.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>

// ── Pins ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // Lüfter-MOSFET Gate
// SDA=8, SCL=9 – vorgeben in Wire.begin() unten

// ── Geräte-Passpord (Kapitel 4) ────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // unbekanntes Gerät
    .unitsCount        = 1,
    .hasFan            = true,        // einzige Wörterbuch-Fähigkeit
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── Status (Kapitel 7) ─────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC-Index zum Einschalten
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: Einstellungen überleben Neustart

// ── VOC-Sensor (Kapitel 5) ────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = noch keine Daten

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // Index: ~100 = normale Luft, höher = schmutziger (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
}

// ── Lüfter (Kapitel 7) ────────────────────────────────────
static void setFan(bool on) {         // on: true = einschalten, false = ausschalten
    if (g_fanOn == on) return;        // schon in gewünschtem Status
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // Wörterbuch-Feld → Cloud → Karte
    s_link.publishTelemetryNow();     // Status-Änderung – sofort veröffentlichen
}

// ── Automatik mit Hysterese (Kapitel 7) ─────────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: an Schwelle einschalten, 20 Punkte darunter ausschalten.
    if (g_vocIndex < 0) return;       // Sensor noch stumm
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Befehls-Callbacks vom Portal (Kapitel 6–7) ──────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // sofort anwenden
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: Einstellungen, Netz, Sensor, Karte ────────────────
void setup() {
    Serial.begin(115200);

    // Einstellungen aus NVS (zweite Argumente – Defaults beim ersten Start).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, Bindung – alles drin
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrie: eigenes Feld vocIndex hinzufügen (Kapitel 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karte: Sensor + Steuerelemente (Kapitel 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Layout (Kapitel 6, optional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: Netz immer, Sensor und Logik pro Sekunde ────────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // Netz, Telemetrie, Befehle – immer zuerst

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // frischer Messwert…
        tickAutoLogic();              // …und sofort Entscheidung
    }
}
```
