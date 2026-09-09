# 多热床类型

你可以在打印机设置中启用它。

启用后，你可以在下拉菜单中选择热床类型，对应的热床温度会被自动设置。
你可以按热床类型在耗材设置中设定热床温度，如下图所示。

![bed-types](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/bed-types.gif?raw=true)

Orca 还支持自定义 G-code 中的 `curr_bed_type` 变量。
例如，以下示例 G-code 可以检测所选热床类型，并为 Klipper 相应调整 G-code 偏移：

```c++
{if curr_bed_type=="Textured PEI Plate"}
  SET_GCODE_OFFSET Z=-0.05
{else}
  SET_GCODE_OFFSET Z=0.0
{endif}
```

可用的热床类型有：

```c++
"Cool Plate"
"Engineering Plate"
"High Temp Plate"
"Textured PEI Plate"
```
