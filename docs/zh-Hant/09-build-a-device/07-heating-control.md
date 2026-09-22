---
title: "櫃加熱控制：溫度維持和風扇"
description: "idryer-core加熱櫃邏輯：透過滯後維持目標溫度，根據溫敏電阻保護加熱器，風扇和門戶命令。"
---

# 加熱控制

本頁你將感應器、設定和電源部分連接到工作邏輯。設備將櫃內保持在設定的溫度，保護加熱器免過熱，並回應門戶命令。

邏輯在`loop()`中執行，位於網路維護旁邊。所有計時器和閾值——無阻塞，無`delay()`。

## 應該發生什麼

櫃的行為由三個簡單規則組成：

1. **溫度維持。** 如果櫃內空氣冷於目標溫度加滯後——開啟加熱。到達目標後——關閉。
2. **加熱器保護。** 溫敏電阻控制加熱器本身。如果超過允許溫度——加熱獨立關閉，無論空氣溫度如何。
3. **風扇。** 開啟以在櫃內傳播熱量，在不需要加熱時關閉。

## 加熱器和風扇開關

控制器透過開關控制加熱器和風扇：MOSFET模組（版本A）或SSR（版本B）——見[接線圖](03-wiring.md)。從程式碼角度，這只是GPIO引腳：`HIGH` — 開啟，`LOW` — 關閉。

用小結構描述此類開關，並製作兩個例項——一個用於加熱器，一個用於風扇。在`src/main.cpp`中新增這個（在`setup()`之前）：

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — 加熱器控制
static GpioOutput myFan{5};      // GPIO5 — 風扇控制
```

引腳號與[接線圖](03-wiring.md)中的相同。在`setup()`中都應初始化：`myHeater.begin();`和`myFan.begin();`。

!!! warning "啟動時的安全狀態"
    `begin()`立即設定`LOW` — 加熱器和風扇關閉，直到邏輯決定否則。這很重要：開啟電源時，加熱器不應意外開啟。

## 透過滯後維持溫度

對於`40-45 °C`的櫃子，簡單的滯後就足夠了：加熱在目標周圍開啟和關閉。這比完整的PID更簡單，對於溫和的保溫效果可靠。

磁滯取自選單（`menu.hysteresis`）——它已在[第 6 章](06-menu.md)中接入。目標溫度由使用者在裝置卡片上啟動儲料櫃時設定（`s_targetC`；卡片在本章後面接入）。只在 Storage 模式下加熱。新增狀態和判斷函式：

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // 本次執行的目標，來自卡片

static void controlLoop() {
    // 只在 Storage 模式下加熱：停止後櫃子自然冷卻。
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // 來自卡片
    float hyst   = (float)menu.hysteresis;           // 來自菜單

    if (air < target - hyst) {
        s_heating = true;     // 冷卻 — 加熱
    } else if (air >= target) {
        s_heating = false;    // 達到目標 — 停止
    }
}
```

目標溫度隨卡片的啟動指令一起送達；它的範圍和預設值是[選單](06-menu.md)中的 `target_temp` 項目。

## 根據溫敏電阻保護加熱器

空氣加熱緩慢，加熱器螺旋快速。沒有單獨的控制，加熱器會在空氣到達目標前就過熱。因此加熱器溫敏電阻設定了硬限制。

```cpp
static const float HEATER_MAX_C = 80.0f;   // 加熱器溫度上限

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // 溫敏電阻

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // 反映在遙測中
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "加熱器上限是保護，不是氣候控制"
    `HEATER_MAX_C`限制加熱器本身的溫度，不是空氣。值取決於加熱器構造和外殼材料。選擇時要有餘量，低於列印件變形的溫度——見[熱耐受材料](../07-3d-printing/04-heat-resistant-materials.md)。

為了更平順的加熱，不是打開/關閉「全開或全閉」，可透過PWM控制功率，`heaterPower01[0]`接受`0.0`至`1.0`的值。對於帶溫和保溫的櫃子，上面的簡單邏輯通常足夠。

## 風扇

風扇在櫃內傳播熱量。最簡單的邏輯——在加熱時開啟：

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // 加熱時旋轉
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // 反映在遙測中
}
```

在系列控制器中，風扇透過有單獨開啟和關閉閾值的溫度控制（例如，`55 °C`時開啟，`35 °C`時關閉），以免在邊界處抖動。對於櫃子，可以應用相同的方法，將閾值繫結到菜單參數。

## 在loop()中組裝

```cpp
void loop() {
    s_link.loop();          // 網路和自動發佈

    // 感應器（見「感應器」步驟）：
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // 決定是否加熱
    applyHeater();   // 應用到加熱器 + 保護
    applyFan();      // 應用到風扇
}
```

