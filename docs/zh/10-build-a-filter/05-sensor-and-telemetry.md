---
title: "智能过滤器：VOC传感器与遥测"
description: "通过I2C读取SGP40，并通过onTelemetryPublish回调在遥测中发布自定义的vocIndex字段。"
---

# 传感器与遥测

本章过滤器开始测量空气并上报数据到云。关键技巧是**遥测中的自定义字段**：生态系统字典对VOC一无所知，但核心允许向遥测添加任何字段。

## 1. 传感器库

在`platformio.ini`的`lib_deps`中添加：

```ini
    adafruit/Adafruit SGP40 Sensor
```

## 2. 读取SGP40

在`src/main.cpp`中（引脚来自[接线图](03-wiring.md)）：

```cpp
#include <Wire.h>
#include <Adafruit_SGP40.h>

static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;   // -1 = 还没有数据

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // measureVocIndex() 自己处理传感器的内部补偿。
    // 指数：~100 = 普通空气，更高 = 更脏（最高500）。
    g_vocIndex = s_sgp.measureVocIndex();
}
```

在`setup()`中调用`initVocSensor()`放在`s_link.begin()`后，`readVocSensor()`在`loop()`中每秒调用一次（用millis计时器，不用`delay`！）：

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

!!! warning "loop中不要用delay()"
    `s_link.loop()`必须经常调用 —— Wi-Fi、MQTT和来自门户的命令都依赖它。`delay(1000)`会冻结所有这些。只用millis计时器。

## 3. 遥测中的自定义字段

每`telemetryPeriodMs`，核心自动收集遥测JSON消息并发送到云。对于我们的设备（一个单元，从字典中只有风机），核心收集这样的消息：

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false } ],
  "rssi": -52,
  "uptime": 120
}
```

结构说明：

- `units` —— **单元**（干燥腔）数组。商业iDryer干燥机最多有四个独立腔室，所以遥测始终是数组，即使只有一个腔室；
- `units[0]` —— 第一个（也是我们唯一的）单元：我们在`Config`中指定了`unitsCount = 1`；
- `fanStatus` —— 字典字段，来自`hasFan = true`；
- `rssi`、`uptime` —— Wi-Fi信号强度和运行时间，核心总是添加。

这条消息中没有VOC —— 核心不知道我们的传感器。但在发送前，核心给你的代码一个机会在消息中添加自定义字段。为此你注册一个**回调**（callback，"回调函数"）—— 一个函数，你把它交给核心，核心自己在每次发布时调用它，把收集的JSON（参数`doc`就是它）传入。

在`setup()`中：

```cpp
s_link.onTelemetryPublish([](JsonObject doc) {
    // doc —— 核心收集的遥测消息（见上面的JSON）。
    // 我们向第一个单元添加自己的vocIndex字段。
    if (g_vocIndex >= 0) {
        doc["units"][0]["vocIndex"] = g_vocIndex;
    }
});
```

`doc["units"][0]["vocIndex"] = g_vocIndex;`这一行读作："在消息`doc`中取数组`units`，其中的元素`0`（我们唯一的单元），写入字段`vocIndex`"。字段名你自己起——在[下一章](06-card.md)中你要引用它来在卡片上显示这个值。

!!! note "如果你看到"hook"这个词"
    在核心源代码中这个回调叫做`PublishHook` —— "hook"（"钩子"）意思相同：库给你"挂接"函数的地方。这些术语可互换；在本文档中我们说"回调"。

!!! note "Lambda与为什么它是"空的""
    构造`[](JsonObject doc) { ... }`叫做**lambda** —— 无名函数，写在使用地点，避免单独定义并取名。

    开头的方括号 —— "捕获列表"：列出函数带走的本地变量。核心规则：**括号总是空的**（`[]`）—— lambda不捕获任何东西，不带任何状态（这叫"无状态"或*stateless*）。

    技术原因：带捕获的lambda需要动态内存分配，而ESP32上频繁分配会碎片化堆，最坏情况下会让Wi-Fi崩溃。所以核心只接受简单函数。

    实际结果：回调需要的所有东西存放在**全局**变量中 —— 像我们的`g_vocIndex`。这个规则对所有`idryer-core`回调都适用。

风机状态用字典方式发布 —— 当你打开/关闭时写入核心字段（逻辑在[第7章](07-auto-logic.md)）：

```cpp
s_link.telemetry.fanOn[0] = fanIsOn;
```

## 4. 手头没有传感器？演示模式

没有SGP40也能走完到卡片的整条路：指数由空气模型算出。从示例中复制`demo_voc.h`文件：

```bash
git clone https://github.com/pavluchenkor/Build-Your-Own-iDryer.git ~/byo-idryer
cp ~/byo-idryer/example/10-filter/src/demo_voc.h src/
```

并在`platformio.ini`中加上编译标志：

```ini
build_flags =
    -DDEMO_VOC=1
```

两条分支都藏在同一个函数后面，`loop()`不知道值是从哪里来的：

```cpp
static void readVocSensor() {
#ifdef DEMO_VOC
    g_vocIndex = demoVocIndex(g_fanOn);
#else
    g_vocIndex = s_sgp.measureVocIndex();
#endif
}
```

模型的表现就像一间正在打印的房间：风机停着时，指数从大约`100`的本底值往上爬，风机工作时下降。模型从你自己的逻辑中取风机状态，所以[第7章](07-auto-logic.md)的阈值和迟滞能看到实际效果。

正式设备不加这个标志：那样编译的就是真实传感器的分支。

## 5. 检查

烧入固件后，在MQTT设备流中（或Serial日志中）遥测看起来像这样：

```json
{
  "units": [ { "unitId": "U1", "fanStatus": false, "vocIndex": 103 } ],
  "rssi": -52,
  "uptime": 120
}
```

`vocIndex` —— 你自己的字段，随着字典的`fanStatus`一起上云。门户已经接收到它，但还不知道怎么用：下一章告诉它。

!!! note "自定义字段只活在实时数据中"
    门户把遥测原样分发给客户端，所以卡片立刻显示`vocIndex`。而服务器写入图表历史的只有字典量 —— 温度、湿度、加热。遥测图表上不会有你的自定义字段；如果需要历史，就自己收集（例如Home Assistant或自己的数据库）。

对传感器呼气或靠近记号笔 —— 指数应该在几秒内明显上升。
