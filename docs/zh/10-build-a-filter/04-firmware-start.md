---
title: "智能过滤器：固件启动和门户绑定"
description: "过滤器固件框架基于idryer-core：非标设备类型的Config、首次启动、在应用中绑定到账户。"
---

# 固件启动

项目框架完全模仿[加热柜示例中的章节](../09-build-a-device/04-firmware-start.md)：PlatformIO、`lib/`中的`idryer-core`，同样的`platformio.ini`（只需将环境名改为`filter`）。这里只讲不同的部分。

## Config：非标设备类型

过滤器既没有加热器，也没有生态系统字典中的气候传感器。从字典功能中它只有风机。在`src/main.cpp`中：

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // 非标设备
    .unitsCount        = 1,
    // 外围设备：字典中只有风机。
    .hasFan            = true,
    // 自动发布周期：
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // 门户识别：
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
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

!!! note "DeviceType::Unknown是正常的"
    类型`Unknown`意思是"门户不知道这个产品"。从前这是个问题：门户没有未知类型的卡片。现在这是标准做法：设备的整个界面由卡片清单描述（[第6章](06-card.md)），门户根据清单构建卡片。类型只对iDryer自己的产品需要，那些有官方卡片的。

`hasFan = true`标志给我们免费得到：遥测中的`fanStatus`字段、卡片上的"风机"单元、以及清单中的实体——全部来自生态系统字典。

## Config中没有VOC传感器——这是应该的

注意：`Config`中没有`hasVoc`标志。`has*`字典描述生态系统知道的外围设备。你的自定义传感器用另外两种机制加入：在遥测中加入它的读数（你自己的字段）并在卡片清单中声明它——这是接下来的两章。这正是这种方法的精妙之处：不需要为每个新设备扩展字典。

## 首次启动和绑定

步骤与储料柜相同：

1. 烧录开发板并打开串口监视器：设备连上 Wi-Fi 之前，日志不会输出。
2. 在 iDryer 应用中：**连接新设备** → **Wi-Fi** 步骤（网络和密码，**连接设备**）→ **绑定** 步骤 → **绑定**。
3. 显示 **设备已绑定** 后，设备已绑定到你的账户，并在门户上变为 `Online`；日志中出现 `MQTT: Connected!`。

详细说明、可能的错误和重新绑定——见[储料柜示例的章节](../09-build-a-device/04-firmware-start.md)。

设备已在门户上可见，但卡片还几乎是空的——还没有数据。继续连接传感器。
