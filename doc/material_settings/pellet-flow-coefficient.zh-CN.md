# 颗粒流量系数（Pellet Flow Coefficient）

打印体积约 1m³ 量级的大型打印机通常使用颗粒（pellets）进行打印。
整体技术与 FDM 打印非常相似。
它就是 FDM 打印，只是不用耗材线而用颗粒。

区别在于：耗材线有用于计算送入体积的 filament_diameter，而颗粒则有一个针对该特定颗粒经验测定的 flow_coefficient（流量系数）。

pellet_flow_coefficient 本质上是某种颗粒堆积密度的度量。
单个颗粒的形状、材料和密度决定了堆积密度，而对 3D 打印来说唯一重要的是：打印机的送料机构/齿轮每转一圈挤出多少该颗粒材料。你可以针对自己的打印机型经验性地推导出这一数值。

我们将 pellet_flow_coefficient 换算为 filament_diameter，这样一切都可以像现有流程一样工作，只需极小的调整。

```math
\text{filament\_diameter} = \sqrt{\frac{4 \times \text{pellet\_flow\_coefficient}}{\pi}}
```

开平方只是让 flow_coefficient 与体积的关系呈线性。

堆积密度越高 → 单圈挤出的材料越多 → pellet_flow_coefficient 越高 → 被视为使用了更大直径的耗材线。
切片的其他所有计算保持不变。
