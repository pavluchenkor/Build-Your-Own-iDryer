---
title: "SHT31および温度計センサーをidryer-coreに接続する"
description: "ESP32上のSHT31気候センサーと加熱器温度計を読み込む：idryer-coreテレメトリーを入力し、iDryerポータルにデータを表示します。"
---

# センサー

このページでは、2つのセンサーを接続し、ポータルにデータを表示します。最初にSHT31（シャフトの気候）、次に温度計（加熱器の温度）。これは、管理ロジックを追加する前の「データを得た」ステップです。

コアで動作する原理は単純です：ループ内のコードは`s_link.telemetry.*`フィールドに新しい読み取り値を書き込み、ファサードは`Config`の`telemetryPeriodMs`ごとにそれらを自動的にクラウドに発行します。発行を手動で呼び出す必要はありません。

## テレメトリーフィールド

シャフト用に、3つのフィールドを使用します（インデックス`[0]` - 最初で唯一のカメラ）：

| フィールド | 何を格納するか | Configのフラグ |
|------|------------|---------------|
| `s_link.telemetry.airTempC[0]` | 空気温度、°C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | 空気湿度、% | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | 加熱器温度、°C | `hasHeaterTemp` |

これら3つのフラグは、ここで`Config`に入れます（完全なリストは章末を参照）。フラグは、そのようなセンサーがデバイスにあることをポータルとアプリに伝えます：フラグがなければカード上にセルは表示されません。

## ルール：センサーコードはloop()をブロックしてはいけません

ファサード`idryer-core`は同じ`loop()`でWi-FiとMQTTを提供しています。したがって、センサーを読むときは`delay()`を呼び出せません。一時停止はネットワークセッションを破ります。センサーはタイマーで調査され、既成の値を読みます。エコシステムの既成ドライバーは既にそのように配置されています。

## ステップ1。SHT31：シャフトの気候

SHT31ドライバーをゼロから書く必要はありません。既成のクラス`Sht31ClimateSensor`は、この章の例 [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet) にあります。それは`robtillaart/SHT31`ライブラリを使用し、ブロッキングなしでセンサーを読みます。

1. `platformio.ini`の`lib_deps`にSHT31ライブラリを追加します：

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. ドライバーの4つのファイルを`src/`フォルダーにコピーします：

    ```bash
    git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
    cp ~/byo-idryer/example/09-cabinet/src/{Sht31ClimateSensor.h,Sht31ClimateSensor.cpp,IClimateSensor.h,sensor_reading.h} src/
    ```

3. I2Cを通じてセンサーを接続し（[接続図](03-wiring.md)を参照）、`src/main.cpp`で読み込みます：

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA、SCL - ボードのピン
    s_climateOk = s_climate.begin();  // 自動的にアドレス0x44または0x45を検出
    s_link.begin();
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
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

ドライバーは1回分の読み取り値を、`sensor_reading.h`の`SensorReading`構造体で返します：

```cpp
struct SensorReading {
    float    temperature = NAN;   // °C、値がなければNAN
    float    humidity    = NAN;   // % RH、値がなければNAN
    float    pressure    = NAN;   // hPa、将来のセンサー用
    uint32_t ts_ms       = 0;     // 読み取り時点のmillis()
    bool     ok          = false; // 温度と湿度が有効ならtrue
    int      err         = 0;     // エラーコード、0はエラーなし
};
```

フラッシュ後、ポータルにシャフトの温度と湿度が表示されます。これはデバイスからの最初のフィードバックです。

## ステップ2。温度計：加熱器の温度

ESP32用の既成の温度計クラスはないため、`src/main.cpp`に直接書きます。温度計は分圧器を通じてADCピンに接続されています（[接続図](03-wiring.md)を参照）：コントローラーは中点の電圧を測定し、それから温度計の抵抗を計算し、次に温度を計算します。

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // ADCピン
static const float SERIES_R   = 4700.0f;   // 分圧器抵抗、オーム
static const float NOMINAL_R  = 100000.0f; // 温度計の抵抗25°C、オーム
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // 温度計データシートのBコエフィシェント

// 加熱器の温度（°C）を返します。
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // ESP32上の0..4095
    float v   = (float)raw / 4095.0f;           // フルスケールの一部
    float r   = SERIES_R * (1.0f - v) / v;      // 温度計の抵抗、オーム
    // スタインハート–ハート方程式のBパラメーター形式 — Wikipedia参照：
    // https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

