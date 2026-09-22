---
title: "Меню устройства из YAML: настройки в NVS и на портале"
description: "Как описать меню устройства на idryer-core в menu.yaml: целевая температура и гистерезис сохраняются в NVS и показываются в меню устройства на портале iDryer."
---

# Меню из YAML

Меню — это набор настроек устройства: целевая температура, гистерezис, пороги вентилятора. На `idryer-core` меню описывается одним файлом `menu.yaml`, а всё остальное — C++-структуры, сохранение в энергонезависимую память (NVS) и публикация на портал — генерируется автоматически.

Это один из ключевых блоков ядра. Вы не пишете код хранения настроек и не придумываете формат для портала — вы только перечисляете параметры в YAML.

## Зачем меню

После предыдущих шагов устройство читает датчики, но все пороги «зашиты» в код. Меню решает три задачи сразу:

- **хранение**: значения переживают перезагрузку (NVS);
- **управление с портала**: портал показывает каждый пункт меню по его типу (число, переключатель);
- **единый источник правды**: один файл описывает и память, и интерфейс.

## Как работает

Один файл `menu.yaml` проходит через генератор при сборке:

```text
menu.yaml → (сборка pio run) → C++-файлы в src/menu/ + NVS + JSON для портала
```

Портал рисует каждый пункт меню по его типу. `role:` даёт пункту переведённую подпись из контракта ядра; пункт без `role:` показывается со своим `title`.

!!! warning "Не редактируйте сгенерированные файлы"
    Файлы `menu_state.*`, `menu_bindings.*`, `menu_ids.h` и другие создаёт генератор. Правьте только `menu.yaml` и пересобирайте — иначе ваши изменения затрутся.

## Шаг 1. Скопируйте шаблон

В библиотеке есть шаблон меню. Скопируйте его в проект:

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## Шаг 2. Подключите генерацию при сборке

Скопируйте образец хука из проекта `iDryer-Storage` (его можно взять как есть, настраивать не нужно):

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

Затем в `platformio.ini` добавьте в секцию `[env:cabinet]` строку `-Isrc/menu` (чтобы код видел `#include <menu_state.h>`) и подключите хук через `extra_scripts`:

```ini
[env:cabinet]
; ... platform / board / lib_deps из главы 4 — без изменений ...

build_flags =
    -Isrc/menu                      ; ← добавили: путь к сгенерированному меню
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← добавили
    pre:extra_scripts/pre_gen_menu.py
```

Хук сам найдёт генератор по пути `lib/idryer-core/menu/menu_gen.py`, поэтому библиотека должна быть подключена через `lib/` (симлинк или копия), как описано в главе 4.

## Шаг 3. Опишите параметры шкафа

Откройте `src/menu/menu.yaml`. В шаблоне уже есть корневой пункт `root` с массивом `children` и примерами параметров. Удалите примеры (`my_param`, `my_flag`, `my_mode_group`) и добавьте свои внутрь `children`. Два последних пункта — `units_count` и `language` — оставьте на месте: это фиксированный контракт с порталом.

Для базового шкафа достаточно нескольких параметров.

Целевая температура хранения:

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # подпись из контракта ядра
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVS-ключ (≤ 15 символов)
  persist: true
  scope: global
  default: 45
