# 抖动 Jerk XY

**Jerk（抖动）**是加速度的变化率，即打印机在不同加速度之间切换的速度。它控制移动过程中的方向变化和速度过渡。

## 拐角控制类型

- **Jerk**：传统方法，为方向变化设定最大速度。
- **[Junction Deviation（结合点偏差）](#junction-deviation)**：现代方法，基于加速度和速度计算拐角速度。

## 主要影响

- **拐角控制**：值越低 = 拐角越平滑、质量越好。值越高 = 过弯越快，但可能出现伪影
- **打印速度**：更高的 jerk 减少方向变化处的减速，提升整体速度
- **表面质量**：更低的 jerk 减少振动和振纹，对外墙尤其重要

当不同运动类型需要特定设置时，此设置会覆盖固件的 jerk 值。Orca 会限制 jerk 不超过打印机"运动能力"设置。

> [!TIP]
> Jerk 可与[压力推进（PA）](pressure-advance-calib)、[自适应压力推进](adaptive-pressure-advance-calib)和[输入整形](input-shaping-calib)配合使用，以优化打印质量和速度。
> 建议按照[校准指南](calibration)的顺序进行以获得最佳效果。

- [拐角控制类型](#拐角控制类型)
- [主要影响](#主要影响)
- [默认](#默认)
  - [外墙](#外墙)
  - [内墙](#内墙)
  - [填充](#填充)
  - [顶面](#顶面)
  - [首层](#首层)
  - [空驶](#空驶)
- [Junction Deviation](#junction-deviation)
- [有用链接](#有用链接)

## 默认

默认 Jerk 值。

> [!NOTE]
> 若此值设为 0，将使用打印机的默认 jerk。

### 外墙

外墙打印的 jerk。通常设为低于正常打印的值以确保更好的质量。

### 内墙

内墙打印的 jerk。通常设为高于外墙但仍合理的值以提升速度。

### 填充

填充打印的 jerk。通常设为高于内墙打印的值以提升速度。

### 顶面

顶面打印的 jerk。通常设为低于填充的值以确保更好的质量。

### 首层

首层打印的 jerk。通常设为低于顶面的值以改善附着。

### 空驶

空驶移动的 jerk。通常设为高于填充的值以缩短空驶时间。

## Junction Deviation

Jerk 的替代方案，Junction Deviation 是 MarlinFW（Marlin2）打印机控制拐角速度的默认方法。
值越高，过弯速度越激进；值越低，拐角越平滑、越受控。

只有当此值低于打印机设置 > 运动能力中设定的 Junction Deviation 值时才会被**覆盖生效**。若高于它，则使用运动能力中配置的值。

计算你的 Junction Deviation 值，请参阅[Junction Deviation 校准指南](cornering-calib#junction-deviation)。

```math
JD = 0,4 \cdot \frac{\text{Jerk}^2}{\text{Accel.}}
```

## 有用链接

- [Klipper Kinematics](https://www.klipper3d.org/Kinematics.html?h=accelerat#acceleration)
- [Marlin Junction Deviation](https://marlinfw.org/docs/configuration/configuration.html#junction-deviation-)
- [JD Explained and Visualized, by Paul Wanamaker](https://reprap.org/forum/read.php?1,739819)
- [Computing JD for Marlin Firmware](https://blog.kyneticcnc.com/2018/10/computing-junction-deviation-for-marlin.html)
- [Improving GRBL: Cornering Algorithm](https://onehossshay.wordpress.com/2011/09/24/improving_grbl_cornering_algorithm/)
- [Pressure Advance Calibration](pressure-advance-calib)
- [Adaptive Pressure Advance](adaptive-pressure-advance-calib)
