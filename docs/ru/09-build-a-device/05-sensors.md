---
title: "Подключение датчиков SHT31 и термистора к idryer-core"
description: "Чтение датчика климата SHT31 и термистора нагревателя на ESP32: заполнение телеметрии idryer-core и вывод данных на портал iDryer."
---

# Датчики

На этой странице вы подключаете два датчика и выводите их данные на портал. Сначала SHT31 (климат шкафа), затем термистор (температура нагревателя). Это шаг «получили данные» перед тем, как добавлять логику управления.

Принцип работы с ядром простой: ваш код в `loop()` записывает свежие показания в поля `s_link.telemetry.*`, а фасад сам публикует их в облако каждые `telemetryPeriodMs` из `Config`. Вызывать публикацию вручную не нужно.

## Поля телеметрии

Для нашего шкафа используются три поля (индекс `[0]` — первая и единственная камера):

| Поле | Что хранит | Флаг в Config |
|------|------------|---------------|
| `s_link.telemetry.airTempC[0]` | температура воздуха, °C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | влажность воздуха, % | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | температура нагревателя, °C | `hasHeaterTemp` |

Эти три флага включаются здесь же, в `Config` (см. полный листинг в конце главы). Флаг говорит порталу и приложению, что у устройства есть такой датчик: без него ячейка на карточке не появится.

## Правило: код датчика не должен блокировать loop()

Фасад `idryer-core` обслуживает Wi-Fi и MQTT в том же `loop()`. Поэтому при чтении датчиков нельзя вызывать `delay()` — пауза рвёт сетевую сессию. Датчик опрашивают по таймеру, а готовое значение просто читают. Готовые драйверы из экосистемы уже устроены так.

## Шаг 1. SHT31: климат шкафа

Драйвер SHT31 писать с нуля не нужно — готовый класс `Sht31ClimateSensor` лежит в примере этой главы, [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet). Он использует библиотеку `robtillaart/SHT31` и читает датчик без блокировки.

1. Добавьте библиотеку SHT31 в `lib_deps` своего `platformio.ini`:

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. Скопируйте в свою папку `src/` четыре файла драйвера:

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. Подключите датчик по I2C (см. [Схему подключения](03-wiring.md)) и прочитайте его в `src/main.cpp`:

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — выводы вашей платы
    s_climateOk = s_climate.begin();  // сам находит адрес 0x44 или 0x45
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
}
```

Один снимок показаний драйвер отдаёт структурой `SensorReading` из `sensor_reading.h`:

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C, NAN если нет значения
    float    humidity    = NAN;   // % RH, NAN если нет значения
    float    pressure    = NAN;   // гПа, под будущие датчики
    uint32_t ts_ms       = 0;     // millis() на момент чтения
    bool     ok          = false; // true, если температура и влажность годные
    int      err         = 0;     // код ошибки, 0 — нет ошибки
};
```

После прошивки на портале появятся температура и влажность шкафа — это первая обратная связь от устройства.

## Шаг 2. Термистор: температура нагревателя

Готового класса термистора для ESP32 у меня нет, поэтому чтение пишем сами прямо в `src/main.cpp`. Термистор подключён к выводу АЦП через преобразователь напряжения (см. [Схему подключения](03-wiring.md)): контроллер измеряет напряжение в средней точке, по нему высчитывается сопротивление термистора, а затем — температуры.

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // вывод ADC
static const float SERIES_R   = 4700.0f;   // резистор делителя, Ом
static const float NOMINAL_R  = 100000.0f; // сопротивление термистора при 25 °C, Ом
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // B-коэффициент из техописания термистора

// Возвращает температуру нагревателя в °C.
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 на ESP32
    float v   = (float)raw / 4095.0f;           // доля от полной шкалы
    float r   = SERIES_R * (1.0f - v) / v;      // сопротивление термистора, Ом
    // Уравнение Стейнхарта–Харта в форме B-параметра — см. Википедию:
    // https://ru.wikipedia.org/wiki/Уравнение_Стейнхарта_—_Харта
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

