# 输入整形（Input Shaping）

在高速移动过程中，振动会引起称为"振纹（ringing）"的现象，即打印表面出现周期性波纹。输入整形通过抵消这些振动提供有效的解决方案，可提高打印质量并减少部件磨损，而无需大幅降低打印速度。

- [Klipper](#klipper)
  - [共振补偿](#共振补偿)
- [Marlin](#marlin)
  - [ZV 输入整形](#zv-输入整形)
  - [固定时间运动](#固定时间运动)
- [致谢](#致谢)

## Klipper

### 共振补偿

Klipper 共振补偿是一组可用于减少振纹、提高打印质量的输入整形模式。
对于 Delta 打印机，通常推荐的模式是 `MZV` 或 `EI`。

1. 前置条件：
   1. 在 OrcaSlicer 中设置：
      1. 高到足以触发振纹的加速度（例如 2000 mm/s²）。
      2. 高到足以触发振纹的速度（例如 100 mm/s）。

> [!NOTE]
> 这些设置取决于你打印机的运动能力和耗材的最大体积流量。如果无法达到引起振纹的速度，请尝试提高耗材的最大体积流量（避免低于 10 mm³/s 的材料）。
      3. Jerk [Klipper Square Corner Velocity](https://www.klipper3d.org/Kinematics.html?h=square+corner+velocity#look-ahead) 设为 5 或较高值（例如 20）。

   2. 在打印机设置中：
      1. 将整形器类型设为 `MZV` 或 `EI`。
         ```gcode
         SET_INPUT_SHAPER SHAPER_TYPE=MZV
         ```
      2. 禁用 [Minimum Cruise Ratio](https://www.klipper3d.org/Kinematics.html#minimum-cruise-ratio)：
         ```gcode
         SET_VELOCITY_LIMIT MINIMUM_CRUISE_RATIO=0
         ```
   3. 使用不透明、高光泽的耗材，使振纹更明显。
2. 打印一定频率范围的输入整形频率测试。

   ![IS_freq_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_freq_menu.png?raw=true)

   1. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的频率。

   ![IS_damp_klipper_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_klipper_print_measure.jpg?raw=true)
   ![IS_freq_klipper_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_freq_klipper_slicer_measure.png?raw=true)

   2. 如果结果不清晰，可以测量 X 和 Y 各自可接受的最小和最大高度，并以该最小值和最大值重复测试。

> [!WARNING]
> 有可能需要设置高于 60Hz 的频率。一些机架非常坚固、机械性能极佳的打印机可能表现出超过 100Hz 的频率。

3. 将 X 和 Y 频率设为上一步找到的值，打印阻尼（Damping）测试。

   ![IS_damp_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_menu.png?raw=true)

   1. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的阻尼。

   ![IS_damp_klipper_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_klipper_print_measure.jpg?raw=true)
   ![IS_damp_klipper_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_klipper_slicer_measure.png?raw=true)

> [!IMPORTANT]
> 并非所有共振补偿模式都支持阻尼。

4. 恢复你的 3D 打印机设置，避免一直使用高加速度和 jerk 值。
5. 保存设置
   1. 你需要进入打印机设置，将 X 和 Y 的频率和阻尼设为上一步找到的值。

## Marlin

### ZV 输入整形

ZV 输入整形向 X、Y 轴的步进运动中引入抗振信号。它将步数分成两半来实现：前一半以一半的频率运行，后一半作为"回声"，延迟振纹周期的一半。这种简单的方法能有效减少振动、提高打印质量并允许更高的速度。

1. 前置条件：
   1. 在 OrcaSlicer 中设置：
      1. 高到足以触发振纹的加速度（例如 2000 mm/s²）。
      2. 高到足以触发振纹的速度（例如 100 mm/s）。

> [!NOTE]
> 这些设置取决于你打印机的运动能力和耗材的最大体积流量。如果无法达到引起振纹的速度，请尝试提高耗材的最大体积流量（避免低于 10 mm³/s 的材料）。

      3. Jerk
         1. 如果使用[经典 Jerk](https://marlinfw.org/docs/configuration/configuration.html#jerk-)，使用较高值（例如 20）。
         2. 如果使用[结合点偏差](https://marlinfw.org/docs/features/junction_deviation.html)（新版 Marlin 默认模式），本测试将使用 0.25（对大多数打印机足够高）。
   2. 使用不透明、高光泽的耗材，使振纹更明显。
2. 打印一定频率范围的输入整形频率测试。

   ![IS_freq_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_freq_menu.png?raw=true)

   1. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的频率。

   ![IS_freq_marlin_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_freq_marlin_print_measure.jpg?raw=true)
   ![IS_freq_marlin_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_freq_marlin_slicer_measure.png?raw=true)

   2. 如果结果不清晰，可以测量 X 和 Y 各自可接受的最小和最大高度，并以该最小值和最大值重复测试。

> [!WARNING]
> 有可能需要设置高于 60Hz 的频率。一些机架非常坚固、机械性能极佳的打印机可能表现出超过 100Hz 的频率。

3. 将 X 和 Y 频率设为上一步找到的值，打印阻尼测试。

   ![IS_damp_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_menu.png?raw=true)

   1. 测量 X 和 Y 方向的高度，并在 OrcaSlicer 中读取该处设定的阻尼。

   ![IS_damp_marlin_print_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_marlin_print_measure.jpg?raw=true)
   ![IS_damp_marlin_slicer_measure](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_marlin_slicer_measure.png?raw=true)

4. 恢复你的 3D 打印机设置，避免一直使用高加速度和 jerk 值。
   1. 重启打印机。
   2. 使用以下 G-code 恢复打印机设置：
   ```gcode
   M501
   ```
5. 保存设置
   1. 你需要进入打印机设置，将 X 和 Y 的频率和阻尼设为上一步找到的值。
   2. 使用以下 G-code 设置频率：
   ```gcode
   M593 X F#Xfrequency D#XDamping
   M593 Y F#Yfrequency D#YDamping
   M500
   ```
   示例
   ```gcode
   M593 X F37.25 D0.16
   M593 Y F37.5 D0.06
   M500
   ```

### 固定时间运动

TODO：此校准测试正在开发中。更多信息见 [Marlin 文档](https://marlinfw.org/docs/gcode/M493.html)。

## 致谢

- **输入整形校准：** [@IanAlexis](https://github.com/IanAlexis) 和 [@RF47](https://github.com/RF47)
- **Klipper 测试：** [@ShaneDelmore](https://github.com/ShaneDelmore)
