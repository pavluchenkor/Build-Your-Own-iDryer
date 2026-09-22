---
title: "Řízení vytápění skříně: údržba teploty a ventilátor"
description: "Logika vytápěné skříně na idryer-core: udržování cílové teploty histerezí, ochrana ohřívače termistorem, ventilátor a příkazy portálu."
---

# Řízení vytápění

Na této stránce spojujete senzory, nastavení a výkonovou část v pracovní logiku. Zařízení udržuje v skříni zadanou teplotu, chrání ohřívač před přehřátím a reaguje na příkazy z portálu.

Logika se provádí v `loop()` vedle údržby sítě. Všechny časovače a prahy jsou neblokující, bez `delay()`.

## Co by se mělo stát

Chování skříně se skládá ze tří jednoduchých pravidel:

1. **Údržba teploty.** Pokud je vzduch ve skříni chladnější než cíl o hodnotu histereze — zapnout vytápění. Když dosáhne cíle — vypnout.
2. **Ochrana ohřívače.** Termistor kontroluje samotný ohřívač. Pokud se přehřál nad přípustnou hranici — vytápění se vypne nezávisle na teplotě vzduchu.
3. **Ventilátor.** Zapíná se, aby se rozprostřelo teplo po skříni, a vypíná se, když vytápění není potřeba.

## Klíče ohřívače a ventilátoru

Ohřívač a ventilátor řadič zapíná pomocí klíče: MOSFET modul (verze A) nebo SSR (verze B) — viz [Schéma zapojení](03-wiring.md). Z pohledu kódu je to jednoduše výstup GPIO: `HIGH` — zapnuto, `LOW` — vypnuto.

Popišme takový klíč malou strukturou a máme dva výskyty — pro ohřívač a ventilátor. Přidejte to do `src/main.cpp` (před `setup()`):

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — řízení ohřívače
static GpioOutput myFan{5};      // GPIO5 — řízení ventilátoru
```

Čísla výstupů jsou stejná jako v [Schématu zapojení](03-wiring.md). V `setup()` oba klíče musí být inicializovány: `myHeater.begin();` a `myFan.begin();`.

!!! warning "Bezpečný stav při spuštění"
    `begin()` okamžitě nastaví `LOW` — ohřívač a ventilátor jsou vypnuty, dokud logika nerozhodne jinak. To je důležité: při zapnutí napájení by se ohřívač neměl náhodou zapnout.

## Údržba teploty histerezí

Pro skříň na `40–45 °C` je dostačující jednoduchá hystereze: vytápění se zapíná a vypíná kolem cíle. To je jednodušší než plný PID a pro měkké udržování tepla funguje spolehlivě.

Hysterezi bereme z menu (`menu.hysteresis`) — to je už připojené v [kapitole 6](06-menu.md). Cílovou teplotu zadá uživatel při spuštění skříně z karty zařízení (`s_targetC`; kartu připojíme dále v této kapitole). Topí se jen v režimu Storage. Přidejte stav a rozhodovací funkci:

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // cíl aktuálního spuštění, z karty

static void controlLoop() {
    // Topit jen v režimu Storage: po Stop skříň chladne.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // z karty
    float hyst   = (float)menu.hysteresis;           // z menu

    if (air < target - hyst) {
        s_heating = true;     // vychladla — topíme
    } else if (air >= target) {
        s_heating = false;    // dosáhla cíle — stop
    }
}
```

Cílová teplota přichází s příkazem ke spuštění z karty; její meze a výchozí hodnota jsou položka `target_temp` v [menu](06-menu.md).

## Ochrana ohřívače termistorem

Vzduch se ohřívá pomalu, ale spirála ohřívače — rychle. Bez samostatného řízení se ohřívač stihne přehřát dříve, než se vzduch dostane na cíl. Proto termistor ohřívače nastavuje tvrdý strop.

```cpp
static const float HEATER_MAX_C = 80.0f;   // strop teploty ohřívače

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // termistor

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // odrazit v telemetrii
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "Strop ohřívače — to je ochrana, ne seřízení klimatu"
    `HEATER_MAX_C` omezuje teplotu samotného ohřívače, ne vzduchu. Hodnota závisí na konstrukci ohřívače a materiálech skříně. Vyberte ji s rezervou pod teplotu, při které se deformují tištěné díly — viz [Žáruvzdorné materiály](../07-3d-printing/04-heat-resistant-materials.md).

Pro měkčí ohřev místo zapínání/vypínání „všechno nebo nic" lze řídit výkon pomocí PWM a pole `heaterPower01[0]` přijímá hodnoty od `0.0` do `1.0`. Pro skříň s měkkým udržováním tepla je výše uvedená jednoduchá logika obvykle dostačující.

## Ventilátor

Ventilátor rozprostírá teplo po skříni. Nejjednodušší logika — zapínáme ho spolu s vytápěním:

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // točíme, zatímco topíme
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // odrazit v telemetrii
}
```