В `loop()` записывайте результат в телеметрию рядом с чтением SHT31:

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "Это упрощённое чтение — подгоните параметры под свой термистор"
    Константы `NOMINAL_R` и `BETA` зависят от конкретного термистора — возьмите их из его техописания (распространённый бытовой термистор — Generic 3950, `100 kΩ`). Формула делителя соответствует схеме из [Схемы подключения](03-wiring.md): термистор к `3.3V`, резистор к `GND`. При другой разводке формула меняется. ADC у ESP32 нелинейный, поэтому для точных измерений показания калибруют — в серийных контроллерах iDryer для этого используется таблица термистора (библиотека `Thermistor`).

Проверка термистора мультиметром — [Проверка термистора](../06-practical-guides/02-checking-thermistor.md).

## Шаг 3. Нет датчиков под рукой? Демо-режим

Пройти путь до карточки можно и без железа: показания посчитает модель шкафа. Скопируйте файл `demo_sensors.h` из того же примера:

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

и добавьте в `platformio.ini` флаг сборки:

```ini
build_flags =
    -DDEMO_SENSORS=1
```

Обе ветки спрятаны за одной функцией, и `loop()` не знает, откуда пришли значения:

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // чтение SHT31 и термистора — как выше
#endif
}
```

Модель ведёт себя как настоящий шкаф: комната медленно колеблется около `24 °C`, включённый нагреватель греет воздух, выключенный даёт остыть, при нагреве падает влажность. Мощность нагрева модель читает из телеметрии, поэтому логика из главы [Управление нагревом](07-heating-control.md) видит отклик и гистерезис работает. Снимки экрана в этом разделе сделаны именно так.

Для рабочего устройства флаг не ставят: тогда собирается ветка с настоящими датчиками.

## Полный `src/main.cpp` после этой главы

Ниже — весь файл целиком. Новые строки относительно прошлой главы помечены `// ← глава 5`; остальное не менялось.

??? note "Что было — `src/main.cpp` после главы 4"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // своё устройство: карточку собирает манифест
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
        // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← глава 5
#include <math.h>                  // ← глава 5
#include "Sht31ClimateSensor.h"    // ← глава 5
#include "demo_sensors.h"    // ← глава 5: показания без датчиков (-DDEMO_SENSORS=1)

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // своё устройство: карточку собирает манифест
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← глава 5
    .hasAirHumidity    = true,     // ← глава 5
    .hasHeaterTemp     = true,  // ← глава 5
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← глава 5: датчик климата SHT31
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← глава 5: термистор нагревателя
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

// ← глава 5: датчики или, с -DDEMO_SENSORS=1, модель шкафа
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
    Wire.begin(8, 9);                 // ← глава 5  (SDA, SCL — выводы вашей платы)
    s_climateOk = s_climate.begin();  // ← глава 5
    s_link.begin();
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← глава 5
}
```

## Проверка результата

![Страница устройства на портале: показания и график](../../img/09-cabinet/05-portal-device.png)
*Показания на карточке и график телеметрии. Температура нагревателя идёт отдельной линией на графике. Меню внизу страницы пока пустое — им займётся следующая глава.*

После этого шага на портале должны отображаться три величины:

- температура воздуха в шкафу;
- влажность в шкафу;
- температура нагревателя.

Если показания «плавают» или явно неверны:

- проверьте общую землю и разводку (помехи от силовых проводов) — [Ошибки проводки](../08-common-mistakes/03-wiring-mistakes.md);
- проверьте номинал резистора делителя и тип термистора;
- убедитесь, что SHT31 отвечает по I2C (правильный адрес и линии).

Диагностика «датчик показывает ерунду» — [Проверка термистора](../06-practical-guides/02-checking-thermistor.md) и [Типовые ошибки](../08-common-mistakes/01-overview.md).

## Что дальше

Данные с датчиков есть. Теперь опишем настройки устройства (целевая температура, гистерезис) в [Меню из YAML](06-menu.md), чтобы их можно было менять с портала и хранить в памяти.
