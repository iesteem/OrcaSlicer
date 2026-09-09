# 裙边（Brim）

裙边是围绕模型底部打印的一层平面结构，用于提高与打印床的附着力。它对底面积小或易翘曲的模型很有用。

![brim](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/brim/brim.png?raw=true)

- [类型](#类型)
  - [自动](#自动)
  - [手动绘制](#手动绘制)
  - [外裙边](#外裙边)
  - [内裙边](#内裙边)
  - [内外裙边](#内外裙边)
  - [鼠耳（Mouse Ears）](#鼠耳mouse-ears)
    - [耳最大角度](#耳最大角度)
    - [耳检测半径](#耳检测半径)
- [宽度](#宽度)
- [裙边-对象间距](#裙边-对象间距)

## 类型

控制裙边在模型外侧和/或内侧如何生成。

### 自动

自动裙边功能通过评估材料属性、零件几何、打印速度和热特性来计算最佳裙边宽度。

- 模型几何
  - 使用模型的包围盒确定尺寸。
  - 高度面积比：`height/(width²*length)`。
- 打印速度
  - 最大打印速度越高，通常推荐的裙边宽度越大。
- 热长度
  - 定义为模型底面的对角线。
  - 参考热长度（按材料）：
    - ABS、PA-CF、PET-CF：100
    - PC：40
    - TPU：1000
- 材料附着系数
  - 默认：1
  - PETG/PCTG：2
  - TPU：0.5

计算出的裙边宽度上限为 20mm 和热长度的 1.5 倍。如果最终宽度小于 5mm 且小于热长度的 1.5 倍，则不生成裙边（宽度 = 0）。

### 手动绘制

仅在"准备"标签页中用 ![toolbar_brimears_dark](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/toolbar_brimears_dark.svg?raw=true) 绘制过的区域生成裙边。

### 外裙边

在模型外轮廓周围创建裙边。
比内裙边更容易去除，但如果去除不干净可能影响模型外观。

![brim-outer](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/brim/brim-outer.png?raw=true)

### 内裙边

在内轮廓周围创建裙边。
比外裙边更难去除、效果更差，且可能遮盖精细的内部细节，但可以隐藏去除裙边后的接痕。

![brim-inner](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/brim/brim-inner.png?raw=true)

### 内外裙边

在模型的外轮廓和内轮廓周围都创建裙边。
此方式结合了两种裙边类型的**缺点**——更难去除且可能遮盖精细细节，但能提高整体附着力。

![brim-outer-inner](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/brim/brim-outer-inner.png?raw=true)

> [!TIP]
>> 复杂模型/材料可以考虑使用[筏（raft）](support_settings_raft)。

### 鼠耳（Mouse Ears）

鼠耳是细小的局部裙边延伸（通常放置在角落和尖锐特征附近），在比完整裙边更省料的同时提高热床附着力并减少翘曲。
几何分析例程会根据配置的角度阈值和检测半径选择候选位置。

![brim-mouse-ears](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/brim/brim-mouse-ears.png?raw=true)

#### 耳最大角度

用于决定鼠耳可以放置在哪里的角度阈值（度）：

- 0° — 禁用；不生成鼠耳。
- 0° 到 180° 之间 — 在局部角度比阈值更尖锐（更小）的特征处生成耳。
- 180° — 几乎任何非直线的特征上都允许生成耳。

#### 耳检测半径

检测尖锐角度之前会对几何进行抽稀。
此参数表示抽稀的偏差最小长度。
设为 0 停用。

## 宽度

模型与最外裙边线之间的距离。
增大此值会加宽裙边，可以提高附着力但会增加材料消耗。

## 裙边-对象间距

最内裙边线与对象之间的间隙。
增大间隙使裙边更容易去除但降低其附着效果；过大的间隙可能使裙边完全接触不到对象，失去裙边的意义。
