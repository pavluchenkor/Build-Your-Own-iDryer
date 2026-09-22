---
title: "将 SHT31 和温度计传感器连接到 idryer-core"
description: "在 ESP32 上读取 SHT31 气候传感器和加热器温度计：填充 idryer-core 遥测数据并在 iDryer 门户网站上输出数据。"
---

# 传感器

在本页中，你连接两个传感器并在门户网站上输出它们的数据。首先是 SHT31（柜气候），然后是温度计（加热器温度）。这是"获得数据"的步骤，在添加管理逻辑之前。

与核心库一起工作的原则很简单：你的代码在 `loop()` 中将新读数写入 `s_link.telemetry.*` 字段，外观自动将它们发布到云端每 `telemetryPeriodMs`，来自 `Config`。你不需要手动调用发布。

## 遥测字段

对于我们的柜子，使用三个字段（索引 `[0]` — 第一个也是唯一的室）：

| 字段 | 存储内容 | Config 中的标志 |
|------|------------|---------------|
| `s_link.telemetry.airTempC[0]` | 空气温度，°C | `hasAirTemp` |
| `s_link.telemetry.airHumidityPct[0]` | 空气湿度，% | `hasAirHumidity` |
| `s_link.telemetry.heaterTempC[0]` | 加热器温度，°C | `hasHeaterTemp` |

我们已经在[前一步的 Config](04-firmware-start.md) 中包括了所有三个标志。

## 规则：传感器代码不应该阻塞 loop()

外观 `idryer-core` 在同一个 `loop()` 中为 Wi-Fi 和 MQTT 服务。因此在读取传感器时，你不能调用 `delay()` — 暂停会断开网络会话。传感器按计时器轮询，已准备好的值只是被读取。生态系统中的现成驱动程序已经以这种方式组织。

## 步骤 1. SHT31：柜气候

你不需要从头开始编写 SHT31 驱动程序 — 现成的 `Sht31ClimateSensor` 类在 `iDryer-Storage` 示例中。它使用 `robtillaart/SHT31` 库并无阻塞地读取传感器。

1. 在你的 `platformio.ini` 的 `lib_deps` 中添加 SHT31 库：

    ```ini
    lib_deps =
        robtillaart/SHT31 @ ^0.5.0
    ```

2. 从 `iDryer-Storage/src/storage/sensors/` 复制四个文件到你的 `src/` 文件夹：`Sht31ClimateSensor.h`、`Sht31ClimateSensor.cpp`、`IClimateSensor.h` 和 `sensor_reading.h`。

3. 通过 I2C 连接传感器（见[接线图](03-wiring.md)）并在 `src/main.cpp` 中读取它：

```cpp
#include <Wire.h>
#include <iDryer.h>
#include "Sht31ClimateSensor.h"

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // SDA, SCL — 你的板的引脚
    s_climateOk = s_climate.begin();  // 自己找到地址 0x44 或 0x45
    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
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

`SensorReading` 结构（字段 `ok`、`temperature`、`humidity`）在 `sensor_reading.h` 中声明。刷入后，门户网站会显示柜温度和湿度 — 这是来自设备的第一个反馈。

## 步骤 2. 温度计：加热器温度

我没有现成的 ESP32 温度计类，所以我们直接在 `src/main.cpp` 中编写。温度计连接到 ADC 引脚通过转换器（见[接线图](03-wiring.md)）：控制器测量中点的电压，根据它计算温度计的电阻，然后是温度。

```cpp
#include <math.h>

static const int   THERM_PIN  = 2;         // ADC 引脚
static const float SERIES_R   = 4700.0f;   // 分压器电阻，欧
static const float NOMINAL_R  = 100000.0f; // 温度计在 25 °C 时的电阻，欧
static const float NOMINAL_T  = 25.0f;     // °C
static const float BETA       = 3950.0f;   // 温度计数据表中的 B 系数

// 返回加热器温度（°C）。
static float readHeaterTempC() {
    int   raw = analogRead(THERM_PIN);          // 0..4095 在 ESP32 上
    float v   = (float)raw / 4095.0f;           // 完整范围的分数
    float r   = SERIES_R * (1.0f - v) / v;      // 温度计电阻，欧
    // Steinhart–Hart 方程（B 参数）：
    float tK  = 1.0f / (1.0f / (NOMINAL_T + 273.15f) + logf(r / NOMINAL_R) / BETA);
    return tK - 273.15f;
}
```

在 `loop()` 中，在 SHT31 读取旁边写结果到遥测：

```cpp
s_link.telemetry.heaterTempC[0] = readHeaterTempC();
```

!!! warning "这是简化的读取 — 根据你的温度计调整参数"
    常数 `NOMINAL_R` 和 `BETA` 取决于特定的温度计 — 从其数据表中获取（常见的家用温度计 — 通用 3950，`100 kΩ`）。分压器公式对应于[接线图](03-wiring.md)中的方案：温度计到 `3.3V`，电阻到 `GND`。对于其他布局，公式会改变。ESP32 ADC 是非线性的，因此对于精确测量，读数是经过校准的 — 在 iDryer 系列控制器中，为此使用温度计表（`Thermistor` 库）。

万用表温度计检查 — [检查温度计](../06-practical-guides/02-checking-thermistor.md)。

## 本章后 `src/main.cpp` 的完整版本

下面是整个文件。相对于上一章的新行标记为 `// ← 第 5 章`；其余的没有改变。

??? note "第 4 章结束后的 `src/main.cpp`"

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

```cpp
#include <iDryer.h>
#include <Wire.h>                  // ← 第 5 章
#include <math.h>                  // ← 第 5 章
#include "Sht31ClimateSensor.h"    // ← 第 5 章

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

// ← 第 5 章: SHT31 气候传感器
static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

// ← 第 5 章: 加热器温度计
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

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);                 // ← 第 5 章（SDA、SCL — 你的板的引脚）
    s_climateOk = s_climate.begin();  // ← 第 5 章
    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    if (s_climateOk) {                                       // ← 第 5 章
        s_climate.tick(millis());
        SensorReading r = s_climate.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();     // ← 第 5 章
}
```

## 检查结果

在此步骤后，门户网站应显示三个值：

- 柜内空气温度；
- 柜内湿度；
- 加热器温度。

如果读数"浮动"或明显错误：

- 检查公共地和接线（来自电源导线的干扰）— [接线错误](../08-common-mistakes/03-wiring-mistakes.md)；
- 检查分压器电阻的标称值和温度计类型；
- 确保 SHT31 通过 I2C 响应（正确的地址和线）。

"传感器显示垃圾"诊断 — [检查温度计](../06-practical-guides/02-checking-thermistor.md)和[常见错误](../08-common-mistakes/01-overview.md)。

## 接下来

我们有来自传感器的数据。现在在[YAML 菜单](06-menu.md)中描述设备设置（目标温度、磁滞），以便可以从门户网站更改它们并存储在内存中。