遙測欄位（`heaterPower01`、`fanOn`）由外觀自動發佈——在門戶中看到設備現在是否加熱和風扇是否工作。

## 卡片：啟動和停止

啟動和停止來自門戶和應用程式中的裝置卡片。韌體把它們宣告為卡片 **動作**：核心庫把它們加入 card 清單，門戶和應用程式自行繪製表單和按鈕。程式碼中無需解析指令——核心庫會呼叫你的函式。

溫度欄位的範圍和預設值透過橋接 `card_menu_bridge.h` 取自選單項目 `target_temp`（30–50 °C，45）。使用者輸入的值隨啟動指令送出，不會寫入選單。在第 6 章的選單標頭檔旁邊加入標頭檔：

```cpp
#include <card/card_menu_bridge.h>
```

動作回呼——放在 `setup()` 之前：

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // 已在 30..50 範圍內
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

在 `setup()` 中，在第 6 章的選單指令之後宣告動作。選單值已經在卡片讀取的快取中：從第 6 章起 `setup()` 就呼叫了 `menu_sync_state_to_cache()`。

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` 和 `"IDLE"` —— 動作之後單元的模式。櫃子閒置時，卡片顯示啟動表單；模式為 `STORAGE` 時，顯示工作階段區塊和停止按鈕。
- `MENU_TARGET_TEMP` —— `target_temp` 項目的 id；產生器把它寫入 `menu_ids.h`。
- `s_link.status.mode[0]` 和 `targetTempC[0]` 顯示腔體目前狀態。每次改變後呼叫 `publishStatusNow()`，卡片會立即切換。
- `iDryer::UnitMode::Storage` —— 溫和保溫模式。這是儲料櫃的主要模式。
- 在門戶的裝置選單中修改儲存溫度——卡片欄位的預設值會隨之改變：核心庫會自己發現選單變化並重新發布清單。

核心庫在 card 清單中加入：

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

在門戶上，閒置儲料櫃的卡片出現 `Temp.` 欄位（45 °C）和 `Storage` 按鈕；啟動後顯示帶目標值的工作階段區塊和 `Stop` 按鈕。在應用程式中，首頁顯示讀數和進行中的工作階段，啟動和停止在裝置頁面。卡片的感測器、欄位和版面在空氣過濾器部分的 [裝置卡片](../10-build-a-filter/06-card.md) 一章中介紹。

!!! warning "回呼中不要使用 delay()"
    動作回呼由網路處理程式呼叫。回呼中的任何阻塞都會中斷 MQTT 工作階段。只修改目標值和狀態，實際工作放在 `loop()` 中。

## 本章後的完整`src/main.cpp`

這是最終完整的設備檔案。相對於上一章的新行標記為`// ← 第7章`。同一個檔案作為現成範例位於儲存庫的`example/09-cabinet/`資料夾中，並透過`pio run -e cabinet`命令構建。

??? note "第6章結束後的`src/main.cpp`"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← 第6章：參數（menu.target_temp …）
    #include <menu_bindings.h>                   // ← 第6章：menu_apply_by_bind
    #include <menu_commands.h>                   // ← 第6章：menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← 第6章：publishConfigRaw

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

    // ← 第6章：門戶上的選單
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
        menu.initDefaults();                     // ← 第6章
        menu.loadFromNVS();                      // ← 第6章
        menu_sync_state_to_cache();              // ← 第6章
        s_link.begin();
        // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← 第6章
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← 第6章
    }

    void loop() {
        s_link.loop();

        // ← 第6章：上線時和收到請求時發布選單
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
#include <card/card_menu_bridge.h>        // ← 第7章

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

// ← 第7章：加熱器和風扇開關
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← 第7章：溫度維持邏輯
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

// ← 第7章：卡片動作
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
    myHeater.begin();              // ← 第7章
    myFan.begin();                 // ← 第7章
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← 第7章
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

    controlLoop();   // ← 第7章
    applyHeater();   // ← 第7章
    applyFan();      // ← 第7章
}
```

## 驗證結果

此步驟後：

- 裝置卡片上的 `Storage` 按鈕以輸入的溫度讓儲料櫃進入 Storage 模式，裝置開始加熱；
- 空氣溫度逼近目標，保持在滯後範圍內；
- 加熱器不會超過`HEATER_MAX_C`；
- 風扇和加熱功率在遙測中可見；
- `Stop` 按鈕關閉加熱並轉為 Idle；在下次啟動前櫃子不再加熱。

## 下一步

邏輯已準備好。剩下的是將設備組裝到外殼中並檢查在打開——[組裝和檢查](08-assembly-and-check.md)。
