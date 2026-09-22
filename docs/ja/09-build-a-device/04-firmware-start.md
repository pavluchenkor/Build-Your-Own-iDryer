---
title: "idryer-coreでのファームウェア開始：最初の起動とポータルバインディング"
description: "idryer-coreライブラリでPlatformIOプロジェクトを作成：platformio.ini、デバイスのConfig、ESP32への最初の書き込み、アプリでのWi-Fi設定とiDryerポータルへのデバイスの紐付け。"
---

# コアでのファームウェア開始

このページでは、ファームウェアプロジェクトを作成し、ESP32をポータルのオンライン状態に進め、ネットワーク部が機能していることを確認します。センサーと加熱ロジックは次のステップで追加します。

アプローチは`iDryer::Link`ファサードに基づいています。単一の`iDryer::Config`構造でデバイスを説明し、`link.begin()`と`link.loop()`を呼び出します。すべてのネットワーク接続はコアが自分で行います。

## 1. ツールを準備する

必要なもの：

- VS CodeとPlatformIOエクステンション；
- USBケーブル；
- Wi-Fi`2.4 GHz`ネットワーク（ESP32は5 GHzのみのネットワークで動作しません）。
- iDryerポータルのアカウントでログインしたiDryerアプリ入りのスマートフォン：デバイスはアプリ経由でWi-Fiを受け取り、アカウントに紐付けられます。

コントローラーファームウェアとボードへの入り方について - [コントローラーファームウェア](../02-controllers/11-flashing-controller.md)。

## 2. プロジェクトを作成する

PlatformIOのプロジェクトは固定構造を持つフォルダーです。プロジェクトフォルダを作成（例`my-cabinet`）し、VS Codeで開きます。内部には次のファイルが必要です：

```text
my-cabinet/
├── platformio.ini        # ビルド設定（ステップ4で入力）
├── lib/
│   └── idryer-core/      # コアライブラリ（シムリンクまたはコピー）
└── src/
    └── main.cpp          # デバイスコード：Config + setup() + loop()
```

以下のコードのすべてのフラグメントは、正確にこれらのファイルに配置されます。各ステップは、どちらにあるかを示します。フォルダ`include/`、`lib/`、`src/`がない場合は、手動で作成してください。

`idryer-core`ライブラリを`lib/`に配置します。PlatformIOはそこから自動的にライブラリを見つけます。最も簡単な方法は、ダウンロードしたライブラリへのシムリンクを作成することです：

```bash
ln -s /path/to/idryer-core lib/idryer-core
```

これはメニュー生成（章6）にも必要です。フックは`lib/idryer-core/`内でジェネレーターを探します。

## 3. Wi-Fiと紐付けはコードに書かない

ファームウェアにはネットワークのパスワードもアカウント情報も入っていません。初回起動時、デバイスにはWi-Fiがなく、設定を待ちます。iDryerアプリが設定を無線（ESPTouch）で送り、続いて使い捨ての紐付けトークンでデバイスをあなたのアカウントに紐付けます。これらはすべてコアが`s_link.begin()`と`s_link.loop()`の中で行います。あなたはアプリの手順を進めるだけです — セクション9。

## 4. platformio.iniを設定する

プロジェクトのルート内の`platformio.ini`を入力してください：

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; コアのライブラリ（MQTT、ArduinoJson、WebSockets、Improv）は
; lib/idryer-core/library.json から自動で入ります。
; ESPAsyncTCP は espMqttClient の依存に含まれる ESP8266 用トランスポートで、
; ESP32 ではビルドできないため除外が必要です。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

`board`を自分のボード（例えば、`esp32-s3-devkitc-1`）に置き換えます。`lib_deps`で`idryer-core`を指定する必要はありません。それは`lib/`（ステップ2）にあります。

!!! note "これらの行の役割"
    コアの依存関係を列挙する必要はありません。PlatformIOが`lib/idryer-core/library.json`から取得します。`lib_ignore = ESPAsyncTCP`は必須です — ないとビルドは`ESPAsyncTCP.cpp`で失敗します。`MQTT_BROKER`と`MQTT_PORT`のフラグも必須です — ないとコアはコンパイルできません（`'MQTT_BROKER' was not declared`）。

## 5. Configでデバイスを説明する

すべての作業は1つのファイルで行われます。 - `src/main.cpp`。それを開き、このステップ以下のコードを書きます。

`iDryer::Config`はデバイスのパスポートです。フラグ`has*`はポータルに何があるかを伝え、発行されるテレメトリーフィールドを決定します。

