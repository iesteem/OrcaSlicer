# 墙与表面（Wall and surfaces）

- [墙打印顺序](#墙打印顺序)
  - [内/外（Inner/Outer）](#innerouter)
  - [内/外/内（Inner/Outer/Inner）](#innerouterinner)
  - [外/内（Outer/Inner）](#outerinner)
  - [先打印填充](#先打印填充)
- [墙环方向](#墙环方向)
- [表面流量比例](#表面流量比例)
- [仅单墙](#仅单墙)
  - [阈值](#阈值)
- [避免穿越墙](#避免穿越墙)
  - [最大绕行长度](#最大绕行长度)
- [小面积流量补偿](#小面积流量补偿)
  - [流量补偿模型](#流量补偿模型)

## 墙打印顺序

内部（内）墙与外部（外）墙的打印顺序。

### 内/外（Inner/Outer）

使用内/外可获得最佳悬垂效果。因为悬垂墙在打印时可以附着到相邻的轮廓上。但此选项会略微降低表面质量，因为外墙被挤压到内墙上时会产生变形。

![inner-outer](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/inner-outer.gif?raw=true)

### 内/外/内（Inner/Outer/Inner）

使用内/外/内可获得最佳的外表面质量和尺寸精度，因为外墙在打印时不会受到内轮廓的干扰。但悬垂性能会下降，因为没有内轮廓可以让外墙依附打印。此选项至少需要 3 道墙才能生效：它先打印从第 3 道轮廓起的内墙，然后打印外墙，最后打印第一道内墙。在大多数情况下，推荐使用此选项而非外/内。

![inner-outer-inner](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/inner-outer-inner.gif?raw=true)

### 外/内（Outer/Inner）

使用外/内可获得与[内/外/内](#innerouterinner)相同的外墙质量和尺寸精度优势。但由于新层的第一段挤出开始于可见表面，Z 接缝的一致性会变差。

![outer-inner](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/outer-inner.gif?raw=true)

### 先打印填充

启用此选项后，先打印[填充](strength_settings_infill)和[顶/底壳](strength_settings_top_bottom_shells)，再打印墙。这对某些悬垂有用，因为填充可以支撑墙。

![infill-first](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/infill-first.gif?raw=true)

**但是**，填充在与墙相连处会略微把已打印的墙向外推，导致外表面质量变差。还可能造成填充透过零件外表面显露出来（"透印"）。

![infill-ghosting](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/infill-ghosting.png?raw=true)

使用此选项时，建议启用[精确墙](quality_settings_precision#精确墙)、使用[内/外/内](#innerouterinner)墙打印顺序，或减小[填充/墙重叠](strength_settings_infill#infill-wall-overlap)，以避免填充把外墙推出。

## 墙环方向

从上向下看时墙环的挤出方向。
默认所有墙逆时针挤出，除非启用了[偶数层反向](quality_settings_overhangs#偶数层反向)。
将此选项设为 Auto 以外的任何值，都会强制指定墙的方向，无视[偶数层反向](quality_settings_overhangs#偶数层反向)。

> [!NOTE]
> 启用螺旋花瓶模式时此选项将被禁用。

## 表面流量比例

此系数影响[顶部或底部实心填充](strength_settings_top_bottom_shells)的材料量。可略微降低此值以获得更光滑的表面。
实际使用的顶面流量 = 此值 × 耗材流量比例（如有对象流量比例则再相乘）。

> [!TIP]
> 在使用非 1 的值之前，建议先[校准流量比例](flow-rate-calib)，确保你的打印机和耗材的流量比例已正确设置。

## 仅单墙

在平面上只使用一道墙，为[顶面填充图案](strength_settings_top_bottom_shells#surface-pattern)留出更多空间。
对小特征特别有用，比如字母，其顶面非常小，墙的[同心圆图案](strength_settings_patterns#concentric)无法妥善覆盖。

![only-one-wall](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/only-one-wall.gif?raw=true)

### 阈值

若需要打印的顶面部分被另一层覆盖，当其宽度低于此值时不会被认定为顶面。这有助于防止"顶面单墙"在本应只由轮廓覆盖的表面上触发。
此值可为 mm 或轮廓挤出宽度的百分比。

![only-one-wall-threshold](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/only-one-wall-threshold.png?raw=true)

> [!WARNING]
> 若启用后下一层存在细薄特征（如字母），可能产生伪影。将此设置设为 0 可消除这些伪影。

## 避免穿越墙

此选项指示切片器在空驶移动时避免穿越轮廓（墙）。
打印头不会直接穿过墙，而是绕行，这可以显著减少表面缺陷和拉丝。

虽然这会略微增加打印时间，但打印质量的提升——尤其是对**PETG**或**TPU**等易拉丝材料——通常值得这一代价。
强烈推荐用于细节件或外观件。

![avoid-crossing-walls](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/avoid-crossing-walls.png?raw=true)

### 最大绕行长度

定义为避免穿越墙所允许的最大绕行距离。
可设为：

- **毫米绝对值：** 绕行可延伸的确切距离（如 `5mm`）。
- 直通路径的**百分比**（如 `50%`）。
- **0** 表示禁用**限制**，允许**任意长度**的绕行。

使用此设置在打印时间与墙质量之间取得平衡——绕行越长，穿越墙越少，但打印越慢。

## 小面积流量补偿

为小填充区域启用自适应流量控制。
此功能有助于解决实心填充小区域中常见的挤出问题，例如窄字母顶部或精细特征的顶端。
在这些情况下，标准挤出流量对于可用空间可能过多，导致过挤出或表面质量差。

![flow-compensation-model](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/flow-compensation-model.png?raw=true)

它通过根据挤出路径长度动态调整挤出流量来工作，确保在小空间内更精确地沉积材料。

这是 @Alexander-T-Moss 的 [Small Area Flow Compensation](https://github.com/Alexander-T-Moss/Small-Area-Flow-Comp) 的原生实现。

### 流量补偿模型

该模型使用一系列"挤出长度–流量修正系数"数值对。每一对定义了特定挤出长度下应使用的流量。
对于列出的点之间的值，流量通过线性插值计算。

![flow-compensation-model-graph](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Wall-Order/flow-compensation-model-graph.png?raw=true)

例如对于以下模型：

| 挤出长度 | 流量修正系数 |
|------------------|------------------------|
| 0                | 0                      |
| 0.2              | 0.4444                 |
| 0.4              | 0.6145                 |
| 0.6              | 0.7059                 |
| 0.8              | 0.7619                 |
| 1.5              | 0.8571                 |
| 2                | 0.8889                 |
| 3                | 0.9231                 |
| 5                | 0.952                  |
| 10               | 1                      |

应写作：

```c++
0,0;
0.2,0.4444;
0.4,0.6145;
0.6,0.7059;
0.8,0.7619;
1.5,0.8571;
2,0.8889;
3,0.9231;
5,0.9520;
10,1;
```
