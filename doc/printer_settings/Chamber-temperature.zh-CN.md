# 腔体温度控制

OrcaSlicer 使用 `M141/M191` 命令控制主动腔体加热器。

如果你的耗材勾选了 `Activate temperature control`（启用温度控制），且打印机的 `Support control chamber temperature`（支持控制腔体温度）选项已勾选，OrcaSlicer 会在 G-code 开头（`Machine G-code` 之前）插入 `M191` 命令。

![Chamber-Temperature-Control-Printer](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Chamber/Chamber-Temperature-Control-Printer.png?raw=true)
![Chamber-Temperature-Control-Material](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Chamber/Chamber-Temperature-Control-Material.png?raw=true)


> [!NOTE]
> 如果机器装有辅助风扇，OrcaSlicer 会在加热期间自动启动风扇，帮助腔体内空气循环。

## 在机器 G-code 中使用腔体温度变量

如有需要，你可以在 `Machine G-code` 中使用腔体温度变量手动控制腔体温度：

- 将腔体温度设为第一种耗材指定的值：
  ```gcode
  M191 S{chamber_temperature[0]}
  ```
- 将腔体温度设为所有耗材中指定的最高值：
  ```gcode
  M191 S{overall_chamber_temperature}
  ```

## Klipper

如果你使用 Klipper，可以定义以下宏来控制主动腔体加热器。
以下是 Klipper 的参考配置。

> [!IMPORTANT]
> 别忘了将配置中的引脚名/数值改成你实际使用的值。

```gcode
[heater_generic chamber_heater]
heater_pin:PB10
max_power:1.0
# Orca 注：这里的温度传感器应是你用于腔体温度的传感器，而不是 PTC 传感器
sensor_type:NTC 100K MGB18-104F39050L32
sensor_pin:PA1
control = pid
pid_Kp = 63.418
pid_ki = 0.960
pid_kd = 1244.716
min_temp:0
max_temp:70

[gcode_macro M141]
gcode:
    SET_HEATER_TEMPERATURE HEATER=chamber_heater TARGET={params.S|default(0)}

[gcode_macro M191]
gcode:
    {% set s = params.S|float %}
    {% if s == 0 %}
        # 如果目标温度为 0，什么都不做
        M117 Chamber heating cancelled
    {% else %}
        SET_HEATER_TEMPERATURE HEATER=chamber_heater TARGET={s}
        # Orca：如果想让热床辅助腔体加热，请取消下面这行的注释
        # M140 S100
        TEMPERATURE_WAIT SENSOR="heater_generic chamber_heater" MINIMUM={s-1} MAXIMUM={s+1}
        M117 Chamber at target temperature
    {% endif %}
```
