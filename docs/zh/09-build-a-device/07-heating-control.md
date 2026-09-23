---
title: "柜加热管理：温度维护和风扇"
description: "idryer-core 加热柜的逻辑：按磁滞维护目标温度，通过温度计保护加热器，风扇和门户网站命令。"
---

# 加热管理

在本页中，你将传感器、设置和电源部分关联为工作逻辑。设备在柜中保持指定的温度，保护加热器免受过热，并响应来自门户网站的命令。

逻辑在 `loop()` 中旁边执行网络服务。所有计时器和阈值是非阻塞的，没有 `delay()`。

## 应该发生什么

柜的行为由三个简单规则组成：

1. **温度维护。** 如果柜内空气比目标低磁滞量 — 打开加热。到达目标时 — 关闭。
2. **加热器保护。** 温度计控制加热器本身。如果它过热 — 加热关闭，无论空气温度如何。
3. **风扇。** 打开以在柜中吹动热量，当不需要加热时关闭。

## 加热器和风扇开关

控制器通过开关管理加热器和风扇：MOSFET 模块（版本 A）或 SSR（版本 B）— 见[接线图](03-wiring.md)。从代码的角度来看，这只是一个 GPIO 引脚：`HIGH` — 打开，`LOW` — 关闭。

用小结构描述这样的开关并制作两个实例 — 用于加热器和风扇。在 `src/main.cpp` 中添加（在 `setup()` 之前）：

```cpp
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};

static GpioOutput myHeater{4};   // GPIO4 — 加热器控制
static GpioOutput myFan{5};      // GPIO5 — 风扇控制
```

引脚号与[接线图](03-wiring.md)相同。在 `setup()` 中两个开关都应初始化：`myHeater.begin();` 和 `myFan.begin();`。

!!! warning "启动时的安全状态"
    `begin()` 立即设置 `LOW` — 加热器和风扇关闭，直到逻辑决定另外。这很重要：打开电源时加热器不应意外打开。

## 按磁滞维护温度

对于 `40-45 °C` 的柜子，简单的磁滞足够：加热打开和关闭围绕目标。这比完整 PID 更简单，对温暖维护可靠。

磁滞取自菜单（`menu.hysteresis`）——它已在[第 6 章](06-menu.md)中接入。目标温度由用户在设备卡片上启动储料柜时设置（`s_targetC`；卡片在本章后面接入）。只在 Storage 模式下加热。添加状态和判断函数：

```cpp
static bool  s_heating = false;
static float s_targetC = 0.0f;   // 本次运行的目标，来自卡片

static void controlLoop() {
    // 只在 Storage 模式下加热：停止后柜子自然冷却。
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) {
        s_heating = false;
        return;
    }
    float air    = s_link.telemetry.airTempC[0];     // SHT31
    float target = s_targetC;                        // 来自卡片
    float hyst   = (float)menu.hysteresis;           // 从菜单

    if (air < target - hyst) {
        s_heating = true;     // 冷了 — 加热
    } else if (air >= target) {
        s_heating = false;    // 到达目标 — 停止
    }
}
```

目标温度随卡片的启动命令一起到达；它的范围和默认值是[菜单](06-menu.md)中的 `target_temp` 项。

## 通过温度计保护加热器

空气缓慢加热，加热器线圈很快。没有单独的控制，加热器可能在空气到达目标之前过热。这就是为什么加热器的温度计设置硬性限制。

```cpp
static const float HEATER_MAX_C = 80.0f;   // 加热器温度的上限

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];   // 温度计

    bool allow = s_heating && heaterTemp < HEATER_MAX_C;

    if (allow) {
        myHeater.on();
        s_link.telemetry.heaterPower01[0] = 1.0f;   // 反映在遥测中
    } else {
        myHeater.off();
        s_link.telemetry.heaterPower01[0] = 0.0f;
    }
}
```

!!! warning "加热器上限是保护，不是气候调节"
    `HEATER_MAX_C` 限制加热器本身的温度，而不是空气。值取决于加热器构造和外壳材料。选择它的余量低于打印部件变形的温度 — 见[耐热材料](../07-3d-printing/04-heat-resistant-materials.md)。

