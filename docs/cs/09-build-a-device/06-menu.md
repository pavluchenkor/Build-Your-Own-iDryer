---
title: "Nabídka zařízení z YAML: nastavení v NVS a na portálu"
description: "Jak popsat nabídku zařízení v idryer-core v menu.yaml: cílová teplota a hystereze se ukládají v NVS a zobrazují se v menu zařízení na portálu iDryer."
---

# Nabídka z YAML

Nabídka je soubor nastavení zařízení: cílová teplota, hystereze, prahy ventilátoru. V `idryer-core` se nabídka popisuje jedním souborem `menu.yaml`, zatímco vše ostatní — C++-struktury, ukládání do paměti bez napájení (NVS) a publikace na portál — se generují automaticky.

Jedná se o jeden z klíčových prvků jádra. Nepíšete kód pro ukládání nastavení a nevymýšlíte formát pro portál — pouze vypíšete parametry v YAML.

## Proč je nabídka potřebná

Po předchozích krocích zařízení čte senzory, ale všechny prahy jsou „pevně zakódovány" v kódu. Nabídka řeší tři úkoly najednou:

- **Ukládání**: hodnoty přežijí restart (NVS);
- **Správa z portálu**: portál zobrazí každou položku menu podle jejího typu (číslo, přepínač);
- **Jediný zdroj pravdy**: jeden soubor popisuje jak paměť, tak rozhraní.

## Jak to funguje

Jeden soubor `menu.yaml` prochází generátorem během sestavení:

```text
menu.yaml → (pio run) → C++-soubory v src/menu/ + NVS + JSON pro portál
```

Portál kreslí každou položku menu podle jejího typu. `role:` dává položce přeložený popisek z kontraktu jádra; položka bez `role:` se zobrazí se svým `title`.

!!! warning "Neupravujte vygenerované soubory"
    Soubory `menu_state.*`, `menu_bindings.*`, `menu_ids.h` a další vytváří generátor. Upravujte pouze `menu.yaml` a znovu sestavte — jinak budou vaše změny přepsány.

## Krok 1. Zkopírujte šablonu

V knihovně je šablona nabídky. Zkopírujte ji do projektu:

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## Krok 2. Připojte generování během sestavení

Zkopírujte vzor háku z projektu `iDryer-Storage` (můžete jej použít takový, jaký je, není potřeba jej konfigurovat):

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

Poté v `platformio.ini` přidejte do oddílu `[env:cabinet]` řádek `-Isrc/menu` (aby kód viděl `#include <menu_state.h>`) a připojte hák pomocí `extra_scripts`:

```ini
[env:cabinet]
; ... platform / board / lib_deps z kapitoly 4 — beze změn ...

build_flags =
    -Isrc/menu                      ; ← přidáno: cesta k vygenerované nabídce
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← přidáno
    pre:extra_scripts/pre_gen_menu.py
```

Hák sám najde generátor na cestě `lib/idryer-core/menu/menu_gen.py`, takže knihovna musí být připojena přes `lib/` (symlink nebo kopie), jak je popsáno v kapitole 4.

## Krok 3. Popište parametry skříně

Otevřete `src/menu/menu.yaml`. V šabloně je již kořenová položka `root` s polem `children` a příklady parametrů. Odstraňte příklady (`my_param`, `my_flag`, `my_mode_group`) a přidejte své vlastní do `children`. Poslední dvě položky — `units_count` a `language` — ponechte na místě: jedná se o pevný kontrakt s portálem.

Pro základní skříň postačují pouze některé parametry.

Cílová teplota skladování:

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # popisek z kontraktu jádra
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVS-klíč (≤ 15 znaků)
  persist: true
  scope: global
  default: 45
