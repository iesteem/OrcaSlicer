# 图案（Patterns）

图案决定材料在打印件内的分布方式。不同的图案在相同密度设置下会影响强度、柔韧性和打印速度。

没有放之四海而皆准的方案，最佳图案取决于具体打印件及其需求。

许多图案可能看起来相似、总体规格相近，但实际表现可能大不相同。
与 3D 打印中的大多数设置一样，经验是判断哪种图案最适合你特定需求的最佳途径。

## 图案速查表

| | 图案 | 适用于 | X-Y 强度 | Z 强度 | 材料用量 | 打印时间 |
|---|---|---|---|---|---|---|
| ![param_monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_monotonic.svg?raw=true) | [Monotonic（单调）](#monotonic) | - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 一般 | 一般 | 中-高 | 中-低 |
| ![param_monotonicline](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_monotonicline.svg?raw=true) | [Monotonic line（单调线）](#monotonic-line) | - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 一般 | 一般 | 中 | 中 |
| ![param_rectilinear](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_rectilinear.svg?raw=true) | [Rectilinear（直线）](#rectilinear) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** - **[熨平](quality_settings_ironing)** | 中-低 | 低 | 中 | 中-低 |
| ![param_alignedrectilinear](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_alignedrectilinear.svg?raw=true) | [Aligned Rectilinear（对齐直线）](#aligned-rectilinear) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 中-低 | 中 | 中 | 中-低 |
| ![param_zigzag](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_zigzag.svg?raw=true) | [Zig Zag（锯齿）](#zig-zag) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-低 | 低 | 中 | 中-低 |
| ![param_crosszag](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_crosszag.svg?raw=true) | [Cross Zag（交叉锯齿）](#cross-zag) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中 | 低 | 中 | 中-低 |
| ![param_lockedzag](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_lockedzag.svg?raw=true) | [Locked Zag（锁定锯齿）](#locked-zag) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-低 | 中-低 | 低 | 极高 |
| ![param_line](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_line.svg?raw=true) | [Line（线）](#line) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 低 | 低 | 中-高 | 中-低 |
| ![param_grid](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_grid.svg?raw=true) | [Grid（网格）](#grid) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 中-高 | 中-低 |
| ![param_triangles](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_triangles.svg?raw=true) | [Triangles（三角形）](#triangles) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 中 | 中-高 | 中-低 |
| ![param_tri-hexagon](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_tri-hexagon.svg?raw=true) | [Tri-hexagon（三角六边形）](#tri-hexagon) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 中-高 | 中-高 | 中-低 |
| ![param_cubic](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_cubic.svg?raw=true) | [Cubic（立方）](#cubic) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 中-高 | 中-低 |
| ![param_adaptivecubic](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_adaptivecubic.svg?raw=true) | [Adaptive Cubic（自适应立方）](#adaptive-cubic) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-高 | 中-高 | 中 | 低 |
| ![param_quartercubic](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_quartercubic.svg?raw=true) | [Quarter Cubic（四分立方）](#quarter-cubic) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 中-高 | 中-低 |
| ![param_supportcubic](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_supportcubic.svg?raw=true) | [Support Cubic（支撑立方）](#support-cubic) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 低 | 低 | 中 | 极低 |
| ![param_lightning](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_lightning.svg?raw=true) | [Lightning（闪电）](#lightning) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 低 | 低 | 低 | 超低 |
| ![param_honeycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_honeycomb.svg?raw=true) | [Honeycomb（蜂窝）](#honeycomb) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 低 | 超高 |
| ![param_3dhoneycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_3dhoneycomb.svg?raw=true) | [3D Honeycomb（3D 蜂窝）](#3d-honeycomb) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-高 | 中-高 | 低 | 高 |
| ![param_lateral-honeycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_lateral-honeycomb.svg?raw=true) | [Lateral Honeycomb（侧向蜂窝）](#lateral-honeycomb) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-低 | 中-低 | 中-高 | 中-低 |
| ![param_lateral-lattice](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_lateral-lattice.svg?raw=true) | [Lateral Lattice（侧向晶格）](#lateral-lattice) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-低 | 低 | 中-高 | 中-低 |
| ![param_crosshatch](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_crosshatch.svg?raw=true) | [Cross Hatch（交叉编织）](#cross-hatch) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-高 | 中-高 | 中-低 | 中-高 |
| ![param_tpmsd](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_tpmsd.svg?raw=true) | [TPMS-D](#tpms-d) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 中-低 | 高 |
| ![param_tpmsfk](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_tpmsfk.svg?raw=true) | [TPMS-FK](#tpms-fk) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 中-高 | 中-高 | 低 | 高 |
| ![param_gyroid](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_gyroid.svg?raw=true) | [Gyroid（螺旋体）](#gyroid) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** | 高 | 高 | 中-低 | 中-高 |
| ![param_concentric](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_concentric.svg?raw=true) | [Concentric（同心圆）](#concentric) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** - **[熨平](quality_settings_ironing)** | 低 | 中 | 中-高 | 中-低 |
| ![param_hilbertcurve](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_hilbertcurve.svg?raw=true) | [Hilbert Curve（希尔伯特曲线）](#hilbert-curve) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 低 | 中 | 低 | 高 |
| ![param_archimedeanchords](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_archimedeanchords.svg?raw=true) | [Archimedean Chords（阿基米德弦）](#archimedean-chords) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 低 | 中 | 中-高 | 中-低 |
| ![param_octagramspiral](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/param_octagramspiral.svg?raw=true) | [Octagram Spiral（八角星螺旋）](#octagram-spiral) | - **[稀疏填充](strength_settings_infill#sparse-infill-density)** - **[实心填充](strength_settings_infill#internal-solid-infill)** - **[表面](strength_settings_top_bottom_shells)** | 低 | 中 | 中-低 | 中 |

> [!NOTE]
> 你可以下载用于计算上表数值的 [infill_desc_calculator.xlsx](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/print_settings/strength/infill_desc_calculator.xlsx?raw=true)。

## Monotonic（单调）

方向一致的[直线](#rectilinear)填充，视觉表面更光滑。

- **水平强度（X-Y）：** 一般
- **垂直强度（Z）：** 一般
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-monotonic.png?raw=true)

## Monotonic line（单调线）

类似[单调](#monotonic)，但避免与轮廓重叠，减少交接处的多余材料。可能引入可见接缝并增加打印时间。

- **水平强度（X-Y）：** 一般
- **垂直强度（Z）：** 一般
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-monotonic-line](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-monotonic-line.png?raw=true)

## Rectilinear（直线）

按填充密度间隔的平行线。每层与上一层垂直打印，因此垂直结合较弱。建议考虑改用新的[锯齿（Zig Zag）](#zig-zag)填充。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**
  - **[熨平](quality_settings_ironing)**

![infill-top-rectilinear](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-rectilinear.png?raw=true)

## Aligned Rectilinear（对齐直线）

按填充间隔分布的平行线，每层与上一层方向相同。垂直于线方向的水平强度良好，但沿线方向很差。
建议配合层锚定以改善非垂直方向的强度。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-aligned-rectilinear](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-aligned-rectilinear.png?raw=true)

## Zig Zag（锯齿）

类似[直线](#rectilinear)，但层间图案连续。可为具有两个对称部分的模型添加对称填充 Y 轴。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-zig-zag](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-zig-zag.png?raw=true)

## Cross Zag（交叉锯齿）

类似[锯齿](#zig-zag)，但通过"填充偏移步长"参数位移每一层。

- **水平强度（X-Y）：** 中
- **垂直强度（Z）：** 低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-cross-zag](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-cross-zag.png?raw=true)

## Locked Zag（锁定锯齿）

[锯齿](#zig-zag)的自适应版本，增加外层"皮肤"纹理以互锁各层，并配以低材料骨架。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 中-低
- **密度计算：** 同[锯齿](#zig-zag)，但靠近墙处增大
- **材料用量：** 中-高
- **打印时间：** 极高
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-locked-zag](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-locked-zag.png?raw=true)

## Line（线）

类似[直线](#rectilinear)，但每条线略微旋转以提高打印速度。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-line](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-line.png?raw=true)

## Grid（网格）

垂直线的双层图案，形成网格。交叠点可能产生噪音或伪影。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-grid](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-grid.png?raw=true)

## Triangles（三角形）

基于三角形的网格，X-Y 强度出色，但交叉点存在三重重叠。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-triangles](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-triangles.png?raw=true)

## Tri-hexagon（三角六边形）

类似[三角形](#triangles)图案，但经过偏移以避免交叉点的三重重叠。此设计结合了三角形和六边形，提供出色的 X-Y 强度。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 中-高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-tri-hexagon](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-tri-hexagon.png?raw=true)

## Cubic（立方）

角朝下的 3D 立方体图案，向各方向分散受力。水平面上的三角形提供良好的 X-Y 强度。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-cubic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-cubic.png?raw=true)

## Adaptive Cubic（自适应立方）

密度自适应的[立方](#cubic)图案：靠近墙更密，中心更疏。在保持强度的同时节省材料和时间，是大型打印的理想选择。

- **水平强度（X-Y）：** 中-高
- **垂直强度（Z）：** 中-高
- **密度计算：** 同[立方](#cubic)，但中心减少
- **材料用量：** 低
- **打印时间：** 低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-adaptive-cubic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-adaptive-cubic.png?raw=true)

## Quarter Cubic（四分立方）

带有额外内部分割的[立方](#cubic)图案，提升 X-Y 强度。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-quarter-cubic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-quarter-cubic.png?raw=true)

## Support Cubic（支撑立方）

Support Cubic 是[立方](#cubic)填充图案的变体，专为支撑顶层设计。材料用量高于闪电填充，但强度更好。不过它仍是一种低密度填充图案。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 低
- **密度计算：** 顶壳层前一层面积的 %
- **材料用量：** 极低
- **打印时间：** 极低
- **材料/时间比（越高越好）：** 中
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-support-cubic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-support-cubic.png?raw=true)

## Lightning（闪电）

超高速、超低材料的填充。为速度和效率而设计，适合快速打印或非结构原型。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 低
- **密度计算：** 顶壳层前一层面积的 %
- **材料用量：** 超低
- **打印时间：** 超低
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-lightning](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-lightning.png?raw=true)

## Honeycomb（蜂窝）

平衡强度与材料用量的六边形图案。每个六边形的复壁会增加材料消耗。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 高
- **打印时间：** 超高
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-honeycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-honeycomb.png?raw=true)

## 3D Honeycomb（3D 蜂窝）

此填充尝试通过打印方形和八边形（保持足够高的垂直角度以与上一层接触）来生成可打印的蜂窝结构。

- **水平强度（X-Y）：** 中-高
- **垂直强度（Z）：** 中-高
- **密度计算：** 未知
- **材料用量：** 中-低
- **打印时间：** 高
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-3d-honeycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-3d-honeycomb.png?raw=true)

## Lateral Honeycomb（侧向蜂窝）

垂直蜂窝图案。抗扭刚度尚可。为机翼等低密度结构而开发。相比[侧向晶格](#lateral-lattice)的改进在于更低密度下性能相同。此填充包含悬垂角参数，可改善层间接触点、降低分层风险。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 中-低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-lateral-honeycomb](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-lateral-honeycomb.png?raw=true)

## Lateral Lattice（侧向晶格）

强度低但柔韧性好的图案。你可以调整**角度 1**和**角度 2**为特定模型优化填充。每个角度调整图案生成的每一层所在平面。0° 为垂直。

- **水平强度（X-Y）：** 中-低
- **垂直强度（Z）：** 低
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-lateral-lattice](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-lateral-lattice.png?raw=true)

## Cross Hatch（交叉编织）

类似[螺旋体](#gyroid)但为线性图案，在内部拐角处形成薄弱点。
切片更容易，但若要更好的强度和柔韧性，请考虑使用 [TPMS-D](#tpms-d) 或[螺旋体](#gyroid)。

- **水平强度（X-Y）：** 中-高
- **垂直强度（Z）：** 中-高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-高
- **材料/时间比（越高越好）：** 中-低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-cross-hatch](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-cross-hatch.png?raw=true)

## TPMS-D

三重周期最小曲面（Schwarz Diamond）。[交叉编织](#cross-hatch)与[螺旋体](#gyroid)的混合体，兼具刚性与平滑过渡。各向同性、各方向均强。此几何的切片速度快于螺旋体，但慢于交叉编织。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 高
- **材料/时间比（越高越好）：** 中-低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-tpms-d](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-tpms-d.png?raw=true)

## TPMS-FK

三重周期最小曲面（Fischer–Koch S）图案。其平滑连续的几何类似骨小梁微结构，在刚性与能量吸收之间取得平衡。与 [TPMS-D](#tpms-d) 相比，其曲率更复杂，可改善功能件的载荷分布和减震。

- **水平强度（X-Y）：** 中-高
- **垂直强度（Z）：** 中-高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 高
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-tpms-fk](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-tpms-fk.png?raw=true)

## Gyroid（螺旋体）

数学上的各向同性曲面，各方向强度相等。其互连结构使其非常适合坚固、柔韧的打印件及树脂灌注。由于生成每条曲线需要大量点，此图案切片耗时可能更长。若模型几何复杂，请考虑使用更简单的填充图案，如 [TPMS-D](#tpms-d) 或[交叉编织](#cross-hatch)。

- **水平强度（X-Y）：** 高
- **垂直强度（Z）：** 高
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-高
- **材料/时间比（越高越好）：** 中-低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**

![infill-top-gyroid](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-gyroid.png?raw=true)

## Concentric（同心圆）

用逐层缩小的外轮廓填充区域，形成同心图案。适合 100% 填充或柔性打印。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**
  - **[熨平](quality_settings_ironing)**

![infill-top-concentric](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-concentric.png?raw=true)

## Hilbert Curve（希尔伯特曲线）

希尔伯特曲线是一种空间填充曲线，可用于创建连续的填充图案。以美观和高效填充空间的能力著称。
由于路径复杂，打印速度很低，导致打印时间长。不推荐用于结构件，但可用于外观用途。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 高
- **材料/时间比（越高越好）：** 低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-hilbert-curve](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-hilbert-curve.png?raw=true)

## Archimedean Chords（阿基米德弦）

螺旋图案，用同心弧填充区域，形成平滑连续的填充。由于其互连的中空结构允许树脂流过并充分固化，因此可以灌注树脂。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中-低
- **材料/时间比（越高越好）：** 中-高
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-archimedean-chords](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-archimedean-chords.png?raw=true)

## Octagram Spiral（八角星螺旋）

外观图案，强度低、打印时间长。

- **水平强度（X-Y）：** 低
- **垂直强度（Z）：** 中
- **密度计算：** 填充总体积的 %
- **材料用量：** 中
- **打印时间：** 中
- **材料/时间比（越高越好）：** 中-低
- **适用于：**
  - **[稀疏填充](strength_settings_infill#sparse-infill-density)**  - **[实心填充](strength_settings_infill#internal-solid-infill)**
  - **[表面](strength_settings_top_bottom_shells)**

![infill-top-octagram-spiral](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-top-octagram-spiral.png?raw=true)
