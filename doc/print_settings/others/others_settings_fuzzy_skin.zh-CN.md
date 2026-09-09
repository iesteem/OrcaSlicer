# 模糊表皮（Fuzzy Skin）

模糊表皮通过随机扰动墙的路径，在模型表面产生刻意为之的粗糙哑光外观。
这些设置控制效果的应用位置、噪声的生成方式以及位移或挤出调制的强度。

它适合制作纹理或遮盖表面瑕疵，但会增加打印时间并影响尺寸精度。

- [模糊表皮模式](#模糊表皮模式)
  - [轮廓](#轮廓)
  - [轮廓和孔](#轮廓和孔)
  - [所有墙](#所有墙)
  - [模糊表皮生成器模式](#模糊表皮生成器模式)
  - [位移（Displacement）](#位移displacement)
  - [挤出（Extrusion）](#挤出extrusion)
  - [组合（Combined）](#组合combined)
- [噪声类型](#噪声类型)
  - [经典（Classic）](#经典classic)
  - [柏林（Perlin）](#柏林perlin)
  - [波状（Billow）](#波状billow)
  - [脊状多重分形（Ridged Multifractal）](#脊状多重分形ridged-multifractal)
  - [Voronoi](#voronoi)
- [点距离](#点距离)
- [表皮厚度](#表皮厚度)
- [表皮特征尺寸](#表皮特征尺寸)
- [表皮噪声倍频数](#表皮噪声倍频数)
- [表皮噪声持续度](#表皮噪声持续度)
- [对首层应用模糊表皮](#对首层应用模糊表皮)
- [致谢](#致谢)

## 模糊表皮模式

选择模型的哪些部分应用模糊表皮效果。

### 轮廓

仅对模型最外轮廓（外轮廓线）应用模糊表皮。
适合在保持内表面光滑的同时制作纹理边缘。

### 轮廓和孔

对外轮廓和内部孔都应用模糊表皮。当你想让凹陷特征也呈现粗糙纹理时有用。

### 所有墙

对每一面墙（外部和内部）应用模糊表皮。这带来最强的整体纹理外观，但会显著增加切片和打印时间。

### 模糊表皮生成器模式

选择用于产生模糊效果的底层方法。每种模式在强度、速度和机械负载方面各有取舍。

### 位移（Displacement）

![Fuzzy-skin-Displacement-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Displacement-mode.png?raw=true)

经典方法是让打印头沿垂直于墙的方向偏移，从而在墙上形成图案。
它结果可预期，但会削弱整个壳体的强度并打开墙内部的孔隙。它还会增加打印机运动机构的机械应力，整体打印速度变慢。

### 挤出（Extrusion）

![Fuzzy-skin-Extrusion-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Extrusion-mode.png?raw=true)

模糊表皮效果通过打印头直线移动时改变挤出塑料量来获得。对运动机构没有额外负载，打印速度不降低，孔隙也不会打开，但图案清晰度约为经典模式的一半。它适合制作"疏松"的墙以降低挤出塑料的内应力，或遮盖侧墙上的打印缺陷——即哑光效果。

> [!CAUTION]
> "模糊表皮厚度"参数不能超过喷嘴直径的大约 70%-125%（需针对不同条件单独选择）！这是一个复杂条件，还与层高有关，决定了线条能挤到多细。
> 还应启用 [Arachne](quality_settings_wall_generator#arachne) 墙生成器模式。

### 组合（Combined）

![Fuzzy-skin-Combined-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Combined-mode.png?raw=true)

这是位移与挤出模式的组合。图案清晰度与经典模式相同，但墙依然坚固紧密。运动机构的负载降低一半。打印速度快于位移模式，但总耗时仍更长。

> [!WARNING]
> 线条粗细的限制与挤出模式相同。

## 噪声类型

选择用于生成随机偏移的噪声算法。不同噪声类型产生不同的视觉纹理。

### 经典（Classic）

简单的均匀随机噪声。产生粗糙、不规则的纹理。

![Fuzzy-skin-classic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-classic.png?raw=true)

### 柏林（Perlin）

[柏林噪声](https://en.wikipedia.org/wiki/Perlin_noise)生成平滑、自然且结构连贯的变化。

![Fuzzy-skin-perlin](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-perlin.png?raw=true)

### 波状（Billow）

波状噪声类似柏林噪声，但外观更成团。它能创建更显著的特征，常用于自然纹理。

![Fuzzy-skin-billow](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-billow.png?raw=true)

### 脊状多重分形（Ridged Multifractal）

创建锐利、参差的特征和高对比度细节。适合石头或大理石般的纹理。

![Fuzzy-skin-ridged-multifractal](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-ridged-multifractal.png?raw=true)

### Voronoi

[Voronoi 噪声](https://en.wikipedia.org/wiki/Worley_noise)将表面划分为 Voronoi 单元并独立位移每个单元，创建拼布状或细胞状纹理。

![Fuzzy-skin-voronoi](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-voronoi.png?raw=true)

## 点距离

每条线段上随机采样点之间的平均距离。
值越小细节越多但计算量越大；值越大纹理越粗、速度越快。

## 表皮厚度

点可位移的最大横向宽度（mm）。它定义了墙可被抖动的幅度。
请保持在此值不高于或接近外墙线宽，并在喷嘴/流量限制内，以获得可靠打印。

## 表皮特征尺寸

连贯噪声特征的基础尺寸（mm）。值越大结构越大越醒目；值越小纹理越细腻。

## 表皮噪声倍频数

使用的连贯噪声倍频数。值越高噪声细节越多，但计算时间也增加。

## 表皮噪声持续度

控制振幅在各倍频间的衰减。持续度越低噪声越平滑；越高则越强的细节被保留。

## 对首层应用模糊表皮

启用后对首层应用模糊表皮。

> [!CAUTION]
> 可能影响热床附着和表面接触。

## 致谢

- **生成器模式作者：** [@pi-squared-studio](https://github.com/pi-squared-studio)。
