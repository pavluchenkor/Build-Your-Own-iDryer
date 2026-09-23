---
title: "將SHT31和溫敏電阻感應器連接到idryer-core"
description: "在ESP32上讀取SHT31氣候感應器和加熱器溫敏電阻：填充idryer-core遙測並將資料輸出到iDryer門戶。"
---

# 感應器

本頁你連接兩個感應器並將它們的資料輸出到門戶。首先是SHT31（櫃氣候），然後是溫敏電阻（加熱器溫度）。這是「獲取資料」步驟，在新增控制邏輯之前。

與核心協作的原理很簡單：你的`loop()`程式碼中的程式碼將新鮮讀數寫入`s_link.telemetry.*`欄位，外觀自動每`telemetryPeriodMs`時間發佈到雲端（來自`Config`）。無需手動呼叫發佈。

## 遙測欄位

對於我們的櫃子，使用三個欄位（索引`[0]`——第一個也是唯一的腔室）：

| 欄位 | 儲存內容 | Config中的標誌 |
|------|----------|--------------|
| `s_link.telemetry.airTempC[0]` | 空氣溫度，°C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | 空氣濕度，% | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | 加熱器溫度，°C | `hasHeaterTemp` |

這三個標誌就在本章的`Config`中啟用（完整程式碼見本章結尾）。標誌告訴門戶和應用程式：裝置有這個感應器；沒有它，卡片上不會出現對應的讀數格。

## 規則：感應器程式碼不應阻止loop()

idryer-core外觀在相同的`loop()`中為Wi-Fi和MQTT服務。因此，讀取感應器時不能呼叫`delay()`——暫停會中斷網路會話。感應器按計時器輪詢，現成的值只被讀取。生態系統中現成的驅動程式已經這樣構造了。

## 步驟1。SHT31：櫃氣候

SHT31驅動程式無需從頭寫起——現成的`Sht31ClimateSensor`類就在本章的範例 [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet) 中。它使用`robtillaart/SHT31`函式庫並無阻塞地讀取感應器。

1. 將SHT31函式庫新增到你`platformio.ini`的`lib_deps`：

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. 把驅動程式的四個檔案複製到你的`src/`資料夾：

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. 透過I2C連接感應器（見[接線圖](03-wiring.md)）並在`src/main.cpp`中讀取它：

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — 你的板的引腳
    s_climateOk = s_climate.begin();  // 自動尋找位址0x44或0x45
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
}
```

驅動程式用`sensor_reading.h`中的`SensorReading`結構交出一次讀數快照：

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C，沒有值時為NAN
    float    humidity    = NAN;   // % RH，沒有值時為NAN
    float    pressure    = NAN;   // hPa，為將來的感應器預留
    uint32_t ts_ms       = 0;     // 讀取當下的millis()
    bool     ok          = false; // 溫度和濕度有效時為true
    int      err         = 0;     // 錯誤碼，0——無錯誤
};
```

刷新後，門戶將顯示櫃的溫度和濕度——這是設備的第一個反饋。

## 步驟2。溫敏電阻：加熱器溫度

我沒有現成的ESP32溫敏電阻類，所以我們直接在`src/main.cpp`中編寫。溫敏電阻透過分壓轉換器連接到ADC引腳（見[接線圖](03-wiring.md)）：控制器測量中點的電壓，據此計算溫敏電阻的電阻，然後是溫度。

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // ADC引腳
static const float SERIES_R   = 4700.0f;   // 分壓電阻，Ω
static const float NOMINAL_R  = 100000.0f; // 溫敏電阻在25 °C時的電阻，Ω
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // 來自溫敏電阻技術規格的B係數

// 返回加熱器溫度（°C）。
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // ESP32上0..4095
    float v   = (float)raw / 4095.0f;           // 完整範圍的比例
    float r   = SERIES_R * (1.0f - v) / v;      // 溫敏電阻電阻，Ω
    // B參數形式的Steinhart-Hart方程——見維基百科：
    // https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

