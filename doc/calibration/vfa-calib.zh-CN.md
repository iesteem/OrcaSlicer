# VFA

垂直细纹（Vertical Fine Artifacts，VFA）是出现在垂直墙上的细小表面缺陷，尤其常见于锐角或方向突变处。这些缺陷通常由机械振动、电机共振或影响打印质量的快速方向变化引起。

- **机械调整**，如调校或更换电机、皮带、同步轮。
- **MMR（电机共振纹，Motor Resonance Rippling）** 是 VFA 的常见子类，由步进电机以共振频率振动导致表面出现周期性波纹。
- **[Jerk/结合点偏差](cornering-calib)** 设置也会导致 VFA，因为它们控制打印机如何处理快速的方向变化。
- **[输入整形](input-shaping-calib)** 可以通过减少打印过程中的振动来缓解 VFA。

## VFA 测试

OrcaSlicer 中的 VFA 速度测试帮助识别哪些打印速度会引发 MRR 纹路。它打印一座垂直塔，塔壁呈多种角度，同时逐步提高打印速度。

![vfa_test_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/vfa/vfa_test_menu.png?raw=true)

![vfa_test_print](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/vfa/vfa_test_print.jpg?raw=true)

打印后检查塔上的 MRR 纹路，寻找表面明显变光滑或变粗糙的速度点，从而定位有问题的速度区间。

之后可以在打印机配置中设置**共振规避速度区间**，跳过会引起可见纹路的速度。

![vfa_resonance_avoidance](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/vfa/vfa_resonance_avoidance.png?raw=true)
