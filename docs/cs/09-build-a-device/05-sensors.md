---
title: "Připojení senzorů SHT31 a termistoru k idryer-core"
description: "Čtení klimatického senzoru SHT31 a termistoru ohřívače na ESP32: vyplnění telemetrie idryer-core a výstup dat na portál iDryer."
---

# Senzory

Na této stránce připojujete dva senzory a výstup jejich dat na portál. Nejprve SHT31 (klima skříně), pak termistor (teplota ohřívače). Toto je krok „získáme data" před přidáním logiky řízení.

Princip práce s jádrem je jednoduchý: váš kód v `loop()` zapisuje čerstvá čtení do polí `s_link.telemetry.*` a fasáda je automaticky publikuje do cloudu každých `telemetryPeriodMs` z `Config`. Ruční vyvolání publikace není potřebné.

## Telemetrická pole

Pro naši skříň se používají tři pole (index `[0]` — první a jediná komora):

| Pole | Co ukládá | Příznak v Config |
|------|-----------|------------------|
| `s_link.telemetry.airTempC[0]` | teplota vzduchu, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | vlhkost vzduchu, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | teplota ohřívače, °C | `hasHeaterTemp` |

Tyto tři příznaky se zapínají právě zde, v `Config` (viz úplný výpis na konci kapitoly). Příznak říká portálu a aplikaci, že zařízení takový senzor má: bez něj se buňka na kartě neobjeví.

## Pravidlo: kód senzoru nesmí blokovat loop()

Fasáda `idryer-core` obsluhuje Wi-Fi a MQTT ve stejném `loop()`. Proto při čtení senzorů nelze volat `delay()` — pauza přeruší síťovou relaci. Senzor se dotazuje podle časovače a hotová hodnota se jednoduše přečte. Hotové ovladače z ekosystému jsou již takto uspořádány.

## Krok 1. SHT31: klima skříně

Psát ovladač SHT31 od nuly není potřebné — gotová třída `Sht31ClimateSensor` leží v příkladu této kapitoly, [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet). Používá knihovnu `robtillaart/SHT31` a čte senzor bez blokování.

1. Přidejte knihovnu SHT31 do `lib_deps` svého `platformio.ini`:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Zkopírujte do své složky `src/` čtyři soubory ovladače:

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. Připojte senzor přes I2C (viz [Schéma zapojení](03-wiring.md)) a přečtěte jej v `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — vývody vaší desky
    s_climateOk = s_climate.begin();  // automaticky najde adresu 0x44 nebo 0x45
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
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

Jeden snímek hodnot vrací ovladač strukturou `SensorReading` ze `sensor_reading.h`:

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C, NAN pokud není hodnota
    float    humidity    = NAN;   // % RH, NAN pokud není hodnota
    float    pressure    = NAN;   // hPa, pro budoucí senzory
    uint32_t ts_ms       = 0;     // millis() v okamžiku čtení
    bool     ok          = false; // true, pokud jsou teplota i vlhkost platné
    int      err         = 0;     // kód chyby, 0 — bez chyby
};
```

Po nahrání do zařízení se na portálu objeví teplota a vlhkost skříně — toto je první zpětná vazba ze zařízení.

## Krok 2. Termistor: teplota ohřívače

Gotové třídy termistoru pro ESP32 nemám, takže si jej napíšeme přímo v `src/main.cpp`. Termistor je připojen na výstup ADC přes měnič napětí (viz [Schéma zapojení](03-wiring.md)): kontrolér měří napětí v středním bodě, odtud se vypočítá odpor termistoru a poté teplota.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // výstup ADC
static const float SERIES_R   = 4700.0f;   // rezistor děliče, Ω
static const float NOMINAL_R  = 100000.0f; // odpor termistoru na 25 °C, Ω
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // B-koeficient z technické dokumentace termistoru

// Vrací teplotu ohřívače v °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 na ESP32
    float v   = (float)raw / 4095.0f;           // podíl z plné stupnice
    float r   = SERIES_R * (1.0f - v) / v;      // odpor termistoru, Ω
    // Steinhart-Hartova rovnice ve tvaru B-parametru — viz Wikipedii:
    // https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