ファイルの先頭`src/main.cpp`に加温シャフト用：

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    // ペリフェリー：
    .hasHeater         = true,    // 制御された加熱器
    .hasFan            = true,    // ファン
    .hasAirTemp        = true,    // 空気温度（SHT31）
    .hasAirHumidity    = true,    // 空気湿度（SHT31）
    .hasHeaterTemp     = true,    // 加熱器温度（温度計）
    // 自動発行期間：
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // ポータル識別：
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "has*フラグはポータルとの契約です"
    対応するフラグが`false`であるテレメトリーフィールドは発行されません。例えば、`hasAirHumidity = true`がなければ、湿度はコード内に書き込まれても、クラウドに行きません。物理的にデバイスに含まれているもののみを有効にしてください。

コンポーネントとフラグのリスト - [システムコンポーネント](02-bom.md)。

## 6. 最小限のメイン

同じファイルの`Config`ブロックの後、`setup()`と`loop()`関数を追加します。最初の起動では、リンクを初期化し、その中にループするだけで十分です：

```cpp
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

`s_link.begin()`がWi-Fi、紐付け、ポータルとの接続を立ち上げます。`revoke`コマンドは、デバイスがアカウントから紐付け解除されたときにポータルから届きます。`handleRevoke()`がデバイスのシークレットを消去し、デバイスは新しい紐付けを待ちます。センサーは[センサー](05-sensors.md)のステップで追加します。

### この章の後の完全な`src/main.cpp`

上記の両方のブロックを1つのファイルに組み合わせます。これは現在のステップの完全な`src/main.cpp`です：

```cpp
#include <iDryer.h>

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

前の章は、**何を追加するか**と**変更後の完全な`src/main.cpp`**を示しているので、常に全体像が見えます。散在するピースではなく。

## 7. フラッシュ

```bash
pio run -e cabinet -t upload
```

## 8. シリアルモニターを開く

```bash
pio device monitor -b 115200
```

デバイスにWi-Fiがない間、ログは出ません。コアはシリアルポートをWebインストーラー（Improv）のために空けておくからです。Wi-Fiがつながるとすぐにログが有効になります。まだ紐付けされていないデバイスでは、ログは次のように終わります：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

モニターは開いたままにして、アプリに進みます。

## 9. アプリでWi-Fiに接続し、デバイスを紐付ける

1. デバイスを使うWi-Fiネットワーク（`2.4 GHz`）に電話を接続し、ポータルのアカウントでiDryerアプリにログインします。
2. ホーム画面で**新しいデバイスを接続**をタップすると、**Wi-Fi**ステップが開きます。
3. ネットワーク名を確認し（位置情報がオンならアプリが自動で入力します）、パスワードを入力して**デバイスを接続**をタップします。アプリは最大90秒間設定を送信し、デバイスがネットワークに参加すると**デバイスが接続されました**と表示されます。**次へ**をタップします。
4. **ペアリング**ステップで**ペアリング**をタップします。アプリはネットワーク上のデバイスを見つけ、ポータルから使い捨ての紐付けトークンを取得してデバイスに渡し、デバイスがオンラインになったことをポータルが確認するまで待ちます。
5. **ペアリングが完了しました**の後、デバイスはポータルとアプリのデバイス一覧に表示されます。

デバイスがすでにネットワーク上にある場合は、すぐに**ペアリング**ステップを開いてください — ウィンドウ上部のチップをタップします。

ログには紐付けが表示されます：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 結果の確認

この段階で、デバイスはポータルでOnlineになっているはずです。センサーのデータはまだありませんが、それで問題ありません。うまくいかない場合：

- アプリがデバイスのネットワーク参加を確認できなかった — パスワードとネットワークが`2.4 GHz`であることを確認してください。パスワードが間違っているとデバイスは再び設定待ちになるので、Wi-Fiステップをやり直します；
- **ペアリング**ステップでアプリがデバイスを見つけられなかった — 電話とデバイスは同じネットワークにあり、ネットワークがデバイスの検出をブロックしていないこと（ゲストネットワークはよくブロックします）；
- デバイスが再起動する — ESP32の電源を確認してください（起動時の電圧降下はリセットのよくある原因です）；
- [電源の間違い](../08-common-mistakes/02-power-mistakes.md)と[コントローラーの間違い](../08-common-mistakes/04-controller-mistakes.md)を参照してください。

## 次のステップ

ネットワーク部が動作しています。[センサー](05-sensors.md)に進んでください：SHT31と温度計を接続し、ポータルにデータを表示します。
