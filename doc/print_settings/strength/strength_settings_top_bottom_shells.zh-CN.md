# 顶壳和底壳（Top and Bottom Shells）

控制顶部和底部实心层（壳）的生成方式。

![top-bottom-shells](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/top-bottom-shells/top-bottom-shells.png?raw=true)

## 壳层数

实心壳层的数量，包括表面层。
当由此值计算出的厚度小于[壳厚度](#壳厚度)时，壳层数会增加。

这些层打印在[稀疏填充](strength_settings_infill)之上，因此增加**壳层数**会提高零件整体强度和顶面质量。
通常建议大多数打印至少使用 3 层壳。

## 壳厚度

若由壳层数计算出的厚度薄于此值，切片时会增加实心层数。这可避免层高较小时壳过薄。
0 表示禁用此设置，壳厚度完全由[壳层数](#壳层数)决定。

## 表面密度

此设置控制顶面和底面的密度。100% 表示实心表面，较低的值生成稀疏表面。
可用于外观用途、改善抓握或创建界面。

## 填充/墙重叠

顶部实心填充区域略微扩大以与墙重叠，改善结合并尽量减少填充与墙相交处的针孔。
25-30% 是不错的起点。百分比值相对稀疏填充的线宽。

> [!TIP]
> 查看 [Monotonic Line（单调线）](strength_settings_patterns#monotonic-line)，了解它与 [Monotonic（单调）](strength_settings_patterns#monotonic)和 [Rectilinear（直线）](strength_settings_patterns#rectilinear)在覆盖方式上的差异。

## 表面图案

此设置控制表面的图案。
若[壳层数](#壳层数)大于 1，表面图案仅应用于最外层壳，其余层使用[内部实心填充图案](strength_settings_infill#internal-solid-infill)。

> [!TIP]
> 请查看[填充图案 Wiki 列表](strength_settings_patterns)，含**详细规格**及各自优缺点。

表面图案有：

- **[Concentric（同心圆）](strength_settings_patterns#concentric)**
- **[Rectilinear（直线）](strength_settings_patterns#rectilinear)**
- **[Monotonic（单调）](strength_settings_patterns#monotonic)**
- **[Monotonic Line（单调线）](strength_settings_patterns#monotonic-line)** 通常推荐用于顶面。
- **[Aligned Rectilinear（对齐直线）](strength_settings_patterns#aligned-rectilinear)**
- **[Hilbert Curve（希尔伯特曲线）](strength_settings_patterns#hilbert-curve)**
- **[Archimedean Chords（阿基米德弦）](strength_settings_patterns#archimedean-chords)**
- **[Octagram Spiral（八角星螺旋）](strength_settings_patterns#octagram-spiral)**
