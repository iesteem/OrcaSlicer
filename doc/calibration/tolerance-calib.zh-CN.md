# 耗材公差校准

每种耗材与打印机的组合都会产生不同的公差。这意味着即使使用相同的耗材和打印配置，不同打印机之间的公差也可能各不相同。
为了修正这些差异，OrcaSlicer 提供：

- 耗材补偿：

  - 收缩（XY）

    ![FilamentShrinkageCompensation](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/FilamentShrinkageCompensation.png?raw=true)

- 工艺补偿：

  - X-Y 孔补偿
  - X-Y 轮廓补偿
  - 精确墙
  - 精确 Z 高度

    ![QualityPrecision](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/QualityPrecision.png?raw=true)

## 便捷模型

OrcaSlicer 内置若干便捷模型，帮助你测试和校准打印机。
在“准备”模式下右键点击打印板，选择“添加便捷模型（Add Handy Model）”即可使用这些模型。
![handy-models-list](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Handy-Models/handy-models-list.png?raw=true)

### Orca 公差测试

此校准测试用于评估打印机和耗材的尺寸精度。模型由一个带有六个六边形孔的底座组成，各孔公差不同：0.0 mm、0.05 mm、0.1 mm、0.2 mm、0.3 mm 和 0.4 mm，另附一个六边形测试块。

![tolerance_hole](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/tolerance_hole.svg?raw=true)

你可以使用 M6 内六角扳手或随附的打印六边形测试块来检查公差。
用卡尺测量孔和内部测试块。根据结果微调 X-Y 孔补偿和 X-Y 轮廓补偿设置，重复该过程直至达到期望精度。

![OrcaToleranceTes_m6](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/OrcaToleranceTes_m6.jpg?raw=true)
![OrcaToleranceTest_print](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Tolerance/OrcaToleranceTest_print.jpg?raw=true)
