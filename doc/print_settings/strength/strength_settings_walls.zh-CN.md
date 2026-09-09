# 墙（Walls）

在 3D 打印中，"墙"指打印件的最外层结构，提供形状和结构强度。
调整墙的设置会显著影响模型的层间附着、强度、外观和打印时间。

![walls](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/walls/walls.png?raw=true)

- [墙数](#墙数)
- [交替额外墙](#交替额外墙)
- [检测薄墙](#检测薄墙)

## 墙数

"墙数"（Wall loops）指外墙循环打印的圈数。
增加墙数将：
- 增强：
  - 层间附着
  - 强度
  - 刚性
- 减少填充透印
- 增加打印时间

## 交替额外墙

此设置每隔一层添加一道额外的墙。这样填充被垂直楔入墙之间，使打印件更结实。
启用此选项时，需要禁用"确保垂直壳厚度"选项。

> [!WARNING]
> 不建议在以下情况使用此选项：
> - [闪电填充](strength_settings_patterns#lightning)，因为可供额外轮廓锚定的填充有限。
> - **[确保垂直壳厚度：全部](strength_settings_advanced#ensure-vertical-shell-thickness)**

## 检测薄墙

默认情况下，墙以闭合回环打印。当墙太薄、容纳不下两条线宽时，启用"检测薄墙"会将其打印为单条挤出线。
以此方式打印的薄墙由于不是闭合回环，表面质量和强度可能下降。

> [!TIP]
> 通常建议使用 [Arachne 墙生成器](quality_settings_wall_generator#arachne)，它会禁用"检测薄墙"，因为它采用不同的墙生成方式。

- 在小细节处，它可以生成传统墙生成方法无法实现的细节。
  ![walls-small-detect-thin-off](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/walls/walls-small-detect-thin-off.png?raw=true)
  ![walls-small-detect-thin-on](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/walls/walls-small-detect-thin-on.png?raw=true)
- 在大件打印中，由于墙厚降低，更容易产生缺陷。
  ![walls-big-detect-thin-off-on](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/walls/walls-big-detect-thin-off-on.png?raw=true)
