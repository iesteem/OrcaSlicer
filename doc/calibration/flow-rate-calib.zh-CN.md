
# 流量（Flow Rate）校准

流量比例（flow ratio）决定挤出的耗材量，对高质量打印至关重要。
校准正确的流量比例可确保一致的层间附着和精确的尺寸。

- 流量比例**过低**会导致欠挤出，造成间隙、层间脆弱和结构强度差。
- 流量比例**过高**会导致过挤出，造成材料多余、表面粗糙和尺寸不准。

- [校准类型](#校准类型)
  - [OrcaSlicer \> 2.3.0 阿基米德弦 + YOLO（推荐）](#orcaslicer--230-阿基米德弦--yolo推荐)
  - [OrcaSlicer \<= 2.3.0 单调线 + 两遍校准](#orcaslicer--230-单调线--两遍校准)
- [致谢](#致谢)

> [!WARNING]
> **BambuLab 打印机：** 请确保**不要**勾选“Flow calibration（流量校准）”选项。
> ![flowrate-Bambulab-uncheck](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowrate-Bambulab-uncheck.png?raw=true)

> [!NOTE]
> v2.3.0 之后，[顶面图案](strength_settings_top_bottom_shells#surface-pattern)从[单调线](strength_settings_patterns#monotonic-line)改为[阿基米德弦](strength_settings_patterns#archimedean-chords)。

## 校准类型

- **YOLO：** 简化方法，使用公式 `旧流量比例 ± 修正值` 在一遍内调整流量。
  - **推荐（Recommended）：** 校准范围 `[-0.05, +0.05]`，流量步长 `0.01`。
  - **完美主义（Perfectionist）：** 校准范围 `[-0.04, +0.035]`，流量步长 `0.005`。
- **两遍校准（2-Pass Calibration）：** 传统方法，使用公式 `旧流量比例 * (100 + 修正值) / 100` 分两遍确定最佳流量。

### OrcaSlicer > 2.3.0 阿基米德弦 + YOLO（推荐）

此方法使用[阿基米德弦](strength_settings_patterns#archimedean-chords)图案，配合 YOLO（推荐）方式进行流量校准。

1. 选择要校准的打印机和耗材。
   此方法基于耗材当前的流量比例，因此请确保先选择了正确的耗材。
2. 在 `Calibration` 菜单的 `Flow Rate` 部分，选择 `YOLO (Recommended)`。
3. 将创建一个包含十一个方块的新项目，每个方块有不同的流量修正值。切片并打印该项目。
   ![flowcalibration-yolo](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowcalibration-yolo.gif?raw=true)
4. 检查打印出的方块，找出表面质量最好的一个。观察要点：
   1. 最光滑的顶面。
   2. 图案弧线之间没有可见间隙。
   3. 内部螺旋与外部弧线之间的线条最少或不可见。
   ![flowcalibration-guide](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowcalibration-guide.png?raw=true)
   在这个例子中，流量修正值为 `+0.01` 的方块效果最好，尽管内部螺旋与外部弧线之间有可见线条；进一步降低流量会开始出现线间间隙。
   ![flowcalibration-example](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowcalibration-example.png?raw=true)
5. 按公式 `旧流量比例 ± 修正值` 更新耗材设置中的流量比例。
   如果你之前的流量比例是 `0.98`，选择了流量修正值为 `+0.01` 的方块，则新值为：`0.98 + 0.01 = 0.99`。
   **记住**保存耗材配置文件。
   ![flowcalibration_update_flowrate](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowcalibration_update_flowrate.png?raw=true)

> [!NOTE]
> 新的阿基米德弦图案使用特定的打印顺序，内部螺旋最后打印，以便你在结束时检查接触线上是否有材料堆积。

### OrcaSlicer <= 2.3.0 单调线 + 两遍校准

此示例使用单调线图案配合两遍校准方式。

![flow-calibration-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flow-calibration-monotonic.gif?raw=true)

1. 选择用于测试的打印机、耗材和工艺。
2. 在 `Calibration` 菜单的 `Flow Rate` 部分，选择 `Pass 1`。
3. 将创建一个包含九个方块的新项目，每个方块有不同的流量修正值。切片并打印该项目。
4. 检查各方块，确定哪个的顶面最光滑。
   ![flowrate-pass1-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flowrate-pass1-monotonic.jpg?raw=true)
   ![flowrate-0-5-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flowrate-0-5-monotonic.jpg?raw=true)
5. 按公式 `旧流量比例 * (100 + 修正值) / 100` 更新耗材设置中的流量比例。
   例如，你之前的流量比例是 `0.98`，选择了流量修正值为 `+5` 的方块，则新值为：`0.98 × (100 + 5) / 100 = 1.029`。
   **记住**保存耗材配置文件。
6. 执行 `Pass 2` 校准。过程与 `Pass 1` 类似，但会生成一个包含十个方块的新项目，其流量修正值范围为 `-9` 到 `0`。
7. 重复步骤 4 和 5。例如，你之前的流量比例是 `1.029`，选择了流量修正值为 `-6` 的方块，则新值为：`1.029 × (100 - 6) / 100 = 0.96726`。
   **记住**保存耗材配置文件。
   ![flowrate-pass2-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flowrate-pass2-monotonic.jpg?raw=true)
   ![flowrate-6-monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flowrate-6-monotonic.jpg?raw=true)
   ![flowcalibration_update_flowrate_monotonic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/monotonic-flow-rate/flowcalibration_update_flowrate_monotonic.png?raw=true)

> [!TIP]
> @ItsDeidara 制作了一个 HTML 工具帮助完成这些计算。如果公式让你头疼，可以试试：[Orca-Slicer-Assistant](https://github.com/ItsDeidara/Orca-Slicer-Assistant)。

## 致谢

- **[阿基米德弦创意](https://makerworld.com/es/models/189543-improved-flow-ratio-calibration-v3#profileId-209504)**：[Jimcorner](https://makerworld.com/es/@jimcorner)
