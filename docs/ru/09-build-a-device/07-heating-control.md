---
title: "Управление нагревом шкафа: поддержание температуры и вентилятор"
description: "Логика нагреваемого шкафа на idryer-core: поддержание целевой температуры по гистерезису, защита нагревателя по термистору, вентилятор и команды портала."
---

# Управление нагревом

На этой странице вы связываете датчики, настройки и силовую часть в рабочую логику. Устройство держит в шкафу заданную температуру, защищает нагреватель от перегрева и реагирует на команды с портала.

Логика выполняется в `loop()` рядом с обслуживанием сети. Все таймеры и пороги — неблокирующие, без `delay()`.

## Что должно происходить

Поведение шкафа складывается из трёх простых правил:

1. **Поддержание температуры.** Если воздух в шкафу холоднее цели на величину гистерезиса — включить нагрев. Когда дошли до цели — выключить.
2. **Защита нагревателя.** Термистор контролирует сам нагреватель. Если он перегрелся выше допустимого — нагрев выключается независимо от температуры воздуха.
3. **Вентилятор.** Включается, чтобы разогнать тепло по шкафу, и выключается, когда нагрев не нужен.

## Ключи нагревателя и вентилятора

Нагреватель и вентилятор контроллер включает через ключ: MOSFET-модуль (версия A) или SSR (версия B) — см. [Схему подключения](03-wiring.md). С точки зрения кода это просто вывод GPIO: `HIGH` — включено, `LOW` — выключено.

Опишем такой ключ маленькой структуры и заведём два экземпляра — для нагревателя и вентилятора. Добавьте это в `src/main.cpp` (до `setup()`):

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — управление нагревателем
static GpioOutput myFan{5};      // GPIO5 — управление вентилятором
```

Номера выводов — те же, что в [Схеме подключения](03-wiring.md). В `setup()` оба ключа надо инициализировать: `myHeater.begin();` и `myFan.begin();`.

!!! warning "Безопасное состояние при старте"
    `begin()` сразу ставит `LOW` — нагреватель и вентилятор выключены, пока логика не решит иначе. Это важно: при включении питания нагреватель не должен оказаться включённым случайно.

## Поддержание температуры по гистерезису

Для шкафа на `40–45 °C` достаточно простого гистерезиса: нагрев включается и выключается вокруг цели. Это проще полноценного PID и для мягкого поддержания тепла работает надёжно.

Гистерезис берём из меню (`menu.hysteresis`) — оно уже подключено в [главе 6](06-menu.md). Целевую температуру пользователь задаёт при запуске шкафа с карточки устройства (`s_targetC`; карточку подключим дальше в этой главе). Греем только в режиме Storage. Добавьте состояние и функцию решения:

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // цель текущего запуска, из карточки

static void controlLoop() {
    // Греем только в режиме Storage: после «Стоп» шкаф остывает.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // из карточки
    float hyst   = (float)menu.hysteresis;           // из меню

    if (air < target - hyst) {
        s_heating = true;     // остыли — греем
    } else if (air >= target) {
        s_heating = false;    // дошли до цели — стоп
    }
}
```

Целевая температура приходит с командой запуска из карточки; её пределы и значение по умолчанию — пункт `target_temp` [меню](06-menu.md).

## Защита нагревателя по термистору

Воздух прогревается медленно, а спираль нагревателя — быстро. Без отдельного контроля нагреватель успеет перегреться до того, как воздух дойдёт до цели. Поэтому термистор нагревателя задаёт жёсткий потолок.

```cpp
static const float HEATER_MAX_C = 80.0f;   // потолок температуры нагревателя

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // термистор

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // отразить в телеметрии
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "Потолок нагревателя — это защита, а не настройка климата"
    `HEATER_MAX_C` ограничивает температуру самого нагревателя, а не воздуха. Значение зависит от конструкции нагревателя и материалов корпуса. Выбирайте его с запасом ниже температуры, при которой деформируются печатные детали — см. [Термостойкие материалы](../07-3d-printing/04-heat-resistant-materials.md).

Для более плавного нагрева вместо включения/выключения «всё или ничего» можно управлять мощностью через ШИМ и поле `heaterPower01[0]` принимает значения от `0.0` до `1.0`. Для шкафа с мягким поддержанием тепла простой логики выше обычно достаточно.

## Вентилятор

Вентилятор разгоняет тепло по шкафу. Простейшая логика — включать его вместе с нагревом:

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // крутим, пока греем
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // отразить в телеметрии
}
```

