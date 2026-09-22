---
title: "來自YAML的設備菜單：NVS和門戶中的設定"
description: "如何在idryer-core中的menu.yaml中描述設備菜單：目標溫度和滯後保存在NVS中，並顯示在iDryer門戶的設備選單中。"
---

# YAML菜單

菜單是設備的一組設定：目標溫度、滯後、風扇閾值。在`idryer-core`上，菜單用一個`menu.yaml`檔案描述，而所有其他內容——C++結構、儲存到非揮發性記憶體（NVS）和發佈到門戶——都是自動生成的。

這是核心的關鍵模組之一。你不編寫設定儲存程式碼，也不為門戶設計格式——你只在YAML中列出參數。

## 為什麼需要菜單

在前面的步驟之後，設備讀取感應器，但所有閾值都"硬編碼"在程式碼中。菜單一次解決三個問題：

- **儲存**：值在重啟後保持（NVS）；
- **從門戶管理**：門戶依類型顯示每個選單項目（數值、開關）；
- **唯一的事實來源**：一個檔案描述記憶體和介面。

## 工作原理

一個`menu.yaml`檔案通過生成器在構建時：

```text
menu.yaml → (pio run構建) → src/menu/中的C++檔案 + NVS + 門戶用JSON
```

門戶依類型繪製每個選單項目。`role:`為項目提供來自核心契約的翻譯標籤；沒有`role:`的項目顯示其`title`。

!!! warning "不要編輯生成的檔案"
    生成器建立`menu_state.*`、`menu_bindings.*`、`menu_ids.h`等檔案。只編輯`menu.yaml`並重新構建——否則你的更改將被抹去。

## 步驟1。複製範本

函式庫中有菜單範本。將其複製到項目：

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## 步驟2。在構建時連接生成

從`iDryer-Storage`項目複製hook樣本（可以按原樣取用，無需配置）：

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

然後在`platformio.ini`中，在`[env:cabinet]`部分新增行`-Isrc/menu`（以便程式碼看到`#include <menu_state.h>`）並透過`extra_scripts`連接hook：

```ini
[env:cabinet]
; ... 第4章的platform / board / lib_deps——無變更 ...

build_flags =
    -Isrc/menu                      ; ← 新增：生成菜單的路徑
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← 新增
    pre:extra_scripts/pre_gen_menu.py
```

Hook會自動在`lib/idryer-core/menu/menu_gen.py`路徑中找到生成器，所以函式庫應透過`lib/`（符號連結或複本）連接，如第4章所述。

## 步驟3。描述櫃的參數

打開`src/menu/menu.yaml`。在範本中已經有根項`root`和數組`children`以及參數範例。刪除範例（`my_param`、`my_flag`、`my_mode_group`）並在`children`內新增自己的。最後兩項——`units_count`和`language`——保留在原位：這是與門戶的固定契約。

對於基本的櫃子，只需幾個參數。

儲存目標溫度：

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # 來自核心契約的標籤
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVS鍵（≤ 15字元）
  persist: true
  scope: global
  default: 45