```

Hystereze (o kolik stupňů se může teplota snížit pod cíl, než se topení znovu zapne):

```yaml
- id: hysteresis
  type: value
  title: { ru: "ГИСТЕРЕЗИС", en: "HYSTERESIS" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint8
  min: 1
  max: 5
  step: 1
  bind: hysteresis
  persist: true
  scope: global
  default: 2
```

!!! note "role: — jedná se o uzavřený seznam"
    Hodnotu `role:` nemůžete vymýšlet libovolně — musí pocházet ze seznamu `canonical_roles` smlouvy jádra. Pokud není vhodná role, sestavení se zastaví a zobrazí seznam povolených. Pro skříň skladování se hodí role z rodiny `storage.*`: `storage.target_temperature`, `storage.target_humidity`, `storage.start`, `storage.stop`. Úplný seznam je v záhlaví `menu.template.yaml`. `role:` je volitelná: parametr bez ní (jako hystereze výše) se ukládá i publikuje stejně, jen popisek se bere z `title`.

Omezení, která nesmíte porušit:

- `bind` — ne delší než 15 znaků (limit klíče NVS);
- nepřidávejte do `menu.yaml` pole `widget:` — portál ani aplikace ho nečtou: položka menu se kreslí podle svého typu.

!!! warning "Zkontrolujte položku ignore_external_cmd ze šablony"
    V šabloně je položka `ignore_external_cmd` a její `bind` — 19 znaků, což překračuje limit 15. Pokud to ponecháte tak, jak to je, generování selhá: `bind 'ignore_external_cmd' ... má 19 znaků, limit je 15`. Buď odstraňte tuto položku, nebo zkraťte `bind` na `ign_ext_cmd` (jako v reálných produktech). Pro základní skříň jej můžete jednoduše odstranit.

## Krok 4. Sestavte projekt a zkontrolujte generování

```bash
pio run -e cabinet
```

Během sestavení pre-hook sám nainstaluje závislosti (jednou) a vygeneruje C++-soubory nabídky. Pokud se `menu.yaml` nezměnil — generování se přeskočí (`up-to-date`).

Zkontrolujte, že generování proběhlo. V logu sestavení se zobrazí řádek o generování nabídky a ve složce `src/menu/` — vygenerované soubory:

```text
src/menu/
├── menu.yaml          # váš soubor (zdroj)
├── menu_state.h/.cpp  # objekt menu se všemi parametry
├── menu_bindings.*    # přístup podle bind + zápis do NVS
├── menu_ids.h
└── menu_meta.h        # a další
```

Pokud sestavení selhalo se zprávou o neznámé `role:` — znamená to, že role není ze seznamu `canonical_roles`. Opravte ji a znovu sestavte. Soubory označené autogen neupravujte ručně.

## Krok 5. Načtěte menu při startu

Připojte vygenerované menu v `src/main.cpp` a načtěte ho v `setup()` — **před** `s_link.begin()`:

```cpp
#include <menu_state.h>      // objekt menu se všemi parametry
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // nastavit výchozí hodnoty z YAML
menu.loadFromNVS();          // uložené hodnoty; při prvním startu se uloží výchozí
menu_sync_state_to_cache();  // hodnoty do cache, ze které se skládá publikované menu
```

Potom jsou parametry dostupné přes globální objekt `menu`:

```cpp
uint16_t target = menu.target_temp;   // přímý přístup k hodnotě
```

## Krok 6. Menu na portálu: publikace a příjem změn

Portál si menu ze zařízení sám nečte: firmware ho publikuje a aplikuje změny, které přijdou zpět. Tři části:

- **publikace** — `menu_buildFullJson()` z jádra sestaví JSON menu z `menu.yaml` a aktuálních hodnot; `devicePublisher()->publishConfigRaw()` ho pošle na portál (MQTT topic `config`) a do aplikace po místní síti;
- **kdy** — když zařízení přejde online a na příkaz `get_config`: portál ho pošle, když otevřete menu zařízení (ozubené kolo na kartě);
- **změna** — portál pošle `set` s `id` položky a novou hodnotou `val`. `menu_apply_by_bind()` zapíše hodnotu do `menu`, do NVS a do cache, pak se menu publikuje znovu a portál ukáže potvrzenou hodnotu.

Přidejte za hlavičky:

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // publikovat menu z loop()

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
        if (v < m.min_val) v = m.min_val;              // meze z menu.yaml
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + cache
        s_menuPending = true;                          // ukázat novou hodnotu na portálu
        return;
    }
}
```

V `setup()`, za `s_link.begin()`:

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

V `loop()`, za `s_link.loop()`:

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // právě jsme přešli online
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "Proč se menu publikuje z loop()"
    Callbacky příkazů se volají hluboko v síťovém handleru. Sestavit tam JSON menu stojí hodně zásobníku, proto callback jen nastaví příznak a publikuje `loop()`.

`applySet()` omezí hodnotu na `min`/`max` položky z `menu.yaml`: příchozímu číslu zařízení slepě nevěří.

## Kompletní `src/main.cpp` po této kapitole

Oproti předchozí kapitole přibyly řádky označené `// ← kapitola 6`: načtení menu, jeho publikace a příjem změn.

??? note "Co bylo — `src/main.cpp` po kapitole 5"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"

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

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
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
        s_link.telemetry.heaterTempC[0] = readHeaterTempC();
    }
    ```

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

## Ověření výsledku

Po nahrání firmwaru:

- ozubené kolo na kartě zařízení otevře stránku zařízení s menu: cílová teplota (portál ji popíše podle role — „Storage temperature“) a **HYSTERESIS**;
- změňte tam hodnotu — zařízení ji přijme, uloží do NVS a znovu publikuje menu a portál ukáže potvrzenou hodnotu;
- po restartu zařízení publikuje uložené hodnoty;
- interní parametry (hystereze) jsou v kódu dostupné přes `menu`.

## Co dále

Nastavení jsou popsány a ukládají se. Nyní je propojíme se železem v [Řízení topení](07-heating-control.md): topič udržuje cílovou teplotu, ventilátor se zapíná na základě prahu.
