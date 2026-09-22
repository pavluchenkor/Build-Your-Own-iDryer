---
title: "在 idryer-core 上启动固件：首次启动和绑定到门户网站"
description: "基于 idryer-core 库创建 PlatformIO 项目：platformio.ini、设备 Config、首次烧录 ESP32、在应用中配置 Wi-Fi 并把设备绑定到 iDryer 门户。"
---

# 在核心上启动固件

在本页中，你创建固件项目，使 ESP32 在门户网站上达到在线状态，并检查网络部分是否有效。在下一步中添加传感器和加热逻辑。

该方法基于 `iDryer::Link` 外观。你用一个 `iDryer::Config` 结构描述设备，调用 `link.begin()` 和 `link.loop()` — 核心库自己处理所有网络连接。

## 1. 准备工具

你需要：

- VS Code 与 PlatformIO 扩展；
- USB 电缆；
- `2.4 GHz` Wi-Fi 网络（ESP32 不适用于仅 `5 GHz` 网络）。
- 一部装有 iDryer 应用并已登录 iDryer 门户账户的智能手机：设备通过它获得 Wi-Fi 网络并绑定到账户。

什么是控制器固件以及它如何进入板——[刷入控制器](../02-controllers/11-flashing-controller.md)。

## 2. 创建项目

在 PlatformIO 中，项目是具有固定结构的文件夹。创建项目文件夹（例如 `my-cabinet`）并在 VS Code 中打开它。里面应该有这些文件：

```text
my-cabinet/
├── platformio.ini        # 构建设置（步骤 4 中填写）
├── lib/
│   └── idryer-core/      # 核心库（符号链接或副本）
└── src/
    └── main.cpp          # 设备代码：Config + setup() + loop()
```

下面的所有代码片段都放在这些文件中——每个步骤都指定了文件的位置。如果没有的话，手动创建 `include/`、`lib/` 和 `src/` 文件夹。

将 `idryer-core` 库放在 `lib/` 中 — PlatformIO 自动在那里查找库。最简单的方法是对下载的库创建符号链接：

```bash
ln -s /path/to/idryer-core lib/idryer-core
```

这也是菜单生成所需的（第 6 章）—— 钩子在 `lib/idryer-core/` 内查找生成器。

## 3. Wi-Fi 和绑定不写在代码里

固件中既没有网络密码，也没有账户数据。首次启动时设备没有 Wi-Fi，会等待配置：iDryer 应用通过无线方式（ESPTouch）发送配置，然后用一次性绑定令牌把设备绑定到你的账户。这些都由核心库在 `s_link.begin()` 和 `s_link.loop()` 中完成，你只需在应用中完成步骤——见第 9 节。

## 4. 配置 platformio.ini

填充项目根目录中的 `platformio.ini`：

```ini
[env:cabinet]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; 核心库的依赖（MQTT、ArduinoJson、WebSockets、Improv）
; 会自动从 lib/idryer-core/library.json 引入。
; ESPAsyncTCP 是 espMqttClient 依赖中的 ESP8266 传输层：
; 它在 ESP32 上无法编译，必须排除。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

将 `board` 替换为你的板（例如 `esp32-s3-devkitc-1`）。不需要在 `lib_deps` 中指定 `idryer-core` — 它在 `lib/` 中（步骤 2）。

!!! note "这些行的作用"
    无需列出核心库的依赖：PlatformIO 会从 `lib/idryer-core/library.json` 获取。`lib_ignore = ESPAsyncTCP` 是必需的——没有它，构建会在 `ESPAsyncTCP.cpp` 处失败。`MQTT_BROKER` 和 `MQTT_PORT` 标志也是必需的——没有它们核心库无法编译（`'MQTT_BROKER' was not declared`）。

## 5. 在 Config 中描述设备

接下来的一切都发生在一个文件中 — `src/main.cpp`。打开它并记录本步骤和后续步骤的代码。

`iDryer::Config` 是设备的护照。`has*` 标志告诉门户网站设备有什么，并确定发布了哪些遥测字段。

对于加热柜，在 `src/main.cpp` 的开头

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,
    // 外围设备：
    .hasHeater         = true,    // 受控加热器
    .hasFan            = true,    // 风扇
    .hasAirTemp        = true,    // 空气温度（SHT31）
    .hasAirHumidity    = true,    // 空气湿度（SHT31）
    .hasHeaterTemp     = true,    // 加热器温度（温度计）
    // 自动发布周期：
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // 门户网站上的标识：
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Storage Cabinet",
};

static iDryer::Link s_link(CFG);
```

