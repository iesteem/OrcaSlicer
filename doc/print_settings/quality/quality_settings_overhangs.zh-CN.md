# 悬垂（Overhangs）

- [检测悬垂墙](#检测悬垂墙)
- [使悬垂可打印](#使悬垂可打印)
  - [最大角度](#最大角度)
  - [孔面积](#孔面积)
- [悬垂额外轮廓](#悬垂额外轮廓)
- [偶数层反向](#偶数层反向)
  - [仅内部反向](#仅内部反向)
  - [反向阈值](#反向阈值)

## 检测悬垂墙

检测相对线宽的悬垂百分比并使用不同速度打印。
当检测到 100% 悬垂的线宽时，使用桥接选项。

![overhang](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/overhangs/overhang.png?raw=true)

## 使悬垂可打印

此设置会修改几何，使悬垂无需支撑材料即可打印。
所有超过[最大角度](#最大角度)的悬垂都会被修改为可打印。

### 最大角度

使更陡悬垂可打印后所允许的最大悬垂角度。
90° 完全不改变模型，允许任何悬垂；0 则用锥形材料替换所有悬垂。

![overhang-printable](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/overhangs/overhang-printable.png?raw=true)

> [!TIP]
> 通常，45° 到 60° 之间的值对大多数打印机和模型都适用。

### 孔面积

模型底面孔在被锥形材料填充前允许的最大面积。
值为 0 将填充模型底部的所有孔。

## 悬垂额外轮廓

在陡峭悬垂和无法锚定桥的区域创建额外的轮廓（悬垂墙）路径。

![extra-perimeters-on-overhangs](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/overhangs/extra-perimeters-on-overhangs.png?raw=true)

## 偶数层反向

在偶数层以相反方向挤出轮廓。这种交替模式借助材料的挤压方向可以显著改善陡峭悬垂。

此设置还有助于减少零件翘曲，因为应力被分布到交替方向而减弱。适合易翘曲材料如 ABS/ASA，也适合弹性耗材如 TPU 和丝绸 PLA。
它还可以减少支撑上方悬浮区域的翘曲。

要让此设置最有效，建议将[反向阈值](#反向阈值)设为 0，使所有墙无论悬垂程度如何都在偶数层交替方向打印。
缺点是外墙可能因挤出方向交替而出现纹理。

![reverse-odd-texture](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/reverse-odd-texture.png?raw=true)

> [!NOTE]
> 仅当[墙环方向](quality_settings_wall_and_surfaces#wall-loop-direction)设为**Auto**且[螺旋花瓶模式](others_settings_special_mode#spiral-vase)**禁用**时可用。

### 仅内部反向

减少外墙纹理的一个简单办法是只反转内部墙。
这仍能带来偶数层交替挤出方向的几乎全部好处（若使用 [inner/outer](quality_settings_wall_and_surfaces#innerouter)），而外墙保持同向打印，表面更光滑。

### 反向阈值

你可以为悬垂反转设定一个"有用"的阈值。
可设为：

- **0**：禁用阈值，即所有墙在偶数层都反向。
- **mm**：固定距离（毫米）。
- **%**：轮廓宽度的百分比。

使用此设置时，悬垂超过阈值的层中墙会产生反转纹理，其余墙按正常方向打印。
这可能导致纹理不均匀，有时被认为比完全反转纹理更差，因此只有确定悬垂反转对你的模型无用时才建议使用。

> [!NOTE]
> 仅在以下条件满足时可用：
> - 已启用[检测悬垂墙](#检测悬垂墙)
> - 未启用[仅内部反向](#仅内部反向)
> 条件不满足时此设置将被隐藏。