V sériovém řadiči je ventilátor řízený teplotou se samostatnými prahy zapnutí a vypnutí (například zapnutí při `55 °C`, vypnutí při `35 °C`), aby nevybíhal na hranici. Pro skříň lze aplikovat stejný přístup, vázáním prahů na parametry menu.

## Montáž v loop()

```cpp
void loop() {
    s_link.loop();          // síť a automatické publikování

    // senzory (viz krok „Senzory"):
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // rozhodujeme, topit nebo ne
    applyHeater();   // aplikujeme na ohřívač + ochrana
    applyFan();      // aplikujeme na ventilátor
}
```

Pole telemetrie (`heaterPower01`, `fanOn`) fasáda publikuje sama — na portálu je vidět, zda zařízení právě topí a zda ventilátor funguje.

## Karta: spuštění a zastavení

Spuštění a zastavení přicházejí z karty zařízení na portálu a v aplikaci. Firmware je deklaruje jako **akce** karty: jádro je přidá do card manifestu a portál i aplikace samy nakreslí formulář a tlačítka. Příkazy v kódu nerozebíráte — jádro zavolá vaši funkci.

Meze pole teploty a výchozí hodnota se berou z položky menu `target_temp` (30–50 °C, 45) přes most `card_menu_bridge.h`. Hodnota, kterou uživatel zadá, odchází s příkazem ke spuštění a do menu se nezapisuje. Přidejte hlavičku vedle hlaviček menu z kapitoly 6:

```cpp
#include <card/card_menu_bridge.h>
```

Callbacky akcí — před `setup()`:

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // už v mezích 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}
```

V `setup()` za příkazy menu z kapitoly 6 deklarujte akce. Hodnoty menu už jsou v cache, ze které čte karta: `setup()` volá `menu_sync_state_to_cache()` od kapitoly 6.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` a `"IDLE"` — režim jednotky po akci. Dokud je skříň nečinná, karta ukazuje formulář spuštění; v režimu `STORAGE` blok relace a tlačítko Zastavit.
- `MENU_TARGET_TEMP` — id položky `target_temp`; generátor ho uloží do `menu_ids.h`.
- `s_link.status.mode[0]` a `targetTempC[0]` ukazují aktuální stav komory. Po každé změně volejte `publishStatusNow()`, aby se karta přepnula hned.
- `iDryer::UnitMode::Storage` — režim jemného udržování tepla. Je to hlavní režim skříně.
- Změňte teplotu uložení v menu zařízení na portálu — výchozí hodnota pole na kartě ji bude následovat: jádro samo zaznamená změnu menu a manifest publikuje znovu.

Jádro přidá do card manifestu:

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

Na portálu dostane karta nečinné skříně pole `Tepl.` s 45 °C a tlačítko `Úložiště`; po spuštění blok relace s cílem a tlačítko `Zastavit`. V aplikaci hlavní obrazovka ukazuje hodnoty a běžící relaci, spuštění a zastavení jsou na stránce zařízení. Senzory, pole a rozvržení karty rozebírá kapitola [Karta zařízení](../10-build-a-filter/06-card.md) v části o vzduchovém filtru.

!!! warning "Žádné delay() v callbackech"
    Callbacky akcí se volají ze síťového handleru. Jakékoli blokování uvnitř přeruší relaci MQTT. Měňte cíl a stav, skutečnou práci dělejte v `loop()`.

## Úplný `src/main.cpp` po této kapitole

Toto je finální, hotový soubor zařízení. Nové řádky oproti předchozí kapitole jsou označeny `// ← kapitola 7`. Stejný soubor leží jako hotový příklad ve složce `example/09-cabinet/` repozitáře a sestavuje se příkazem `pio run -e cabinet`.