在`loop()`中，將結果與SHT31讀數一起寫入遙測：

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "這是簡化的讀數——根據你的溫敏電阻調整參數"
    常數`NOMINAL_R`和`BETA`取決於具體的溫敏電阻——從其技術規格表中取得（常見的家用溫敏電阻——Generic 3950，`100 kΩ`）。分壓公式對應[接線圖](03-wiring.md)中的方案：溫敏電阻至`3.3V`，電阻至`GND`。對於不同的佈線，公式會改變。ESP32上的ADC是非線性的，因此對於精確測量，讀數是校準的——在iDryer系列控制器中，使用溫敏電阻表（`Thermistor`函式庫）進行此操作。

用萬用表檢查溫敏電阻——[溫敏電阻檢查](../06-practical-guides/02-checking-thermistor.md)。

## 步驟3。手邊沒有感應器？示範模式

沒有硬體也能走完到卡片的整條路：讀數由櫃子的模型算出來。從同一個範例複製`demo_sensors.h`檔案：

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

並在`platformio.ini`中加上建置旗標：

```ini
build_flags =
    -DDEMO_SENSORS=1
```

兩個分支都藏在同一個函式後面，`loop()`不知道值是從哪裡來的：

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // 讀取SHT31和溫敏電阻——如上
#endif
}
```

模型的行為和真正的櫃子一樣：房間溫度在`24 °C`附近緩慢波動，加熱器開啟時空氣升溫，關閉時逐漸冷卻，加熱時濕度下降。模型從遙測中讀取加熱功率，因此[加熱控制](07-heating-control.md)一章的邏輯能看到回應，滯後也能正常運作。本節中的螢幕截圖就是這樣做出來的。

正式裝置不要設這個旗標：那時建置的是使用真實感應器的分支。

## 本章後的完整`src/main.cpp`

下面是整個檔案。相對於上一章的新行標記為`// ← 第5章`；其餘部分未改變。

??? note "第4章結束後的`src/main.cpp`"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // 自製裝置：卡片由 card 清單組成
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
        // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← 第5章
#include <math.h>                  // ← 第5章
#include "Sht31ClimateSensor.h"    // ← 第5章
#include "demo_sensors.h"    // ← 第5章：沒有感應器時的讀數（-DDEMO_SENSORS=1）

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自製裝置：卡片由 card 清單組成
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← 第5章
    .hasAirHumidity    = true,     // ← 第5章
    .hasHeaterTemp     = true,  // ← 第5章
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← 第5章：SHT31氣候感應器
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← 第5章：加熱器溫敏電阻
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

// ← 第5章：感應器，或者在 -DDEMO_SENSORS=1 時用櫃子模型
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
    Wire.begin(8, 9);                 // ← 第5章（SDA, SCL — 你的板的引腳）
    s_climateOk = s_climate.begin();  // ← 第5章
    s_link.begin();
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← 第5章
}
```

## 驗證結果

![門戶上的裝置頁面：讀數和圖表](../../img/09-cabinet/05-portal-device.png)
*卡片上的讀數和遙測圖表。加熱器溫度在圖表上是單獨的一條線。頁面下方的選單目前是空的——下一章會處理它。*

此步驟後，門戶應顯示三個值：

- 櫃中空氣溫度；
- 櫃中濕度；
- 加熱器溫度。

如果讀數「漂浮」或明顯錯誤：

- 檢查公共接地和佈線（電源線干擾）——[接線錯誤](../08-common-mistakes/03-wiring-mistakes.md)；
- 檢查分壓電阻的額定值和溫敏電阻類型；
- 確保SHT31透過I2C回應（正確的位址和線路）。

「感應器顯示垃圾」診斷——[溫敏電阻檢查](../06-practical-guides/02-checking-thermistor.md)和[常見錯誤](../08-common-mistakes/01-overview.md)。

## 下一步

我們有來自感應器的資料。現在在[YAML菜單](06-menu.md)中描述設備設定（目標溫度、滯後），以便可以從門戶更改它們。
