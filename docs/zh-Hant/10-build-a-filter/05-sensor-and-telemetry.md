---
title: "智慧濾清器：VOC 感測器與遙測"
description: "透過 I2C 讀取 SGP40 並透過 onTelemetryPublish 回呼將自訂 vocIndex 欄位發佈到 iDryer 遙測。"
---

# 感測器與遙測

在本章中，濾清器開始測量空氣並將資料傳送到雲端。關鍵技巧——**遙測中的自訂欄位**：生態系統辭彙對 VOC 一無所知，但核心允許在遙測中新增任何欄位。

## 1. 感測器庫

在 `platformio.ini` 中，將其新增到 `lib_deps`：

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. 讀取 SGP40

在 `src/main.cpp` 中（接腳來自[接線圖](03-wiring.md)）：

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = 還沒有資料

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() 自己進行內部感測器補償
    // 指標：~100 = 普通空氣，更高 = 更髒（最高 500）
    g_vocIndex = s_sgp.measureVocIndex();
}
```

在 `setup()` 中加入 `s_link.begin()` 後的 `initVocSensor()` 呼叫，在 `loop()` 中每秒加入 `readVocSensor()`（用 millis 計時器，不用 `delay`！）：

```cpp
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();
    }
}
```

!!! warning "loop 中沒有 delay()"
    `s_link.loop()` 必須持續被呼叫——Wi-Fi、MQTT 和來自入口網站的命令都在它上面。`delay(1000)` 會凍結所有這些。只用 millis 計時器。

## 3. 遙測中的自訂欄位

每 `telemetryPeriodMs` 核心自己收集遙測 JSON 訊息並將其傳送到雲端。對於我們的裝置（一個單元，從辭彙技能中只有風扇），核心組建這樣的訊息：

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

結構說明：

- `units`——**單元**（乾燥腔）陣列。商業 iDryer 乾燥機最多有四個獨立腔室，所以遙測始終是陣列，即使只有一個腔室；
- `units[0]`——第一個（對我們來說唯一的）單元：我們在 `Config` 中指定了 `unitsCount = 1`；
- `fanStatus`——辭彙欄位，來自 `hasFan = true`；
- `rssi`、`uptime`——Wi-Fi 訊號強度和運行時間，核心總是新增。

這條訊息中沒有 VOC——核心不知道我們的感測器。但就在發送前，核心給你的程式碼機會在訊息中寫入自訂欄位。為此你註冊一個**回呼**（callback，「回呼」）——一個函數，你提供給核心，核心在每次發佈時自己呼叫它，將收集的 JSON 作為內部傳遞（`doc` 參數——就是它）。

在 `setup()` 中：

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc 是核心收集的遙測訊息（見上面的 JSON）
    // 我們在第一個單元中寫入我們的 vocIndex 欄位
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

行 `doc["units"][0]["vocIndex"] = g_vocIndex;` 讀起來是這樣的：「在訊息 `doc` 中，取陣列 `units`，在其中第 0 個元素（我們唯一的單元）並寫入 `vocIndex` 欄位」。欄位名稱由你決定——在[下一章](06-card.md)中你會參照它來在卡片上顯示值。

!!! note "如果你遇到「hook」這個詞"
    在核心原始碼中，這個回呼被稱為 `PublishHook`——「hook」（「掛鉤」）意味著同樣的事情：一個庫給你「掛上」函數的點。這兩個術語可以互換；在本文件中我們說「回呼」。

!!! note "Lambda 和為什麼它是「空的」"
    構造 `[](JsonObject doc) { ... }` 稱為 **lambda**——這是一個沒有名字的函數，寫在使用的地方，所以你不需要單獨導出它並發明一個名字。

    開頭的方括號——「捕捉列表」：列出函數帶給自己的本地變數。核心的規則：**方括號總是空的**（`[]`）——lambda 不捕捉任何東西，也不帶著任何狀態走（這被稱為 *stateless*，「無狀態」）。

    技術原因：帶捕捉的 lambda 需要動態記憶體，在 ESP32 上頻繁分配會使堆碎片化，最壞情況下會擊落 Wi-Fi。所以核心只接受簡單的函數。

    實務的結論是一個：所有回呼需要的東西都儲存在**全域**變數中——像我們的 `g_vocIndex`。這個規則適用於所有 `idryer-core` 回呼。

風扇狀態用辭彙方式發佈——當你啟動/關閉它時就寫進核心欄位（邏輯在[第 7 章](07-auto-logic.md)）：

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. 手邊沒有感測器？示範模式

沒有 SGP40 也能走完到卡片為止的流程：指標由空氣模型算出。從範例中複製 `demo_voc.h` 檔案：

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

並在 `platformio.ini` 中加入編譯旗標：

```ini
build_flags =
    -DDEMO_VOC=1
```

兩個分支都藏在同一個函數後面，`loop()` 不知道值是從哪裡來的：

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

模型的行為就像一個裡面有印表機正在列印的房間：風扇停著時，指標從約 `100` 的背景值慢慢往上爬；風扇運轉時則下降。模型的風扇狀態取自你自己的邏輯，因此[第 7 章](07-auto-logic.md)的閾值和遲滯可以直接看到效果。

正式裝置不要設這個旗標：那時編譯的就是真實感測器的分支。

## 5. 檢查

燒錄韌體後，在裝置的 MQTT 串流中（或在序列埠發佈日誌中），遙測訊息看起來像這樣：

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` 是你的自訂欄位，進了雲端，並排在辭彙 `fanStatus` 旁。入口網站已經收到它，但還不知道用它做什麼：在下一章向它展示。

!!! note "自訂欄位只活在即時資料中"
    入口網站把遙測原樣轉發給用戶端，所以卡片會立刻顯示 `vocIndex`。而寫進圖表歷史的，伺服器只記錄辭彙量——溫度、濕度、加熱。遙測圖表上不會有你的自訂欄位；如果需要歷史，就自己收集（例如 Home Assistant 或自己的資料庫）。

對著感測器呼氣或靠近麥克筆——指標應在幾秒內明顯上升。
