// Нагреваемый шкаф хранения филамента на ESP32 + idryer-core.
// Эталонный пример к главе 9. Собирается: pio run -e cabinet.
#include <Wire.h>
#include <math.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"
#include <menu_state.h>                      // параметры: menu.target_temp …
#include <menu_bindings.h>                   // menu_apply_by_bind, menu_sync_state_to_cache
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw
#include <card/card_menu_bridge.h>           // параметры действий карточки из меню

// ── Паспорт устройства (глава 4) ─────────────────────────────────────
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

// ── Датчик климата SHT31 (глава 5) ───────────────────────────────────
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ── Термистор нагревателя (глава 5) ──────────────────────────────────
static const int   THERM_PIN  = 2;         // вывод ADC
static const float SERIES_R   = 4700.0f;   // резистор делителя, Ом
static const float NOMINAL_R  = 100000.0f; // сопротивление термистора при 25 °C, Ом
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // B-коэффициент из техописания термистора

static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);
    float v   = (float)raw / 4095.0f;
    float r   = SERIES_R * (1.0f - v) / v;
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}

// ── Ключи нагревателя и вентилятора (глава 7) ────────────────────────
// Простой ключ на GPIO: вывод управляет MOSFET-модулем (версия A) или
// SSR (версия B). on() = HIGH, off() = LOW.
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};   // GPIO4 — управление нагревателем
static GpioOutput myFan{5};      // GPIO5 — управление вентилятором

// ── Логика поддержания температуры (глава 7) ─────────────────────────
static bool        s_heating    = false;
static float       s_targetC    = 0.0f;    // цель текущего запуска (из карточки)
static const float HEATER_MAX_C = 80.0f;   // потолок температуры нагревателя

static void controlLoop() {
    // Греем только в режиме хранения: после «Стоп» шкаф остывает.
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) { s_heating = false; return; }
    float air    = s_link.telemetry.airTempC[0];   // SHT31
    float target = s_targetC;                      // из карточки
    float hyst   = (float)menu.hysteresis;         // из меню
    if (air < target - hyst)  s_heating = true;    // остыли — греем
    else if (air >= target)   s_heating = false;   // дошли до цели — стоп
}

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // термистор
    bool  allow = s_heating && heaterTemp < HEATER_MAX_C;
    if (allow) myHeater.on(); else myHeater.off();
    s_link.telemetry.heaterPower01[0] = allow ? 1.0f : 0.0f;
}

static void applyFan() {
    if (s_heating) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = s_heating;
}

// ── Меню на портале (глава 6) ────────────────────────────────────────
// Портал сам меню не читает: прошивка публикует его при выходе в онлайн и
// по команде get_config, а изменения приходят командой set.
static bool s_menuPending = false;   // публиковать меню из loop()

static void publishMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    const size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
}

// set {id, val}: значение в пределах пункта меню → menu, NVS и кэш; затем
// меню публикуется заново, и портал показывает подтверждённое значение.
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

// ── Действия карточки (глава 7) ──────────────────────────────────────
// Запуск: температура приходит из карточки, SDK уже зажал её в пределы
// пункта меню target_temp. В меню она не пишется.
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
    Wire.begin(8, 9);                 // SDA, SCL — выводы вашей платы
    s_climateOk = s_climate.begin();  // сам находит адрес 0x44 или 0x45
    myHeater.begin();
    myFan.begin();
    menu.initDefaults();              // дефолты из menu.yaml
    menu.loadFromNVS();               // сохранённые значения; при первом старте сохраняются дефолты
    menu_sync_state_to_cache();       // значения — в кэш: из него собирается меню и читает карточка
    s_link.begin();                   // Wi-Fi и привязка — через приложение (глава 4)

    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    // Меню на портале (глава 6).
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    // Карточка: действие «Хранение» с температурой из меню и «Стоп».
    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}

void loop() {
    s_link.loop();   // сеть + автопубликация телеметрии/статуса

    // Меню — при выходе в онлайн и по запросу. Не из колбэка команды: там
    // сборка JSON стоит много стека.
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

    controlLoop();
    applyHeater();
    applyFan();
}
