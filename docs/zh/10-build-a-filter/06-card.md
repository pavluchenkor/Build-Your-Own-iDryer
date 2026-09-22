---
title: "智能过滤器：门户卡片（卡片清单）"
description: "动态设备卡片：通过link.card()声明VOC传感器、模式、阈值和布局——门户和应用自动构建界面。"
---

# 设备卡片

这是整个章节的核心内容。这里设备在门户和移动应用上获得界面——**完全无需它们任何一行代码**。

## 工作原理

设备发布**卡片清单** —— 机器可读的描述"什么要显示、什么要控制"。门户和应用读取清单并构建卡片：传感器变成带实时值的单元格，控制变成按钮、输入框和列表。布局也可以从固件指定。

你不需要手动发布任何东西：通过`link.card()`声明实体，核心自动收集清单并在连接时发送。

## 1. 声明实体

所有声明在`setup()`中做，`s_link.begin()`后。我们的过滤器有三个实体：VOC读数、模式列表和阈值字段。我们逐个讲，最后把块整合到一起。

### 通用原则：id与label

每个实体有两个名字，不要混淆：

- **id** —— 内部、机器名(`"voc"`、`"mode"`)。拉丁字母、数字、下划线，无空格。实体就靠id彼此识别，布局、命令和门户都用它。起好一次就不改；
- **label** —— 人类的标签(`"VOC index"`、`"Mode"`)。你写什么用户就看什么。可随意改。

### 传感器：VOC读数

```cpp
s_link.card().sensor(
    "voc",              // id：实体的内部名
    "VOC index",        // label：卡片上的标签
    "",                 // unit：单位显示在数字右侧（"°C"、"%"、"g"）；
                        //   VOC指数没有单位——空字符串
    "units[0].vocIndex" // path：数值从哪里取——遥测JSON内的路径。
                        //   这正是我们在第5章添加的字段：
                        //   doc["units"][0]["vocIndex"]。名字必须完全一致，
                        //   否则卡片上显示破折号。
);
```

传感器是"只读"单元格：门户用`path`从遥测中取值并显示。传感器没有命令。

### 选择列表：工作模式

```cpp
// 列表选项。用户在下拉菜单中看到它们的原样。
static const char* kModes[] = { "auto", "on", "off" };

s_link.card().select(
    "mode",                  // id：实体的内部名
    "Mode",                  // label：卡片上的标签
    kModes,                  // options：选项数组（上面声明的）
    3,                       // 数组中的选项个数——auto、on、off = 三个。
                             //   C++自己不知道数组长度，由我们告诉
    [](const char* opt) {    // 回调：函数，当用户在门户上选择选项时
                             //   核心会调用它。opt —— 选中的字符串，比如"on"
        onModeSelected(opt); //   传给我们的逻辑（会在第7章写）
    }
);
```

这里出现了机制的第二部分：**控制**。当用户在门户上选择选项，设备收到命令，核心自己接收并检查（不在`options`中的陌生字符串不会到你这里），然后用选中的值调用你的回调。不需要手动解析MQTT消息——你的责任从`onModeSelected`内部开始。

### 数字字段：启动阈值

```cpp
s_link.card().number(
    "threshold",       // id：实体的内部名
    "VOC threshold",   // label：卡片上的标签
    100,               // min：门户不会让输入更小
    400,               // max：也不会更大；核心还会
                       //   在自己一端按这些边界剪裁
    10,                // step：用上下箭头改变的步长
    "",                // unit：单位；指数没有
    [](float v) {              // 回调：用户发送新值时调用；
                               //   v —— min..max范围内的数字
        onThresholdChanged(v); //   传给我们的逻辑（会在第7章写）
    }
);
```

### 整合到一起

最终的块应该在`setup()`中。`onModeSelected`和`onThresholdChanged`函数会在第7章写；为了现在代码能编译，在`setup()`上面声明它们的占位符：

```cpp
// 占位符：真正的实现会在第7章写（自动逻辑）。
static void onModeSelected(const char* opt) {}
static void onThresholdChanged(float v) {}

void setup() {
    Serial.begin(115200);
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

    // 卡片：传感器 + 两个控制器。
    s_link.card().sensor("voc", "VOC index", "", "units[0].vocIndex");

    static const char* kModes[] = { "auto", "on", "off" };
    s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) {
        onModeSelected(opt);
    });

    s_link.card().number("threshold", "VOC threshold", 100, 400, 10, "", [](float v) {
        onThresholdChanged(v);
    });
}
```

