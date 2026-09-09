# 压力推进（Pressure Advance）

压力推进是一项在加减速过程中补偿喷嘴内耗材压力滞后的功能。它有助于减少垂料、渗料和挤出不一致等问题，尤其在拐角或快速移动时，从而提高打印质量。

OrcaSlicer 提供三种校准压力推进值的方法，各有利弊。请注意每种方法都有两个版本：直驱挤出机版和远端（Bowden）挤出机版。请务必为你的测试选择正确的版本。

> [!WARNING]
> **Marlin 打印机：** 固件必须启用线性提前（Linear Advance，M900）。
> **并非所有打印机都默认启用。**

> [!WARNING]
> **Bambulab 打印机：** 请确保不要勾选"Flow calibration"选项。
> ![flowrate-Bambulab-uncheck](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowrate-Bambulab-uncheck.png?raw=true)

- [校准](#校准)
  - [塔方法](#塔方法)
  - [图案方法](#图案方法)
  - [线方法](#线方法)

## 校准

你可以使用不同的方法校准压力推进值，各有利弊。

这些方法得到的结果应保存到材料配置中。
![pressure_advance_enable](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pressure_advance_enable.png?raw=true)

> [!TIP]
> 考虑使用[自适应压力推进](adaptive-pressure-advance-calib)方法以获得更精确的结果，
> 尤其是高速打印机。

### 塔方法

塔方法耗时可能稍长，但不依赖首层质量。

1. 选择用于测试的打印机、耗材和工艺。
2. 检查打印件的每个拐角，标出整体效果最好的高度。
3. 本例选择了 8mm 的高度，因此压力推进值应按 `压力推进起始值 + (压力推进步长 x 测量值)` 计算；示例：`0 + (0.002 x 8) = 0.016`。
   ![pa-tower](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-tower.jpg?raw=true)
   ![pa-tower-measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-tower-measure.jpg?raw=true)

> [!TIP]
> @ItsDeidara 制作了一个 HTML 工具帮助计算。如果公式让你头疼，可以看[这里](https://github.com/ItsDeidara/Orca-Slicer-Assistant)。

### 图案方法

图案方法改编自 [Andrew Ellis 的图案方法生成器](https://ellis3dp.com/Pressure_Linear_Advance_Tool/)，后者源自 [Sineos](https://github.com/Sineos/k-factorjs) 开发的 [Marlin 图案方法](https://marlinfw.org/tools/lin_advance/k-factor.html)。

[使用和读取图案方法的说明](https://ellis3dp.com/Print-Tuning-Guide/articles/pressure_linear_advance/pattern_method.html)见 [Ellis 的打印调优指南](https://ellis3dp.com/Print-Tuning-Guide/)，仅需注意与 OrcaSlicer 的少数差异。

测试配置窗口允许用户在单个项目中生成一个或多个测试。多个测试会被放置在打印板上，必要时会自动增加额外的板。

1. 单个测试 \
   ![pa-pattern-single](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-pattern-single.png?raw=true)
2. 批量模式测试（单板多个测试）\
   ![pa-pattern-batch](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-pattern-batch.png?raw=true)

生成测试后，板上会放置一个或多个小长方体棱柱，每个测试用例对应一个。棱柱对象有几个用途：

1. 测试图案本身作为自定义 G-code 插入到每一层，与你手动操作相同。长方体棱柱提供了插入该 G-code 所需的层。这也意味着**当你切换到预览面板时将看到完整的测试图案：**

![pa-pattern-batch-plater](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-pattern-batch-plater.png?raw=true)

2. 棱柱充当把手，通过移动棱柱可以把测试图案移动到板上的任意位置。
3. 每个测试对象都预配置了目标参数并反映在对象名称中。可通过对象列表面板逐个调整每个棱柱的测试参数：

![pa-pattern-batch-objects](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-pattern-batch-objects.png?raw=true)

接下来，Ellis 的生成器允许调整特定的打印机、耗材和打印配置设置。你可以在 OrcaSlicer 中通过调整“准备”面板中的设置来做出同样的更改，与其他打印一样。启动校准测试时会应用 Ellis 的默认设置。关于这些设置，有几点需要注意：

1. Ellis 将线宽指定为耗材直径的百分比。Orca 图案方法也是如此，以提供其建议的默认值，即将 Ellis 的百分比与你指定的喷嘴直径结合使用。
2. 线宽方面，图案仅使用 `Default` 和 `First layer` 线宽。
3. 速度方面，图案仅使用 `First layer speed -> First layer` 和 `Other layers speed -> Outer wall` 速度。
4. 数字下方的填充图案无法更改，因为它并不是从设置中读取的填充图案。图案 G-code 全部是自定义编写的，因此那个“填充”实际上是手绘的，没有经过能让 Orca 将其识别为填充的常规流程。

### 线方法

线方法测试快捷、简单。但其精度在很大程度上取决于首层质量。建议为该测试开启热床网格调平。

步骤：

1. 选择用于测试的打印机、耗材和工艺。
2. 打印项目并检查结果。选择对应最均匀线条的值，并在耗材设置中更新你的压力推进值。
3. 本测试中，压力推进值 `0.016` 似乎最佳。

   ![pa-line](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-line.gif?raw=true)

   ![pa-lines](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-lines.png?raw=true)

   ![pa-line-0-016](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-line-0-016.png?raw=true)
