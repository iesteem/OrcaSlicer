# 首层速度（Initial layer speed）

首层打印速度低于其余层是被广泛推荐的做法。这有助于确保与热床的牢固附着，降低边缘翘曲或卷曲的概率，并更好地补偿轻微的调平误差。

## 首层

此设置决定第一层的打印速度，不含[实心填充](strength_settings_top_bottom_shells)区域。它适用于[外/内墙](strength_settings_walls)，以及[底层层数](strength_settings_top_bottom_shells#shell-layers)设为 0 时的[稀疏填充](strength_settings_infill)。
调整此速度有助于确保首层的附着和打印质量。

## 首层填充

专门用于第一层[实心填充](strength_settings_top_bottom_shells#shell-layers)区域的速度。这些区域需要更精确、更一致的挤出，为后续层创建平整稳定的表面。此部分打印过快可能导致内应力过高（翘曲风险增大）、层不均匀或附着失败。

## 首层空驶速度

设置第一层的空驶（非打印移动）速度。这不影响打印质量，可设为[空驶速度](speed_settings_travel)的百分比。
通常设为[空驶速度](speed_settings_travel)的 100%，但若想减少振动，或打印机在高速空驶移动时有问题，可以降低此值。

## 慢速层数

指定最初多少层以降低的速度打印。速度不会在第一层后直接跳到全速，而是在这个层数内线性逐步提升。这种渐进提速有助于保持附着，并在打印早期阶段给予更高的稳定性，尤其对接触面积小的打印件或易翘曲材料。

![number-of-slow-layers](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/speed/number-of-slow-layers.png?raw=true)
