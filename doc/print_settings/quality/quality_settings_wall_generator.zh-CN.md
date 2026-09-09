# 墙生成器（Wall Generator）

墙生成器决定模型外墙和内墙（轮廓）的打印方式。

- [Classic](#classic)
- [Arachne](#arachne)
  - [墙过渡阈值角度](#墙过渡阈值角度)
  - [墙过渡过滤余量](#墙过渡过滤余量)
  - [墙过渡长度](#墙过渡长度)
  - [墙分布数量](#墙分布数量)
  - [最小墙宽](#最小墙宽)
    - [首层最小墙宽](#首层最小墙宽)
  - [最小特征尺寸](#最小特征尺寸)
  - [最小墙长](#最小墙长)

## Classic

Classic 墙生成器是许多切片器使用的简单可靠方法。它按设定的[线宽](quality_settings_line_width)沿模型轮廓挤出，生成尽可能多的墙（受[墙数](strength_settings_walls#wall-loops)限制）。
此方法不改变挤出宽度，适合快速、可预测的切片。

![wallgenerator-classic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/WallGenerator/wallgenerator-classic.png?raw=true)

## Arachne

Arachne 墙生成器会动态调整挤出宽度，以更紧密地贴合模型形状。这使它能更好地处理细薄特征，并在墙数量之间平滑过渡。

![wallgenerator-arachne](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/WallGenerator/wallgenerator-arachne.png?raw=true)

> [!NOTE]
> [A Framework for Adaptive Width Control of Dense Contour-Parallel Toolpaths in Fused Deposition Modeling](https://www.sciencedirect.com/science/article/pii/S0010448520301007?via%3Dihub)

### 墙过渡阈值角度

定义算法在偶数与奇数墙数之间创建过渡所需的最小角度（单位：度）。若楔形超过此角度，则不会添加额外的中间墙。降低此值会减少中间墙，但可能导致尖角处欠挤出或过挤出。

### 墙过渡过滤余量

通过在最小墙宽周围定义一个容差范围，防止墙数量的快速切换。挤出宽度将保持在以下范围内：

```math
\left[ \text{最小墙宽} - \text{余量},\ 2 \times \text{最小墙宽} + \text{余量} \right]
```

值越高，过渡、空驶移动和挤出启停越少，但可能增大挤出变化并引发打印质量问题。以喷嘴直径的百分比表示。

### 墙过渡长度

控制墙数量之间的过渡向模型内部延伸的距离。值越低会缩短或消除中间墙，缩短打印时间，但可能减少狭窄区域的覆盖。

### 墙分布数量

设定允许改变宽度的墙的数量（从外墙向内计数）。值越低，宽度变化越局限于内墙，使外墙保持一致以获得最佳表面质量。

### 最小墙宽

定义可用于表现细薄特征的最窄墙宽。若特征比此值更窄，墙宽将与特征宽度一致。以喷嘴直径的百分比表示。

#### 首层最小墙宽

指定首层的最小墙宽。建议设为喷嘴直径，以改善附着并确保底层墙稳定。

### 最小特征尺寸

模型特征被打印所需的最小宽度。低于此值的特征会被跳过；高于此值的特征会被加宽到[最小墙宽](#最小墙宽)。以喷嘴直径的百分比表示。

### 最小墙长

避免产生增加不必要时间的极短或孤立墙段。
增大此值会移除不相连的短墙，**提高效率**。

> [!NOTE]
> 顶面和底面不受此设置影响，以避免视觉伪影。
> 可使用高级设置中的 One Wall Threshold（单墙阈值）来调整 OrcaSlicer 将区域判定为顶面的激进程度。此选项仅在此设置超过 0.5 时，或启用单墙顶面时出现。
