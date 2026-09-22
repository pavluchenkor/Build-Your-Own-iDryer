---
title: "Connecting SHT31 and thermistor sensors to idryer-core"
description: "Reading SHT31 climate sensor and heater thermistor on ESP32: populating idryer-core telemetry and outputting data to the iDryer portal."
---

# Sensors

On this page you connect two sensors and output their data to the portal. First the SHT31 (cabinet climate), then the thermistor (heater temperature). This is the "read data" step before adding control logic.

The principle of working with the core is simple: your code in `loop()` writes fresh readings to `s_link.telemetry.*` fields, and the facade publishes them to the cloud every `telemetryPeriodMs` from `Config`. You do not need to call publish manually.

## Telemetry fields

For our cabinet, three fields are used (index `[0]` — the first and only chamber):

| Field | What it stores | Flag in Config |
|-------|----------------|---|
| `s_link.telemetry.airTempC[0]` | air temperature, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | air humidity, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | heater temperature, °C | `hasHeaterTemp` |

We have already enabled all three flags in the [Config in the previous step](04-firmware-start.md).

## Rule: sensor code must not block loop()

The `idryer-core` facade services Wi-Fi and MQTT in the same `loop()`. Therefore, when reading sensors you cannot call `delay()` — the pause breaks the network session. Sensors are polled on a timer, and the ready value is simply read. Ready-made drivers from the ecosystem are already structured this way.

## Step 1. SHT31: cabinet climate

You do not need to write the SHT31 driver from scratch — a ready-made `Sht31ClimateSensor` class is available in the `iDryer-Storage` example. It uses the `robtillaart/SHT31` library and reads the sensor without blocking.

1. Add the SHT31 library to `lib_deps` in your `platformio.ini`:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Copy four files from `iDryer-Storage/src/storage/sensors/` to your `src/` folder: `Sht31ClimateSensor.h`, `Sht31ClimateSensor.cpp`, `IClimateSensor.h`, and `sensor_reading.h`.

3. Connect the sensor via I2C (see [Wiring diagram](03-wiring.md)) and read it in `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — pins on your board
    s_climateOk = s_climate.begin();  // auto-finds address 0x44 or 0x45
    s_link.begin();
    // The portal unlinked the device: erase the secret, wait for a new pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
}
```

The `SensorReading` structure (fields `ok`, `temperature`, `humidity`) is declared in `sensor_reading.h`. After flashing, cabinet temperature and humidity will appear on the portal — this is the first feedback from the device.

## Step 2. Thermistor: heater temperature

I do not have a ready-made thermistor class for ESP32, so we read and write directly in `src/main.cpp`. The thermistor is connected to an ADC pin through a voltage converter (see [Wiring diagram](03-wiring.md)): the controller measures the voltage at the midpoint, the thermistor resistance is calculated from it, and then the temperature.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // ADC pin
static const float SERIES_R   = 4700.0f;   // divider resistor, Ohm
static const float NOMINAL_R  = 100000.0f; // thermistor resistance at 25 °C, Ohm
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // B-coefficient from thermistor datasheet

// Returns heater temperature in °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 on ESP32
    float v   = (float)raw / 4095.0f;           // fraction of full scale
    float r   = SERIES_R * (1.0f - v) / v;      // thermistor resistance, Ohm
    // Steinhart–Hart equation (B-parameter):
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

In `loop()` write the result to telemetry alongside SHT31 reading:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "This is simplified reading — adjust parameters for your thermistor"
    The `NOMINAL_R` and `BETA` constants depend on the specific thermistor — get them from its datasheet (common household thermistor — Generic 3950, `100 kΩ`). The divider formula corresponds to the circuit in [Wiring diagram](03-wiring.md): thermistor to `3.3V`, resistor to `GND`. With a different layout, the formula changes. The ADC on ESP32 is nonlinear, so for accurate measurements readings are calibrated — in production iDryer controllers a thermistor table is used for this (library `Thermistor`).

Checking the thermistor with a multimeter — [Checking a thermistor](../06-practical-guides/02-checking-thermistor.md).

## Complete `src/main.cpp` after this chapter

Below is the entire file. New lines relative to the previous chapter are marked `// ← chapter 5`; the rest is unchanged.

??? note "What was — `src/main.cpp` after chapter 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Dryer,
        .unitsCount        = 1,
        .hasHeater         = true,
        .hasFan            = true,
        .hasAirTemp        = true,
        .hasAirHumidity    = true,
        .hasHeaterTemp     = true,
        .telemetryPeriodMs = 5000,
        .statusPeriodMs    = 10000,
        .hardwareVersion   = "1.0",
        .firmwareVersion   = "0.1.0",
        .model             = "DIY Storage Cabinet",
    };
    static iDryer::Link s_link(CFG);

    void setup() {
        Serial.begin(115200);
        s_link.begin();
        // The portal unlinked the device: erase the secret, wait for a new pairing.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← chapter 5
#include <math.h>                  // ← chapter 5
#include "Sht31ClimateSensor.h"    // ← chapter 5

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    .hasHeater         = true,
    .hasFan            = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = true,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← chapter 5: SHT31 climate sensor
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← chapter 5: heater thermistor
static const int   THERM_PIN  = 2;
static const float SERIES_R   = 4700.0f;
static const float NOMINAL_R  = 100000.0f;
static const float NOMINAL_T  = 25.0f;
static const float BETA       = 3950.0f;

static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);
    float v   = (float)raw / 4095.0f;
    float r   = SERIES_R * (1.0f - v) / v;
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← chapter 5  (SDA, SCL — pins on your board)
    s_climateOk = s_climate.begin();  // ← chapter 5
    s_link.begin();
    // The portal unlinked the device: erase the secret, wait for a new pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {                                       // ← chapter 5
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();     // ← chapter 5
}
```

## Checking the result

After this step, three values should be displayed on the portal:

- air temperature in the cabinet;
- humidity in the cabinet;
- heater temperature.

If readings "float" or are clearly incorrect:

- check common ground and wiring (noise from power wires) — [Wiring mistakes](../08-common-mistakes/03-wiring-mistakes.md);
- check the divider resistor rating and thermistor type;
- make sure SHT31 responds on I2C (correct address and lines).

Diagnostics for "sensor shows garbage" — [Checking a thermistor](../06-practical-guides/02-checking-thermistor.md) and [Common mistakes](../08-common-mistakes/01-overview.md).

## What's next

You have data from sensors. Now we will describe device settings (target temperature, hysteresis) in [YAML Menu](06-menu.md) so they can be changed from the portal and stored in memory.
