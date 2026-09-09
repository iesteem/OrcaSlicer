# 填充旋转模板元语言（Infill rotation template metalanguage）

此元语言提供了一种定义 3D 打印中[图案](strength_settings_patterns)[方向与旋转](strength_settings_infill#direction-and-rotation)的方式。

- [基本指令](#基本指令)
  - [快速示例](#快速示例)
  - [角度定义](#角度定义)
  - [运行时指令](#运行时指令)
  - [连接符号](#连接符号)
  - [计数](#计数)
  - [长度修饰符](#长度修饰符)
- [指令与示例详解](#指令与示例详解)
  - [简单绝对指令](#简单绝对指令)
  - [相对指令](#相对指令)
  - [重复、调整与一次性指令](#重复调整与一次性指令)
  - [范围指令](#范围指令)
  - [固定层数指令](#固定层数指令)
- [复杂模板示例](#复杂模板示例)
- [致谢](#致谢)

## 基本指令

## 快速示例

- `0` - 固定 0° 方向（X 轴）
- `0, 90` - 每层在 0° 和 90° 之间交替
- `0, 15, 30` - 每层在 0°、15° 和 30° 之间交替
- `+90` - 每层旋转 90°（序列为 90°、180°、270°、0° ...）
- `+45` - 每层旋转 45°，分散性更高
- `+30/50%` - 在接下来 50% 模型高度内线性旋转 30°
- `+45/10#` - 在 10 个标准层的范围内线性旋转 45°
- `+15#10` - 保持相同角度 10 层，然后旋转 +15°；每 10 层重复
- `B!, +30` - 底壳层不参与旋转，之后每层旋转 30°
- `0, +30, +90` - 使用 0°、+30°、+90° 的重复序列

`[±]α[*ℤ or !][joint sign, or its combinations][-][ℕ, B or T][length modifier][* or !]` - **稀疏**填充的全长模板指令

`[±]α*` - 仅设置初始旋转角度

 

> [!NOTE]
> `[...]` - 方括号中的值为可选项

### 角度定义

`[±]α` - 设置填充旋转角度的指令（对于某高度范围的连接填充，此角度是有限的）：

- `α:β` - 将角度 α 的值设为完整 360° 旋转的百分比
- `+α` - 设置正的相对角度（逆时针）
- `-α` - 设置负的相对角度（顺时针）

### 运行时指令

`[*, *ℤ 或 !]` - 运行时指令：

- `*` - "哑"指令标记。用于设置初始角度，不执行其他动作
- `*ℤ` - 将指令重复 ℤ 次
- `!` - 一次性执行指令

 

### 连接符号

`[joint sign]` - 决定填充转向连接方式的符号：

- `/` - 填充线性位移。例如 `+22.5/50%`  
  ![linear-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/linear-joint.png?raw=true)
- `#` - 多层填充，在结束角度处垂直位移。例如 `+22.5#50%`  
  ![multiple-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/multiple-joint.png?raw=true)
- `#-` - 多层填充，在初始角度处垂直位移。例如 `+22.5#-50%`  
  ![multiple-joint-initial-angle](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/multiple-joint-initial-angle.png?raw=true)
- `|` - 多层填充，在中间角度处垂直位移。例如 `+22.5|50%`  
  ![multiple-joint-middle-angle](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/multiple-joint-middle-angle.png?raw=true)
- `N` - 正弦函数填充（垂直连接）。例如 `+22.5N50%`  
  ![v-sinus-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/v-sinus-joint.png?raw=true)
- `n` - 正弦函数填充（垂直连接，惰性）。例如 `+22.5n50%`  
  ![v-sinus-joint-lazy](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/v-sinus-joint-lazy.png?raw=true)
- `Z` - 正弦函数填充（水平连接）。例如 `+22.5Z50%`  
  ![z-h-sinus-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/z-h-sinus-joint.png?raw=true)
- `z` - 正弦函数填充（水平连接，惰性）。例如 `+22.5z50%`  
  ![h-sinus-joint-lazy](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/h-sinus-joint-lazy.png?raw=true)
- `L` - 四分之一圆填充（水平到垂直连接）。例如 `+22.5L50%`  
  ![vh-quarter-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/vh-quarter-joint.png?raw=true)
- `l` - 四分之一圆填充（垂直到水平连接）。例如 `+22.5l50%`  
  ![hv-quarter-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/hv-quarter-joint.png?raw=true)
- `U` - 平方函数填充。例如 `+22.5U50%`  
  ![squared-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/squared-joint.png?raw=true)
- `u-` - 平方函数填充（反向）。例如 `+22.5u-50%`  
  ![squared-joint-inverse](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/squared-joint-inverse.png?raw=true)
- `Q` - 三次函数填充。例如 `+22.5Q50%`  
  ![cubic-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/cubic-joint.png?raw=true)
- `q-` - 三次函数填充（反向）。例如 `+22.5q-50%`  
  ![cubic-joint-inverse](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/cubic-joint-inverse.png?raw=true)
- `$` - 反正弦方法填充。例如 `+22.5$50%`  
  ![arcsinus-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/arcsinus-joint.png?raw=true)
- `~` - 随机角度填充。例如 `+22.5~50%`  
  ![random-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/random-joint.png?raw=true)
- `^` - 伪随机角度填充。例如 `+22.5^50%`  
  ![pseudorandom-joint](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/pseudorandom-joint.png?raw=true)

### 计数

`[-]ℕ` - 计算转向发生的距离：

- `ℕ` - 按 ℕ 层计数。例如 `+22.5/50%`  
  ![infill-counting](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/infill-counting.png?raw=true)
- `-ℕ` - 表示连接形状将向上翻转。例如 `+22.5/-50%`  
  ![infill-counting-flipped](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/infill-counting-flipped.png?raw=true)
- `B` - 在等于 bottom_shell_layers 参数的接下来若干层上计数
- `T` - 在等于 top_shell_layers 参数的接下来若干层上计数

### 长度修饰符

`ℕ[length modifier]` - 指定转向发生的距离：

- `ℕmm` - 距离以毫米为单位
- `ℕcm` - 距离以厘米为单位
- `ℕm` - 距离以米为单位
- `ℕ'` - 距离以英尺为单位
- `ℕ"` - 距离以英寸为单位
- `ℕ#` - 距离以 ℕ 层标准高度为单位
- `ℕ%` - 距离以模型高度的百分比表示

## 指令与示例详解

每条指令由符号和数字的组合写成，以逗号或空格分隔。
对于更复杂的指令，会使用自动格式化使模板更易读。

> [!NOTE]
> 所有示例均基于 20x20x20mm 立方体模型、5% 密度直线填充、100 层、每层 0.2mm 厚。无墙和上下壳。初始角度为 0。

### 简单绝对指令

包括为每层简单定义角度。注意此角度的初始设置也受填充角度字段中的值影响。

- `0`、`15`、`45.5`、`256.5605`... - 仅按现有角度填充。初始方向从 X 轴开始，可接受的取值范围为 0 到 360  
  - `0` 以及 `+0`、`-0` 或空模板  
  ![0](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/0.png?raw=true)
  - `45`  
  ![45](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/45.png?raw=true)
  - `0, 30` - 每层在 0 度和 30 度方向之间简单交替。  
  ![0-30](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/0-30.png?raw=true)
- `0%`、`10%`、`25%`、`100%`... - 以 360 度完整旋转的相对比例确定填充角度。分别旋转 0、36、90、0 度。  
  - `25%` - 等价于 `90` 指令。  
  ![90](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/90.png?raw=true) 
- `30, 60, 90, 120, 150, 0` - 更复杂的指令，定义每层转 30 度。模板行结束后会重新从第一条指令读取，如此循环直到填满整个模型高度。

### 相对指令

- `+30` - 逆时针旋转的简短指令。等价于 `30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330, 0` 或 `30, 60, 90, 120, 150, 0` 指令。  
  ![+30](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+30.png?raw=true)
- `-30` - 同样的指令，但为顺时针旋转。等价于 `330, 300, 270, 240, 210, 180, 150, 120, 90, 60, 30, 0` 或 `330, 300, 270, 240, 210, 0` 指令。
- `+150` - 可指定不同的非整倍角度以获得更好的填充分散性 = `150, 300, 90, 240, 30, 180, 330, 120, 270, 60, 210, 0` ...
- `+45` - 等价于 `45, 90, 135, 180, 225, 270, 315, 0` 或 `45, 90, 135, 0` 指令。  
  ![+45](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+45.png?raw=true)
- `+90` - 等价于 `90, 180, 270, 0` 或 `90, 0` 指令。  
  ![+90](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+90.png?raw=true)
- `+15%` - 适合按十进制划分角度 = `54, 108, 162, 270, 324, 18, 72, 126, 180, 234, 288, 342, 36, 90, 144, 196, 252, 306, 0` ...
- `+30, +90` - 复杂指令，在这些位置设置每层的旋转 = `30, 120, 150, 240, 270, 0` ...  
  ![+30+90](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+30+90.png?raw=true)
- `0, +30, +90` - 复杂指令，在这些位置设置每层的旋转 = `0, 30, 120` ...  
  ![0+30+90](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/0+30+90.png?raw=true)

### 重复、调整与一次性指令

- `5, 10, +20` - 无修饰符的简单指令。按序列设置角度 = 5, 10, 30, 5, 10, 30, 5, 10 ...
- `5, 10*, +20` - `*` 符号设置初始角度，不带数量标记、不处理层。适用于设置一组层的方向，下文会说明。按序列设置角度 = 5, 30, 5, 30, 5, 30, 5 ...
- `5, 10!, +20` - `!` 符号表示该指令在模型的整个高度上仅执行一次。按序列设置角度 = 5, 10, 30, 5, 25, 5, 25, 5 ...
- `5, 10*!, +20` - 也可组合使用 `*` 和 `!` 符号 = 5, 30, 5, 25, 5, 25, 5, 25 ...
- `5, 10*3, +20` - 若 `*` 符号后写有数字，表示该指令的重复次数。按序列设置角度 = 5, 10, 10, 10, 30, 5, 10, 10, 10, 30, 5, 10 ...

### 范围指令

将组织一个组合的层集，其中一层相对另一层的旋转也是预先确定的。
你可以指定多少层按某个角度旋转，以及依据何种数学规律执行旋转。该规律通过写入特定符号并在其后指定数值来确定。
以下符号可用于确定转向形状：`/` `#` `#-` `|` `N` `n` `Z` `z` `L` `l` `U` `u` `Q` `q` `$` `~` `^`。其用途见[连接符号](#连接符号)。

此外，数值后若有范围修饰符，则旋转将按所描述的长度进行。
以下修饰符可用于确定转向范围：`mm` `cm` `m` `'` `"` `#` `%`。其用途见[长度修饰符](#长度修饰符)。

若数值前有 `-` 符号，则初始填充角度与结束角度互换。这在某些情况下对连接线性填充有用。使用范围指令时旋转角度的绝对值无效。
需要注意的是，这不会是精确的长度，而是向下对齐到最近的层。

- `+45/100` - 在接下来 100 层内线性旋转 45 度。对此模型，此指令等价于 `+45/100%`，因为模型有 100 层。  
  ![+45-100](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+45-100.png?raw=true)
- 当改变指令高度为 `+45/50` 或 `+45/50%` 时 - 最终角度将为 90 度，因为旋转发生了两次。  
  ![+45-50](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+45-50.png?raw=true)
- `-50%Z1cm` - 按正弦函数将一厘米的填充顺时针旋转 180 度。

### 固定层数指令

有两个字母符号 `T` 和 `B`，可确定模型顶部和底部的壳层数。适用于计算跳过这些层数以对齐填充。

- `B!, +30` - 底壳层不参与旋转，之后每层以 30 度转向填充  
- `+30/1cm, T` - 将一厘米的填充线性旋转 30 度，然后跳过等于顶壳层数的层数、不旋转。  

 

## 复杂模板示例

- `+10L25%, -10l25%, -10L25%, +10l25%` - 用 10 度振幅的正弦周期填充模型  
  ![10period](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/10period.png?raw=true)
- `+30/-10#` - 在 10 个标准层高度（即标准层高 0.2mm × 10 = 2mm）内反向线性旋转 30 度。  
  ![+30-10](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+30-10.png?raw=true)
- `+360~100%` 或 `+100%~100%` - 用每层随机方向的填充填满模型。  
  ![+360-100p](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/fill/Template-metalanguage/+360-100p.png?raw=true)

## 致谢

- **功能作者：** [@pi-squared-studio](https://github.com/pi-squared-studio)。
