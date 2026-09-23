---
title: "YAML 格式的设备菜单：NVS 和门户网站中的设置"
description: "如何在 idryer-core 上的 menu.yaml 中描述设备菜单：目标温度和磁滞保存在 NVS 中，并显示在 iDryer 门户网站的设备菜单中。"
---

# YAML 格式的菜单

菜单是设备的一组设置：目标温度、磁滞、风扇阈值。在 `idryer-core` 上，菜单用单个文件 `menu.yaml` 描述，所有其他的 — C++ 结构、保存到非易失性内存（NVS）和发布到门户网站 — 自动生成。

这是核心库的关键块之一。你不编写设置存储代码，也不为门户网站设计格式 — 你只在 YAML 中列出参数。

## 为什么需要菜单

在前面的步骤之后，设备读取传感器，但所有阈值都"硬编码"在代码中。菜单立即解决三个任务：

- **存储**：值在重启后保持（NVS）；
- **从门户网站管理**：门户网站按类型显示每个菜单项（数值、开关）；
- **单一信息源**：一个文件描述内存和界面。

## 它如何工作

单个文件 `menu.yaml` 在构建期间通过生成器：

```text
menu.yaml → （构建 pio run） → src/menu/ 中的 C++ 文件 + NVS + 门户网站 JSON
```

门户网站按类型绘制每个菜单项。`role:` 为菜单项提供来自核心合约的翻译标签；没有 `role:` 的项显示其 `title`。

!!! warning "不编辑生成的文件"
    文件 `menu_state.*`、`menu_bindings.*`、`menu_ids.h` 等是由生成器创建的。只编辑 `menu.yaml` 并重新构建 — 否则你的更改会被覆盖。

    菜单项常量的名字构成很简单：`MENU_` 加上大写的 `id`。项 `target_temp` 给出 `MENU_TARGET_TEMP`，`hysteresis` 给出 `MENU_HYSTERESIS`。这些常量在第 7 章会用到。

## 步骤 1. 复制模板

库中有一个菜单模板。将其复制到你的项目：

```bash
mkdir -p src/menu
cp path/to/idryer-core/menu/menu.template.yaml src/menu/menu.yaml
```

## 步骤 2. 在构建时连接生成

从 `iDryer-Storage` 项目复制钩子示例（可以按原样使用，不需要配置）：

```bash
mkdir -p extra_scripts
cp path/to/iDryer-Storage/extra_scripts/pre_gen_menu.py extra_scripts/pre_gen_menu.py
```

然后在 `platformio.ini` 中添加到 `[env:cabinet]` 部分一行 `-Isrc/menu`（以便代码看到 `#include <menu_state.h>`）并通过 `extra_scripts` 连接钩子：

```ini
[env:cabinet]
; ... 第 4 章的 platform / board / lib_deps — 没有变化 ...

build_flags =
    -Isrc/menu                      ; ← 添加了：生成菜单的路径
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1

extra_scripts =                     ; ← 添加了
    pre:extra_scripts/pre_gen_menu.py
```

