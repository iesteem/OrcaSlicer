# 强度高级（Strength Advanced）

- [填充方向对齐模型](#填充方向对齐模型)
- [桥填充方向](#桥填充方向)
- [最小稀疏填充阈值](#最小稀疏填充阈值)
- [填充组合](#填充组合)
  - [最大层高](#最大层高)
- [检测窄内部实心填充](#检测窄内部实心填充)
- [确保垂直壳厚度](#确保垂直壳厚度)

## 填充方向对齐模型

使填充和表面填充的方向跟随模型在热床上的朝向。
启用后，填充方向随模型旋转，以保持最优特性。

![fill-direction-to-model](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/fill-direction-to-model.png?raw=true)

## 桥填充方向

桥接角度覆盖。
若保持为 0，桥接角度将自动计算。否则，将使用指定的角度打印桥。
用 180° 表示 0° 角。

## 最小稀疏填充阈值

小于阈值的稀疏填充区域会被替换为[内部实心填充](strength_settings_infill#internal-solid-infill)。
此设置有助于确保小面积稀疏填充不会损害打印强度。对具有复杂设计或细小特征的模型特别有用，因为这些地方稀疏填充可能提供不了足够支撑。

## 填充组合

自动将若干层的[稀疏填充](strength_settings_infill)合并，使它们一起打印，从而缩短打印时间并提高强度。墙仍以原始[层高](quality_settings_layer_height)打印。

![fill-combination](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/fill-combination.png?raw=true)

### 最大层高

合并后稀疏填充的最大层高。
设为 0 或 100% 时使用喷嘴直径（最大限度缩短打印时间），设为约 80% 则最大限度提高稀疏填充强度。

填充合并的层数由该值除以层高并向下取整得出。

可使用绝对毫米值（如 0.4mm 喷嘴用 0.32mm）或百分比（如 80%）。此值不得大于喷嘴直径。

## 检测窄内部实心填充

此选项自动检测窄小的内部实心填充区域。若启用，这些区域将使用[同心圆图案](strength_settings_patterns#concentric)以加快打印。否则默认使用[直线图案](strength_settings_patterns#rectilinear)。

## 确保垂直壳厚度

在倾斜表面附近添加实心填充，以保证垂直壳厚度（顶部和底部实心层）。

- **无（None）**： 任何地方都不添加实心填充。**注意：** 若模型有斜面请谨慎使用。
- **仅关键（Critical Only）**： 避免为墙添加实心填充。
- **中等（Moderate）**： 仅为倾斜严重的表面添加实心填充。
- **全部（All，默认）**： 为所有合适的倾斜表面添加实心填充。
