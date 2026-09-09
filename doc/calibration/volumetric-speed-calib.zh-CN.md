# 最大体积流量（FlowRate）校准

每个材料配置都包含一个**最大体积流量**设置，用于限制你的[打印速度](speed_settings_other_layers_speed)，以防止堵料、欠挤出或层间附着不良等问题。

该值取决于**材料**、**机器**、**喷嘴直径**甚至**挤出机配置**，因此针对你的具体打印机和每种使用的耗材进行校准非常重要。

> [!NOTE]
> 即使是同一类材料（如 PLA），**品牌**和**颜色**也会显著影响最大流量。

> [!TIP]
> 如果你打算提高速度或流量，建议**提高喷嘴温度**，最好偏向你耗材推荐范围的上限。可使用[温度塔校准](temp-calib#喷嘴温度塔)找到该范围。

## 校准概述

系统会提示你输入测试设置：起始体积流量、结束体积流量和步长。除非你已大致了解耗材的下限或上限，否则建议使用默认值（起始 5mm³/s、结束 20mm³/s、步长 0.5）。选择“OK”，切片打印板并发送到打印机。

打印完成后，记下层间开始失效、质量开始下降的位置。

> [!TIP]
> **表面光泽的变化**（亮面 vs 哑面）通常是材料降解或层间附着不良的直观信号。

![mvf_measurement_point](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/MVF/mvf_measurement_point.jpg?raw=true)

用卡尺或直尺测量缺陷开始出现前的模型**高度**。
![mvf_caliper_sample_mvf](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/MVF/mvf_caliper_sample_mvf.jpg?raw=true)

然后你可以：

- 使用以下公式

  ```math
  耗材最大体积流量 = 起始值 + (测量高度 * 步长)
  ```

  本例中（19mm），计算为：`5 + (19 * 0.5) = 14.5mm³/s`

- 在 OrcaSlicer 的“预览”标签页中，确保选中"flow"配色方案。滚动到你测量的层高处，点击滑块，即可指示该耗材的最大流量水平。
![mvf_gui_flow](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/MVF/mvf_gui_flow.png?raw=true)

确定最大体积流量后，即可在耗材设置中填入该值。这将确保打印机不会超过该耗材的最大流量。
![mvf_material_settings](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/MVF/mvf_material_settings.png?raw=true)

> [!NOTE]
> 此测试是理想情况，未考虑回抽或其他可能加剧堵料或欠挤出的设置。
> 你可能需要将流量降低 10%-20%（甚至更多）以确保打印质量/强度。
> **以高体积流量打印可能导致层间附着不良甚至喷嘴堵塞。**

> [!TIP]
> @ItsDeidara 制作了一个 HTML 工具帮助计算。如果公式让你头疼，可以看[这里](https://github.com/ItsDeidara/Orca-Slicer-Assistant)。
