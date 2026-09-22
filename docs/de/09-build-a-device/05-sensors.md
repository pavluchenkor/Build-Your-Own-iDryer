---
title: "Anschluss von SHT31- und Thermistorsensoren an idryer-core"
description: "Auslesen des SHT31-Klimasensors und des Thermistorsensors des Heizers auf dem ESP32: Befüllen der Telemetrie von idryer-core und Ausgabe von Daten auf das iDryer-Portal."
---

# Sensoren

Auf dieser Seite verbinden Sie zwei Sensoren und geben ihre Daten auf dem Portal aus. Zunächst SHT31 (Schrank-Klima), dann Thermistor (Heizer-Temperatur). Dies ist der Schritt „Daten erhalten" vor dem Hinzufügen der Steuerlogik.

Das Prinzip der Arbeit mit dem Kern ist einfach: Ihr Code in `loop()` schreibt frische Messwerte in die Felder `s_link.telemetry.*`, und die Fassade veröffentlicht sie automatisch jeden `telemetryPeriodMs` aus `Config` in die Cloud. Ein manueller Aufruf der Veröffentlichung ist nicht erforderlich.

## Telemetrie-Felder

Für unseren Schrank werden drei Felder verwendet (Index `[0]` — erste und einzige Kammer):

| Feld | Speicherinhalt | Flag in Config |
|------|---|---|
| `s_link.telemetry.airTempC[0]` | Lufttemperatur, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | Luftfeuchte, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | Heizer-Temperatur, °C | `hasHeaterTemp` |

Alle drei Flags haben wir bereits im [Config aus dem vorherigen Schritt](04-firmware-start.md) aktiviert.

## Regel: Sensoren-Code darf loop() nicht blockieren

Die `idryer-core` Fassade bedient Wi-Fi und MQTT in derselben `loop()`. Daher können Sie beim Auslesen von Sensoren nicht `delay()` aufrufen — eine Pause unterbricht die Netzsitzung. Der Sensor wird nach einem Timer abgefragt, und der fertige Wert wird einfach gelesen. Fertige Treiber aus dem Ökosystem sind bereits so konstruiert.

## Schritt 1. SHT31: Schrank-Klima

Es ist nicht nötig, den SHT31-Treiber von Grund auf zu schreiben — die fertige Klasse `Sht31ClimateSensor` gibt es im `iDryer-Storage`-Beispiel. Sie verwendet die `robtillaart/SHT31` Bibliothek und liest den Sensor blockierungsfrei.

1. Fügen Sie die SHT31-Bibliothek in `lib_deps` Ihrer `platformio.ini` hinzu:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Kopieren Sie vier Dateien aus `iDryer-Storage/src/storage/sensors/` in Ihren `src/`-Ordner: `Sht31ClimateSensor.h`, `Sht31ClimateSensor.cpp`, `IClimateSensor.h` und `sensor_reading.h`.

3. Verbinden Sie den Sensor über I2C (siehe [Schaltplan](03-wiring.md)) und lesen Sie ihn in `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — Pins Ihres Boards
    s_climateOk = s_climate.begin();  // findet die Adresse 0x44 oder 0x45 selbst
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
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

Die `SensorReading` Struktur (Felder `ok`, `temperature`, `humidity`) ist in `sensor_reading.h` deklariert. Nach dem Firmware-Upload werden Temperatur und Feuchte des Schranks auf dem Portal angezeigt — dies ist die erste Rückmeldung vom Gerät.

## Schritt 2. Thermistor: Heizer-Temperatur

Es gibt keine fertige Thermistor-Klasse für den ESP32, daher schreiben wir sie direkt in `src/main.cpp`. Der Thermistor ist an einem ADC-Pin über einen Spannungswandler angeschlossen (siehe [Schaltplan](03-wiring.md)): Der Controller misst die Spannung an der mittleren Leitung, berechnet daraus den Thermistor-Widerstand und dann die Temperatur.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // ADC-Pin
static const float SERIES_R   = 4700.0f;   // Teiler-Widerstand, Ohm
static const float NOMINAL_R  = 100000.0f; // Thermistor-Widerstand bei 25 °C, Ohm
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // B-Koeffizient aus dem Thermistor-Datenblatt

// Gibt die Heizer-Temperatur in °C zurück.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 auf ESP32
    float v   = (float)raw / 4095.0f;           // Bruchteil der Gesamtskala
    float r   = SERIES_R * (1.0f - v) / v;      // Thermistor-Widerstand, Ohm
    // Steinhart-Hart-Gleichung (B-Parameter):
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

Schreiben Sie das Ergebnis in `loop()` in die Telemetrie neben der SHT31-Ablesung:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "Dies ist vereinfachtes Auslesen — passen Sie die Parameter an Ihren Thermistor an"
    Die Konstanten `NOMINAL_R` und `BETA` hängen vom spezifischen Thermistor ab — nehmen Sie sie aus dem Datenblatt (ein verbreiteter gewöhnlicher Thermistor — Generic 3950, `100 kΩ`). Die Teiler-Formel entspricht der Schaltung aus [Schaltplan](03-wiring.md): Thermistor auf `3.3V`, Widerstand auf `GND`. Bei anderer Verdrahtung ändert sich die Formel. Der ADC auf dem ESP32 ist nichtlinear, daher werden die Messwerte für genaue Messungen kalibriert — in Serie iDryer-Controllern wird dafür eine Thermistor-Tabelle verwendet (Bibliothek `Thermistor`).

Überprüfung des Thermistors mit einem Multimeter — [Thermistor überprüfen](../06-practical-guides/02-checking-thermistor.md).

## Vollständig `src/main.cpp` nach diesem Kapitel

Unten ist die gesamte Datei. Neue Zeilen relativ zum letzten Kapitel sind mit `// ← Kapitel 5` gekennzeichnet; der Rest hat sich nicht geändert.

??? note "Was bisher da war — `src/main.cpp` nach Kapitel 4"

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
        // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← Kapitel 5
#include <math.h>                  // ← Kapitel 5
#include "Sht31ClimateSensor.h"    // ← Kapitel 5

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

// ← Kapitel 5: SHT31 Klimasensor
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← Kapitel 5: Heizer-Thermistor
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
    Wire.begin(8, 9);                 // ← Kapitel 5  (SDA, SCL — Pins Ihres Boards)
    s_climateOk = s_climate.begin();  // ← Kapitel 5
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {                                       // ← Kapitel 5
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();     // ← Kapitel 5
}
```

## Überprüfung des Ergebnisses

Nach diesem Schritt sollten drei Werte auf dem Portal angezeigt werden:

- Lufttemperatur im Schrank;
- Feuchte im Schrank;
- Heizer-Temperatur.

Wenn die Messwerte „schwankend" sind oder offensichtlich falsch:

- überprüfen Sie die Gesamterde und die Verdrahtung (Störungen von Stromkabeln) — [Verdrahtungsfehler](../08-common-mistakes/03-wiring-mistakes.md);
- überprüfen Sie den Nominalwert des Teiler-Widerstands und den Typ des Thermistors;
- stellen Sie sicher, dass der SHT31 über I2C antwortet (richtige Adresse und Leitungen).

Diagnose „Sensor zeigt Müll" — [Thermistor überprüfen](../06-practical-guides/02-checking-thermistor.md) und [Typische Fehler](../08-common-mistakes/01-overview.md).

## Was ist als Nächstes

Wir haben Daten von den Sensoren. Jetzt werden wir die Geräteeinstellungen (Zieltemperatur, Hysterese) im [Menü aus YAML](06-menu.md) beschreiben, um sie vom Portal aus ändern und im Speicher speichern zu können.
