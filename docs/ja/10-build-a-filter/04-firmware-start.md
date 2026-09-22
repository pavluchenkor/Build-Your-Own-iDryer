---
title: "スマートフィルター: ファームウェア開始とポータルへのバインディング"
description: "フィルターのファームウェアスケルトンをidryer-coreで: 非標準型デバイスのConfig、初回起動、アプリでのアカウントへのペアリング。"
---

# ファームウェア開始

プロジェクトのひな型は[キャビネットサンプルの章](../09-build-a-device/04-firmware-start.md)と全く同じです: PlatformIO、`lib/`内の`idryer-core`、同じ`platformio.ini`（環境名を`filter`に変えるだけ）。ここでは異なる部分だけを説明します。

## Config: 非標準型デバイス

フィルターには、ヒーターもエコシステム辞書のクライメートセンサーもありません。辞書スキルの中でこのデバイスが持つのはファンだけです。`src/main.cpp`内:

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // 非標準デバイス
    .unitsCount        = 1,
    // 周辺機器: エコシステム辞書から「ファン」だけあります。
    .hasFan            = true,
    // 自動公開期間:
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // ポータル上の識別:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
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

!!! note "DeviceType::Unknown — これで問題ありません"
    `Unknown`型は「ポータルがこのデバイス種別を知らない」という意味です。以前はこれが問題でした: 未知の型にはポータル側にカードがありませんでした。現在はこれが正規の手順です: デバイスのインターフェースはcard manifestが完全に記述し（[第6章](06-card.md)）、ポータルはそれに基づいてカードを構築します。型（deviceType）が必要なのは、専用カードを持つiDryer純正デバイスだけです。

`hasFan = true`フラグを設定するだけで、テレメトリの`fanStatus`フィールド、カード上の「ファン」セル、マニフェスト内のエンティティがすべて自動的に追加されます — エコシステム辞書の機能です。

## VOCセンサーはConfigにない — それで正しい

注意してください: `Config`には「hasVoc」というフラグはありません。`has*`辞書はエコシステムが知っている周辺機器を記述するものです。独自センサーは辞書経由ではなく、2つの別のメカニズムで追加します: テレメトリに独自フィールドを書き込み、card manifestで宣言する — 次の2章がその内容です。これがこのアプローチの本質です: 新しいデバイスごとに辞書を拡張する必要はありません。

## 初回起動と登録

手順はキャビネットと同じです：

1. ボードに書き込み、シリアルモニターを開きます。デバイスにWi-Fiがない間、ログは出ません。
2. iDryerアプリで：**新しいデバイスを接続** → **Wi-Fi**ステップ（ネットワークとパスワード、**デバイスを接続**）→ **ペアリング**ステップ → **ペアリング**。
3. **ペアリングが完了しました**の後、デバイスはあなたのアカウントに紐付けられ、ポータルで`Online`になります。ログには`MQTT: Connected!`が表示されます。

詳細、起こりうるエラー、再ペアリングについては[キャビネットの例の章](../09-build-a-device/04-firmware-start.md)を参照してください。

ポータルにデバイスが表示されますが、カードはまだほぼ空です — データがないためです。センサーの接続に進みましょう。