В серийном контроллере вентилятор управляется по температуре с отдельными порогами включения и выключения (например, включение при `55 °C`, выключение при `35 °C`), чтобы он не дёргался у границы. Для шкафа можно применить тот же подход, привязав пороги к параметрам меню.

## Собираем в loop()

```cpp
void loop() {
    s_link.loop();          // сеть и автопубликация

    // датчики (см. шаг «Датчики»):
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // решаем, греть или нет
    applyHeater();   // применяем к нагревателю + защита
    applyFan();      // применяем к вентилятору
}
```

Поля телеметрии (`heaterPower01`, `fanOn`) фасад публикует сам — на портале видно, греет ли устройство сейчас и работает ли вентилятор.

## Карточка: запуск и остановка

Запуск и остановка приходят с карточки устройства на портале и в приложении. Прошивка объявляет их **действиями** карточки: ядро добавляет их в card-манифест, а портал и приложение сами рисуют форму и кнопки. Разбирать команды в коде не нужно — ядро вызывает вашу функцию.

Пределы поля температуры и значение по умолчанию берутся из пункта меню `target_temp` (30–50 °C, 45) через мост `card_menu_bridge.h`. Значение, которое ввёл пользователь, уходит с командой запуска и в меню не пишется. Добавьте заголовок рядом с заголовками меню из главы 6:

```cpp
#include <card/card_menu_bridge.h>
```

Колбэки действий — перед `setup()`:

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // уже в пределах 30..50
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

В `setup()` после команд меню из главы 6 объявите действия. Значения меню уже лежат в кэше, из которого читает карточка: `menu_sync_state_to_cache()` вызывается в `setup()` с главы 6.

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` и `"IDLE"` — режим юнита после действия. Пока шкаф простаивает, карточка показывает форму запуска; в режиме `STORAGE` — блок сессии и кнопку «Стоп».
- `MENU_TARGET_TEMP` — id пункта `target_temp`; генератор кладёт его в `menu_ids.h`.
- `s_link.status.mode[0]` и `targetTempC[0]` показывают текущее состояние камеры. Вызывайте `publishStatusNow()` после каждого изменения, чтобы карточка переключилась сразу.
- `iDryer::UnitMode::Storage` — режим мягкого поддержания тепла. Это основной режим шкафа.
- Измените температуру хранения в меню устройства на портале — значение поля на карточке пойдёт за ней: ядро само заметит изменение меню и перепубликует манифест.

Ядро добавляет в card-манифест:

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

На портале у простаивающего шкафа на карточке появляются поле `Темп.` со значением 45 °C и кнопка `Хранение`; после запуска — блок сессии с целью и кнопка `Стоп`. В приложении на главной — показания и идущая сессия, запуск и остановка — на странице устройства. Сенсоры, поля и раскладка карточки разобраны в главе [Карточка устройства](../10-build-a-filter/06-card.md) раздела про фильтр воздуха.

!!! warning "Никаких delay() в колбэках"
    Колбэки действий вызываются из сетевого обработчика. Любая блокировка внутри рвёт MQTT-сессию. Меняйте цель и статус, а реальную работу делайте в `loop()`.

## Полный `src/main.cpp` после этой главы

Это финальный, законченный файл устройства. Новые относительно прошлой главы строки помечены `// ← глава 7`. Этот же файл лежит как готовый пример в папке `example/09-cabinet/` репозитория и собирается командой `pio run -e cabinet`.

??? note "Что было — `src/main.cpp` после главы 6"

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

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include <menu_state.h>
#include <menu_bindings.h>
#include <menu_commands.h>
#include <local_access/device_publisher.h>
#include <card/card_menu_bridge.h>        // ← глава 7

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

// ← глава 7: ключи нагревателя и вентилятора
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← глава 7: логика поддержания температуры
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

// ← глава 7: действия карточки
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
    myHeater.begin();              // ← глава 7
    myFan.begin();                 // ← глава 7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← глава 7
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

    controlLoop();   // ← глава 7
    applyHeater();   // ← глава 7
    applyFan();      // ← глава 7
}
```

## Проверка результата

После этого шага:

- кнопка `Хранение` на карточке устройства переводит шкаф в режим Storage с введённой температурой, устройство начинает греть;
- температура воздуха подтягивается к цели и держится в пределах гистерезиса;
- нагреватель не уходит выше `HEATER_MAX_C`;
- вентилятор и мощность нагрева видны в телеметрии;
- кнопка `Стоп` выключает нагрев и переводит в Idle; до следующего запуска шкаф не греет.

## Что дальше

Логика готова. Остаётся собрать устройство в корпус и проверить под включением — [Сборка и проверка](08-assembly-and-check.md).
