# 熨平（Ironing）

熨平是一种通过抹平顶层来改善 3D 打印表面质量的后处理。它在相同高度再打印一遍，但使用非常[低的流量](#流量)和特定[图案](#图案)。结果是更光滑的表面，可提升打印件的美观度，代价是打印时间增加。

![ironing](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/ironing/ironing.png?raw=true)

> [!IMPORTANT]
> 熨平会使耗材在热端中移动非常缓慢，增加热爬升和堵料风险。熨平期间请留意打印机，确保热端散热充分以防卡料。

## 类型

此设置控制熨平哪些层。

- **顶面（Top Surfaces）**：熨平所有[顶面](strength_settings_top_bottom_shells)。这是最常见的设置，用于抹平打印件顶层。
  ![ironing-top-surfaces](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/ironing/ironing-top-surfaces.png?raw=true)
- **最顶面（Topmost Surface）**：仅熨平打印件的最后一层[顶层](strength_settings_top_bottom_shells)。适合只需抹平最后一层的打印件。
  ![ironing-topmost-surface](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/ironing/ironing-topmost-surface.png?raw=true)
- **所有实心层（All solid layers）**：熨平所有实心层，包括[内部实心填充](strength_settings_infill#internal-solid-infill)和[顶层](strength_settings_top_bottom_shells)。适合所有实心表面都需要非常光滑的打印件，但打印时间可能大幅增加。
    ![ironing-all-solid-layers](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/ironing/ironing-all-solid-layers.png?raw=true)

## 图案

熨平时使用的图案。通常，最好的图案是对表面覆盖效率最高的那个。

> [!TIP]
> 请查看[填充图案 Wiki 列表](strength_settings_patterns)，含**详细规格**及各自优缺点。

熨平图案有：

- **[同心圆](strength_settings_patterns#concentric)**
- **[直线](strength_settings_patterns#rectilinear)**

## 流量

熨平期间挤出的材料量。
此百分比相对正常流量。值越低表面越光滑但可能覆盖不全；值越高覆盖越好但可能导致过挤出或表面更粗糙。

## 线间距

熨平线之间的距离。
建议设为等于或小于喷嘴直径，以获得最佳覆盖和表面质量。

## 内缩

与边缘保持的距离，有助于防止熨平面边缘过挤出。

![ironing-inset](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/ironing/ironing-inset.png?raw=true)

如果此值设为 0，熨平路径将直接从轮廓边缘开始，不做任何内缩。这意味着[熨平图案](#图案)会一直延伸到被熨平顶面的外边界。

## 角度

熨平的角度。
负数表示禁用此功能并使用[稀疏填充方向](strength_settings_infill#direction)。

## 速度

熨平速度的更多信息请查看[其他层速度设置](speed_settings_other_layers_speed#ironing-speed)。