为获得更平滑的加热，而不是"全部或无"的打开/关闭，你可以通过 PWM 管理功率，且 `heaterPower01[0]` 字段接受从 `0.0` 到 `1.0` 的值。对于温暖维护柜，上面的简单逻辑通常足够。

## 风扇

风扇在柜中吹动热量。最简单的逻辑是在我们加热时打开它：

```cpp
static void applyFan() {
    bool fanOn = s_heating;          // 当我们加热时旋转
    if (fanOn) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = fanOn;   // 反映在遥测中
}
```

在系列控制器中，风扇通过温度以独立阈值管理（例如，在 `55 °C` 打开，在 `35 °C` 关闭），以防它在边界附近抖动。对于柜子，你可以应用相同的方法，将阈值绑定到菜单参数。

## 在 loop() 中汇总

```cpp
void loop() {
    s_link.loop();          // 网络和自动发布

    // 传感器（见"传感器"步骤）：
    s_climate.tick(millis());
    SensorReading c = s_climate.get();
    if (c.ok) {
        s_link.telemetry.airTempC[0]       = c.temperature;
        s_link.telemetry.airHumidityPct[0] = c.humidity;
    }
    s_link.telemetry.heaterTempC[0] = readHeaterTempC();

    controlLoop();   // 决定是否加热
    applyHeater();   // 应用于加热器 + 保护
    applyFan();      // 应用于风扇
}
```

遥测字段（`heaterPower01`、`fanOn`）外观自己发布 — 门户网站上可见设备现在是否在加热，风扇是否工作。

## 卡片：启动和停止

启动和停止来自门户和应用中的设备卡片。固件把它们声明为卡片 **动作**：核心库把它们加入 card 清单，门户和应用自行绘制表单和按钮。代码中无需解析命令——核心库会调用你的函数。

![带启动表单的卡片](../../img/09-cabinet/07-portal-card.png)
*卡片由清单生成：左边是读数，包括加热功率和风扇，右边是带温度和启动按钮的表单。门户此前对这台设备一无所知——一切都来自固件。*

![应用中的同一张卡片](../../img/09-cabinet/07-app-card.png)
*应用中是一样的，而且出自同一份清单：读数、温度字段和启动按钮。*

温度字段的范围和默认值通过桥接 `card_menu_bridge.h` 取自菜单项 `target_temp`（30–50 °C，45）。用户输入的值随启动命令发送，不会写入菜单。在第 6 章的菜单头文件旁边添加头文件：

```cpp
#include <card/card_menu_bridge.h>
```

动作回调——放在 `setup()` 之前：

```cpp
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // 已在 30..50 范围内
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}
```

在 `setup()` 中，在第 6 章的菜单命令之后声明动作。菜单值已经在卡片读取的缓存中：从第 6 章起 `setup()` 就调用了 `menu_sync_state_to_cache()`。

```cpp
auto& card = s_link.card();
idryer::card_menu::attach(card);
card.action("storage", "STORAGE", onStorage)
    .name("ru", "Хранение").name("en", "Storage")
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
card.action("stop", "IDLE", onStop)
    .name("ru", "Стоп").name("en", "Stop");
```

- `"STORAGE"` 和 `"IDLE"` —— 动作之后单元的模式。柜子空闲时，卡片显示启动表单；模式为 `STORAGE` 时，显示会话块和停止按钮。
- `MENU_TARGET_TEMP` —— `target_temp` 项的 id；生成器把它写入 `menu_ids.h`。
- `s_link.status.mode[0]` 和 `targetTempC[0]` 显示腔体当前状态。每次改变后调用 `publishStatusNow()`，卡片会立即切换。
- `iDryer::UnitMode::Storage` —— 温和保温模式。这是储料柜的主要模式。
- 在门户的设备菜单中修改存储温度——卡片字段的默认值会随之改变：核心库会自己发现菜单变化并重新发布清单。

核心库在 card 清单中加入：

```json
"actions": [
  {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
   "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
               "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
  {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
]
```

