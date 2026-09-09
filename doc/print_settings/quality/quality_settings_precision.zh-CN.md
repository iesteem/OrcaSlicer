# 精度（Precision）

本节介绍影响打印精度的设置。这些设置可以帮助你获得更好的尺寸精度、减少伪影，并提升整体打印质量。

- [切片间隙闭合半径](#切片间隙闭合半径)
- [分辨率](#分辨率)
- [圆弧拟合](#圆弧拟合)
- [X-Y 补偿](#xy-补偿)
- [象脚补偿](#象脚补偿)
- [精确墙](#精确墙)
  - [技术解释](#技术解释)
- [精确 Z 高度](#精确-z-高度)
- [多边形孔](#多边形孔)

## 切片间隙闭合半径

在三角网格切片过程中，小于 2 倍间隙闭合半径的裂缝会被填充。
间隙闭合操作可能降低最终打印分辨率，因此建议将该值保持在合理较低的水平。

## 分辨率

生成 G-code 路径前会对模型轮廓进行简化，以避免产生过多的点和 G-code 行。
值越小分辨率越高，但切片时间越长。如果在低性能机器上处理大模型，可以增大此值以加快切片。

## 圆弧拟合

启用后将得到包含 [G2 和 G3](https://marlinfw.org/docs/gcode/G002-G003.html) 圆弧移动指令的 G-code 文件。

模型切片完成后，此功能会在可能的地方用圆弧替换直线段。这对曲面特别有用，因为打印机可以更流畅地运动，同时减少 G-code 指令数量，提升整体打印质量。

由于用圆弧替代了许多短直线段，同一模型的 G-code 文件会更小。这可以改善打印质量并缩短打印时间，尤其是曲面模型。

![arc-fitting](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/arc-fitting.svg?raw=true)

> [!IMPORTANT]
> 此选项仅适用于支持 G2 和 G3 指令的机器，并可能增加打印机端的 CPU 负担。

> [!NOTE]
> **Klipper 机器**建议禁用此选项。
> Klipper 不会从圆弧指令中获益，因为固件会再次将圆弧拆分为直线段。切片器把线段转为圆弧、固件又转回线段，反而会导致表面质量下降。

## X-Y 补偿

用于补偿模型的外部尺寸。
通过此选项可以补偿材料膨胀或收缩，这些可能由多种因素引起，如耗材类型、温度波动或打印机校准问题。

请参照[校准指南](https://github.com/SoftFever/OrcaSlicer/wiki/Calibration)和[耗材公差校准](https://github.com/SoftFever/OrcaSlicer/wiki/tolerance-calib)，为你的打印机与耗材组合确定正确的值。

## 象脚补偿

此功能用于补偿"象脚"效应，即打印件最初几层比其余层更宽的现象，原因包括：

- 上方材料的重量。
- 材料的热膨胀。
- 热床温度过高。
- 热床高度不准确。

![elephant-foot](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/elephant-foot.svg?raw=true)

为减轻此效应，OrcaSlicer 允许你指定一个负向偏移距离，应用到指定的最初若干层。这一调整有效缩小了最初几层的宽度，有助于获得更准确的最终打印尺寸。

![elephant-foot-compensation](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/elephant-foot-compensation.png?raw=true)

## 精确墙

"精确墙"（Precise Wall）是 OrcaSlicer 引入的一项特色功能，通过略微增大外墙与内墙之间的间距，来提升打印件的尺寸精度并减少层间不一致。

### 技术解释

首先，需要理解流量、挤出宽度与空间等基本概念。
Slic3r 有一份详细讲解这些内容的优秀文档，可参阅[这篇文章](https://manual.slic3r.org/advanced/flow-math)。

Slic3r 及其分支（如 PrusaSlicer、SuperSlicer 和 OrcaSlicer）假设挤出路径呈椭圆形，并以此考虑重叠部分。例如，若墙宽设为 0.4mm、层高为 0.2mm，由于重叠，两条并排铺设的墙的总厚度是 0.714mm 而不是 0.8mm。

- **精确墙关闭**

  ![PreciseWallOff](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/PreciseWallOff.svg?raw=true)

- **精确墙开启**

  ![PreciseWallOn](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/PreciseWallOn.svg?raw=true)

这种处理方式增强了 3D 打印件的强度，但也有副作用。例如，使用内-外墙顺序时，外墙可能被向外推出，导致尺寸不准和更明显的层间不一致。

需要记住，这种流量处理方式是 Slic3r 及其分支特有的。其他切片软件（如 Cura）假设挤出路径是矩形的，因此不包含重叠。在 Cura 中，两条 0.4mm 的墙会得到 0.8mm 的外壳厚度。

OrcaSlicer 沿用了 Slic3r 的流量处理方式。为解决上述缺点，OrcaSlicer 引入了"精确墙"功能。启用该功能后，外墙与其相邻内墙之间的重叠被设为零。这确保打印件整体强度不受影响，同时尺寸精度和层间一致性得到改善。

## 精确 Z 高度

即使模型高度不是[层高](quality_settings_layer_height)的整数倍，此功能也能确保切片后模型的 Z 高度准确。

例如，用 0.2mm 层高切一个 20mm × 20mm × 20.1mm 的立方体，通常由于按层高递增，最终高度会是 20.2mm。

启用此参数后，最后五层的层高会被调整，使最终切片高度与实际物体高度一致，得到准确的 20.1mm（如图所示）。

- **精确 Z 高度关闭**

  ![PreciseZOff](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/PreciseZOff.png?raw=true)

- **精确 Z 高度开启**

  ![PreciseZOn](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/PreciseZOn.png?raw=true)

## 多边形孔

多边形孔（Polyhole）是 FFF 3D 打印中用于提高圆孔精度的一种技术。它不把孔建模为完美的圆，而是用边数较少的多边形来表示孔。这种简化迫使切片器把每段当作直线处理，打印更可靠。通过精心选择边数并确保多边形位于孔的外边界上，可以得到更接近预期直径的孔。

![PolyHoles](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Precision/PolyHoles.png?raw=true)

- 原始实现：[SuperSlicer Polyholes](https://github.com/supermerill/SuperSlicer/wiki/Polyholes)
- 思想与数学：[Hydraraptor](https://hydraraptor.blogspot.com/2011/02/polyholes.html)
