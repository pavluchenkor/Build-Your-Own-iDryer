---
title: "スマートフィルター: VOCセンサーとテレメトリ"
description: "I2CでSGP40を読み取り、onTelemetryPublishコールバックを通じてiDryerテレメトリに独自vocIndexフィールドを公開。"
---

# センサーとテレメトリ

この章では、フィルターが空気の計測を開始してクラウドにデータを送信します。核心テクニック — **テレメトリ内の独自フィールド**: エコシステム辞書はVOCを知りませんが、コアはテレメトリに任意のフィールドを追加する手段を提供します。

## 1. センサーライブラリ

`platformio.ini`で`lib_deps`に追加:

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. SGP40読み取り

`src/main.cpp`内（ピン — [配線図](03-wiring.md)から）:

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = データはまだない

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex()は内部センサー補償を自己管理。
    // インデックス: ~100 = 通常の空気、高い = より汚い（最大500）。
    g_vocIndex = s_sgp.measureVocIndex();
}
```

`initVocSensor()`の呼び出しは`setup()`内の`s_link.begin()`の後に追加し、`readVocSensor()`は`loop()`内で毎秒呼び出します（millisタイマー経由、`delay`は使いません）:

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

!!! warning "loopの中でdelayを使わないでください"
    `s_link.loop()`は常に呼び出し続ける必要があります — Wi-Fi、MQTT、ポータルからのコマンド処理はすべてここで行われます。`delay(1000)`を入れるとこれらがすべて止まります。必ずmillisタイマーを使ってください。

## 3. テレメトリ内の独自フィールド

`telemetryPeriodMs`ごとに、コアは自動的にテレメトリJSONメッセージを収集してクラウドに送信します。私たちのデバイス（1ユニット、辞書スキルからはファンのみ）の場合、コアは以下のようなメッセージを収集します:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

構造を分解:

- `units` — デバイスの**ユニット**（チャンバー）の配列。シリアルiDryerドライヤーは最大4つの独立したチャンバーを持つことができるため、テレメトリはつねに配列で、1つのチャンバーの場合でも;
- `units[0]` — 最初の（そして唯一の）ユニット: `Config`で`unitsCount = 1`を指定;
- `fanStatus` — 辞書フィールド、`hasFan = true`から出現;
- `rssi`, `uptime` — Wi-Fiレベルと稼働時間、コアは常時追加。

このメッセージにはVOCに関する情報がありません — コアは私たちのセンサーを知らないからです。しかし送信直前に、コアは**コールバック**（callback）を呼び出す機会を提供します。コールバックとは「あなたがコアに渡す関数」で、コアは各テレメトリ公開時にそれを呼び出し、組み立て済みのJSON（引数`doc`がそれです）を渡してきます。

`setup()`内:

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc — それはコアが集めたテレメトリメッセージ（上記のJSON参照）。
    // 最初のユニットに独自フィールドvocIndexを追加。
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

`doc["units"][0]["vocIndex"] = g_vocIndex;`は以下のように読みます: 「メッセージ`doc`で配列`units`を取り、そこにある要素`0`（最初のユニット）を取り、フィールド`vocIndex`を書き込む」。フィールド名はあなたが選びます — [次の章](06-card.md)でそれを参照して、カード上に値を表示します。

!!! note "「フック」という言葉について"
    コアのソースではこのコールバックを`PublishHook`と呼んでいます — 「フック」（hook）は同じ意味です: ライブラリがあなたの関数を「引っ掛ける」ための接続点。どちらの用語も同義で使われます。このドキュメントでは「コールバック」で統一します。

!!! note "ラムダと、なぜキャプチャリストが「空」なのか"
    `[](JsonObject doc) { ... }`という書き方は**ラムダ**と呼ばれます — 名前のない関数で、使う場所にそのまま書けるため、別途定義して名前を付ける手間が省けます。

    先頭の角括弧は「キャプチャリスト」です: 関数が外から借りてくるローカル変数をここに列挙します。コアのルール: **括弧は常に空**（`[]`）— ラムダは何もキャプチャせず、状態を持ち込みません（*stateless*、「ステートレス」と呼ばれます）。

    理由は技術的なものです: キャプチャありのラムダは動的メモリ確保を必要とし、ESP32で頻繁に確保するとヒープが断片化して最悪の場合Wi-Fi接続が不安定になります。そのためコアはシンプルな関数のみを受け付けます。

    実践的な結論: コールバックが必要とするものはすべて**グローバル**変数に保持してください — `g_vocIndex`がその例です。このルールはすべての`idryer-core`コールバックに適用されます。

ファンの状態は辞書の方法で公開します — オン/オフ切替時にコアのフィールドに書き込むだけです（ロジックは[第7章](07-auto-logic.md)）:

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. センサーが手元にない場合: デモモード

SGP40なしでもカードまでの道のりは体験できます: インデックスは空気のモデルが計算します。サンプルからファイル`demo_voc.h`をコピーしてください:

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

そして`platformio.ini`にビルドフラグを追加します:

```ini
build_flags =
    -DDEMO_VOC=1
```

どちらの分岐も1つの関数の中に隠されているので、`loop()`は値がどこから来たかを知りません:

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

このモデルは、プリンターが印刷している部屋のように振る舞います: ファンが止まっている間、インデックスは`100`前後のバックグラウンド値から上昇し、ファンが回ると下がります。ファンの状態はモデルがあなた自身のロジックから取るため、[第7章](07-auto-logic.md)の閾値とヒステリシスが実際に動くのを確認できます。

実運用のデバイスではこのフラグは付けません: その場合は本物のセンサーを使う分岐がビルドされます。

## 5. 確認

書き込み後、デバイスのMQTTストリーム（またはシリアルログ公開）でテレメトリは以下のように見えます:

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` は独自フィールドで、辞書フィールド`fanStatus`と並んでクラウドに送られています。ポータルはすでに受け取っていますが、まだ何を表示すべきか知りません。次の章でそれを教えます。

!!! note "独自フィールドはリアルタイムでのみ生きる"
    ポータルはテレメトリをそのままクライアントへ配信するため、カードには`vocIndex`がすぐ表示されます。一方、グラフ用の履歴にサーバーが書き込むのは辞書の値だけです — 温度、湿度、加熱。独自フィールドはテレメトリのグラフには現れません。履歴が必要なら自分で蓄積してください（例: Home Assistantや独自のデータベース）。

センサーに息を吹きかけるか、マーカーを近づけてみてください — インデックスが数秒で大きく上昇するはずです。
