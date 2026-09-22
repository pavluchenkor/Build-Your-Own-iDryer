---
title: "智能过滤器：自动逻辑"
description: "基于VOC指数的阈值和滞后，从门户的auto/on/off模式，NVS设置保存和风机状态发布。"
---

# 自动逻辑

把所有东西连接起来：传感器决策，风机执行，门户控制。

## 1. 状态与设置

在`src/main.cpp`顶部：

```cpp
#include <Preferences.h>

static const int FAN_PIN = 4;

enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;   // VOC指数启动值
static bool       g_fanOn     = false;

static Preferences s_prefs;
```

`Preferences`（NVS）—— ESP32的非易失性存储：模式和阈值在重启后仍会保留。

## 2. 风机控制

所有打开和关闭都集中在一个`setFan`函数中。它接受一个参数`on` —— 所需状态：`true` = 打开，`false` = 关闭。接下来代码中我们总是调用`setFan(true)` / `setFan(false)`，它处理所有例行工作：拉动引脚、记住状态、告诉门户。

```cpp
static void setFan(bool on) {      // on —— 参数：true = 打开，false = 关闭
    if (g_fanOn == on) return;     // 已经是所需状态——什么都不做
    g_fanOn = on;                  // 在全局变量中记住新状态
    digitalWrite(FAN_PIN, on ? HIGH : LOW);  // 物理上打开/关闭风机开关

    // 告诉核心状态：fanOn[0] —— 遥测的字典字段
    // （来自hasFan = true；[0] —— 我们唯一的单元，如第5章）。
    // 从这里走向云和卡片的"风机"单元。
    s_link.telemetry.fanOn[0] = on;

    // 状态变化——理由立刻发送遥测，别等周期。
    s_link.publishTelemetryNow();
}
```

`publishTelemetryNow()`让反应瞬间：在门户上按下——一秒内卡片显示确认的状态。正是确认：iDryer门户永远不会"猜"状态，它显示设备实际发来的。

## 3. 自动逻辑与滞后

如果恰好在阈值处打开风机，它会在阈值附近频繁抖动（开了又关）。解决方法是加入死区：

```cpp
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto：在阈值打开，在阈值下20点关闭。
    if (g_vocIndex < 0) return;                      // 传感器还沉默
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}
```

调用`tickAutoLogic()`在同一地方读传感器——在`loop()`中每秒的计时器块内。这正是第5章的`loop()`，添加一行。完整的现在这样：

```cpp
void loop() {
    s_link.loop();                        // 网络、遥测、命令——总是第一个
    
    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {     // 每秒：
        s_lastReadMs = now;
        readVocSensor();                  //   读VOC（第5章）
        tickAutoLogic();                  //   立刻做风机决策
    }
}
```

秒块内的顺序不是偶然：首先是新鲜的传感器读数，然后是基于它的决策。

## 4. 来自门户的回调

那些我们在[第6章](06-card.md)许诺的函数：

```cpp
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();   // 立刻应用，别等下一个周期
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}
```

这些函数**替换**第6章的占位符——删掉空版本。

注意这个代码中**没有**什么：MQTT解析、主题、JSON命令。用户在门户上选择`on` —— 核心收到命令，检查它，调用`onModeSelected("on")`。所有传输机制都是核心的责任。

## 5. 最终setup()

还需在`setup()`中加两件事：从NVS加载已保存的设置（开始时，让逻辑立刻用它们）和配置风机引脚。完整的`setup()`本章后这样：

```cpp
void setup() {
    Serial.begin(115200);

    // 从NVS加载设置：用户过去选择的。
    s_prefs.begin("filter");   // 打开NVS中的"filter"命名空间
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);
    // getUChar/getInt的第二个参数——默认值：首次启动时返回
    // 当NVS中还什么都没存。

    pinMode(FAN_PIN, OUTPUT);  // 风机开关引脚——输出

    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // 遥测：自定义vocIndex字段（第5章）。
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // 卡片：传感器 + 控制器（第6章）。
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // 布局（第6章，可选）。
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}
```

## 6. 场景检查

| 动作 | 期望 |
|---|---|
| 模式`auto`，对传感器吹气 | VOC增长，到阈值时风机打开，卡片显示"打开" |
| 空气净化 | 低于阈值−20风机自动关闭 |
| 从门户模式`on` | 风机转，无论VOC值 |
| 从门户模式`off` | 风机停，VOC继续显示 |
| 重启板 | 模式和阈值保存 |

## 7. 最终代码：src/main.cpp完整

第4–7章的所有代码，收集到一个文件中。如果有不一致，对照这个清单。

