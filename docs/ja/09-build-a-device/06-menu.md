---
title: "YAMLからのデバイスメニュー：NVSとポータルの設定"
description: "idryer-coreでのYAMLデバイスメニューの記述方法：目標温度とヒステリシスはNVSに保存され、iDryerポータルのデバイスメニューに表示されます。"
---

# YAMLからのメニュー

メニューは、デバイスの設定の集合です：目標温度、ヒステリシス、ファンのしきい値。`idryer-core`では、メニューは単一の`menu.yaml`ファイルで記述され、その後すべて - C++構造、不揮発メモリ（NVS）への保存、ポータルへの発行 - 自動的に生成されます。

これはコアの主要なブロックの1つです。設定保存コードを書かず、ポータルのフォーマットを発明しません。YAMLでパラメーターをリストするだけです。

## メニューの理由

前のステップの後、デバイスはセンサーを読みますが、すべてのしきい値はコードに「ハード」です。メニューは3つのタスクを一度に解決します：

- **保存**：値は再起動後に保持されます（NVS）；
- **ポータルからの制御**：ポータルは各メニュー項目をその型（数値、スイッチ）で表示します；
- **単一の信頼できる情報源**：1つのファイルがメモリとインターフェースの両方を説明します。

## どのように機能するか

単一の`menu.yaml`ファイルはビルド中にジェネレーターを通過します：

```text
menu.yaml → (pio runビルド) → src/menu/内のC++ファイル + NVS + ポータル用JSON
```

ポータルは各メニュー項目をその型で描画します。`role:`は項目にコア契約の翻訳済みラベルを与えます。`role:`のない項目は自身の`title`で表示されます。

!!! warning "生成されたファイルを編集しないでください"
    `menu_state.*`、`menu_bindings.*`、`menu_ids.h`などのファイルはジェネレーターによって作成されます。`menu.yaml`のみを編集してリビルドします。そうしないと、変更は上書きされます。

    項目の定数名は単純に決まります：`MENU_`にその`id`を大文字にしたものを付けます。項目`target_temp`は`MENU_TARGET_TEMP`、`hysteresis`は`MENU_HYSTERESIS`になります。これらの定数は第7章で必要になります。

## ステップ1。テンプレートをコピーする

ライブラリにはメニューテンプレートがあります。プロジェクトにコピーします：

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## ステップ2。ビルド時に生成を接続する

プロジェクト`iDryer-Storage`からサンプルフックをコピーします（そのまま使用でき、調整は不要です）：

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

次に、`platformio.ini`の`[env:cabinet]`セクションに`-Isrc/menu`行を追加します（コードが`#include <menu_state.h>`を見るため）と`extra_scripts`を通じたフック接続：

```ini
[env:cabinet]
; ... 章4からのplatform/board/lib_deps - 変更なし ...

build_flags =
    -Isrc/menu                      ; ← 追加：生成されたメニューへのパス
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← 追加
    pre:extra_scripts/pre_gen_menu.py
```

