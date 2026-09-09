# OrcaSlicer 中的空气过滤/排风扇控制

OrcaSlicer 使用 `M106 P3` 命令控制空气过滤/排风扇。

如果你使用 Klipper，可以定义一个 `M106` 宏来同时控制普通模型冷却风扇、辅助风扇和排风扇。

以下是 Klipper 的参考配置。

> [!NOTE]
> 别忘了把配置中的引脚名改成你实际使用的引脚名。

```ini
# 不使用 [fan]，这里用 [fan_generic] 定义默认的模型冷却风扇
# 这是默认的模型冷却风扇
[fan_generic fan0]
pin: PA7
cycle_time: 0.01
hardware_pwm: false

# 这是辅助风扇
# 如果没有辅助风扇，请注释掉
[fan_generic fan2]
pin: PA8
cycle_time: 0.01
hardware_pwm: false

# 这是排风扇
# 如果没有排风扇，请注释掉
[fan_generic fan3]
pin: PA9
cycle_time: 0.01
hardware_pwm: false

[gcode_macro M106]
gcode:
    {% set fan = 'fan' + (params.P|int if params.P is defined else 0)|string %}
    {% set speed = (params.S|float / 255 if params.S is defined else 1.0) %}
    SET_FAN_SPEED FAN={fan} SPEED={speed}
```