钩子将自动通过 `lib/idryer-core/menu/menu_gen.py` 路径查找生成器，因此库应该通过 `lib/`（符号链接或副本）连接，如第 4 章所述。生成器由 PlatformIO 用自带的 Python 运行——不需要另外安装什么。如果这一步构建仍然失败，把错误文本发到社区里问：[Telegram](https://t.me/iDryer)、[Discord](https://discord.gg/jGce5eeHHz)。

## 步骤 3. 描述柜的参数

打开 `src/menu/menu.yaml`。模板已有带 `children` 数组的根项 `root` 和参数示例。删除示例（`my_param`、`my_flag`、`my_mode_group`）并在 `children` 中添加你的。最后两个项 — `units_count` 和 `language` — 保留：这是与门户网站的固定合约。

对于基本柜子，几个参数就足够了。

存储目标温度：

```yaml
- id: target_temp
  type: value
  role: storage.target_temperature   # 来自核心合约的标签
  title: { ru: "ТЕМПЕРАТУРА", en: "TARGET TEMP" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint16
  min: 30
  max: 50
  step: 1
  bind: target_temp            # NVS 密钥（≤ 15 字符）
  persist: true
  scope: global
  default: 45
```

磁滞（温度可以低于目标多少度，然后加热再次打开）：

```yaml
- id: hysteresis
  type: value
  title: { ru: "ГИСТЕРЕЗИС", en: "HYSTERESIS" }
  unit:  { ru: "°C", en: "°C" }
  vtype: uint8
  min: 1
  max: 5
  step: 1
  bind: hysteresis
  persist: true
  scope: global
  default: 2
```

!!! note "role: — 这是一个封闭的列表"
    `role:` 的值不能任意发明 — 它必须来自核心合约的 `canonical_roles` 列表。如果没有合适的角色，构建将停止并显示允许的列表。对于存储柜，适合 `storage.*` 系列的角色：`storage.target_temperature`、`storage.target_humidity`、`storage.start`、`storage.stop`。完整列表在 `menu.template.yaml` 的顶部。`role:` 是可选的：没有它的参数（如上面的磁滞）同样会被保存和发布，只是标签取自 `title`。

不能违反的限制：

- `bind` — 不超过 15 个字符（NVS 密钥限制）；
- 不要在 `menu.yaml` 中添加 `widget:` 字段——门户网站和应用都不读取它：菜单项按其类型绘制。

!!! warning "检查模板中的 ignore_external_cmd 项"
    模板有 `ignore_external_cmd` 项，其 `bind` 为 19 个字符，超过 15 的限制。如果保留原样，生成将失败：`bind 'ignore_external_cmd' ... 具有 19 个字符，限制为 15`。要么删除这个项，要么将 `bind` 缩短到 `ign_ext_cmd`（如在真实产品中）。对于基本柜子，你可以简单地删除它。

## 步骤 4. 构建项目并检查生成

```bash
pio run -e cabinet
```

在构建期间，pre-hook 将自动放置依赖项（一次）并生成 C++ 菜单文件。如果 `menu.yaml` 未改变 — 生成被跳过（`up-to-date`）。

检查生成是否通过。在构建日志中，出现关于菜单生成的一行，在 `src/menu/` 文件夹中 — 生成的文件：

```text
src/menu/
├── menu.yaml          # 你的文件（源）
├── menu_state.h/.cpp  # 带有所有参数的菜单对象
├── menu_bindings.*    # 按 bind 的访问 + NVS 写入
├── menu_ids.h
└── menu_meta.h        # 及其他
```

如果构建失败，出现有关未知 `role:` 的消息 — 这意味着角色没有写在 `canonical_roles` 列表中。纠正它并重新构建。标记为 autogen 的文件不要手动编辑。

## 步骤 5. 启动时加载菜单

在 `src/main.cpp` 中接入生成的菜单，并在 `setup()` 中加载——**在** `s_link.begin()` **之前**：

```cpp
#include <menu_state.h>      // 带有所有参数的菜单对象
#include <menu_bindings.h>   // menu_sync_state_to_cache, menu_apply_by_bind

menu.initDefaults();         // 从 YAML 设置默认值
menu.loadFromNVS();          // 已保存的值；首次启动时保存默认值
menu_sync_state_to_cache();  // 把值写入缓存，发布的菜单由此构建
```

之后即可通过全局对象 `menu` 访问参数：

```cpp
uint16_t target = menu.target_temp;   // 直接访问值
```

## 步骤 6. 门户上的菜单：发布并接收修改

门户不会自己从设备读取菜单：由固件发布菜单，并应用传回来的修改。分三部分：

- **发布**——核心库的 `menu_buildFullJson()` 根据 `menu.yaml` 和当前值构建菜单 JSON；`devicePublisher()->publishConfigRaw()` 把它发送到门户（MQTT 主题 `config`）并通过局域网发送到应用；
- **时机**——设备上线时以及收到 `get_config` 命令时：打开设备菜单（卡片上的齿轮）时门户会发送该命令；
- **修改**——门户发送带有菜单项 `id` 和新值 `val` 的 `set`。`menu_apply_by_bind()` 把值写入 `menu`、NVS 和缓存，然后重新发布菜单，门户显示已确认的值。

在 include 之后添加：

```cpp
#include <menu_commands.h>                   // menu_buildFullJson
#include <local_access/device_publisher.h>   // publishConfigRaw

static bool s_menuPending = false;   // 在 loop() 中发布菜单

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
        if (v < m.min_val) v = m.min_val;              // menu.yaml 中的范围
        if (v > m.max_val) v = m.max_val;
        menu_apply_by_bind(g_bindings[i].bind, v);     // menu + NVS + 缓存
        s_menuPending = true;                          // 在门户上显示新值
        return;
    }
}
```

在 `setup()` 中，`s_link.begin()` 之后：

```cpp
s_link.onCommand("get_config", [](JsonObjectConst) { s_menuPending = true; });
s_link.onCommand("set", [](JsonObjectConst data) { applySet(data); });
```

在 `loop()` 中，`s_link.loop()` 之后：

```cpp
static bool s_wasOnline = false;
const bool online = s_link.isOnline();
if (online && !s_wasOnline) s_menuPending = true;   // 刚刚上线
s_wasOnline = online;
if (s_menuPending) {
    s_menuPending = false;
    publishMenu();
}
```

!!! note "为什么在 loop() 中发布菜单"
    命令回调在网络处理程序深处被调用。在那里构建菜单 JSON 会占用大量栈空间，所以回调只设置标志，由 `loop()` 发布。

`applySet()` 会把值限制在 `menu.yaml` 中该项的 `min`/`max` 之间：设备不会盲目信任收到的数字。

## 本章后 `src/main.cpp` 的完整版本

与上一章相比，新增了标记为 `// ← 第 6 章` 的行：加载菜单、发布菜单以及接收修改。

??? note "第 5 章结束后的 `src/main.cpp`"

    ```cpp
    #include <iDryer.h>
    #include <Wire.h>
    #include <math.h>
    #include "Sht31ClimateSensor.h"
    #include "demo_sensors.h"    // 没有传感器时的读数（-DDEMO_SENSORS=1）

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

    void setup() {
        Serial.begin(115200);
        Wire.begin(8, 9);
        s_climateOk = s_climate.begin();
        s_link.begin();
        // 设备在门户上被解绑：清除密钥，等待重新绑定。
        s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
    }

    void loop() {
        s_link.loop();

        readSensors();
    }
    ```

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

## 检查结果

![门户上的设备菜单](../../img/09-cabinet/06-portal-menu.png)
*菜单来自设备：存储温度和磁滞及其取值范围。可以直接在这里修改值——设备会接受、保存并重新发送菜单。*

刷入后：

- 设备卡片上的齿轮会打开带菜单的设备页面：目标温度（门户按角色为其加标签——“Storage temperature”）和 **HYSTERESIS**；
- 在那里修改一个值——设备接受它、保存到 NVS 并重新发布菜单，门户显示已确认的值；
- 重启后设备发布已保存的值；
- 内部参数（磁滞）可在代码中通过 `menu` 访问。

## 接下来

设置已描述并存储。现在在[加热控制](07-heating-control.md)中将它们与硬件连接：加热器保持目标温度，风扇按阈值打开。
