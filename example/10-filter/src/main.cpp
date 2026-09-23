// ============================================================
// Умный фильтр воздуха на idryer-core.
// SGP40 (VOC) + вентилятор через MOSFET, авто/ручной режим,
// управление и карточка на портале через card-манифест.
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // индекс без датчика (-DDEMO_VOC=1)

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
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// Датчик или, с -DDEMO_VOC=1, модель воздуха (глава 5).
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // Индекс: ~100 = обычный воздух, выше = грязнее (макс 500).
    g_vocIndex = s_sgp.measureVocIndex();
#endif
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
