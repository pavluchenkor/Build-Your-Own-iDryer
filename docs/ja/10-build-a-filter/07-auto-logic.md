---
title: "スマートフィルター: オートメーション ロジック"
description: "VOCインデックスでの閾値とヒステリシス、ポータルからのauto/on/offモード、NVSでの設定保存、ファン状態公開。"
---

# オートメーション ロジック

すべてを繋ぎ合わせます: センサーが判断し、ファンが回り、ポータルが制御します。

## 1. 状態と設定

`src/main.cpp`の開始:

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOCインデックス起動
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences`（NVS）はESP32の不揮発性メモリです: モードと閾値は再起動後も保持されます。

## 2. ファン管理

ファンのオン/オフはすべて1つの関数`setFan`に集約します。引数`on`は目的の状態: `true` = オン、`false` = オフです。コード全体でこの先は`setFan(true)` / `setFan(false)`と呼ぶだけで、ピン操作・状態記録・ポータル通知といった定形処理をすべて担当します。

```cpp
static void setFan(bool on) {      // on — 引数: true = 起動、false = 停止
    if (g_fanOn == on) return;     // 既に必要な状態 — 何もしない
    g_fanOn = on;                  // グローバル変数に新しい状態を記録
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // ファンキーを物理的にオン/オフ

    // コアに状態を通知: fanOn[0] — テレメトリ辞書フィールド
    // （hasFan = trueから生まれた; [0] — 第5章のような唯一のユニット）。
    // ここからそれはクラウドに行き、カードの「ファン」セルに表示。
    s_link.telemetry.fanOn[0] = on;

    // 状態変更 — すぐテレメトリを送信する理由、周期を待たない。
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()`によって応答が即座になります: ポータルで操作 → 約1秒後にカードが確認済みの状態を表示。重要: iDryerポータルは状態を「予測」しません。デバイスが実際に送信したものだけを表示します。

## 3. ヒステリシス付きオートメーション

閾値ちょうどでオン/オフを繰り返すと、閾値付近でファンがチャタリング（頻繁なオン/オフ）を起こします。ヒステリシスで解決します:

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: 閾値で起動、20ポイント下で停止。
    if (g_vocIndex < 0) return;                      // センサーはまだ無言
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

`tickAutoLogic()`の呼び出し場所はセンサー読み取りと同じ — `loop()`の毎秒タイマーの中です。第5章の`loop()`に1行追加するだけで、全体はこうなります:

```cpp
void loop() {
    s_link.loop();                        // ネット、テレメトリ、コマンド — 常に最初
    
    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // 毎秒:
        s_lastReadMs = now;
        readVocSensor();                  //   VOCを読む（第5章）
        tickAutoLogic();                  //   そしてすぐファン決定
    }
}
```

1秒ブロック内の順序は意図的です: 最初に新しいセンサー値を取得し、その後すぐに判断します。

## 4. ポータルからのコールバック

[第6章](06-card.md)で約束した関数:

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // すぐ適用、次のティック待たない
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

これらの関数は第6章のスタブを**置き換えます** — 空のバージョンは削除してください。

注目: このコードに**ないもの**: MQTTパース、トピック名、JSONコマンド処理。ユーザーがポータルのリストで`on`を選択 → コアがコマンドを受け取り、検証し、`onModeSelected("on")`を呼び出します。転送の仕組み全体はコアの責任です。

## 5. 最終setup()

`setup()`に追加する残りは2つ: NVSから保存済み設定をロード（最初に読み込むことでロジックが即座にそれを使える）と、ファンピンの設定です。この章を終えた時点での完全な`setup()`はこうなります:

```cpp
void setup() {
    Serial.begin(115200);

    // NVSから設定: ユーザーが過去選んだもの。
    s_prefs.begin("filter");   // NVS内のネームスペース"filter"を開く
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // getUChar/getIntの第2引数 — デフォルト: 初回起動時に戻る、
    // NVSにまだ何も保存ない時。

    pinMode(FAN_PIN, OUTPUT);  // ファンキーピン — 出力

    s_link.begin();
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // テレメトリ: 独自フィールドvocIndex（第5章）。
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // カード: センサー + 制御（第6章）。
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // レイアウト（第6章、オプション）。
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. シナリオ確認

| アクション | 期待 |
|---|---|
| モード `auto`、センサーに息を吹きかける | VOC増加、閾値でファン起動、カード「オン」表示 |
| 空気きれい | 閾値−20以下でファン自動停止 |
| ポータルから`on`モード | VOCに関わらずファン回転 |
| ポータルから`off`モード | ファンは停止、VOC継続表示 |
| ボードリセット | モードと閾値が保存されている |

実際にはこのように見えます。閾値は`150`、インデックスがそこに達し — ファンが自動で起動しました:

![閾値でオートメーションがファンを起動](../../img/10-filter/07-portal-auto-on.png)
*モード`auto`: 閾値`150`に対してインデックス`151` — ファンはオン。*

![モバイルアプリでの同じ状態](../../img/10-filter/07-app-auto-on.png)
*アプリも同じ状態を表示します: 値が上がり、ファンが動いています。*

手動モードはオートメーションより優先されます: `Mode` → `on`にすると、空気がすでにきれいでもファンは回り続けます:

![手動モード: 空気がきれいでもファンはオン](../../img/10-filter/07-portal-mode-on.png)
*モード`on`、インデックス`132` — 閾値より下ですがファンは動いています: ポータルからのコマンドはオートメーションより優先されます。*

## 7. 最終コード: src/main.cpp 全体

第4〜7章のすべてのコードを1つのファイルにまとめたものです。手元のコードで合わない部分があれば、このリストと照合してください:

```cpp
// ============================================================
// idryer-coreのスマートエアフィルター。
// SGP40（VOC）+ MOSFETファン、auto/手動モード、
// card manifestを通じたポータル制御とカード。
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>
#include "demo_voc.h"                 // センサーなしのインデックス（-DDEMO_VOC=1）

// ── ピン ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // ファンMOSFETゲート
// SDA=8, SCL=9 — 下記のWire.begin()で設定

// ── デバイス仕様（第4章）────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // 非標準デバイス
    .unitsCount        = 1,
    .hasFan            = true,        // 唯一の辞書スキル
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── 状態（第7章）─────────────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOCインデックス起動
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS: 設定は再起動を生き残る

// ── VOCセンサー（第5章）────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = データなし

static void initVocSensor() {
#ifndef DEMO_VOC
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
#endif
}

// センサー、または-DDEMO_VOC=1で空気のモデル（第5章）。
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    // インデックス: ~100 = 通常空気、高い = より汚い（最大500）。
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}

// ── ファン（第7章）────────────────────────────────────────────
static void setFan(bool on) {         // on: true = 起動、false = 停止
    if (g_fanOn == on) return;        // 既に必要な状態
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // 辞書フィールド → クラウド → カード
    s_link.publishTelemetryNow();     // 状態変更は即座公開
}

// ── ヒステリシス付きオートメーション（第7章）─────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto: 閾値で起動、20ポイント下で停止。
    if (g_vocIndex < 0) return;       // センサーはまだ無言
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── ポータルコマンド コールバック（第6-7章）────────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // すぐ適用
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup: 設定、ネット、センサー、カード────────────────────
void setup() {
    Serial.begin(115200);

    // NVSから設定ロード（第2引数は初回起動時のデフォルト値）。
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi、MQTT、バインディング — 全部内部
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // テレメトリ: 独自フィールドvocIndexを追加（第5章）。
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // カード: センサー + 制御（第6章）。
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // 工業用カード レイアウト（第6章、オプション）。
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop: ネット常時、センサー毎秒とロジック────────────────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // ネット、テレメトリ、コマンド — 常に最初

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // 新しい値…
        tickAutoLogic();              // …そしてすぐ決定
    }
}
```
