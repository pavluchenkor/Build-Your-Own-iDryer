---
title: "シャフト加熱制御：温度保持とファン"
description: "idryer-coreでのシャフト加熱ロジック：ヒステリシスによる目標温度の保持、温度計による加熱器保護、ファン、ポータルコマンド。"
---

# 加熱制御

このページでは、センサー、設定、電力部を実際のロジックに接続します。デバイスはシャフト内の設定温度を保つ、加熱器を過熱から保護し、ポータルコマンドに応答します。

ロジックは、ネット保守と同じ`loop()`内で実行されます。すべてのタイマーとしきい値は非ブロッキング（`delay()`なし）です。

## 何が起こるべきか

シャフトの動作は3つの単純なルールで構成されています：

1. **温度保持。**シャフト内の空気が目標よりヒステリシス値だけ低い場合 - ヒートをオンにします。目標に達したら - オフにします。
2. **加熱器保護。**温度計は加熱器自体を制御します。許容値より過熱した場合 - 空気温度に関係なく、ヒートはオフになります。
3. **ファン。**シャフト周辺の熱を分散させるためにオンになり、ヒートが不要になるとオフになります。

## 加熱器とファンのキー

加熱器とファンはコントローラーがキーを通じてオンにします：MOSFETモジュール（バージョンA）またはSSR（バージョンB）。[接続図](03-wiring.md)を参照してください。コードの観点からは、これは単なるGPIOピンです：`HIGH` - オン、`LOW` - オフ。

小さな構造を通じてこのようなキーを説明し、加熱器とファン用の2つのインスタンスを作成します。`src/main.cpp`に（`setup()`の前）追加：

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 - 加熱器制御
static GpioOutput myFan{5};      // GPIO5 - ファン制御
```

ピン番号は[接続図](03-wiring.md)と同じです。`setup()`では、両方のキーを初期化する必要があります：`myHeater.begin();`と`myFan.begin();`。

!!! warning "開始時の安全な状態"
    `begin()`は直ちに`LOW`を設定します。加熱器とファンがオフ（ロジックが別の決定をするまで）。これが重要です：電源をオンする際、加熱器が誤ってオンになってはいけません。

## ヒステリシスによる温度保持

`40～45 °C`のシャフトの場合、単純なヒステリシスで十分です：ヒートは目標周辺でオン/オフします。これはフル機能のPIDよりもシンプルで、温かい保持に確実に機能します。

ヒステリシスはメニュー（`menu.hysteresis`）から取ります。メニューは[第6章](06-menu.md)ですでに接続済みです。目標温度は、ユーザーがデバイスカードからキャビネットを起動するときに設定します（`s_targetC`。カードはこの章の後半で接続します）。加熱するのは Storage モードのときだけです。状態と判定関数を追加します：

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // 現在の運転の目標値（カードから）

static void controlLoop() {
    // Storage モードのときだけ加熱：停止後はキャビネットが冷える
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // カードから
    float hyst   = (float)menu.hysteresis;           // メニューから

    if (air < target - hyst) {
        s_heating = true;     // 冷えた - 暖める
    } else if (air >= target) {
        s_heating = false;    // 目標に達した - 停止
    }
}
```

目標温度はカードからの起動コマンドと一緒に届きます。その範囲とデフォルト値は[メニュー](06-menu.md)の項目 `target_temp` です。

## 温度計による加熱器保護

空気はゆっくり温まり、スパイラルヒーター は速く温まります。加熱器の単独制御がなければ、空気が目標に達する前に加熱器は過熱する可能性があります。したがって、加熱器の温度計は固いキャップを設定します。

```cpp
static const float HEATER_MAX_C = 80.0f;   // 加熱器温度キャップ

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // 温度計

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // テレメトリーに反映
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "加熱器キャップは保護です（気候設定ではありません）"
    `HEATER_MAX_C`は空気ではなく加熱器自体の温度を制限します。値は加熱器の構造と材料に依存します。プリント部品が変形する温度より下で、十分に選択してください。[耐熱材](../07-3d-printing/04-heat-resistant-materials.md)を参照してください。

より円滑な加熱のため、オン/オフ「すべてまたは何もない」の代わりにPWMを通じて電力を管理できます。フィールド`heaterPower01[0]`は`0.0`から`1.0`の値を受け入れます。温かい保持を持つシャフトの場合、上記の単純ロジックで十分です。

## ファン

ファンはシャフト周辺の熱を分散させます。最も単純なロジックは、ヒートと同時にそれを有効にすることです：

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // 暖まっている間、回転
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // テレメトリーに反映
}
```

シリーズコントローラーでは、ファンは温度で個別のしきい値（たとえば、オン時55 °C、オフ時35 °C）で制御され、境界でジャンプしないようにしています。シャフトの場合、同じアプローチを使用し、しきい値をメニューパラメーターに結合できます。

## loop()に集めます

```cpp
void loop() {
    s_link.loop();          // ネットと自動発行

    // センサー（「センサー」ステップを参照）：
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // 暖めるか決定
    applyHeater();   // 加熱器に適用 + 保護
    applyFan();      // ファンに適用
}
```