フックは自動的にパス`lib/idryer-core/menu/menu_gen.py`でジェネレーターを見つけます。ライブラリは第4章で説明されているとおり、`lib/`（シムリンクまたはコピー）を通じて接続される必要があります。ジェネレーターはPlatformIOが自身のPythonで実行するため、別途インストールするものはありません。それでもこのステップでビルドが失敗する場合は、エラーの文面をコミュニティで見せてください：[Telegram](https://t.me/iDryer)、[Discord](https://discord.gg/jGce5eeHHz)。

## ステップ3。シャフトのパラメーターを説明する

`src/menu/menu.yaml`を開きます。テンプレートには既にルートアイテム`root`（`children`配列と例パラメーター）があります。例（`my_param`、`my_flag`、`my_mode_group`）を削除し、`children`内に独自を追加します。最後の2つのアイテム`units_count`と`language`はそのままにしておきます。これはポータルとの固定された契約です。

基本的なシャフトには、いくつかのパラメーターで十分です。

保管用の目標温度：

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # コア契約のラベル
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVSキー（15文字以下）
  persist: true
  scope: global
  default: 45
```

ヒステリシス（温度が目標より何度下がるか、その後ヒートが再度オンになるまで）：

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

!!! note "role：はクローズドリストです"
    `role:`の値は任意には発明できません。コア契約の`canonical_roles`リストからである必要があります。適切なロールがない場合、ビルドは停止し、許可されたリストが表示されます。保管用シャフトの場合、`storage.*`ファミリーロール：`storage.target_temperature`、`storage.target_humidity`、`storage.start`、`storage.stop`が適切です。完全なリスト - `menu.template.yaml`のヘッダーで。`role:`は任意です。`role:`なしのパラメーター（上記のヒステリシスなど）も同じように保存・公開され、ラベルだけが`title`から取られます。

守る必要がある制限：

- `bind`- 15文字以下（NVSキーの制限）；
- `menu.yaml`に`widget:`フィールドを追加しないでください。ポータルもアプリもこれを読みません。メニュー項目はその型で描画されます。

!!! warning "テンプレートのignore_external_cmdアイテムを確認してください"
    テンプレートに`ignore_external_cmd`アイテムがあり、`bind` - 19文字（制限15）。そのまま放置すると、生成は失敗します：`bind 'ignore_external_cmd' ... 19文字、制限15`。このアイテムを削除するか、`bind`を`ign_ext_cmd`に短縮します（実際の製品のように）。基本的なシャフトの場合、単に削除できます。

## ステップ4。プロジェクトをビルドして生成を確認します

```bash
pio run -e cabinet
```

ビルド中、フックは依存関係を自分で配置（一度）し、C++メニューファイルを生成します。`menu.yaml`が変わらなかった場合、生成はスキップされます（`最新`）。

生成が成功したかを確認します。ビルドログにメニュー生成について行が表示され、`src/menu/`フォルダに生成されたファイルがあります：

```text
src/menu/
├── menu.yaml          # あなたのファイル（ソース）
├── menu_state.h/.cpp  # すべてのパラメーターを含むメニューオブジェクト
├── menu_bindings.*    # bind + NVSへの書き込みでのアクセス
├── menu_ids.h
└── menu_meta.h        # 他
```

ビルドが未知の`role:`について失敗した場合、ロールが`canonical_roles`リストの外です。修正してリビルドします。autogenマークのファイルを手動で編集しないでください。

## ステップ5。起動時にメニューを読み込む

生成されたメニューを`src/main.cpp`に接続し、`setup()`で**`s_link.begin()`より前に**読み込みます：

```cpp
#include <menu_state.h>      // すべてのパラメーターを持つメニューオブジェクト
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // YAMLからのデフォルト値を設定
menu.loadFromNVS();          // 保存済みの値。初回起動時はデフォルト値が保存される
menu_sync_state_to_cache();  // 公開するメニューの元になるキャッシュへ値を反映
```

これで、パラメーターはグローバルオブジェクト`menu`から使えます：

```cpp
uint16_t target = menu.target_temp;   // 値への直接アクセス
```

## ステップ6。ポータルのメニュー：公開と変更の受け付け

ポータルはデバイスからメニューを自分で読み取りません。ファームウェアがメニューを公開し、戻ってくる変更を適用します。3つの部分があります：

- **公開** — コアの`menu_buildFullJson()`が`menu.yaml`と現在の値からメニューのJSONを組み立て、`devicePublisher()->publishConfigRaw()`がそれをポータル（MQTTトピック`config`）とローカルネットワーク経由のアプリに送ります；
- **タイミング** — デバイスがオンラインになったときと、`get_config`コマンドを受けたとき。ポータルはデバイスのメニューを開いたとき（カードの歯車）にこれを送ります；
- **変更** — ポータルは項目の`id`と新しい値`val`を付けて`set`を送ります。`menu_apply_by_bind()`が値を`menu`、NVS、キャッシュに書き込み、メニューを再公開すると、ポータルに確定した値が表示されます。

インクルードの後に追加します：

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // メニューは loop() から公開する

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
        if (v < m.min_val) v = m.min_val;              // menu.yaml の範囲
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + キャッシュ
        s_menuPending = true;                          // 新しい値をポータルに表示
        return;
    }
}
```

`setup()`の`s_link.begin()`の後に：

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

`loop()`の`s_link.loop()`の後に：

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // オンラインになったところ
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "メニューを loop() から公開する理由"
    コマンドのコールバックはネットワークハンドラーの奥深くで呼ばれます。そこでメニューのJSONを組み立てるとスタックを多く消費するため、コールバックはフラグを立てるだけにして、`loop()`が公開します。

`applySet()`は値を`menu.yaml`の項目の`min`/`max`に収めます。デバイスは受け取った数値をそのまま信用しません。

## この章の後の完全な`src/main.cpp`

前の章と比べて、`// ← 章6`の印が付いた行が追加されました：メニューの読み込み、公開、変更の受け付けです。

??? note "前回 — 第5章後の`src/main.cpp`"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include "demo_sensors.h"    // センサーなしでの読み取り値（-DDEMO_SENSORS=1）

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // 自作デバイス：カードはマニフェストが組み立てる
        .unitsCount        = 1,
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

    // 読み取り値：センサー、または-DDEMO_SENSORS=1ならシャフトのモデル
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
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        s_link.begin();
        // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();

        readSensors();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include "demo_sensors.h"    // センサーなしでの読み取り値（-DDEMO_SENSORS=1）
#include <menu_state.h>                      // ← 章6：パラメーター（menu.target_temp …）
#include <menu_bindings.h>                   // ← 章6：menu_apply_by_bind
#include <menu_commands.h>                   // ← 章6：menu_buildFullJson
#include <local_access/device_publisher.h>   // ← 章6：publishConfigRaw

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自作デバイス：カードはマニフェストが組み立てる
    .unitsCount        = 1,
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

// 読み取り値：センサー、または-DDEMO_SENSORS=1ならシャフトのモデル
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

    readSensors();
}
```

## 結果の確認

![ポータル上のデバイスメニュー](../../img/09-cabinet/06-portal-menu.png)
*メニューはデバイスから届きました：保管温度とヒステリシス、それぞれの上下限付き。値はここで直接変更でき、デバイスはそれを受け付けて保存し、メニューを再送します。*

フラッシュ後：

- デバイスカードの歯車でデバイスページとメニューが開きます：目標温度（ポータルはロールでラベルを付けます — 「Storage temperature」）と**HYSTERESIS**；
- そこで値を変更すると、デバイスが受け付けてNVSに保存し、メニューを再公開し、ポータルに確定した値が表示されます；
- 再起動後、デバイスは保存された値を公開します；
- 内部パラメーター（ヒステリシス）はコードから`menu`で使えます。

## 次のステップ

設定が説明され、保存されています。次に、[加熱制御](07-heating-control.md)のハードウェアに接続します：加熱器が目標温度を保つ、ファンはしきい値で常時オンになります。
