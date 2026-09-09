# 拐角

拐角（Cornering）是 3D 打印中影响打印质量和精度的关键环节。它决定了打印机在移动过程中如何处理方向变化，尤其是在拐角和曲线处。正确的拐角设置可以减少振纹、重影和过冲等缺陷，带来更干净、更精确的打印。

## Jerk

TODO：Jerk 校准尚未实现。

## 结合点偏差（Junction Deviation）

结合点偏差是 **Marlin 固件（Marlin 2.x）** 控制拐角速度的默认方法。
数值越高拐角越激进，数值越低拐角越平滑、越受控。
Marlin 中的默认值通常为 `0.08mm`，对某些打印机来说可能过高并引起振纹。可以考虑降低该值以减少振纹，但不要设得过低，否则会导致拐角速度过慢。

```math
JD = 0.4 \cdot \frac{\text{Jerk}^2}{\text{Acceleration}}
```

1. 前置条件：
   1. 检查你的打印机是否启用了结合点偏差。在打印机高级设置中查找 `Junction deviation`。
   2. 在 OrcaSlicer 中设置：
      1. 高到足以触发振纹的加速度（例如 2000 mm/s²）。
      2. 高到足以触发振纹的速度（例如 100 mm/s）。
   3. 使用不透明、高光泽的耗材，使振纹更明显。
2. 你需要打印结合点偏差测试。
   ![jd_first_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_first_menu.png?raw=true)
   1. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的频率。
      ![jd_first_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_first_print_measure.jpg?raw=true)
      ![jd_first_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_first_slicer_measure.png?raw=true)
   2. 如示例所示，你可能需要低于 `0.08mm` 的值。为了找到更好的 JD 最大值，可在拐角开始失去锐度的位置附近设定最大值，打印新的校准塔。
   3. 以新的最大值打印第二个结合点偏差测试。
      ![jd_second_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_second_menu.png?raw=true)
   4. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的频率。
      ![jd_second_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_second_print_measure.jpg?raw=true)
      ![jd_second_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_second_slicer_measure.png?raw=true)
3. 保存设置
   1. 在 [打印机设置/运动能力/Jerk 限制] 中设置你的最大结合点偏差值。
      ![jd_printer_jerk_limitation](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/JunctionDeviation/jd_printer_jerk_limitation.png?raw=true)
   2. 使用以下 G-code 设置该值：

   ```gcode
   M205 J#JunctionDeviationValue
   M500
   ```

   示例

   ```gcode
   M205 J0.012
   M500
   ```

   3. 重新编译你的 MarlinFW
      1. 在 Configuration.h 中取消注释并设置：

      ```cpp
      #define JUNCTION_DEVIATION_MM 0.012  // (mm) Distance from real junction edge
      ```

      2. 确保经典 Jerk 已禁用（已注释）：

      ```cpp
      //#define CLASSIC_JERK
      ```

## 致谢

- **结合点偏差机器限制** [@RF47](https://github.com/RF47)
- **结合点偏差校准** [@IanAlexis](https://github.com/IanAlexis)
- **快速塔模型** [@RF47](https://github.com/RF47)
