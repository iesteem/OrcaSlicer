# 填充（Infill）

填充是 3D 打印件的内部结构，提供强度和支撑。可以调整它来平衡材料用量、打印时间和零件强度。

- [稀疏填充密度](#稀疏填充密度)
- [填充线倍增](#填充线倍增)
  - [使用场景](#使用场景)
- [方向与旋转](#方向与旋转)
  - [方向](#方向)
  - [旋转](#旋转)
- [填充墙重叠](#填充墙重叠)
- [应用缝隙填充](#应用缝隙填充)
- [过滤微小缝隙](#过滤微小缝隙)
- [锚定](#锚定)
- [内部实心填充](#内部实心填充)
- [额外实心填充](#额外实心填充)
  - [间隔模式](#间隔模式)
  - [显式层列表](#显式层列表)
- [稀疏填充图案](#稀疏填充图案)
- [致谢](#致谢)

## 稀疏填充密度

填充密度决定填充 3D 打印内部所用的材料量。通常以百分比表示，100% 为完全实心。

- 更高的密度会增加
  - 强度
  - 材料用量
  - 打印时间。

> [!NOTE]
> 密度通常以填充总体积的百分比计算，而非打印总体积。
> 不过，**并非所有图案对密度的解释都相同**，因此实际材料用量可能不同。
> 每种图案的材料用量见[图案章节](strength_settings_patterns)。

## 填充线倍增

此设置允许使用多条平行线生成所选的[填充图案](#稀疏填充图案)，同时保持设定的[填充密度](#稀疏填充密度)和整体材料用量。

![infill-multiline-1-5](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-multiline-1-5.gif?raw=true)

> [!NOTE]
> Orca 的做法与其他切片器不同——后者只是简单地把线数和材料用量翻倍，生成比预期更密的填充。
>
>| 填充密度 % | 填充线数 | Orca 密度 | 其他切片器密度 |
>|--------------------|--------------|--------------|-----------------------|
>| 10%                | 2            | 10%          | 20%                   |
>| 25%                | 2            | 25%          | 50%                   |
>| 40%                | 2            | 40%          | 80%                   |
>| 10%                | 3            | 10%          | 30%                   |
>| 25%                | 3            | 25%          | 75%                   |
>| 40%                | 3            | 40%          | 120% *                |
>| 10%                | 5            | 10%          | 50%                   |
>| 25%                | 5            | 25%          | 125% *                |
>| 40%                | 5            | 40%          | 200% *                |
>
> *其他切片器可能将结果限制为 100%。

### 使用场景

- 增加线数（如 2 或 3）可以在不增加材料用量的情况下**提高零件强度**和**打印速度**。
- **阻燃应用：** 某些阻燃材料（如 PolyMax PC-FR）要求最小打印墙/填充厚度——通常 1.5–3mm——才能符合标准。由于填充贡献零件整体厚度，使用多线有助于达到所需厚度，而无需换大喷嘴或 100% 填充。这对 PC 等易在全实心时翘曲的高温材料尤其有用。
- 用多种线宽创建**外观**填充图案（如[网格](strength_settings_patterns#grid)或[蜂窝](strength_settings_patterns#honeycomb)）——无需依赖 CAD 建模，也不局限于单一挤出宽度。

![infill-multiline-aesthetic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-multiline-aesthetic.gif?raw=true)

> [!WARNING]
> 对于自相交填充（如[立方](strength_settings_patterns#cubic)、[网格](strength_settings_patterns#grid)），线数大于 3 可能因交点处线重叠导致层错位、挤出机堵料或其他问题。
>
> ![infill-multiline-overlapping](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/infill-multiline-overlapping.gif?raw=true)

## 方向与旋转

> [!TIP]
> 你可以使用[填充旋转模板元语言](strength_settings_infill_rotation_template_metalanguage)创建更复杂的图案。

### 方向

控制填充线的方向，以优化或强化打印。

![fill-direction](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/fill-direction.png?raw=true)

### 旋转

此参数按照指定模板为每层的稀疏填充方向添加旋转。
模板是以逗号分隔的角度（度）列表。

例如：

```c++
0,90
```

![fill-rotation](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/fill-rotation.png?raw=true)

第一层使用 0°，第二层使用 90°，后续层循环重复此模式。

其他例子：

```c++
0,45,90
```

```c++
0,60,120,180
```

> [!NOTE]
> 若层数多于角度数，序列将循环重复。

> [!IMPORTANT]
> 并非所有稀疏[图案](strength_settings_patterns)都支持旋转。

## 填充墙重叠

填充区域略微扩大以与墙重叠，获得更好的结合。百分比值相对稀疏填充的线宽。将此值设为约 10-15%，可尽量减少可能导致表面粗糙的过挤出和材料堆积。

- **填充墙重叠关闭**

![InfillWallOverlapOff](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/InfillWallOverlapOff.svg?raw=true)

- **填充墙重叠开启**

![InfillWallOverlapOn](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/InfillWallOverlapOn.svg?raw=true)

## 应用缝隙填充

为选定的实心表面启用缝隙填充。
将被填充的最小缝隙长度可通过"过滤微小缝隙"选项控制。

1. **所有位置：** 对顶面、底面和内部实心表面应用缝隙填充，以获得最大强度。
2. **顶面和底面：** 仅对顶面和底面应用缝隙填充，平衡打印速度，减少实心填充中潜在的过挤出，并确保顶底表面没有针孔。
3. **无：** 对所有实心填充区域禁用缝隙填充。

注意，若使用[经典轮廓生成器](quality_settings_wall_generator#classic)，当轮廓之间放不下整条线宽时，也可能在轮廓之间生成缝隙填充。
该轮廓缝隙填充不受此设置控制。

若你希望移除所有缝隙填充（包括经典轮廓生成的），可将"过滤微小缝隙"设为一个很大的数，如 999999。

但并不建议这样做，因为轮廓之间的缝隙填充有助于模型强度。若轮廓之间生成了过多缝隙填充，更好的选择是切换到 [Arachne 墙生成器](quality_settings_wall_generator#arachne)，并用此选项控制是否生成外观性的顶底表面缝隙填充。

## 过滤微小缝隙

不打印长度小于指定阈值（mm）的缝隙填充。
此设置适用于顶面、底面和实心填充，以及（若使用[经典轮廓生成器](quality_settings_wall_generator#classic)）墙缝隙填充。

## 锚定

用一小段附加轮廓将填充线连接到内部轮廓。若以百分比表示（如 15%），基于填充挤出宽度计算。
OrcaSlicer 会尝试将两条相近的填充线连接到一小段轮廓上。若找不到短于此参数的轮廓段，填充线将只连接到一侧的轮廓段，且所取轮廓段长度受 infill_anchor 限制，但不超过此参数。若设为 0，将使用旧的填充连接算法，效果与 1000 & 0 相同。

- **锚定关闭**

![InfillAnchorOff](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/InfillAnchorOff.png?raw=true)

- **锚定开启**

![InfillAnchorOn](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/InfillAnchorOn.png?raw=true)
## 内部实心填充

内部实心填充的线图案。若启用了[检测窄内部实心填充](strength_settings_advanced#detect-narrow-internal-solid-infill)，小面积区域将使用[同心圆图案](strength_settings_patterns#concentric)。


## 额外实心填充

在特定层插入额外实心填充，为打印的关键部位增加强度。此功能允许你策略性地加固零件，而无需改变整体稀疏填充密度。

![extra-solid-infill](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/extra-solid-infill.gif?raw=true)

该模式支持两种格式：

### 间隔模式
- **简单间隔**： `N` - 每 N 层插入 1 个实心层，等于 `N#1`
- **多层**： `N#K` - 每 N 层插入 K 个连续实心层
- **可选 K**： `N#` - `N#1` 的简写

示例：
```
5 or 5#1    # 每 5 层插入 1 个实心层
5#          # 同 5#1
10#2        # 每 10 层插入 2 个连续实心层
```

### 显式层列表
使用逗号分隔的值指定确切层号（从 1 开始）。每项可以是单个层 `N`，或范围 `N#K` 表示从第 N 层开始插入 K 个连续实心层：

```
1,7,9       # 在第 1、7、9 层插入实心层
5,15,25     # 在第 5、15、25 层插入实心层
5,9#2,18    # 在第 5 层；第 9、10 层（因为 #2）；以及第 18 层插入
```

> [!NOTE]
> - 层号从 1 开始（第一层为第 1 层）
> - `#K` 在间隔和显式列表中都是可选的（`N#` 等于 `N#1`）
> - 实心层是在正常稀疏填充图案之外额外插入的

> [!TIP]
> 使用此功能可以：
> - 在应力集中点增加强度
> - 加固安装孔或连接点
> - 为功能件创建内部结构
> - 为高件添加周期性加固
> - 通过在显式列表前加 0（层号从 1 开始，0 会被忽略）在特定高度插入单个实心层。示例：`0,15` 仅在第 15 层插入实心层。

> [!WARNING]
> 包含实心填充的层可能比周围层耗时明显更长。这种时间差可能导致类似 Z 环纹的凸起。若发现伪影，请考虑调整冷却或速度。

## 稀疏填充图案

> [!TIP]
> 请查看[填充图案 Wiki 列表](strength_settings_patterns)，含**详细规格**及各自优缺点。

## 致谢

- **[填充线倍增](#填充线倍增)实现** - [@RF47](https://github.com/RF47)
- **Wiki 页面：** [IanAlexis](https://github.com/IanAlexis)。
