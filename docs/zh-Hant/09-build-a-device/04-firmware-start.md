---
title: "idryer-core上的韌體啟動：首次執行和門戶綁定"
description: "基於 idryer-core 函式庫建立 PlatformIO 專案：platformio.ini、裝置 Config、首次燒錄 ESP32、在應用程式中設定 Wi-Fi 並把裝置綁定到 iDryer 門戶。"
---

# 核心上的韌體啟動

本頁你建立一個韌體項目，將ESP32帶到門戶上的線上狀態，並驗證網路部分工作。感應器和加熱邏輯將在後續步驟中新增。

該方法基於`iDryer::Link`外觀。你用一個`iDryer::Config`結構描述設備，呼叫`link.begin()`和`link.loop()`——核心會自動處理所有網路連接。

## 1. 準備工具

你需要：

- VS Code及PlatformIO擴充功能；
- USB連接線；
- `2.4 GHz` Wi-Fi網路（ESP32不支援僅`5 GHz`的網路）；
- 一支裝有 iDryer 應用程式（[App Store](https://apps.apple.com/app/idryer/id6760609044)、[Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)）並已登入 iDryer 門戶帳戶的智慧型手機：裝置透過它取得 Wi-Fi 網路並綁定到帳戶；
- 核心函式庫 [idryer-core](https://github.com/pavluchenkor/idryer-core)；
- 本章的現成專案——教材倉庫中的 [example/09-cabinet](https://github.com/pavluchenkor/Build-Your-Own-iDryer/tree/main/example/09-cabinet)：感測器驅動和後面建議複製的其他檔案都取自這裡。

什麼是控制器韌體以及它如何進入板——[控制器韌體](../02-controllers/11-flashing-controller.md)。

## 2. 建立項目

在PlatformIO中，項目是具有固定結構的資料夾。建立項目資料夾（例如`my-cabinet`）並在VS Code中打開。內部應該有這些檔案：

```text
my-cabinet/
├── platformio.ini        # 構建設定（步驟4填寫）
├── lib/
│   └── idryer-core/      # 核心函式庫（符號連結或複本）
└── src/
    └── main.cpp          # 設備程式碼：Config + setup() + loop()
```

下面代碼片段的所有片段都放在這些檔案中——每個步驟都指定放在哪裡。如果`include/`、`lib/`和`src/`資料夾不存在，請手動建立。

將`idryer-core`函式庫放在`lib/`中——PlatformIO會自動在那裡找到函式庫。最簡單的方法是為下載的函式庫建立符號連結：

```bash
git clone https://github.com/pavluchenkor/idryer-core.git ~/idryer-core
ln -s ~/idryer-core lib/idryer-core
```

也可以不用符號連結，直接把函式庫資料夾複製到`lib/idryer-core`——效果相同。

菜單生成也需要這個（第6章）——hook在`lib/idryer-core/`內尋找生成器。

## 3. Wi-Fi 和綁定不寫在程式碼裡

韌體中既沒有網路密碼，也沒有帳戶資料。首次啟動時裝置沒有 Wi-Fi，會等待設定：iDryer 應用程式以無線方式（ESPTouch）傳送設定，接著用一次性綁定權杖把裝置綁定到你的帳戶。這些都由核心庫在 `s_link.begin()` 和 `s_link.loop()` 中完成，你只需在應用程式中完成步驟——見第 9 節。

**網路設定如何進入裝置。** 沒有存過網路的板子會監聽電波，就像一台還沒調到電台的收音機。手機此時把網路名稱和密碼「敲」進空中——差不多就是摩斯電碼，只是用 Wi-Fi 封包。板子接到這段傳送後連上網路，之後每次開機都會自己上網。這不需要額外的引腳和電線：用的是板子本身的天線，只要還沒有網路就會自動啟動，持續最多 90 秒。

如果無線方式沒有成功，還有有線的路徑：網頁安裝程式 [install.idryer.org](https://install.idryer.org) 透過 USB 把網路和綁定權杖交給板子——和應用程式做的事情一樣，只是走線纜。把板子重新啟動一次也有幫助：重啟後它會重新等待設定。

## 4. 配置platformio.ini

在項目根目錄填寫`platformio.ini`：

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; 核心庫的相依套件（MQTT、ArduinoJson、WebSockets、Improv）
; 會自動從 lib/idryer-core/library.json 引入。
; ESPAsyncTCP 是 espMqttClient 相依套件中的 ESP8266 傳輸層：
; 它在 ESP32 上無法編譯，必須排除。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

將`board`替換為你的板（例如`esp32-s3-devkitc-1`）。`idryer-core`本身不需要在`lib_deps`中指定——它位於`lib/`（步驟2）。

!!! note "這些行的作用"
    不需要列出核心庫的相依套件：PlatformIO 會從 `lib/idryer-core/library.json` 取得。`lib_ignore = ESPAsyncTCP` 是必要的——沒有它，建置會在 `ESPAsyncTCP.cpp` 處失敗。`MQTT_BROKER` 和 `MQTT_PORT` 旗標也是必要的——沒有它們核心庫無法編譯（`'MQTT_BROKER' was not declared`）。

## 5. 在Config中描述設備

接下來的一切都在一個檔案中進行——`src/main.cpp`。打開它並記錄此步驟和後續步驟的程式碼。

`iDryer::Config`是設備的護照。`has*`標誌告訴門戶設備有什麼，並確定發佈哪些遙測欄位。

對於加熱的櫃子，在`src/main.cpp`的開頭

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自製裝置：卡片由 card 清單組成
    .unitsCount        = 1,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "has*標誌——這是與門戶的契約"
    對應`has*`標誌為`false`的遙測欄位不會發佈。例如，沒有`hasAirHumidity = true`，濕度將不會進入雲端，即使你在程式碼中寫入它。只啟用設備中實際存在的內容。

元件清單和標誌——[系統組成](02-bom.md)。

## 6. 最小主函式

在相同檔案的`Config`塊之後，新增`setup()`和`loop()`函式。對於首次啟動，足以啟動連結並在`loop()`中執行它：

```cpp
void setup() {
    Serial.begin(115200);
    s_link.begin();
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

`s_link.begin()` 會啟動 Wi-Fi、綁定以及與門戶的連線。當裝置從帳戶解除綁定時，門戶會送來 `revoke` 指令：`handleRevoke()` 清除裝置金鑰，裝置接著等待重新綁定。感測器在[感測器](05-sensors.md)一步中加入。

### 本章後的完整`src/main.cpp`

將上述兩個塊放入一個檔案——這是此步驟的整個`src/main.cpp`：

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自製裝置：卡片由 card 清單組成
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
    // 裝置在門戶上被解除綁定：清除金鑰，等待重新綁定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

前面的章節展示了**要新增什麼**和**變更後的完整`src/main.cpp`**，所以你始終看到整體情況，而不是零散的片段。

## 7. 刷新

```bash
pio run -e cabinet -t upload
```

## 8. 開啟序列埠監視器

```bash
pio device monitor -b 115200
```

在裝置連上 Wi-Fi 之前，日誌不會輸出：核心庫把序列埠留給網頁安裝程式（Improv）。Wi-Fi 一連上，日誌就會開啟。對於尚未綁定的裝置，日誌最後是這樣的：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

最後一行正是這一步需要的：網路有了，綁定金鑰還沒有，裝置在等待應用程式送來的權杖。保持監視器開啟，轉到應用程式。

## 9. 在應用程式中連接 Wi-Fi 並綁定裝置

1. 把手機連接到裝置將要使用的 Wi-Fi 網路（`2.4 GHz`），並用門戶帳戶登入 iDryer 應用程式。
2. 在首頁點選 **连接新设备**——開啟 **Wi-Fi** 步驟。
3. 核對網路名稱（開啟定位時應用程式會自動填入），輸入密碼並點選 **连接设备**。應用程式最多傳送 90 秒；裝置加入網路後顯示 **设备已连接**。點選 **下一步**。
4. 在 **绑定** 步驟中點選 **绑定**。應用程式在網路中找到裝置，從門戶取得一次性綁定權杖並交給裝置，然後等待門戶確認裝置已上線。
5. 顯示 **设备已绑定** 後，裝置會出現在門戶和應用程式的裝置清單中。

如果裝置已經在網路中，可以直接開啟 **绑定** 步驟——點選視窗頂部的對應標籤。

![應用程式中的 Wi-Fi 步驟：網路名稱和密碼](../../img/09-cabinet/04-app-wifi.png)
*步驟 **Wi-Fi**：應用程式以無線方式把網路交給裝置。*

![綁定步驟：應用程式在網路中找到裝置](../../img/09-cabinet/04-app-pairing.png)
*步驟 **绑定**：應用程式依序號在網路中找到裝置。別人的裝置會標示為已占用。*

![「裝置已綁定」訊息](../../img/09-cabinet/04-app-paired.png)
*完成：裝置已綁定到帳戶，馬上就會出現在清單中。*

日誌中可以看到綁定過程：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 驗證結果

此時裝置應在門戶上顯示為 Online。還沒有感測器資料——這是正常的：`Config` 還沒有宣告任何感測器，卡片也就沒有東西可顯示。

![綁定後門戶上的裝置卡片](../../img/09-cabinet/04-portal-card.png)
*門戶上的裝置：名稱、Idle 狀態、連線圖示。還沒有讀數——它們會在下一章出現。*

名稱 `Device DEVICE_…` 是出廠名稱。用名稱旁的鉛筆圖示重新命名裝置：後面的範例中它叫「Storage cabinet」。

如果出了問題：

- 應用程式沒有等到裝置加入網路——檢查密碼以及網路是否為 `2.4 GHz`；密碼錯誤時裝置會重新等待設定，請重新啟動板子並重複 Wi-Fi 步驟；
- 網路始終無法以無線方式送達——改用 USB，透過網頁安裝程式 [install.idryer.org](https://install.idryer.org) 做同樣的事；
- 在 **绑定** 步驟中應用程式沒有找到裝置——手機和裝置必須在同一網路中，且網路不能阻擋裝置探索（訪客網路常常會阻擋）；
- 裝置反覆重新啟動——檢查 ESP32 的供電（啟動時的電壓下降是重置的常見原因）；
- 建置失敗並出現錯誤——到社群提問：[Telegram](https://t.me/iDryer)、[Discord](https://discord.gg/jGce5eeHHz)；
- 參見[供電錯誤](../08-common-mistakes/02-power-mistakes.md)和[控制器錯誤](../08-common-mistakes/04-controller-mistakes.md)。

## 下一步

網路部分工作。轉到[感應器](05-sensors.md)：連接SHT31和溫敏電阻，並在門戶中看到它們的資料。