!!! note "has* 标志是与门户网站的合约"
    没有相应的 `false` 标志的遥测字段不会被发布。例如，如果没有 `hasAirHumidity = true`，湿度不会进入云端，即使你在代码中写入它。只包括设备中物理存在的内容。

组件和标志的列表——[系统组成](02-bom.md)。

## 6. 最小主程序

在相同的文件中，在 `Config` 块之后添加 `setup()` 和 `loop()` 函数。对于首次启动，已经足够启动链接并在 `loop()` 中运行它：

```cpp
void setup() {
    Serial.begin(115200);
    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

`s_link.begin()` 会启动 Wi-Fi、绑定以及与门户的连接。当设备从账户解绑时，门户会发来 `revoke` 命令：`handleRevoke()` 清除设备密钥，设备随后等待重新绑定。传感器在[传感器](05-sensors.md)一步中添加。

### 本章后 `src/main.cpp` 的完整版本

将上面的两个块放在一个文件中 — 这是此步骤中的整个 `src/main.cpp`：

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
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
}
```

前一章展示了**要添加什么**以及**更改后的完整 `src/main.cpp`**，这样你总能看到完整的画面，而不是分散的片段。

## 7. 刷入固件

```bash
pio run -e cabinet -t upload
```

## 8. 打开串口监视器

```bash
pio device monitor -b 115200
```

在设备连上 Wi-Fi 之前，日志不会输出：核心库把串口留给网页安装程序（Improv）。Wi-Fi 一连上，日志就会开启。对于尚未绑定的设备，日志最后是这样的：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

保持监视器打开，转到应用。

## 9. 在应用中连接 Wi-Fi 并绑定设备

1. 把手机连接到设备将要使用的 Wi-Fi 网络（`2.4 GHz`），并用门户账户登录 iDryer 应用。
2. 在首页点击 **连接新设备**——打开 **Wi-Fi** 步骤。
3. 核对网络名称（开启定位时应用会自动填写），输入密码并点击 **连接设备**。应用最多发送 90 秒；设备加入网络后显示 **设备已连接**。点击 **下一步**。
4. 在 **绑定** 步骤中点击 **绑定**。应用在网络中找到设备，从门户获取一次性绑定令牌并交给设备，然后等待门户确认设备已上线。
5. 显示 **设备已绑定** 后，设备会出现在门户和应用的设备列表中。

如果设备已经在网络中，可以直接打开 **绑定** 步骤——点击窗口顶部的对应标签。

日志中可以看到绑定过程：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 检查结果

此时设备应在门户上显示为 Online。还没有传感器数据——这是正常的。如果出了问题：

- 应用没有等到设备加入网络——检查密码以及网络是否为 `2.4 GHz`；密码错误时设备会重新等待配置，请重复 Wi-Fi 步骤；
- 在 **绑定** 步骤中应用没有找到设备——手机和设备必须在同一网络中，且网络不能阻止设备发现（访客网络常常会阻止）；
- 设备反复重启——检查 ESP32 的供电（启动时的电压跌落是复位的常见原因）；
- 参见[供电错误](../08-common-mistakes/02-power-mistakes.md)和[控制器错误](../08-common-mistakes/04-controller-mistakes.md)。

## 接下来

网络部分工作正常。转到[传感器](05-sensors.md)：连接 SHT31 和温度计并在门户网站上查看它们的数据。
