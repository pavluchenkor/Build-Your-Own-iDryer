---
title: "智慧濾清器：入口網站卡片（card manifest）"
description: "動態裝置卡片：透過 link.card() 宣告 VOC 感測器、模式、閾值和佈局——入口網站和應用自動組建介面。"
---

# 裝置卡片

這是整個章節的關鍵篇幅。在這裡，裝置在入口網站和行動應用程式上獲得介面——**完全沒有它們那邊的程式碼**。

## 它如何工作

裝置發佈 **card manifest**——機器可讀的描述「要顯示什麼和用什麼控制」。入口網站和應用程式讀取清單並組建卡片：感測器變成有即時值的儲存格、控制變成按鈕、輸入欄位和清單。你也可以從韌體指定佈局。

你不需要手動發佈任何東西：你透過 `link.card()` 宣告實體，核心自己組建清單並在連接時傳送。

## 1. 宣告實體

所有宣告都在 `setup()` 中進行，在 `s_link.begin()` 之後。我們的濾清器有三個實體：VOC 讀數、模式清單和閾值欄位。讓我們分別講每一個，在最後把方塊放在一起。

### 一般原則：id 和 label

每個實體有兩個名字，別搞混了：

- **id**——內部的、機器的名字（`"voc"`、`"mode"`）。拉丁字母、數字、底線，沒有空格。實體透過 id 識別，佈局、命令和入口網站彼此通信。想好一個——之後別改；
- **label**——人類的標籤（`"VOC index"`、`"Mode"`）。你寫什麼，使用者在卡片上就看到什麼。可以自由改變。

### 感測器：VOC 讀數

```cpp
s_link.card().sensor(
    "voc",              // id：實體的內部名字
    "VOC index",        // label：卡片上的標籤
    "",                 // unit：數字右邊的測量單位（"°C"、"%"、"g"）；
                        //   VOC 指標沒有單位——空字串
    "units[0].vocIndex" // path：從哪裡取值——遙測 JSON 內的路徑
                        //   這正是我們在第 5 章新增的欄位：
                        //   doc["units"][0]["vocIndex"]。名字必須完全相符
                        //   否則卡片上會顯示破折號
);
```

感測器是「唯讀」儲存格：入口網站按 `path` 從遙測取值並顯示。感測器沒有命令。

### 選擇清單：工作模式

```cpp
// 清單選項。使用者在下拉清單中看到它們。
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id：實體的內部名字
    "Mode",                  // label：卡片上的標籤
    kModes,                  // options：選項陣列（上面宣告）
    3,                       // 陣列中的選項數量——auto、on、off = 三個
                             //   C++ 自己不知道陣列長度，我們告訴它
    [](const char* opt) {    // 回呼：當使用者在入口網站選擇選項時
                             //   核心會呼叫的函數
                             //   opt 是選中的字串，例如「on」
        onModeSelected(opt); //   我們把它傳給我們的邏輯（第 7 章寫）
    }
);
```

這是機制的第二部分：**控制**。當使用者在入口網站選擇選項，裝置收到命令，核心自己接收並驗證（不在 `options` 中的陌生字串不會到達你），然後用選中的值呼叫你的回呼。不需要手動解析 MQTT 訊息——你的責任從 `onModeSelected` 內部開始。

### 數字欄位：啟動閾值

```cpp
s_link.card().number(
    "threshold",       // id：實體的內部名字
    "VOC threshold",   // label：卡片上的標籤
    100,               // min：入口網站不會讓你輸入更少
    400,               // max：也不更多；核心會額外在這些邊界裁剪值
    10,                // step：用方向鍵改變值的步幅
    "",                // unit：測量單位；指標沒有
    [](float v) {              // 回呼：當使用者傳送新值時被呼叫；
                               //   v 是邊界 min..max 內的數字
        onThresholdChanged(v); //   我們把它傳給邏輯（第 7 章寫）
    }
);
```

### 放在一起

最終在 `setup()` 中的方塊——應該在你程式碼中剩下的樣子。我們會在第 7 章寫 `onModeSelected` 和 `onThresholdChanged` 函數；為了讓程式碼現在編譯，在 `setup()` 上面用空實體宣告它們：

