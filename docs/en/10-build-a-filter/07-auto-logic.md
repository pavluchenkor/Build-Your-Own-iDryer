---
title: "Smart filter: automation logic"
description: "VOC index threshold and hysteresis, auto/on/off modes from portal, saving settings to NVS, and publishing fan status."
---

# Automation logic

Connecting everything: sensor decides, fan spins, portal controls.

## 1. State and settings

To the beginning of `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC index to turn on
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — non-volatile memory of ESP32: mode and threshold survive a reboot.

## 2. Fan control

All turning on and off goes into one `setFan` function. It takes one argument `on` — desired state: `true` = turn on, `false` = turn off. Further in the code we always call `setFan(true)` / `setFan(false)`, and it does all the routine: toggles the pin, remembers the state, and tells the portal.

```cpp
static void setFan(bool on) {      // on — argument: true = turn on, false = turn off
    if (g_fanOn == on) return;     // already in desired state — do nothing
    g_fanOn = on;                  // remember new state in global variable
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // physically turn fan switch on/off

    // Tell the core the state: fanOn[0] — vocabulary telemetry field
    // (appeared from hasFan = true; [0] — our only unit, like in chapter 5).
    // From here it will go to the cloud and to the "Fan" cell on the card.
    s_link.telemetry.fanOn[0] = on;

    // State change — reason to send telemetry immediately, don't wait for period.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` makes the response instant: clicked on portal — within a second the card shows the confirmed state. Confirmed: iDryer portal never "guesses" state, it shows what the device actually sent.

## 3. Automation with hysteresis

If you turn the fan on exactly at the threshold, around the threshold it will chatter on/off. Fixed with a gap:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: turn on at threshold, turn off 20 points below.
    if (g_vocIndex < 0) return;                      // sensor still silent
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

We will call `tickAutoLogic()` in the same place we read the sensor — in `loop()` with a one-second timer. This is the same `loop()` from chapter 5, adding one line to it. Now it looks like:

```cpp
void loop() {
    s_link.loop();                        // network, telemetry, commands — always first

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // once per second:
        s_lastReadMs = now;
        readVocSensor();                  //   read VOC (chapter 5)
        tickAutoLogic();                  //   and immediately decide on fan
    }
}
```

The order inside the one-second block is not accidental: fresh sensor reading first, then decision on it.

## 4. Callbacks from portal

Those very functions we promised in [chapter 6](06-card.md):

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // apply immediately, don't wait for next tick
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

These functions **replace** the stubs from chapter 6 — delete the empty versions.

Notice what this code **doesn't have**: MQTT parsing, topics, JSON commands. User selected `on` in list on portal → core got the command, checked it, called `onModeSelected("on")`. All transport mechanics — core's responsibility.

## 5. Final setup()

Two more things to add to `setup()`: loading saved settings from NVS (at the beginning, so logic works with them immediately) and configuring the fan pin. Complete `setup()` after this chapter looks like:

```cpp
void setup() {
    Serial.begin(115200);

    // Settings from NVS: what the user chose before.
    s_prefs.begin("filter");   // open "filter" namespace in NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Second arguments of getUChar/getInt — default values: will return
    // on the very first startup, when NVS has nothing saved yet.

    pinMode(FAN_PIN, OUTPUT);  // fan switch pin — to output

    s_link.begin();
    // The portal unlinked the device: erase the secret, wait for a new pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetry: custom vocIndex field (chapter 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Card: sensor + controls (chapter 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Layout (chapter 6, optional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Scenario verification

| Action | Expected |
|---|---|
| Mode `auto`, blow on sensor | VOC grows, at threshold fan turns on, card shows "On" |
| Air cleaned | below threshold−20 fan turns off by itself |
| Mode `on` from portal | fan spins regardless of VOC |
| Mode `off` from portal | fan stops, VOC continues showing |
| Board reboot | mode and threshold saved |

This is what it looks like live. The threshold is `150`, the index reached it — the fan turned on by itself:

![Automation turned the fan on at the threshold](../../img/10-filter/07-portal-auto-on.png)
*Mode `auto`: index `151` with threshold `150` — the fan is on.*

![The same in the mobile app](../../img/10-filter/07-app-auto-on.png)
*The app shows the same state: the value has grown, the fan is running.*

Manual mode overrides the automation: `Mode` → `on`, and the fan spins even when the air is already clean:

![Manual mode: the fan is on with clean air](../../img/10-filter/07-portal-mode-on.png)
*Mode `on`, index `132` — below the threshold, but the fan is running: a command from the portal outweighs the automation.*

## 7. Complete code: src/main.cpp whole

All code from chapters 4–7, assembled into one file. If something doesn't match yours — check against this listing.

```cpp
// ============================================================
// Smart air filter on idryer-core.
// SGP40 (VOC) + fan via MOSFET, auto/manual mode,
// control and card on portal via card manifest.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // index without a sensor (-DDEMO_VOC=1)

// ── Pins ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // MOSFET gate for fan
// SDA=8, SCL=9 — set in Wire.begin() below

// ── Device passport (chapter 4) ──────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // non-standard device
    .unitsCount        = 1,
    .hasFan            = true,        // only vocabulary skill
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── State (chapter 7) ────────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC index to turn on
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: settings survive reboot

// ── VOC sensor (chapter 5) ───────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = no data yet

static void initVocSensor() {
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// The sensor or, with -DDEMO_VOC=1, the air model (chapter 5).
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // Index: ~100 = normal air, higher = dirtier (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}

// ── Fan (chapter 7) ──────────────────────────────────────────
static void setFan(bool on) {         // on: true = turn on, false = turn off
    if (g_fanOn == on) return;        // already in desired state
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // vocabulary field → cloud → card
    s_link.publishTelemetryNow();     // state change — publish immediately
}

// ── Automation with hysteresis (chapter 7) ──────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: turn on at threshold, turn off 20 points below.
    if (g_vocIndex < 0) return;       // sensor still silent
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Command callbacks from portal (chapters 6–7) ─────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // apply immediately
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: settings, network, sensor, card ──────────────────
void setup() {
    Serial.begin(115200);

    // Settings from NVS (second arguments — defaults on first run).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, linking — all inside
    // The portal unlinked the device: erase the secret, wait for a new pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetry: add custom vocIndex field (chapter 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Card: sensor + controls (chapter 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Factory card layout (chapter 6, optional).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: network always, sensor and logic once per second ──
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // network, telemetry, commands — always first

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // fresh reading…
        tickAutoLogic();              // …and immediately decision on it
    }
}
```