在门户上，空闲储料柜的卡片出现 `Temp.` 字段（45 °C）和 `Storage` 按钮；启动后显示带目标值的会话块和 `Stop` 按钮。在应用中，首页显示读数和正在进行的会话，启动和停止在设备页面。卡片的传感器、字段和布局在空气过滤器部分的 [设备卡片](../10-build-a-filter/06-card.md) 一章中介绍。

!!! warning "回调中不要使用 delay()"
    动作回调由网络处理程序调用。回调中的任何阻塞都会中断 MQTT 会话。只修改目标值和状态，实际工作放在 `loop()` 中。

## 本章后 `src/main.cpp` 的完整版本

这是最终的、完整的设备文件。相对于上一章的新行标记为 `// ← 第 7 章`。这个文件也位于存储库的 `example/09-cabinet/` 文件夹中作为现成示例，并通过 `pio run -e cabinet` 命令构建。

??? note "第 6 章结束后的 `src/main.cpp`"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include "demo_sensors.h"    // 没有传感器时的读数（-DDEMO_SENSORS=1）
    #include <menu_state.h>                      // ← 第 6 章：参数（menu.target_temp …）
    #include <menu_bindings.h>                   // ← 第 6 章：menu_apply_by_bind
    #include <menu_commands.h>                   // ← 第 6 章：menu_buildFullJson
    #include <local_access/device_publisher.h>   // ← 第 6 章：publishConfigRaw

    static const iDryer::Config CFG = {
        .deviceType        = iDryer::DeviceType::Unknown,   // 自制设备：卡片由清单生成
        .unitsCount        = 1,
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

    static Sht31ClimateSensor s_climate(&Wire);
    static bool               s_climateOk = false;

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

    // 读数：传感器，或者在 -DDEMO_SENSORS=1 时用柜子模型
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

    // ← 第 6 章：门户上的菜单
    static bool s_menuPending = false;

    static void publishMenu() {
        static char buf[MENU_FULL_JSON_BUF_SIZE];
        const size_t len = menu_buildFullJson(buf, sizeof(buf));
        if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
    }

    static void applySet(JsonObjectConst data) {
        const int id = data["id"] | -1;
        float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                         : data["val"].as<float>();
        for (uint16_t i = 0; i < g_bindings_count; i++) {
            if ((int)g_bindings[i].id != id) continue;
            const MenuMeta& m = g_menu_meta[id];
            if (v < m.min_val) v = m.min_val;
            if (v > m.max_val) v = m.max_val;
            menu_apply_by_bind(g_bindings[i].bind, v);
            s_menuPending = true;
            return;
        }
    }

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        menu.initDefaults();                     // ← 第 6 章
        menu.loadFromNVS();                      // ← 第 6 章
        menu_sync_state_to_cache();              // ← 第 6 章
        s_link.begin();
        // 设备在门户上被解绑：清除密钥，等待重新绑定。
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
        s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });   // ← 第 6 章
        s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });           // ← 第 6 章
    }

    void loop() {
        s_link.loop();

        // ← 第 6 章：上线时和收到请求时发布菜单
        static bool s_wasOnline = false;
        const bool online = s_link.isOnline();
        if (online && !s_wasOnline) s_menuPending = true;
        s_wasOnline = online;
        if (s_menuPending) {
            s_menuPending = false;
            publishMenu();
        }

        readSensors();
    }
    ```

```cpp
#include <iDryer.h>
#include <Wire.h>
#include <math.h>
#include "Sht31ClimateSensor.h"
#include "demo_sensors.h"    // 没有传感器时的读数（-DDEMO_SENSORS=1）
#include <menu_state.h>
#include <menu_bindings.h>
#include <menu_commands.h>
#include <local_access/device_publisher.h>
#include <card/card_menu_bridge.h>        // ← 第 7 章

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Unknown,   // 自制设备：卡片由清单生成
    .unitsCount        = 1,
    .hasHeater         = true,     // ← 第 7 章
    .hasFan            = true,        // ← 第 7 章
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

static Sht31ClimateSensor s_climate(&Wire);
static bool               s_climateOk = false;

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

