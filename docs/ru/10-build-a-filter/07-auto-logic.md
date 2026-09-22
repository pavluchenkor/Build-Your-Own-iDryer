---
title: "Умный фильтр: логика автоматики"
description: "Порог и гистерезис по VOC-индексу, режимы auto/on/off с портала, сохранение настроек в NVS и публикация состояния вентилятора."
---

# Логика автоматики

Соединяем всё: датчик решает, вентилятор крутит, портал управляет.

## 1. Состояние и настройки

В начало `src/main.cpp`:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC-индекс включения
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences` (NVS) — энергонезависимая память ESP32: режим и порог переживут перезагрузку.

## 2. Управление вентилятором

Всё включение и выключение сведём в одну функцию `setFan`. Она принимает один аргумент `on` — желаемое состояние: `true` = включить, `false` = выключить. Дальше по коду мы всегда зовём `setFan(true)` / `setFan(false)`, а она делает всю рутину: дёргает пин, запоминает состояние и сообщает порталу.

```cpp
static void setFan(bool on) {      // on — аргумент: true = включить, false = выключить
    if (g_fanOn == on) return;     // уже в нужном состоянии — ничего не делаем
    g_fanOn = on;                  // запоминаем новое состояние в глобальной переменной
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // физически включаем/выключаем ключ вентилятора

    // Сообщаем состояние ядру: fanOn[0] — словарное поле телеметрии
    // (появилось из hasFan = true; [0] — наш единственный юнит, как в главе 5).
    // Отсюда оно уедет в облако и на ячейку «Вентилятор» карточки.
    s_link.telemetry.fanOn[0] = on;

    // Смена состояния — повод отправить телеметрию сразу, не ждать периода.
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` делает отклик мгновенным: нажали на портале — через секунду карточка показывает подтверждённое состояние. Именно подтверждённое: портал iDryer никогда не «угадывает» состояние, он показывает то, что устройство реально прислало.

## 3. Автоматика с гистерезисом

Если включать вентилятор ровно на пороге, около порога он будет дребезжать вкл/выкл. Лечится зазором:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: включаемся на пороге, выключаемся на 20 пунктов ниже.
    if (g_vocIndex < 0) return;                      // датчик ещё молчит
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

Вызывать `tickAutoLogic()` будем там же, где читаем датчик, — в `loop()` по секундному таймеру. Это тот самый `loop()` из главы 5, в него добавляется одна строка. Целиком он теперь выглядит так:

```cpp
void loop() {
    s_link.loop();                        // сеть, телеметрия, команды — всегда первым

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // раз в секунду:
        s_lastReadMs = now;
        readVocSensor();                  //   читаем VOC (глава 5)
        tickAutoLogic();                  //   и сразу принимаем решение по вентилятору
    }
}
```

Порядок внутри секундного блока не случаен: сначала свежее показание датчика, потом решение по нему.

## 4. Колбэки с портала

Те самые функции, что мы обещали в [главе 6](06-card.md):

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // применяем сразу, не ждём следующего тика
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

Эти функции **заменяют** заглушки из главы 6 — пустые версии удалите.

Обратите внимание, чего в этом коде **нет**: разбора MQTT, топиков, JSON команд. Пользователь выбрал `on` в списке на портале → ядро получило команду, проверило её и вызвало `onModeSelected("on")`. Вся транспортная механика — забота ядра.

## 5. Финальный setup()

Осталось добавить в `setup()` две вещи: загрузку сохранённых настроек из NVS (в начале, чтобы логика сразу работала с ними) и настройку пина вентилятора. Целиком `setup()` после этой главы выглядит так:

```cpp
void setup() {
    Serial.begin(115200);

    // Настройки из NVS: то, что пользователь выбирал в прошлые разы.
    s_prefs.begin("filter");   // открыть пространство имён "filter" в NVS
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // Вторые аргументы getUChar/getInt — значения по умолчанию: вернутся
    // при самом первом запуске, когда в NVS ещё ничего не сохранено.

    pinMode(FAN_PIN, OUTPUT);  // пин ключа вентилятора — на выход

    s_link.begin();
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Телеметрия: своё поле vocIndex (глава 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Карточка: сенсор + органы управления (глава 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Разметка (глава 6, по желанию).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. Проверка сценариев

| Действие | Ожидание |
|---|---|
| Режим `auto`, дунуть на датчик | VOC растёт, на пороге вентилятор включается, карточка показывает «Вкл» |
| Воздух очистился | ниже порога−20 вентилятор выключается сам |
| Режим `on` с портала | вентилятор крутит независимо от VOC |
| Режим `off` с портала | вентилятор стоит, VOC продолжает показываться |
| Перезагрузка платы | режим и порог сохранились |

## 7. Итоговый код: src/main.cpp целиком

Весь код глав 4–7, собранный в один файл. Если что-то не сходится с вашим — сверяйтесь с этим листингом.

```cpp
// ============================================================
// Умный фильтр воздуха на idryer-core.
// SGP40 (VOC) + вентилятор через MOSFET, авто/ручной режим,
// управление и карточка на портале через card-манифест.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>

// ── Пины ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // затвор MOSFET вентилятора
// SDA=8, SCL=9 — задаются в Wire.begin() ниже

// ── Паспорт устройства (глава 4) ────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // нестандартное устройство
    .unitsCount        = 1,
    .hasFan            = true,        // единственный словарный навык
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── Состояние (глава 7) ─────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC-индекс включения
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: настройки переживают перезагрузку

// ── Датчик VOC (глава 5) ────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = данных ещё нет

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // Индекс: ~100 = обычный воздух, выше = грязнее (макс 500).
    g_vocIndex = s_sgp.measureVocIndex();
}

// ── Вентилятор (глава 7) ────────────────────────────────────
static void setFan(bool on) {         // on: true = включить, false = выключить
    if (g_fanOn == on) return;        // уже в нужном состоянии
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // словарное поле → облако → карточка
    s_link.publishTelemetryNow();     // смена состояния — публикуем сразу
}

// ── Автоматика с гистерезисом (глава 7) ─────────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: включаемся на пороге, выключаемся на 20 пунктов ниже.
    if (g_vocIndex < 0) return;       // датчик ещё молчит
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── Колбэки команд с портала (главы 6–7) ────────────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // применяем сразу
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: настройки, сеть, датчик, карточка ────────────────
void setup() {
    Serial.begin(115200);

    // Настройки из NVS (вторые аргументы — дефолты первого запуска).
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi, MQTT, привязка — всё внутри
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // Телеметрия: дописываем своё поле vocIndex (глава 5).
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // Карточка: сенсор + органы управления (глава 6).
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // Заводская разметка карточки (глава 6, по желанию).
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: сеть всегда, датчик и логика раз в секунду ────────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // сеть, телеметрия, команды — всегда первым

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // свежее показание…
        tickAutoLogic();              // …и сразу решение по нему
    }
}
```