V `loop()` zapisujte výsledek do telemetrie vedle čtení SHT31:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "Toto je zjednodušené čtení — upravte parametry pro váš termistor"
    Konstanty `NOMINAL_R` a `BETA` závisí na konkrétním termistoru — vezměte je z jeho technické dokumentace (běžný domácí termistor je Generic 3950, `100 kΩ`). Vzorec děliče odpovídá schématu z [Schématu zapojení](03-wiring.md): termistor na `3.3V`, rezistor na `GND`. Při jiném rozložení se vzorec změní. ADC na ESP32 je nelineární, takže pro přesná měření se čtení kalibrují — v sériových kontrolérech iDryer se k tomu používá tabulka termistoru (knihovna `Thermistor`).

Ověření termistoru multimetrem — [Ověření termistoru](../06-practical-guides/02-checking-thermistor.md).

## Krok 3. Nemáte senzory po ruce? Demo režim

Projít cestu až ke kartě lze i bez železa: hodnoty spočítá model skříně. Zkopírujte soubor `demo_sensors.h` ze stejného příkladu:

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

a přidejte do `platformio.ini` příznak sestavení:

```ini
build_flags =
    -DDEMO_SENSORS=1
```

Obě větve jsou schované za jednou funkcí a `loop()` neví, odkud hodnoty přišly:

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // čtení SHT31 a termistoru — jako výše
#endif
}
```

Model se chová jako skutečná skříň: místnost pomalu kolísá kolem `24 °C`, zapnutý ohřívač vzduch ohřívá, vypnutý jej nechá vychladnout, při ohřevu klesá vlhkost. Výkon ohřevu model čte z telemetrie, takže logika z kapitoly [Řízení ohřevu](07-heating-control.md) vidí odezvu a hystereze funguje. Snímky obrazovky v této části vznikly právě takto.

Pro provozní zařízení se příznak nenastavuje: pak se sestaví větev se skutečnými senzory.

## Úplný `src/main.cpp` po této kapitole

Níže je celý soubor dohromady. Nové řádky ve srovnání s předchozí kapitolou jsou označeny `// ← kapitola 5`; zbytek se nezměnil.

??? note "Co bylo — `src/main.cpp` po kapitole 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // vlastní zařízení: kartu sestavuje manifest
        .unitsCount        = 1,
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
        // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← kapitola 5
#include <math.h>                  // ← kapitola 5
#include "Sht31ClimateSensor.h"    // ← kapitola 5
#include "demo_sensors.h"    // ← kapitola 5: hodnoty bez senzorů (-DDEMO_SENSORS=1)

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // vlastní zařízení: kartu sestavuje manifest
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← kapitola 5
    .hasAirHumidity    = true,     // ← kapitola 5
    .hasHeaterTemp     = true,  // ← kapitola 5
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← kapitola 5: senzor klimatu SHT31
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← kapitola 5: termistor ohřívače
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

// ← kapitola 5: senzory nebo, s -DDEMO_SENSORS=1, model skříně
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();
#endif
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← kapitola 5  (SDA, SCL — vývody vaší desky)
    s_climateOk = s_climate.begin();  // ← kapitola 5
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← kapitola 5
}
```

## Ověření výsledku

![Stránka zařízení na portálu: hodnoty a graf](../../img/09-cabinet/05-portal-device.png)
*Hodnoty na kartě a graf telemetrie. Teplota ohřívače jde na grafu samostatnou čarou. Menu dole na stránce je zatím prázdné — tím se zabývá další kapitola.*

Po tomto kroku by se na portálu měly zobrazovat tři hodnoty:

- teplota vzduchu ve skříni;
- vlhkost ve skříni;
- teplota ohřívače.

Pokud se čtení „pohybují" nebo jsou zřejmě nesprávná:

- zkontrolujte společné uzemění a rozvedení (rušení od silných vedení) — [Chyby zapojení](../08-common-mistakes/03-wiring-mistakes.md);
- zkontrolujte hodnotu rezistoru děliče a typ termistoru;
- ujistěte se, že SHT31 reaguje na I2C (správná adresa a vedení).

Diagnostika „senzor ukazuje nesmysly" — [Ověření termistoru](../06-practical-guides/02-checking-thermistor.md) a [Běžné chyby](../08-common-mistakes/01-overview.md).

## Co dál

Máme data ze senzorů. Teď popíšeme nastavení zařízení (cílová teplota, hystereze) v [Menu z YAML](06-menu.md), aby je bylo možné měnit z portálu a ukládat v paměti.