```

滯後（溫度可以在重新啟動加熱之前下降到目標以下的度數）：

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

!!! note "role: — 這是一個封閉的清單"
    `role:`的值不能隨意想象——它必須來自核心契約的`canonical_roles`列表。如果沒有合適的角色，構建將停止並顯示允許的列表。對於儲存櫃，`storage.*`家族的角色很合適：`storage.target_temperature`、`storage.target_humidity`、`storage.start`、`storage.stop`。完整清單——在`menu.template.yaml`的標題中。`role:`是選用的：沒有它的參數（如上面的滯後）同樣會被儲存和發布，只是標籤取自`title`。

不能違反的限制：

- `bind` — 不超過15個字元（NVS鍵限制）；
- 不要在`menu.yaml`中新增`widget:`欄位——門戶和應用程式都不讀取它：選單項目依其類型繪製。

!!! warning "檢查範本中的ignore_external_cmd項目"
    在範本中有一個`ignore_external_cmd`項，其`bind`——19個字元，超過限制15。如果保留原樣，生成將失敗：`bind 'ignore_external_cmd' ... 有19個字元，限制15`。要麼刪除此項，要麼將`bind`縮短為`ign_ext_cmd`（如實際產品中）。對於基本櫃子，可以簡單地刪除它。

## 步驟4。構建項目並驗證生成

```bash
pio run -e cabinet
```

在構建時，pre-hook本身會安裝依賴項（一次），並生成菜單的C++檔案。如果`menu.yaml`未改變——生成被跳過（`up-to-date`）。

驗證生成已通過。構建日誌中出現菜單生成行，`src/menu/`資料夾中生成文件：

```text
src/menu/
├── menu.yaml          # 你的檔案（源碼）
├── menu_state.h/.cpp  # 帶有所有參數的menu物件
├── menu_bindings.*    # 透過bind存取 + NVS寫入
├── menu_ids.h
└── menu_meta.h        # 及其他
```

如果構建因未知`role:`而失敗——角色不在`canonical_roles`清單中。修復它並重新構建。標記為autogen的檔案不要手動編輯。

## 步驟 5. 啟動時載入選單

在 `src/main.cpp` 中接入產生的選單，並在 `setup()` 中載入——**在** `s_link.begin()` **之前**：

```cpp
#include <menu_state.h>      // 帶有所有參數的menu物件
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // 從YAML設定預設值
menu.loadFromNVS();          // 已儲存的值；首次啟動時儲存預設值
menu_sync_state_to_cache();  // 把值寫入快取，發布的選單由此建立
```

之後即可透過全域物件 `menu` 存取參數：

```cpp
uint16_t target = menu.target_temp;   // 直接存取值
```

## 步驟 6. 門戶上的選單：發布並接收修改

門戶不會自己從裝置讀取選單：由韌體發布選單，並套用傳回來的修改。分三部分：

- **發布**——核心庫的 `menu_buildFullJson()` 根據 `menu.yaml` 和目前的值建立選單 JSON；`devicePublisher()->publishConfigRaw()` 把它傳送到門戶（MQTT 主題 `config`）並透過區域網路傳送到應用程式；
- **時機**——裝置上線時以及收到 `get_config` 指令時：開啟裝置選單（卡片上的齒輪）時門戶會送出該指令；
- **修改**——門戶送出帶有選單項目 `id` 和新值 `val` 的 `set`。`menu_apply_by_bind()` 把值寫入 `menu`、NVS 和快取，然後重新發布選單，門戶顯示已確認的值。

在 include 之後加入：

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // 在 loop() 中發布選單

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
        if (v < m.min_val) v = m.min_val;              // menu.yaml 中的範圍
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + 快取
        s_menuPending = true;                          // 在門戶上顯示新值
        return;
    }
}
```

在 `setup()` 中，`s_link.begin()` 之後：

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

在 `loop()` 中，`s_link.loop()` 之後：

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // 剛剛上線
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "為什麼在 loop() 中發布選單"
    指令回呼在網路處理程式深處被呼叫。在那裡建立選單 JSON 會占用大量堆疊空間，所以回呼只設定旗標，由 `loop()` 發布。

`applySet()` 會把值限制在 `menu.yaml` 中該項目的 `min`/`max` 之間：裝置不會盲目信任收到的數字。

## 本章後的完整`src/main.cpp`

與上一章相比，新增了標記為 `// ← 第6章` 的行：載入選單、發布選單以及接收修改。

??? note "第5章結束後的`src/main.cpp`"

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
        // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
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

## 驗證結果

刷新後：

- 裝置卡片上的齒輪會開啟帶選單的裝置頁面：目標溫度（門戶依角色為其加標籤——「Storage temperature」）和 **HYSTERESIS**；
- 在那裡修改一個值——裝置接受它、儲存到 NVS 並重新發布選單，門戶顯示已確認的值；
- 重新啟動後裝置發布已儲存的值；
- 內部參數（滯後）可在程式碼中透過 `menu` 存取。

## 下一步

設定已描述並儲存。現在在[加熱控制](07-heating-control.md)中連接它們：加熱器維持目標溫度，風扇根據閾值開啟。