```cpp
// 空實體：本體我們會在第 7 章寫（自動化邏輯）
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
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

    // 卡片：感測器 + 兩個控制
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

風扇呢？你**不需要**宣告它：`Config` 中的 `hasFan = true` 旗標已經自動新增了「風扇」儲存格到清單——這是辭彙技能，核心完全知道它。

!!! note "回呼中的方括號——總是空的"
    `[](const char* opt) { ... }` 是 lambda，無名函數；我們在[第 5 章的邊欄](05-sensor-and-telemetry.md)詳細討論過。提醒規則：捕捉方括號總是空的（`[]`），我們不「帶著」任何東西進 lambda，所有需要的都儲存在全域變數中——像第 7 章的 `g_mode` 和 `g_threshold`。

## 2. 自動卡片佈局

你根本不需要指定佈局。入口網站會自己從宣告的實體組建卡片——並清潔地組建：讀數儲存格組成行（最多三個每行，然後換行），控制下面，每個在自己的行上，全部用入口網站品牌風格。對大多數裝置來說這足夠了——介面沒有絲毫思考佈局就看起來井然有序。

卡片上的實體順序——它們在 `setup()` 中的宣告順序。

## 3. 自訂卡片佈局（選擇性）

首先——卡片是什麼樣的。卡片是垂直堆疊的**行**。一行感測器儲存格平分寬度：一個占滿整行，兩個各占一半，三個各占三分之一。行中的控制項（清單、欄位、按鈕）以整行寬度上下排列。

前一節的自動佈局自己將實體放進這些行。如果你想決定什麼和什麼一起站著，用 `layoutRow` 呼叫手動指定行。一個呼叫 = 一行，呼叫順序 = 行順序，由上而下：

```cpp
// 行 1：兩個儲存格——VOC 指標和風扇，各佔一半寬
s_link.card().layoutRow("voc", "fan");

// 第 2 行：兩個控制項——模式和閾值，上下排列。
s_link.card().layoutRow("mode", "threshold");
```

在 `layoutRow` 中傳遞的是實體的 **id**——你宣告它們時給的內部名字（這就是為什麼 id 需要）。`"fan"` 是風扇辭彙實體的 id，被 `hasFan` 旗標建立。

卡片上給出這種組合：

```text
┌─ Air filter ────────────────────┐
│  ▮ 129            │  ≋ Off      │   ← 行 1：voc、fan
├───────────────────┴─────────────┤
│  Mode                           │   ← 行 2：mode、threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [100                         ] │
└─────────────────────────────────┘
```

注意這些儲存格：入口網站只畫圖示和數值，不畫標籤。實體的 `label` 會用作螢幕閱讀器的名稱以及其他介面，卡片上的空間因此省下來。控制項的標籤則看得見——上圖中的 `Mode` 和 `VOC threshold`。

你沒提到的實體不會消失——入口網站會在下面用自動排列補齊。所以你可以只佈置「主要」部分，其餘交給自動排版。

## 4. 發佈到 MQTT 的內容

核心發佈到主題 `idryer/{serial}/card`（保留）：

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

不必深究這個 JSON——核心從你的呼叫自動生成它。但了解一下有用：如果你寫韌體**不用** `idryer-core`（Rust、MicroPython，隨意），只要自己發佈這樣的 JSON——入口網站相容任何格式符合的內容。

## 5. 檢查

燒錄後打開**入口網站的儀表板**——卡片就住在工廠裝置卡片所在的地方，行動應用程式中也一樣。裝置本身的頁面上沒有它：那裡是辭彙量的圖表、選單和整合。

![濾清器卡片在入口網站的儀表板上](../../img/10-filter/06-portal-card.png)
*卡片依 card manifest 組建：VOC 指標和風扇在同一行，模式和閾值在下面。*

![行動應用程式中的同一張卡片](../../img/10-filter/06-app-card.png)
*應用程式用同一份 card manifest 建出卡片——手機不需要另外寫程式碼。*

- 指標儲存格顯示即時值（向感測器吹氣——數字在下次更新時增加）；
- 風扇儲存格——開/關；
- **Mode**——下拉清單，**VOC threshold**——數字欄位：修改後約 0.6 秒送出，沒有確認按鈕。清單和欄位從第一個選項和最小值開始——它們只送出指令，不顯示裝置目前的設定；狀態由儲存格顯示（例如風扇）。

選擇模式和閾值現在什麼都不做——回呼空實體。我們會在[下一章](07-auto-logic.md)把它們活過來。

!!! note "這就是那個概念"
    注意發生了什麼：你用韌體中的五行描述了介面——它出現在入口網站和應用程式。同樣的技巧對任何你的裝置有效：改變 id、標籤和回呼。

!!! note "有啟動和停止的操作"
    清單、欄位和按鈕會立即送出值。有啟動和停止的操作——烘乾、加熱、照明——宣告為帶模式和啟動參數的卡片**動作**：卡片顯示啟動表單，操作進行時顯示帶停止按鈕的工作階段區塊。範例——儲料櫃部分的[第 7 章](../09-build-a-device/07-heating-control.md)。