```

Гистерезис (на сколько градусов температура может уйти ниже цели, прежде чем снова включится нагрев):

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

!!! note "role: — это закрытый список"
    Значение `role:` нельзя придумать произвольно — оно должно быть из списка `canonical_roles` контракта ядра. Если подходящей роли нет, сборка остановится и покажет список допустимых. Для шкафа хранения подходят роли семейства `storage.*`: `storage.target_temperature`, `storage.target_humidity`, `storage.start`, `storage.stop`. Полный список — в шапке `menu.template.yaml`. `role:` необязателен: параметр без него (как гистерезис выше) хранится и публикуется так же, только подпись берётся из `title`.

Ограничения, которые нельзя нарушать:

- `bind` — не длиннее 15 символов (лимит ключа NVS);
- не добавляйте поле `widget:` в `menu.yaml` — портал и приложение его не читают: пункт меню рисуется по своему типу.

!!! warning "Проверьте пункт ignore_external_cmd из шаблона"
    В шаблоне есть пункт `ignore_external_cmd`, и его `bind` — 19 символов, что превышает лимит 15. Если оставить как есть, генерация упадёт: `bind 'ignore_external_cmd' ... имеет 19 символов, лимит 15`. Либо удалите этот пункт, либо укоротите `bind` до `ign_ext_cmd` (как в реальных продуктах). Для базового шкафа его можно просто удалить.

## Шаг 4. Соберите проект и проверьте генерацию

```bash
pio run -e cabinet
```

При сборке pre-hook сам поставит зависимости (один раз) и сгенерирует C++-файлы меню. Если `menu.yaml` не менялся — генерация пропускается (`up-to-date`).

Проверьте, что генерация прошла. В логе сборки появляется строка о генерации меню, а в папке `src/menu/` — сгенерированные файлы:

```text
src/menu/
├── menu.yaml          # ваш файл (исходник)
├── menu_state.h/.cpp  # объект menu со всеми параметрами
├── menu_bindings.*    # доступ по bind + запись в NVS
├── menu_ids.h
└── menu_meta.h        # и другие
```

Если сборка упала с сообщением про неизвестную `role:` — значит роль написана не из списка `canonical_roles`. Исправьте её и пересоберите. Файлы с пометкой autogen руками не редактируйте.

## Шаг 5. Загрузите меню при старте

Подключите сгенерированное меню в `src/main.cpp` и загрузите его в `setup()` — **до** `s_link.begin()`:

```cpp
#include <menu_state.h>      // объект menu со всеми параметрами
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // выставить значения по умолчанию из YAML
menu.loadFromNVS();          // сохранённые значения; при первом старте сохраняются дефолты
menu_sync_state_to_cache();  // значения — в кэш, из которого собирается публикуемое меню
```

После этого параметры доступны через глобальный объект `menu`:

```cpp
uint16_t target = menu.target_temp;   // прямой доступ к значению
```

## Шаг 6. Меню на портале: публикация и приём изменений

Портал сам меню у устройства не читает: прошивка его публикует и применяет изменения, которые приходят обратно. Три части:

- **публикация** — `menu_buildFullJson()` из ядра собирает JSON меню из `menu.yaml` и текущих значений; `devicePublisher()->publishConfigRaw()` отправляет его на портал (MQTT-топик `config`) и в приложение по локальной сети;
- **когда** — при выходе устройства в онлайн и по команде `get_config`: портал присылает её, когда вы открываете меню устройства (шестерёнка на карточке);
- **изменение** — портал присылает `set` с `id` пункта и новым значением `val`. `menu_apply_by_bind()` записывает значение в `menu`, в NVS и в кэш, затем меню публикуется заново, и портал показывает подтверждённое значение.

Добавьте после заголовков:

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // публиковать меню из loop()

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
        if (v < m.min_val) v = m.min_val;              // пределы из menu.yaml
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + кэш
        s_menuPending = true;                          // показать новое значение на портале
        return;
    }
}
```

В `setup()`, после `s_link.begin()`:

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

В `loop()`, после `s_link.loop()`:

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // только что вышли в онлайн
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "Почему меню публикуется из loop()"
    Колбэки команд вызываются глубоко в сетевом обработчике. Сборка JSON меню там стоит много стека, поэтому колбэк только поднимает флаг, а публикует `loop()`.

`applySet()` зажимает значение в `min`/`max` пункта из `menu.yaml`: входящему числу устройство вслепую не доверяет.

## Полный `src/main.cpp` после этой главы

По сравнению с прошлой главой добавлены строки с пометкой `// ← глава 6`: загрузка меню, его публикация и приём изменений.

??? note "Что было — `src/main.cpp` после главы 5"

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
        // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
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
#include <menu_state.h>                      // ← глава 6: параметры (menu.target_temp …)
#include <menu_bindings.h>                   // ← глава 6: menu_apply_by_bind
#include <menu_commands.h>                   // ← глава 6: menu_buildFullJson
#include <local_access/device_publisher.h>   // ← глава 6: publishConfigRaw

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

// ← глава 6: меню на портале
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
    menu.initDefaults();                     // ← глава 6
    menu.loadFromNVS();                      // ← глава 6
    menu_sync_state_to_cache();              // ← глава 6
    s_link.begin();
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← глава 6
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← глава 6
}

void loop() {
    s_link.loop();

    // ← глава 6: публикуем меню при выходе в онлайн и по запросу
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

## Проверка результата

После прошивки:

- шестерёнка на карточке устройства открывает страницу устройства с меню: целевая температура (портал подписывает её по роли — «Температура хранения») и **HYSTERESIS**;
- измените там значение — устройство примет его, сохранит в NVS и заново опубликует меню, а портал покажет подтверждённое значение;
- после перезагрузки устройство публикует сохранённые значения;
- внутренние параметры (гистерезис) доступны в коде через `menu`.

## Что дальше

Настройки описаны и хранятся. Теперь свяжем их с железом в [Управлении нагревом](07-heating-control.md): нагреватель держит целевую температуру, вентилятор включается по порогу.