テレメトリーフィールド（`heaterPower01`、`fanOn`）はファサード自身で発行されます。ポータルに現在ヒートしているかとファンが動作しているかが見えます。

## カード：起動と停止

起動と停止は、ポータルとアプリのデバイスカードから届きます。ファームウェアはこれをカードの **アクション** として宣言します。コアがそれを card マニフェストに追加し、ポータルとアプリがフォームとボタンを自分で描画します。コード内でコマンドを解析する必要はありません。コアがあなたの関数を呼びます。

温度フィールドの範囲とデフォルト値は、ブリッジ `card_menu_bridge.h` を通じてメニュー項目 `target_temp`（30〜50 °C、45）から取ります。ユーザーが入力した値は起動コマンドと一緒に送られ、メニューには書き込まれません。第6章のメニューのヘッダーの隣にヘッダーを追加します：

```cpp
#include <card/card_menu_bridge.h>
```

アクションのコールバック — `setup()` の前に：

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // すでに 30..50 の範囲内
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

`setup()`で、第6章のメニューのコマンドの後にアクションを宣言します。メニューの値はすでにカードが読むキャッシュにあります。第6章から`setup()`が`menu_sync_state_to_cache()`を呼んでいるためです。

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` と `"IDLE"` — アクション後のユニットのモード。キャビネットが待機中はカードが起動フォームを表示し、モードが `STORAGE` のときはセッションブロックと停止ボタンを表示します。
- `MENU_TARGET_TEMP` — 項目 `target_temp` の id。ジェネレーターが `menu_ids.h` に書き出します。
- `s_link.status.mode[0]` と `targetTempC[0]` はチャンバーの現在の状態を示します。変更のたびに `publishStatusNow()` を呼ぶと、カードがすぐに切り替わります。
- `iDryer::UnitMode::Storage` — 穏やかな保温モード。キャビネットの主要なモードです。
- ポータルのデバイスメニューで保管温度を変えると、カードのフィールドのデフォルト値もそれに従います。コアがメニューの変更を自分で検知し、マニフェストを再公開します。

コアは card マニフェストに次を追加します：

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

ポータルでは、待機中のキャビネットのカードに `温度` フィールド（45 °C）と `保管` ボタンが表示されます。起動後は、目標値付きのセッションブロックと `停止` ボタンです。アプリでは、ホーム画面に測定値と実行中のセッションが表示され、起動と停止はデバイスページにあります。カードのセンサー、フィールド、レイアウトは、エアフィルターのセクションの章[デバイスカード](../10-build-a-filter/06-card.md)で説明しています。

!!! warning "コールバック内で delay() を使わない"
    アクションのコールバックはネットワークハンドラーから呼ばれます。内部でブロックすると MQTT セッションが切れます。目標値とステータスだけを変更し、実際の処理は `loop()` で行ってください。

## この章の後の完全な`src/main.cpp`

これは最終的で完全なデバイスファイルです。前の章に対する新しい行は`// ← 章7`でマークされています。同じファイルはリポジトリの`example/09-cabinet/`フォルダーに完成した例として存在し、`pio run -e cabinet`コマンドで構築されます。

??? note "前回 — 第6章後の`src/main.cpp`"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include <menu_state.h>                      // ← 章6：パラメーター（menu.target_temp …）
    #include <menu_bindings.h>                   // ← 章6：menu_apply_by_bind
    #include <menu_commands.h>                   // ← 章6：menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← 章6：publishConfigRaw

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

    // ← 章6：ポータルのメニュー
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
        menu.initDefaults();                     // ← 章6
        menu.loadFromNVS();                      // ← 章6
        menu_sync_state_to_cache();              // ← 章6
        s_link.begin();
        // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← 章6
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← 章6
    }

    void loop() {
        s_link.loop();

        // ← 章6：オンライン時と要求時にメニューを公開
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
#include <card/card_menu_bridge.h>        // ← 章7

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

// ← 章7：加熱器とファンのキー
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← 章7：温度保持ロジック
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

// ← 章7：カードのアクション
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
    myHeater.begin();              // ← 章7
    myFan.begin();                 // ← 章7
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← 章7
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

    controlLoop();   // ← 章7
    applyHeater();   // ← 章7
    applyFan();      // ← 章7
}
```

## 結果の確認

このステップの後：

- デバイスカードの `保管` ボタンで、入力した温度でキャビネットが Storage モードになり、デバイスが加熱を始める；
- 空気温度がターゲットに上昇し、ヒステリシス内で保たれます；
- 加熱器は`HEATER_MAX_C`より上に行きません；
- ファンと加熱電力がテレメトリーに表示されます；
- `停止` ボタンで加熱が止まり Idle になる。次に起動するまでキャビネットは加熱しない。

## 次のステップ

ロジックは完成しました。ケースにデバイスを組み立てるだけです。確認する準備 - [組み立てと確認](08-assembly-and-check.md)。