`loop()`では、SHT31の読み込みの近くでテレメトリーに結果を書き込みます：

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "これは簡略化された読み取り - 温度計に合わせてパラメーターを調整します"
    定数`NOMINAL_R`と`BETA`は特定の温度計に依存します。データシートから取得してください（一般的な家庭用温度計 - Generic 3950、`100 kΩ`）。分圧器の式は[接続図](03-wiring.md)のスキームに対応しています：温度計を`3.3V`へ、抵抗を`GND`へ。別のルーティングの場合、式は変わります。ESP32のADCは非線形なので、正確な測定には読み取りを調整します。シリアルiDryerコントローラーでは、これは温度計テーブル（`Thermistor`ライブラリ）を使用します。

マルチメーターでの温度計チェック - [温度計チェック](../06-practical-guides/02-checking-thermistor.md)。

## ステップ3。センサーが手元にない場合：デモモード

ハードウェアなしでもカードまでの道のりを進められます：読み取り値はシャフトのモデルが計算します。同じ例から`demo_sensors.h`ファイルをコピーしてください：

```bash
cp ~/byo-idryer/example/09-cabinet/src/demo_sensors.h src/
```

そして`platformio.ini`にビルドフラグを追加します：

```ini
build_flags =
    -DDEMO_SENSORS=1
```

両方の分岐は1つの関数の中に隠れており、`loop()`は値がどこから来たのかを知りません：

```cpp
static void readSensors() {
#ifdef DEMO_SENSORS
    demoSensors(s_link.telemetry);
#else
    // SHT31と温度計の読み取り — 上記のとおり
#endif
}
```

モデルは本物のシャフトのように振る舞います：室温は`24 °C`前後でゆっくり揺れ、加熱器がオンなら空気は温まり、オフなら冷めていき、加熱時には湿度が下がります。モデルは加熱の出力をテレメトリーから読むため、[加熱制御](07-heating-control.md)の章のロジックは応答を見ることができ、ヒステリシスが機能します。このセクションのスクリーンショットは、まさにこの方法で撮影されたものです。

実際に動作させるデバイスではこのフラグは設定しません。その場合は本物のセンサーを使う分岐がビルドされます。

## この章の後の完全な`src/main.cpp`

以下は完全なファイルです。前の章に対する新しい行は`// ← 章5`でマークされています。残りは変わりませんでした。

??? note "前回 — 第4章後の`src/main.cpp`"

    ```cpp
    #include <iDryer.h>

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // 自作デバイス：カードはマニフェストが組み立てる
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
        // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← 章5
#include <math.h>                  // ← 章5
#include "Sht31ClimateSensor.h"    // ← 章5
#include "demo_sensors.h"    // ← 章5：センサーなしでの読み取り値（-DDEMO_SENSORS=1）

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自作デバイス：カードはマニフェストが組み立てる
    .unitsCount        = 1,
    .hasAirTemp        = true,     // ← 章5
    .hasAirHumidity    = true,     // ← 章5
    .hasHeaterTemp     = true,  // ← 章5
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};
static iDryer::Link s_link(CFG);

// ← 章5：SHT31気候センサー
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← 章5：加熱器温度計
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

// ← 章5：センサー、または-DDEMO_SENSORS=1ならシャフトのモデル
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
    Wire.begin(8, 9);                 // ← 章5（SDA、SCL - ボードのピン）
    s_climateOk = s_climate.begin();  // ← 章5
    s_link.begin();
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    readSensors();   // ← 章5
}
```

## 結果の確認

![ポータルのデバイスページ：読み取り値とグラフ](../../img/09-cabinet/05-portal-device.png)
*カード上の読み取り値とテレメトリーのグラフ。加熱器の温度はグラフ上で別の線として表示されます。ページ下部のメニューはまだ空です — 次の章で扱います。*

このステップの後、ポータルに3つの値を表示する必要があります：

- シャフト内の空気温度；
- シャフト内の湿度；
- 加熱器の温度。

読み取り値が「浮遊」しているか明らかに間違っている場合：

- 共通グラウンドと配線を確認してください（電力線からのノイズ）。[配線エラー](../08-common-mistakes/03-wiring-mistakes.md)；
- 分圧器抵抗の名目値と温度計タイプを確認してください；
- SHT31がI2Cで応答していることを確認してください（正しいアドレスと線）。

センサーが「ナンセンス」を示していることの診断 - [温度計チェック](../06-practical-guides/02-checking-thermistor.md)と[典型的なエラー](../08-common-mistakes/01-overview.md)。

## 次のステップ

センサーからのデータがあります。次に、デバイス設定（目標温度、ヒステリシス）を[YAMLからのメニュー](06-menu.md)で説明して、ポータルと利用可能なメモリから変更できるようにします。
