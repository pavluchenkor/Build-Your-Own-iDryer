---
title: "Smart filter: device card on portal (card manifest)"
description: "Dynamic device card: declaring VOC sensor, mode, threshold, and layout through link.card() — portal and app build interface automatically."
---

# Device card

This is the main chapter of the section. Here the device gets an interface on the portal and in the mobile app — **without a single line of code on their side**.

## How it works

The device publishes a **card manifest** — a machine-readable description of "what to show and what to control." The portal and app read the manifest and build the card: sensors become cells with live values, controls become buttons, input fields and lists. You can also set the layout from the firmware.

You don't need to publish anything manually: you declare entities through `link.card()`, and the core itself assembles the manifest and sends it on connection.

## 1. Declaring entities

All declarations are made in `setup()`, after `s_link.begin()`. Our filter has three entities: VOC reading, mode list, and threshold field. We'll go through each one separately, then put the whole block together.

### General principle: id and label

Each entity has two names, don't confuse them:

- **id** — the internal, machine-facing name (`"voc"`, `"mode"`). Latin letters, digits, underscores, no spaces. The layout, commands, and portal all refer to the entity by its id. Pick it once and never change it;
- **label** — the human-facing caption (`"VOC index"`, `"Mode"`). Whatever you write here is exactly what the user sees on the card. Change it as freely as you like.

### Sensor: VOC reading

```cpp
s_link.card().sensor(
    "voc",              // id: internal entity name
    "VOC index",        // label: caption on card
    "",                 // unit: unit of measurement to the right of the number ("°C", "%", "g");
                        //   VOC index has no unit — empty string
    "units[0].vocIndex" // path: where to take the value — path inside telemetry JSON.
                        //   This is the EXACT field we added in chapter 5:
                        //   doc["units"][0]["vocIndex"]. Names must match
                        //   letter for letter, or the card will show a dash.
);
```

A sensor is a read-only cell: the portal reads the value from telemetry at `path` and displays it. Sensors carry no commands.

### List selection: operation mode

```cpp
// List options. The user will see them in the dropdown menu as-is.
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id: internal entity name
    "Mode",                  // label: caption on card
    kModes,                  // options: array of choices (declared above)
    3,                       // number of options in the array — auto, on, off = three.
                             //   C++ doesn't know array length itself, we tell it
    [](const char* opt) {    // callback: function the core will call when
                             //   the user selects an option on the portal.
                             //   opt — the selected string, e.g. "on"
        onModeSelected(opt); //   pass it to our logic (we'll write in chapter 7)
    }
);
```

Here appears the second half of the mechanism: **control**. When the user selects an option on the portal, the device receives a command, the core receives it itself, checks it (foreign strings not in `options` won't reach you), and calls your callback with the selected value. You don't need to parse MQTT messages manually — your responsibility begins inside `onModeSelected`.

### Numeric field: firing threshold

```cpp
s_link.card().number(
    "threshold",       // id: internal entity name
    "VOC threshold",   // label: caption on card
    100,               // min: portal won't let you enter less than this
    400,               // max: won't let you enter more; the core will also
                       //   clip the value by these boundaries on its side
    10,                // step: value change step with arrow buttons
    "",                // unit: unit of measurement; the index has none
    [](float v) {              // callback: called when the user sends
                               //   a new value; v — number within min..max
        onThresholdChanged(v); //   pass it to our logic (we'll write in chapter 7)
    }
);
```

### Putting it together

Final view of the block in `setup()` — what should remain in your code. Functions `onModeSelected` and `onThresholdChanged` we'll write in chapter 7; to compile the code now, declare them as stubs **above** `setup()`:

```cpp
// Stubs: we'll write the real bodies in chapter 7 (automation logic).
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
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

    // Card: sensor + two controls.
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

What about the fan? You **don't need to** declare it: the `hasFan = true` flag in `Config` already added the "Fan" cell to the manifest automatically — this is a vocabulary skill, the core knows all about it.

!!! note "Square brackets in callbacks — always empty"
    `[](const char* opt) { ... }` — this is a lambda, a nameless function; we discussed it in detail in [the tip in chapter 5](05-sensor-and-telemetry.md). Reminder: core rule is brackets are always empty (`[]`), don't take anything with you into the lambda, store everything needed in global variables — like `g_mode` and `g_threshold` from the next chapter.

## 2. Automatic card layout

You don't have to specify layout at all. The portal will assemble the card from the declared entities by itself — and will assemble it neatly: read cells group into rows (up to three per row, then wrap), controls go below, each on its own line, everything in the portal's branded styling. For most devices this is enough — the interface turns out neat without a single thought about layout.

The order of entities on the card — order of their declaration in `setup()`.

## 3. Custom card layout (optional)

First — how the card is structured. A card is a vertical stack of **rows**. A row of sensor cells divides the width evenly: one cell takes the full width, two — half each, three — a third each. Controls (list, field, button) in a row are stacked one under another at full width.

The automatic layout from the previous section arranges entities into these rows automatically. If you want to decide yourself what goes next to what — set the rows manually with `layoutRow` calls. One call = one row, order of calls = order of rows from top to bottom:

```cpp
// Row 1: two cells — VOC index and fan, each half width.
s_link.card().layoutRow("voc", "fan");

// Row 2: two controls — mode and threshold, one under the other.
s_link.card().layoutRow("mode", "threshold");
```

To `layoutRow` you pass the **id** of entities — those internal names you gave them when declaring (that's what id was for). `"fan"` — id of the vocabulary fan entity, created by the `hasFan` flag.

On the card this will give this arrangement:

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  Fan        │   ← row 1: voc, fan
│  103              │  Off        │
├───────────────────┴─────────────┤
│  Mode                           │   ← row 2: mode, threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

Entities you don't mention in any row won't disappear — the portal will draw them below in automatic list. So you can layout only the "main" and leave the rest to automation.

## 4. What goes on the air

The core will publish to topic `idryer/{serial}/card` (retained):

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

You don't need to understand this JSON — the core generates it from your calls. But it is useful to know: if you write firmware **not** on `idryer-core` (Rust, MicroPython, whatever), it is enough to publish such JSON yourself — the portal doesn't care who produced the JSON, as long as the format matches.

## 5. Verification

Flash and open the dashboard on the portal:

- cell **VOC index** shows live index (blow on the sensor — the number grows on the next update);
- cell **Fan** — On/Off;
- **Mode** — dropdown list, **VOC threshold** — number field: the value is sent about 0.6 s after you change it, without a confirm button. The list and the field start from the first option and the minimum — they send commands and do not show the current device setting; the state is shown by the cells (for example, the fan).

Selecting a mode or threshold does nothing yet — the callbacks are stubs. We'll bring them to life in [the next chapter](07-auto-logic.md).

!!! note "This is that very concept"
    Notice what happened: you described the interface with five lines in the firmware — and it appeared on the portal and in the app. The same technique works for any of your devices: only id, captions, and callbacks change.

!!! note "Operations with a start and a stop"
    A list, a field and a button send a value right away. Operations with a start and a stop — drying, heating, lighting — are declared as card **actions** with a mode and launch parameters: the card shows a start form and, while the operation runs, a session block with a Stop button. Example — [chapter 7](../09-build-a-device/07-heating-control.md) of the cabinet section.