??? note "Co bylo — `src/main.cpp` po kapitole 6"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← kapitola 6: parametry (menu.target_temp …)
    #include <menu_bindings.h>                   // ← kapitola 6: menu_apply_by_bind
    #include <menu_commands.h>                   // ← kapitola 6: menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← kapitola 6: publishConfigRaw

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

    static Sht31ClimateSensor s_climate(&Wire);
    static bool               s_climateOk = false;

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

    // ← kapitola 6: menu na portálu
    static bool s_menuPending = false;

    static void publishMenu() {
        static char buf[MENU_FULL_JSON_BUF_SIZE];
        const size_t len = menu_buildFullJson(buf, sizeof(buf));
        if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
    }

    static void applySet(JsonObjectConst data) {
        const int id = data["id"] | -1;
        float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                         : data["val"].as<float>();
        for (uint16_t i = 0; i < g_bindings_count; i++) {
            if ((int)g_bindings[i].id != id) continue;
            const MenuMeta& m = g_menu_meta[id];
            if (v < m.min_val) v = m.min_val;
            if (v > m.max_val) v = m.max_val;
            menu_apply_by_bind(g_bindings[i].bind, v);
            s_menuPending = true;
            return;
        }
    }

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        menu.initDefaults();                     // ← kapitola 6
        menu.loadFromNVS();                      // ← kapitola 6
        menu_sync_state_to_cache();              // ← kapitola 6
        s_link.begin();
        // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← kapitola 6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← kapitola 6
    }

    void loop() {
        s_link.loop();

        // ← kapitola 6: publikujeme menu po přechodu online a na žádost
        static bool s_wasOnline = false;
        const bool online = s_link.isOnline();
        if (online && !s_wasOnline) s_menuPending = true;
        s_wasOnline = online;
        if (s_menuPending) {
            s_menuPending = false;
            publishMenu();
        }

        if (s_climateOk) {
            s_climate.tick(millis());
            SensorReading r = s_climate.get();
            if (r.ok) {
                s_link.telemetry.airTempC[0]       = r.temperature;
                s_link.telemetry.airHumidityPct[0] = r.humidity;
            }
        }
        s_link.telemetry.heaterTempC[0] = readHeaterTempC();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include <menu_state.h>
#include <menu_bindings.h>
#include <menu_commands.h>
#include <local_access/device_publisher.h>
#include <card/card_menu_bridge.h>        // ← kapitola 7

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

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

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

static bool s_menuPending = false;

static void publishMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    const size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
}

static void applySet(JsonObjectConst data) {
    const int id = data["id"] | -1;
    float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                     : data["val"].as<float>();
    for (uint16_t i = 0; i < g_bindings_count; i++) {
        if ((int)g_bindings[i].id != id) continue;
        const MenuMeta& m = g_menu_meta[id];
        if (v < m.min_val) v = m.min_val;
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);
        s_menuPending = true;
        return;
    }
}

// ← kapitola 7: klíče ohřívače a ventilátoru
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← kapitola 7: logika údržby teploty
static bool        s_heating    = false;
static float       s_targetC    = 0.0f;
static const float HEATER_MAX_C = 80.0f;

static void controlLoop() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) { s_heating = false; return; }
    float air    = s_link.telemetry.airTempC[0];
    float target = s_targetC;
    float hyst   = (float)menu.hysteresis;
    if (air < target - hyst)  s_heating = true;
    else if (air >= target)   s_heating = false;
}

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];
    bool  allow = s_heating && heaterTemp < HEATER_MAX_C;
    if (allow) myHeater.on(); else myHeater.off();
    s_link.telemetry.heaterPower01[0] = allow ? 1.0f : 0.0f;
}

static void applyFan() {
    if (s_heating) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = s_heating;
}

// ← kapitola 7: akce karty
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    s_climateOk = s_climate.begin();
    myHeater.begin();              // ← kapitola 7
    myFan.begin();                 // ← kapitola 7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // Zařízení bylo na portálu odpojeno: smazat tajný klíč, čekat na nové spárování.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← kapitola 7
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}

void loop() {
    s_link.loop();

    static bool s_wasOnline = false;
    const bool online = s_link.isOnline();
    if (online && !s_wasOnline) s_menuPending = true;
    s_wasOnline = online;
    if (s_menuPending) {
        s_menuPending = false;
        publishMenu();
    }

    if (s_climateOk) {
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // ← kapitola 7
    applyHeater();   // ← kapitola 7
    applyFan();      // ← kapitola 7
}
```

## Kontrola výsledku

Po tomto kroku:

- tlačítko `Úložiště` na kartě zařízení převede skříň do režimu Storage se zadanou teplotou, zařízení začne topit;
- teplota vzduchu se přiblíží k cíli a zůstane v mezích histereze;
- ohřívač nepřekročí `HEATER_MAX_C`;
- ventilátor a výkon topení jsou vidět v telemetrii;
- tlačítko `Zastavit` vypne topení a převede do Idle; do dalšího spuštění skříň netopí.

## Co dál

Logika je hotova. Zbývá složit zařízení do skříně a zkontrolovat pod napájením — [Montáž a kontrola](08-assembly-and-check.md).