```cpp
// ============================================================
// idryer-core上的智能空气过滤器。
// SGP40（VOC）+ 通过MOSFET的风机，自动/手动模式，
// 通过卡片清单的门户管理和卡片。
// ============================================================

#include <iDryer.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include <Preferences.h>

// ── 引脚 ────────────────────────────────────────────────────
static const int FAN_PIN = 4;         // 风机MOSFET栅极
// SDA=8、SCL=9 —— 在下面Wire.begin()中设置

// ── 设备规格（第4章） ────────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown, // 非标设备
    .unitsCount        = 1,
    .hasFan            = true,        // 唯一的字典功能
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "DIY Air Filter",
};

static iDryer::Link s_link(CFG);

// ── 状态（第7章） ─────────────────────────────────────────────
enum class FilterMode : uint8_t { Auto, On, Off };
static FilterMode g_mode      = FilterMode::Auto;
static int32_t    g_threshold = 150;  // VOC指数启动值
static bool       g_fanOn     = false;

static Preferences s_prefs;           // NVS：设置在重启时保留

// ── VOC传感器（第5章） ──────────────────────────────────────
static Adafruit_SGP40 s_sgp;
static int32_t g_vocIndex = -1;       // -1 = 没数据

static void initVocSensor() {
    Wire.begin(/*SDA=*/8, /*SCL=*/9);
    if (!s_sgp.begin()) {
        Serial.println("[VOC] SGP40 not found, check wiring");
    }
}

static void readVocSensor() {
    // 指数：~100 = 普通空气，更高 = 更脏（最高500）。
    g_vocIndex = s_sgp.measureVocIndex();
}

// ── 风机（第7章） ────────────────────────────────────────────
static void setFan(bool on) {         // on：true = 打开，false = 关闭
    if (g_fanOn == on) return;        // 已是所需状态
    g_fanOn = on;
    digitalWrite(FAN_PIN, on ? HIGH : LOW);
    s_link.telemetry.fanOn[0] = on;   // 字典字段 → 云 → 卡片
    s_link.publishTelemetryNow();     // 状态变化——立刻发布
}

// ── 自动逻辑与滞后（第7章） ─────────────────────────────────
static void tickAutoLogic() {
    if (g_mode == FilterMode::On)  { setFan(true);  return; }
    if (g_mode == FilterMode::Off) { setFan(false); return; }

    // Auto：在阈值打开，在阈值下20点关闭。
    if (g_vocIndex < 0) return;       // 传感器还沉默
    if (!g_fanOn && g_vocIndex >= g_threshold)      setFan(true);
    if ( g_fanOn && g_vocIndex <= g_threshold - 20) setFan(false);
}

// ── 命令回调（第6–7章） ──────────────────────────────────────
static void onModeSelected(const char* opt) {
    if      (strcmp(opt, "auto") == 0) g_mode = FilterMode::Auto;
    else if (strcmp(opt, "on")   == 0) g_mode = FilterMode::On;
    else                               g_mode = FilterMode::Off;
    s_prefs.putUChar("mode", (uint8_t)g_mode);
    tickAutoLogic();                  // 立刻应用
}

static void onThresholdChanged(float v) {
    g_threshold = (int32_t)v;
    s_prefs.putInt("thr", g_threshold);
}

// ── setup：设置、网络、传感器、卡片 ────────────────────────
void setup() {
    Serial.begin(115200);

    // 从NVS加载设置（第二个参数——首次启动的默认值）。
    s_prefs.begin("filter");
    g_mode      = (FilterMode)s_prefs.getUChar("mode", (uint8_t)FilterMode::Auto);
    g_threshold = s_prefs.getInt("thr", 150);

    pinMode(FAN_PIN, OUTPUT);

    s_link.begin();                   // Wi-Fi、MQTT、绑定——全在里面
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    initVocSensor();

    // 遥测：添加自定义vocIndex字段（第5章）。
    s_link.onTelemetryPublish([](JsonObject doc) {
        if (g_vocIndex >= 0) {
            doc["units"][0]["vocIndex"] = g_vocIndex;
        }
    });

    // 卡片：传感器 + 控制器（第6章）。
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });

    // 工厂布局（第6章，可选）。
    s_link.card().layoutRow("voc", "fan");
    s_link.card().layoutRow("mode", "threshold");
}

// ── loop：网络总是，传感器和逻辑每秒 ────────────────────
static uint32_t s_lastReadMs = 0;

void loop() {
    s_link.loop();                    // 网络、遥测、命令——总是第一个

    uint32_t now = millis();
    if (now - s_lastReadMs >= 1000) {
        s_lastReadMs = now;
        readVocSensor();              // 新鲜读数…
        tickAutoLogic();              // …和立刻决策
    }
}
```
