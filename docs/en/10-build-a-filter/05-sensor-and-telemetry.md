---
title: "Smart filter: VOC sensor and telemetry"
description: "Reading SGP40 via I2C and publishing custom vocIndex field to iDryer telemetry via onTelemetryPublish callback."
---

# Sensor and telemetry

In this chapter, the filter starts measuring air and sending data to the cloud. The key technique — **custom field in telemetry**: the ecosystem vocabulary knows nothing about VOC, but the core allows you to add any field to telemetry.

## 1. Sensor library

In `platformio.ini`, add to `lib_deps`:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. Reading SGP40

In `src/main.cpp` (pins — from [the diagram](03-wiring.md)):

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = no data yet

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() maintains internal sensor compensation by itself.
    // Index: ~100 = normal air, higher = dirtier (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
}
```

Add the `initVocSensor()` call to `setup()` after `s_link.begin()`, and `readVocSensor()` — in `loop()` once per second (using millis timer, not `delay`!):

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

!!! warning "No delay() in loop"
    `s_link.loop()` should be called constantly — Wi-Fi, MQTT, and portal commands depend on it. `delay(1000)` will freeze all of this. Only millis timers.

## 3. Custom field in telemetry

Every `telemetryPeriodMs`, the core itself assembles a JSON telemetry message and sends it to the cloud. For our device (one unit, only a fan from the vocabulary skills) the core assembles this message:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

Let us break down the structure:

- `units` — array of device **units** (chambers). The serial iDryer dryer can have up to four independent chambers, so telemetry is always an array, even if there's only one chamber;
- `units[0]` — first (and our only) unit: we specified `unitsCount = 1` in `Config`;
- `fanStatus` — vocabulary field, appeared because of `hasFan = true`;
- `rssi`, `uptime` — Wi-Fi level and uptime, the core adds always.

There is nothing about VOC in this message — the core does not know about our sensor. But right before sending, the core gives your code a chance to add its own fields to the message. For this you register a **callback** — a function you give to the core, and the core calls it on every publication, passing the assembled JSON inside (the `doc` parameter — that's it).

In `setup()`:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — the telemetry message assembled by the core (see JSON above).
    // We add our vocIndex field to the first unit.
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

The line `doc["units"][0]["vocIndex"] = g_vocIndex;` reads like: "in message `doc` take the array `units`, in it element `0` (our only unit) and write the `vocIndex` field there." You invent the field name yourself — in [the next chapter](06-card.md) you will reference it to show the value on the card.

!!! note "If you encounter the word hook"
    In the core sources, this callback is called a `PublishHook` — "hook" means the same thing: a point where the library lets you "hook" your function. The terms are interchangeable; in this documentation we say "callback."

!!! note "Lambda and why it's 'empty'"
    The construct `[](JsonObject doc) { ... }` is called a **lambda** — a function with no name, written right where it is used, so you don't have to extract it separately and invent a name.

    Square brackets at the beginning — "capture list": variables from surrounding scope that the function takes with it. Core rule: **brackets are always empty** (`[]`) — the lambda captures nothing and carries no state with it (this is called *stateless*).

    The reason is technical: lambdas with captures require dynamic memory, and frequent allocations on ESP32 fragment the heap and can crash Wi-Fi in the worst case. So the core accepts only simple functions.

    Practical conclusion: everything the callback needs, store in **global** variables — like our `g_vocIndex`. This rule applies to all `idryer-core` callbacks.

The fan status is published the vocabulary way — just write it to the core field when you turn it on/off (logic — in [chapter 7](07-auto-logic.md)):

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. No sensor at hand? Demo mode

You can walk the whole path up to the card without an SGP40: the index will be computed by an air model. Copy the `demo_voc.h` file from the example:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

and add a build flag to `platformio.ini`:

```ini
build_flags =
    -DDEMO_VOC=1
```

Both branches are hidden behind a single function, and `loop()` does not know where the value came from:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

The model behaves like a room with a printer running in it: while the fan is off, the index creeps up from a background value of about `100`, and while the fan runs it falls. The model takes the fan state from your own logic, so the threshold and hysteresis from [chapter 7](07-auto-logic.md) can be seen at work.

For a real device the flag is not set: then the branch with the real sensor is built.

## 5. Verification

After flashing, in the MQTT stream of the device (or in Serial log of publications), telemetry looks like this:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` — your own field, sent to the cloud alongside the vocabulary `fanStatus`. The portal already receives it, but does not yet know what to do with it: show it to the portal in the next chapter.

!!! note "A custom field lives in real time"
    The portal passes telemetry to clients as is, so the card shows `vocIndex` right away. But into the history for charts the server writes only vocabulary values — temperature, humidity, heating. Your own field will not be on the telemetry chart; if you need history, collect it yourself (for example, Home Assistant or your own database).

Breathe on the sensor or bring a marker to it — the index should noticeably grow within seconds.