// 读数：传感器，或者在 -DDEMO_SENSORS=1 时用柜子模型
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

static bool s_menuPending = false;

static void publishMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    const size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len > 0) s_link.devicePublisher()->publishConfigRaw(buf, len);
}

static void applySet(JsonObjectConst data) {
    const int id = data["id"] | -1;
    float v = data["val"].is<bool>() ? (data["val"].as<bool>() ? 1.0f : 0.0f)
                                     : data["val"].as<float>();
    for (uint16_t i = 0; i < g_bindings_count; i++) {
        if ((int)g_bindings[i].id != id) continue;
        const MenuMeta& m = g_menu_meta[id];
        if (v < m.min_val) v = m.min_val;
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);
        s_menuPending = true;
        return;
    }
}

// ← 第 7 章：加热器和风扇开关
struct GpioOutput {
    int pin;
    void begin() { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void on()    { digitalWrite(pin, HIGH); }
    void off()   { digitalWrite(pin, LOW); }
};
static GpioOutput myHeater{4};
static GpioOutput myFan{5};

// ← 第 7 章：温度维护逻辑
static bool        s_heating    = false;
static float       s_targetC    = 0.0f;
static const float HEATER_MAX_C = 80.0f;

static void controlLoop() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Storage) { s_heating = false; return; }
    float air    = s_link.telemetry.airTempC[0];
    float target = s_targetC;
    float hyst   = (float)menu.hysteresis;
    if (air < target - hyst)  s_heating = true;
    else if (air >= target)   s_heating = false;
}

static void applyHeater() {
    float heaterTemp = s_link.telemetry.heaterTempC[0];
    bool  allow = s_heating && heaterTemp < HEATER_MAX_C;
    if (allow) myHeater.on(); else myHeater.off();
    s_link.telemetry.heaterPower01[0] = allow ? 1.0f : 0.0f;
}

static void applyFan() {
    if (s_heating) myFan.on(); else myFan.off();
    s_link.telemetry.fanOn[0] = s_heating;
}

// ← 第 7 章：卡片动作
static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    s_climateOk = s_climate.begin();
    myHeater.begin();              // ← 第 7 章
    myFan.begin();                 // ← 第 7 章
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();
    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
    s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });

    auto& card = s_link.card();                          // ← 第 7 章
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}

void loop() {
    s_link.loop();

    static bool s_wasOnline = false;
    const bool online = s_link.isOnline();
    if (online && !s_wasOnline) s_menuPending = true;
    s_wasOnline = online;
    if (s_menuPending) {
        s_menuPending = false;
        publishMenu();
    }

    readSensors();

    controlLoop();   // ← 第 7 章
    applyHeater();   // ← 第 7 章
    applyFan();      // ← 第 7 章
}
```

## 检查结果

![刚启动存储后的卡片](../../img/09-cabinet/07-portal-session.png)
*刚启动后：Storage 模式，目标 45 °C，功率 100 %，风扇已开，开始计时。*

![加热三分钟后的卡片和图表](../../img/09-cabinet/07-portal-heating.png)
*三分钟后：柜内空气升温，湿度下降，加热器到达工作温度。图上可以看到功率按磁滞开合。*

![从应用启动存储](../../img/09-cabinet/07-app-session.png)
*存储也可以从应用启动：出现"停止"按钮和计时。*

![三分钟后应用中的加热](../../img/09-cabinet/07-app-heating.png)
*三分钟后的应用：目标 45 °C 已到 43.9 °C，湿度从 51 % 降到 33 %。图上温度向上，湿度向下。*

在这一步之后：

- 设备卡片上的 `Storage` 按钮以输入的温度让储料柜进入 Storage 模式，设备开始加热；
- 空气温度上升到目标并保持在磁滞范围内；
- 加热器不超过 `HEATER_MAX_C`；
- 风扇和加热电源在遥测中可见；
- `Stop` 按钮关闭加热并转为 Idle；在下次启动前柜子不再加热。

## 接下来

逻辑准备好了。仍然在外壳中组装设备并在打开时检查 — [组装和检查](08-assembly-and-check.md)。