风机呢？**不用**声明它：`Config`中的`hasFan = true`标志已经自动向清单添加了"风机"单元——这是字典功能，核心自己知道关于它的一切。

!!! note "回调中的方括号总是空的"
    `[](const char* opt) { ... }` —— 这是lambda，无名函数；我们在[第5章的注记中](05-sensor-and-telemetry.md)详细讲过。提醒一下核心规则：捕获括号总是空的（`[]`），lambda不"带走"任何东西，所有需要的存在全局变量中 —— 像下一章的`g_mode`和`g_threshold`。

## 2. 自动卡片布局

根本不用指定布局。门户会自己从声明的实体组装卡片——而且组装得很整齐：显示单元格按行分组（一行最多三个，然后折行），控制器排在下面，每个占一行，全部用门户官方风格。对大多数设备这够了——界面看起来整洁而无需考虑排版。

卡片上的实体顺序 —— 它们在`setup()`中声明的顺序。

## 3. 自定义卡片布局（可选）

先看卡片怎么组织。卡片是竖直堆叠的**行**。一行传感器单元格平分宽度：一个占满整行，两个各占一半，三个各占三分之一。行中的控件（列表、字段、按钮）以整行宽度上下排列。

前一节的自动布局自己把实体放在这些行中。如果你要自己决定什么和什么并排——手动指定行，用`layoutRow`调用。一个调用 = 一行，调用顺序 = 行顺序从上到下：

```cpp
// 行1：两个单元格——VOC指数和风机，各占一半。
s_link.card().layoutRow("voc", "fan");

// 第 2 行：两个控件——模式和阈值，上下排列。
s_link.card().layoutRow("mode", "threshold");
```

`layoutRow`传的是实体的**id** —— 你给它们起的那些内部名字（这就是id的用处）。`"fan"`是风机字典实体的id，由`hasFan`标志创建。

卡片上会得到这样的布局：

```text
┌─ DIY Air Filter ────────────────┐
│  VOC index        │  风机       │   ← 行1：voc、fan
│  103              │  关        │
├───────────────────┴─────────────┤
│  Mode                           │   ← 行2：mode、threshold
│  [auto                       ▾] │
│  VOC threshold                  │
│  [150                         ] │
└─────────────────────────────────┘
```

你没在任何行中提到的实体不会消失 —— 门户会在下面用自动列表补齐。这样你可以只排版"主要"部分，其余交给自动。

## 4. 发布到MQTT的内容

核心会发布到主题`idryer/{serial}/card`（retained）：

```json
{
  "v": 1,
  "entities": [
    { "id": "fan",  "type": "binary_sensor", "device_class": "fan",
      "source": "telemetry", "path": "units[0].fanStatus" },
    { "id": "voc",  "type": "sensor", "label": "VOC index",
      "source": "telemetry", "path": "units[0].vocIndex" },
    { "id": "mode", "type": "select", "label": "Mode",
      "options": ["auto", "on", "off"], "action": "card.mode", "arg": "value" },
    { "id": "threshold", "type": "number", "label": "VOC threshold",
      "min": 100, "max": 400, "step": 10, "action": "card.threshold", "arg": "value" }
  ],
  "layout": [ ["voc", "fan"], ["mode", "threshold"] ]
}
```

不必深究这个JSON——核心从你的调用自动生成它。但了解一下有用：如果你写固件**不用**`idryer-core`（Rust、MicroPython，随意），只需自己发布这样的JSON —— 门户兼容任何格式相符的内容。

## 5. 检查

烧录后打开门户的仪表板：

- **VOC index**单元格显示实时指数（对传感器吹气——数字在下次更新时增长）；
- **风机**单元格 —— 打开/关闭；
- **Mode** —— 下拉列表，**VOC threshold** —— 数字字段：修改后约 0.6 秒发送，没有确认按钮。列表和字段从第一个选项和最小值开始——它们只发送命令，不显示设备当前的设置；状态由单元格显示（例如风机）。

选择模式和阈值现在什么都不做——回调是占位符。我们将在[下一章](07-auto-logic.md)激活它们。

!!! note "这正是那个概念"
    注意发生了什么：你在固件中用五行代码描述了界面——它在门户和应用中出现了。同样的手法对你任何设备都行：只改id、标签和回调。

!!! note "有启动和停止的操作"
    列表、字段和按钮会立即发送值。有启动和停止的操作——烘干、加热、照明——声明为带模式和启动参数的卡片**动作**：卡片显示启动表单，操作进行时显示带停止按钮的会话块。示例——储料柜部分的[第 7 章](../09-build-a-device/07-heating-control.md)。
