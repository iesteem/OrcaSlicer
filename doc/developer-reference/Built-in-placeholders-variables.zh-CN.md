# 内置占位符变量

> [!NOTE]
> 本页内容正在编写中。以下列表可能不完整。

## 在自定义 G-code 中使用占位符变量

占位符变量可以在"自定义 G-code"部分使用，例如在打印前的 G-code 中。这样你可以在开始打印前执行自定义代码，例如在打印前预热打印机。

这些占位符会在切片时被解析并转换为对应的值。占位符必须使用 `{}` 包裹，例如 `{first_layer_print_min[0]}`。

> [!IMPORTANT]
> 本文档中使用的"层"一词指打印的 Z 高度。

> [!NOTE]
> 目前并非所有变量都可用。可用性取决于固件（以及部分情况下的主机）。

## 占位符变量列表

| 占位符 | 描述 | 备注/示例 |
|---|---|---|
| layer_num | 当前层号 | 从 1 开始。在首层的自定义 G-code 中，其值为 1 |
| layer_z | 当前层的 Z 高度 | |
| max_layer_num | 最大层号 | |
| max_layer_z | 最大层号对应的 Z 高度 | |
| zhop | Z 轴抬升高度 | |
| e_position_at_zhop_start | 抬升开始时的 E 位置 | Klipper 不适用 |
| e_position_at_zhop_end | 抬升结束时的 E 位置 | Klipper 不适用 |
| e_position_at_zhop_max | 抬升期间的最大 E 位置 | Klipper 不适用 |
| current_extruder | 当前挤出机 | 从 0 开始（0 = 左侧挤出机，1 = 右侧挤出机） |
| total_toolchanges | 计划的换料次数 | |
| layer_num | 参见上文 | |
| [int] | 数组索引 | 从 0 开始。更多信息见下文 |
| extruded_volume_total | 挤出耗材的总体积 | mm³ |
| extruded_weight_total | 挤出耗材的总重量 | g。需要在耗材设置中设置密度 |
| extruded_volume_current_extruder | 当前挤出机挤出耗材的体积 | mm³ |
| extruded_weight_current_extruder | 当前挤出机挤出耗材的重量 | g |
| first_layer_print_min | 首层最左侧挤出内容的 X 坐标 | mm |
| first_layer_print_max | 首层最右侧挤出内容的 X 坐标 | mm |
| first_layer_print_size | 首层挤出内容沿 X 轴的宽度 | mm |
| first_layer_print_centre | 首层挤出内容沿 X 轴的中心点 | mm |
| total_layer_count | 总层数 | |
| filament_used | 本次打印使用的耗材长度 | mm |
| filament_used_gt2 | GT2 同步带信息 | |
| filament_cost | 本次打印使用的耗材成本 | |
| num_printing_extruders | 打印中使用的挤出机数量 | |
| bed_temperature | 热床温度 | 指当前挤出机的耗材预设热床温度。使用方法参见"初始回抽"示例 |
| bed_temperature_initial_print | 打印开始时的热床温度 | |
| bed_temperature_overall_layer_min | 所有层中最低热床温度 | |
| bed_temperature_overall_layer_max | 所有层中最高热床温度 | |
| chamber_temperature | 腔体温度 | 指当前挤出机的耗材预设腔体温度 |
| temperature | 喷嘴温度 | 指当前挤出机的耗材预设喷嘴温度。使用方法参见"初始回抽"示例 |
| temperature_overall_layer_min | 所有层中最低喷嘴温度 | |
| temperature_overall_layer_max | 所有层中最高喷嘴温度 | |
| timestamp | 当前时间戳 | 使用 "yyyy.MM.dd HH:mm:ss" 格式 |

## 示例

### K 形起始 G-code —— 在打印前将喷嘴移开并预热

```text
M104 S170                                  ; 设置喷嘴温度为 170 度但不等待
M140 S[bed_temperature{first_layer_temperature[0]}] ; 设置热床温度但不等待
PRINT_START BED=[first_layer_bed_temperature] NOZZLE=[first_layer_temperature[0] * 0.7] ; 调用 K 形宏（此处将喷嘴预热至目标温度的 70% 以避免糊料）
```

### 初始回抽

以下示例在打印开始前将喷嘴中的耗材回抽 1mm。使用的变量为 `e_position_at_zhop_max`、`e_position_at_zhop_start` 和 `e_position_at_zhop_end`。

```text
M104 S170                                  ; 设置喷嘴温度但不等待
M140 S[bed_temperature{first_layer_temperature[0]}] ; 根据耗材设置热床温度但不等待
PRINT_START BED=[first_layer_bed_temperature] NOZZLE=[first_layer_temperature[0] * 0.7] ; 调用 K 形宏
G92 E{e_position_at_zhop_max + 1.5} ; 将当前 E 位置设置为抬升最大值 +1.5
G1 E{e_position_at_zhop_max + 0.5} F400 ; 以 400mm/min 的速度将耗材回抽 1mm
```

## 反馈

如果你想提供反馈，请加入 [Discord](https://discord.gg/7zF9qNYvdz)。
