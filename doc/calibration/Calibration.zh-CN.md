# 校准指南

本指南为 OrcaSlicer 的校准过程提供结构化的全面概述。

内容涵盖流量、压力推进、温度塔、回抽测试及高级校准技术等关键方面。每节都包含分步说明和图示，帮助你更好地理解并有效执行每项校准。

要使用校准功能，请在 OrcaSlicer 界面的 **Calibration** 区找到它们。

![calibration](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/calibration.png?raw=true)

> [!IMPORTANT]
> 完成校准流程后，记得新建一个项目以退出校准模式。

推荐的校准顺序如下：

1. **[温度](temp-calib)：** 先校准喷嘴和热床温度。这至关重要，因为温度影响耗材的黏度，进而影响耗材通过喷嘴的流动程度以及对打印床的附着力。

   <img alt="temp-tower" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Temp-calib/temp-tower.jpg?raw=true" height="200">

2. **[流量](flow-rate-calib)：** 校准流量以确保挤出正确的耗材量。这对获得精确的尺寸和良好的层间附着非常重要。

   <img alt="flowcalibration-example" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Flow-Rate/flowcalibration-example.png?raw=true" height="200">

3. **[压力推进](pressure-advance-calib)：** 校准压力推进设置，以提高打印质量并减少喷嘴内压力波动引起的缺陷。

   - **[自适应压力推进](adaptive-pressure-advance-calib)：** 一种高级校准技术，可针对不同打印速度和几何形状进一步优化压力推进设置。

      <img alt="pa-tower" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/pa/pa-tower.jpg?raw=true" height="200">

4. **[回抽](retraction-calib)：** 校准回抽设置以尽量减少拉丝并提高打印质量。建议在流量和压力推进校准之后进行，这样可确保打印机已处于最佳挤出状态。

   <img alt="retraction_test_print" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/retraction/retraction_test_print.jpg?raw=true" height="200">

5. **[最大体积流量](volumetric-speed-calib)：** 校准耗材的最大体积流量。这可确保打印机能够承载该耗材的流量而不出现欠挤出或过挤出等问题。

   <img alt="mvf_measurement_point" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/MVF/mvf_measurement_point.jpg?raw=true" height="200">

6. **[拐角](cornering-calib)：** 校准 Jerk/结合点偏差设置，以提高打印质量并减少锐角和方向变化引起的缺陷。

     <img alt="jd_second_print_measure" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_second_print_measure.jpg?raw=true" height="200">

7. **[输入整形](input-shaping-calib)：** 一种高级校准技术，通过补偿打印机的机械振动来减少振纹并提高打印质量。

   <img alt="IS_damp_marlin_print_measure" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_marlin_print_measure.jpg?raw=true" height="200">

8. **[VFA](vfa-calib)：** 提供 VFA 速度测试用于查找共振速度。

   <img alt="vfa_test_print" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/vfa/vfa_test_print.jpg?raw=true" height="200">

---

**[公差](tolerance-calib)：** 校准打印机的公差，确保其能精确复现被打印模型的尺寸。这对实现零件间的良好配合、确保最终打印件满足期望规格非常重要。

   <img alt="OrcaToleranceTes_m6" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/OrcaToleranceTes_m6.jpg?raw=true" height="200">

---

_致谢：_

- _流量测试和回抽测试的灵感来自 [SuperSlicer](https://github.com/supermerill/SuperSlicer)。_
- _PA 线方法 inspired by [K-factor Calibration Pattern](https://marlinfw.org/tools/lin_advance/k-factor.html)。_
- _PA 塔方法 inspired by [Klipper](https://www.klipper3d.org/Pressure_Advance.html)。_
- _温度塔模型改编自 [Smart compact temperature calibration tower](https://www.thingiverse.com/thing:2729076)。_
- _最大流量测试的灵感来自 Stefan（CNC Kitchen），测试所用模型是其 [Extrusion Test Structure](https://www.printables.com/model/342075-extrusion-test-structure) 的再创作。_
- _ZV 输入整形 inspired by [Marlin Input Shaping](https://marlinfw.org/docs/features/input_shaping.html) 与 [Ringing Tower 3D STL](https://marlinfw.org/assets/stl/ringing_tower.stl)。_
