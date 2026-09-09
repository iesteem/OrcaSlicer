# 辅助风扇

OrcaSlicer 使用 `M106 P#` / `M107 P#` 控制切片器管理的所有风扇。

- `P0`：模型冷却风扇（默认层风扇）
- `P1`（如有）：附加风扇
- `P2`：常用作 Aux / CPAP / 增压风扇
- `P3`（及以上）：有时用作排风/腔体风扇等

在 Klipper 中，你可以创建宏来同时转换 OrcaSlicer 的数字风扇索引 `P` 和物理风扇的**人类可读名称**。这样既保持与生成的 G-code（M106 P0 / M106 P2 …）的兼容性，又能在内部按名称寻址风扇。

> [!WARNING]
> 请调整引脚名和参数（功率、cycle_time 等）以匹配你的硬件。

- [简单方案（仅索引 → fan0、fan2、fan3）](#简单方案仅索引--fan0fan2fan3)
- [高级方案（索引 ⇄ 名称映射）](#高级方案索引--名称映射)
  - [快速定制](#快速定制)
  - [用法](#用法)

## 简单方案（仅索引 → fan0、fan2、fan3）

这是最初的基础示例，直接拼接 `P` 索引（`fan0`、`fan2`、`fan3`）。如果不需要自定义名称，请使用此方案：

```ini
# 模型冷却风扇
[fan_generic fan0]
pin: PA7
cycle_time: 0.01
hardware_pwm: false

# 辅助风扇（如果没有请注释掉）
[fan_generic fan2]
pin: PA8
cycle_time: 0.01
hardware_pwm: false

# 排风/腔体风扇（如果没有请注释掉）
[fan_generic fan3]
pin: PA9
cycle_time: 0.01
hardware_pwm: false

[gcode_macro M106]
gcode:
    {% set fan = 'fan' + (params.P|int if params.P is defined else 0)|string %}
    {% set speed = (params.S|float / 255 if params.S is defined else 1.0) %}
    SET_FAN_SPEED FAN={fan} SPEED={speed}

[gcode_macro M107]
gcode:
    {% set fan = 'fan' + (params.P|int if params.P is defined else 0)|string %}
    {% if params.P is defined %}
    SET_FAN_SPEED FAN={fan} SPEED=0
    {% else %}
    # 无 P -> 关闭典型已定义风扇
    SET_FAN_SPEED FAN=fan0 SPEED=0
    SET_FAN_SPEED FAN=fan2 SPEED=0
    SET_FAN_SPEED FAN=fan3 SPEED=0
    {% endif %}
```

## 高级方案（索引 ⇄ 名称映射）

允许使用 `CPAP`、`EXHAUST` 等描述性名称。当你重新接线或改用风扇而不想更改切片器输出时非常有用，只需保持 `fan_map` 更新即可。

```ini
# 使用友好名称的示例，注释标注了 OrcaSlicer 索引

[fan_generic CPAP]        # OrcaSlicer fan 0
pin: PB7
max_power: 0.8
shutdown_speed: 0
kick_start_time: 0.100
cycle_time: 0.005
hardware_pwm: False
off_below: 0.10

[fan_generic EXHAUST]     # OrcaSlicer fan 3
pin: PE5
#max_power:
#shutdown_speed:
cycle_time: 0.01
hardware_pwm: False
#kick_start_time:
off_below: 0.2

# 如果你还有其他风扇（例如 P2），在此添加：
# [fan_generic AUX]
# pin: PXn

[gcode_macro M106]
description: "Set fan speed (Orca compatible)"
gcode:
    {% set fan_map = {
        0: "CPAP",      # Orca P0 → CPAP 鼓风机
        3: "EXHAUST",   # Orca P3 → 排风
        # 2: "AUX",     # 如果定义了 AUX 则取消注释
    } %}
    {% set p = params.P|int if 'P' in params else 0 %}
    {% set fan = fan_map[p] if p in fan_map else fan_map[0] %}
    {% set speed = (params.S|float / 255 if 'S' in params else 1.0) %}
    SET_FAN_SPEED FAN={fan} SPEED={speed}

[gcode_macro M107]
description: "Turn off fans. No P = all, P# = specific"
gcode:
    {% set fan_map = {
        0: "CPAP",
        3: "EXHAUST",
        # 2: "AUX",
    } %}
    {% if 'P' in params %}
        {% set p = params.P|int %}
        {% if p in fan_map %}
            SET_FAN_SPEED FAN={fan_map[p]} SPEED=0
        {% else %}
            RESPOND PREFIX="warn" MSG="Unknown fan index P{{p}}"
        {% endif %}
    {% else %}
        # 无 P -> 关闭所有映射的风扇
        {% for f in fan_map.values() %}
            SET_FAN_SPEED FAN={f} SPEED=0
        {% endfor %}
    {% endif %}
```

### 快速定制

1. 在 `fan_map` 中增删条目，以反映切片器可能使用的索引。
2. 在每个 `[fan_generic]` 旁保留类似 `# fan X OrcaSlicer` 的注释，便于对照。
3. 根据风扇类型（CPAP 鼓风机 vs 轴流排风）调整 `max_power`、`off_below`、`cycle_time`。

### 用法

- 在 OrcaSlicer 中：`M106 P0 S255`（CPAP 100%）、`M106 P3 S128`（EXHAUST 约 50%）。
- 关闭单个：`M107 P3`。全部关闭：`M107`。
- 你仍可以在 Klipper 控制台手动使用 `SET_FAN_SPEED FAN=CPAP SPEED=0.7`。

---

选择最适合你工作流程的方案；高级版本在保持与标准 OrcaSlicer G-code 输出完全兼容的同时，提供了额外的清晰度和灵活性。
