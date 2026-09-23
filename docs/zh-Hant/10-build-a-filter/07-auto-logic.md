---
title: "智慧濾清器：自動化邏輯"
description: "VOC 指標的閾值和遲滯、來自入口網站的 auto/on/off 模式、在 NVS 中保存設定並發佈風扇狀態。"
---

# 自動化邏輯

把所有東西串連起來：感測器決策，風扇執行，入口網站控制。

## 1. 狀態和設定

在 `src/main.cpp` 開頭：

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC 指標啟動點
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences`（NVS）——ESP32 的非揮發性記憶體：模式和閾值在重新啟動後仍會保留。

## 2. 風扇控制

我們把所有開啟和關閉都歸結為一個函數 `setFan`。它取一個引數 `on`——想要的狀態：`true` = 開啟，`false` = 關閉。下去的程式碼中我們總是呼叫 `setFan(true)` / `setFan(false)`，它做所有慣例：轉動接腳、記得狀態並通知入口網站。

```cpp
static void setFan(bool on) {      // on 引數：true = 開啟，false = 關閉
    if (g_fanOn == on) return;     // 已在想要的狀態——什麼都不做
    g_fanOn = on;                  // 在全域變數中記得新狀態
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // 實際上開啟/關閉風扇開關

    // 告知核心狀態：fanOn[0] 是遙測辭彙欄位
    //（來自 hasFan = true；[0] 是我們唯一的單元，如第 5 章）
    // 從這裡它進去雲端和卡片「風扇」儲存格
    s_link.telemetry.fanOn[0] = on;

    // 狀態改變——立即傳送遙測，不要等週期
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()` 讓反應瞬間：在入口網站點擊——一秒後卡片顯示確認狀態。已確認的：iDryer 入口網站永遠不會「猜測」狀態，它顯示裝置真的傳來的。

## 3. 自動化與遲滯

如果恰好在閾值處開啟風扇，它會在閾值附近頻繁抖動（開了又關）。解決方法是加入死區：

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // 自動：在閾值開啟，在 20 點以下關閉
    if (g_vocIndex < 0) return;                      // 感測器還沒說話
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

我們會在同樣讀感測器的地方呼叫 `tickAutoLogic()`——在 `loop()` 中每秒計時器。這是第 5 章的 `loop()`，加一行。現在整個看起來像這樣：

```cpp
void loop() {
    s_link.loop();                        // 網路、遙測、命令——總是先
    
    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // 每秒：
        s_lastReadMs = now;
        readVocSensor();                  //   讀 VOC（第 5 章）
        tickAutoLogic();                  //   並立即決定風扇
    }
}
```

秒塊內的順序不是巧合：先新鮮感測器讀數，然後對它決定。

## 4. 來自入口網站的回呼

我們在[第 6 章](06-card.md)承諾的那些函數：

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // 立即套用，不等下一個計時節拍
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

這些函數**替換**第 6 章的空實體——移除空版本。

注意這個程式碼中**沒有**什麼：MQTT 分析、主題、JSON 命令。使用者在入口網站上選擇了清單中的 `on` → 核心收到命令、驗證並用 `onModeSelected("on")` 呼叫。所有傳輸機制都是核心的責任。

## 5. 最終 setup()

還要在 `setup()` 中加兩件事：從 NVS 載入儲存設定（在開始，所以邏輯立即有它們）和設定風扇接腳。這章後 `setup()` 整個看起來像這樣：

```cpp
void setup() {
    Serial.begin(115200);

    // 從 NVS 設定：使用者在過去選擇的
    s_prefs.begin("filter");   // 在 NVS 中打開「filter」命名空間
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // getUChar/getInt 的第二個引數——預設值：當 NVS 中還沒有
    // 什麼（首次啟動時）時，它們會回傳

    pinMode(FAN_PIN, OUTPUT);  // 風扇開關接腳——設為輸出

    s_link.begin();
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // 遙測：自訂 vocIndex 欄位（第 5 章）
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // 卡片：感測器 + 控制（第 6 章）
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // 佈局（第 6 章，選擇性）
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. 檢查情境

| 動作 | 期望 |
|---|---|
| 模式 `auto`，向感測器吹氣 | VOC 增加，在閾值處風扇啟動，卡片顯示「開」 |
| 空氣清淨 | 低於閾值−20，風扇自己關閉 |
| 入口網站的模式 `on` | 風扇不管 VOC 轉動 |
| 入口網站的模式 `off` | 風扇停止，VOC 繼續顯示 |
| 重新啟動主板 | 模式和閾值保存 |

實際看起來是這樣。閾值 `150`，指標到達它——風扇自己啟動了：

![自動化在閾值處啟動了風扇](../../img/10-filter/07-portal-auto-on.png)
*模式 `auto`：指標 `151`、閾值 `150`——風扇已開啟。*

![行動應用程式中的同一畫面](../../img/10-filter/07-app-auto-on.png)
*應用程式顯示同樣的狀態：數值上升，風扇運轉中。*

手動模式會壓過自動化：`Mode` → `on`，即使空氣已經乾淨，風扇照樣轉：

![手動模式：空氣乾淨時風扇仍開啟](../../img/10-filter/07-portal-mode-on.png)
*模式 `on`、指標 `132`——低於閾值，但風扇仍運轉：來自入口網站的命令優先於自動化。*

## 7. 最終程式碼：src/main.cpp 完整

第 4–7 章所有程式碼，組裝成一個檔案。如果你的東西對不上——用這個清單檢查。

```cpp
// ============================================================
// 智慧空氣濾清器在 idryer-core 上
// SGP40（VOC）+ 透過 MOSFET 的風扇，自動/手動模式，
// 透過 card manifest 在入口網站控制和卡片
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // 無感測器時的指標（-DDEMO_VOC=1）

// ── 接腳 ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // 風扇 MOSFET 閘極
// SDA=8、SCL=9 在下面的 Wire.begin() 中設定

// ── 裝置身份（第 4 章）────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // 非標準裝置
    .unitsCount        = 1,
    .hasFan            = true,        // 唯一的辭彙技能
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── 狀態（第 7 章）─────────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC 指標啟動
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS：設定在重啟後保留

// ── VOC 感測器（第 5 章）────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = 還沒有資料

static void initVocSensor() {
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// 感測器，或在 -DDEMO_VOC=1 時用空氣模型（第 5 章）。
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // 指標：~100 = 普通空氣，更高 = 更髒（最高 500）
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}

// ── 風扇（第 7 章）────────────────────────────────────────
static void setFan(bool on) {         // on：true = 開啟，false = 關閉
    if (g_fanOn == on) return;        // 已在想要的狀態
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // 辭彙欄位 → 雲端 → 卡片
    s_link.publishTelemetryNow();     // 狀態改變——立即發佈
}

// ── 遲滯的自動化（第 7 章）────────────────────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // 自動：在閾值開啟，在 20 點以下關閉
    if (g_vocIndex < 0) return;       // 感測器還沒說話
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── 來自入口網站的命令回呼（第 6–7 章）───────────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // 立即套用
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup：設定、網路、感測器、卡片───────────────────────────
void setup() {
    Serial.begin(115200);

    // 從 NVS 設定（第二個引數——首次啟動預設）
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi、MQTT、綁定——全部內部
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // 遙測：寫入自訂 vocIndex 欄位（第 5 章）
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // 卡片：感測器 + 控制（第 6 章）
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // 卡片的出廠佈局（第 6 章，選擇性）
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop：網路總是，感測器和邏輯每秒─────────────────────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // 網路、遙測、命令——總是先

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // 新鮮讀數…
        tickAutoLogic();              // …並立即決定
    }
}
```
