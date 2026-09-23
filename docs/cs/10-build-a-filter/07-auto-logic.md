---
title: "Chytrý filtr: logika automatiky"
description: "Práh a hystereze podle VOC indexu, režimy auto/on/off z portálu, uložení nastavení v NVS a publikace stavu ventilátoru."
---

# Logika automatiky

Spojujeme všechno: senzor rozhoduje, ventilátor točí, portál řídí.

## 1. Stav a nastavení

Na začátek `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC index zapnutí
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — energeticky nezávislá paměť ESP32: režim a práh přežijí restart.

## 2. Řízení ventilátoru

Veškeré zapínání a vypínání svedeme do jediné funkce `setFan`. Přijímá jeden argument `on` — požadovaný stav: `true` = zapnout, `false` = vypnout. Dál v kódu vždy voláme `setFan(true)` / `setFan(false)` a ona obstará veškerou rutinu: přepne pin, zapamatuje si stav a oznámí to portálu.

```cpp
static void setFan(bool on) {      // on — argument: true = zapnout, false = vypnout
    if (g_fanOn == on) return;     // již v požadovaném stavu — nic neděláme
    g_fanOn = on;                  // zapamatujeme si nový stav v globální proměnné
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // fyzicky zapínáme/vypínáme klíč ventilátoru

    // Řekneme stav jádru: fanOn[0] — slovníkové pole telemetrických dat
    // (objevilo se kvůli hasFan = true; [0] — náš jediný blok, jak v kapitole 5).
    // Odtud si to vezme cloud a na buňku „Ventilátor" karty.
    s_link.telemetry.fanOn[0] = on;

    // Změna stavu — důvod poslat telemetrické údaje hned, nečekat na období.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` zajistí okamžitou odezvu: kliknuli v portálu — za sekundu karta zobrazuje potvrzený stav. Právě potvrzený: portál iDryer stav nikdy „neodhaduje", zobrazuje výhradně to, co zařízení skutečně odeslalo.

## 3. Automatika s hysterezí

Kdybychom spínali přesně na prahu, ventilátor by kolem prahové hodnoty kmital zap/vyp. Léčí se mrtvým pásmem:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: zapínáme na prahu, vypínáme o 20 bodů níže.
    if (g_vocIndex < 0) return;                      // senzor ještě mlčí
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

Funkci `tickAutoLogic()` budeme volat na stejném místě jako čtení senzoru — v `loop()` podle sekundního časovače. Jde o ten samý `loop()` z kapitoly 5, přidáváme jediný řádek. Celý vypadá takto:

```cpp
void loop() {
    s_link.loop();                        // síť, telemetrické údaje, příkazy — vždy první
    
    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // jednou za sekundu:
        s_lastReadMs = now;
        readVocSensor();                  //   čteme VOC (kapitola 5)
        tickAutoLogic();                  //   a hned se rozhodujeme o ventilátoru
    }
}
```

Pořadí uvnitř sekundního bloku není náhodné: nejdřív čerstvé čtení senzoru, potom rozhodnutí na jeho základě.

## 4. Callbacky z portálu

To jsou ty samé funkce, které jsme slibovali v [kapitole 6](06-card.md):

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // aplikujeme hned, nečekáme na příští tik
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Tyto funkce **nahrazují** zástupce z kapitoly 6 — prázdné verze smažte.

Všimněte si, co v tomto kódu **chybí**: parsování MQTT, topiky, JSON příkazy. Uživatel vybral `on` v seznamu v portálu → jádro příkaz přijalo, ověřilo a zavolalo `onModeSelected("on")`. Veškerá transportní mechanika je starostí jádra.

## 5. Finální setup()

Do `setup()` zbývá přidat dvě věci: načtení uložených nastavení z NVS (na začátek, aby logika s nimi pracovala hned od startu) a nastavení pinu ventilátoru. Celý `setup()` po této kapitole vypadá takto:

```cpp
void setup() {
    Serial.begin(115200);

    // Nastavení z NVS: to, co si uživatel nastavil naposledy.
    s_prefs.begin("filter");   // otevřít jmenný prostor "filter" v NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Druhé argumenty getUChar/getInt — výchozí hodnoty: vrátí se
    // při prvním spuštění, kdy v NVS ještě nic není uloženo.

    pinMode(FAN_PIN, OUTPUT);  // pin klíče ventilátoru — na výstup

    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrické údaje: vlastní pole vocIndex (kapitola 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karta: senzor + ovládací prvky (kapitola 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Rozvržení (kapitola 6, podle přání).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Ověření scénářů

| Akce | Očekávaný výsledek |
|---|---|
| Režim `auto`, vydechnout na senzor | VOC roste, na prahu se ventilátor zapne, karta zobrazí „Zap" |
| Vzduch se vyčistil | pod prahem−20 se ventilátor sám vypne |
| Režim `on` z portálu | ventilátor běží nezávisle na VOC |
| Režim `off` z portálu | ventilátor stojí, VOC se nadále zobrazuje |
| Restart desky | režim a práh se zachovaly |

Takto to vypadá živě. Práh `150`, index se k němu dostal — ventilátor se sám zapnul:

![Automatika zapnula ventilátor na prahu](../../img/10-filter/07-portal-auto-on.png)
*Režim `auto`: index `151` při prahu `150` — ventilátor je zapnutý.*

![Totéž v mobilní aplikaci](../../img/10-filter/07-app-auto-on.png)
*Aplikace ukazuje tentýž stav: hodnota vzrostla, ventilátor běží.*

Ruční režim přebije automatiku: `Mode` → `on`, a ventilátor běží, i když je vzduch už čistý:

![Ruční režim: ventilátor zapnutý při čistém vzduchu](../../img/10-filter/07-portal-mode-on.png)
*Režim `on`, index `132` — pod prahem, ale ventilátor běží: příkaz z portálu má přednost před automatikou.*

## 7. Výsledný kód: src/main.cpp celý

Veškerý kód kapitol 4–7, složený do jednoho souboru. Pokud se něco neshoduje s vaší verzí — srovnávejte s tímto výpisem.

```cpp
// ============================================================
// Chytrý vzduchový filtr na idryer-core.
// SGP40 (VOC) + ventilátor skrz MOSFET, auto/ruční režim,
// řízení a karta v portálu skrz card manifest.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // index bez senzoru (-DDEMO_VOC=1)

// ── Piny ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // gate MOSFET ventilátoru
// SDA=8, SCL=9 — nastavují se v Wire.begin() níže

// ── Konfigurace zařízení (kapitola 4) ───────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // nestandardní zařízení
    .unitsCount        = 1,
    .hasFan            = true,        // jediná slovníková schopnost
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── Stav (kapitola 7) ─────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC index zapnutí
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: nastavení přežijí restart

// ── Senzor VOC (kapitola 5) ────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = údaje ještě nejsou

static void initVocSensor() {
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// Senzor nebo, s -DDEMO_VOC=1, model vzduchu (kapitola 5).
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // Index: ~100 = normální vzduch, více = špinavěji (max 500).
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}

// ── Ventilátor (kapitola 7) ────────────────────────────────
static void setFan(bool on) {         // on: true = zapnout, false = vypnout
    if (g_fanOn == on) return;        // již v požadovaném stavu
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // slovníkové pole → cloud → karta
    s_link.publishTelemetryNow();     // změna stavu — publikujeme hned
}

// ── Automatika s hysterezí (kapitola 7) ─────────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: zapínáme na prahu, vypínáme o 20 bodů níže.
    if (g_vocIndex < 0) return;       // senzor ještě mlčí
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Callbacky příkazů z portálu (kapitoly 6–7) ────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // aplikujeme hned
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: nastavení, síť, senzor, karta ────────────────
void setup() {
    Serial.begin(115200);

    // Nastavení z NVS (druhé argumenty — výchozí hodnoty prvního spuštění).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, připojení — vše uvnitř
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Telemetrické údaje: doplňujeme vlastní pole vocIndex (kapitola 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Karta: senzor + ovládací prvky (kapitola 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Tovární rozvržení karty (kapitola 6, podle přání).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: síť vždy, senzor a logika jednou za sekundu ────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // síť, telemetrické údaje, příkazy — vždy první

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // čerstvé čtení…
        tickAutoLogic();              // …a hned rozhodnutí podle něj
    }
}
```
