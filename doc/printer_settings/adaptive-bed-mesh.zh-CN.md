# 自适应热床网格支持

OrcaSlicer 为多种固件（包括 Marlin、Klipper 和 RepRapFirmware (RRF)）全面引入了自适应热床网格（adaptive bed meshing）支持。

此功能允许用户在机器开始 G-code 中无缝集成自适应热床网格命令。

该实现设计简洁，无需额外插件或更改固件设置，直接从 OrcaSlicer 提升用户体验和打印质量。

![ABM-PrinterConfig](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Adaptative-Bed-Mesh/ABM-PrinterConfig.png?raw=true)

## OrcaSlicer 中的设置

`热床网格最小值（Bed mesh min）`：此选项设定允许的热床网格区域的最小点。由于探针的 XY 偏移，大多数打印机无法探测整个热床。为确保探测点不超出热床区域，应适当设置热床网格的最小和最大点。OrcaSlicer 会确保 adaptive_bed_mesh_min/adaptive_bed_mesh_max 的值不超过这些最小/最大点。此信息通常可从打印机制造商处获取。默认设置为 (-99999, -99999)，表示无限制，允许探测整个热床。

`热床网格最大值（Bed mesh max）`：此选项设定允许的热床网格区域的最大点。由于探针的 XY 偏移，大多数打印机无法探测整个热床。为确保探测点不超出热床区域，应适当设置热床网格的最小和最大点。OrcaSlicer 会确保 adaptive_bed_mesh_min/adaptive_bed_mesh_max 的值不超过这些最小/最大点。此信息通常可从打印机制造商处获取。默认设置为 (99999, 99999)，表示无限制，允许探测整个热床。

`探针点间距（Probe point distance）`：此选项设定 X 和 Y 方向上探针点（网格尺寸）之间的首选间距，默认 X 和 Y 均为 50mm。

`网格边距（Mesh margin）`：此选项决定自适应热床网格区域在 XY 方向上额外扩展的距离。

> [!NOTE]
> Klipper 用户：OrcaSlicer 会根据边距调整自适应热床网格区域。建议在 Klipper 配置中将边距设为 0，或在调用 BED_MESH_CALIBRATE 命令时传入 0（请参考下面的示例）。

## 自适应热床网格命令可用的 G-code 变量

`bed_mesh_probe_count`：表示 X 和 Y 方向的探测点数量。该值根据自适应热床网格区域的尺寸和探针点间距计算得出。

`adaptive_bed_mesh_min`：指定自适应热床网格区域的最小坐标，定义网格的起始点。

`adaptive_bed_mesh_max`：确定自适应热床网格区域的最大坐标，指示网格的终点。

`ALGORITHM`：标识用于自适应热床网格插值的算法。此变量对 Klipper 用户有用。如果 bed_mesh_probe_count 小于 4，算法设为 `lagrange`；否则设为 `bicubic`。

## OrcaSlicer 中自适应热床网格的使用示例

### Marlin

```gcode
; Marlin 尚不支持指定探测点数量，因此我们只指定探测区域
G29 L{adaptive_bed_mesh_min[0]} R{adaptive_bed_mesh_max[0]} F{adaptive_bed_mesh_min[1]} B{adaptive_bed_mesh_max[1]} T V4
```

### Klipper

```gcode
; 始终传入 `ADAPTIVE_MARGIN=0`，因为 Orca 已在内部处理了 `adaptive_bed_mesh_margin`
; 务必将 ADAPTIVE 设为 0，否则 Klipper 会使用自己的自适应热床网格逻辑
BED_MESH_CALIBRATE mesh_min={adaptive_bed_mesh_min[0]},{adaptive_bed_mesh_min[1]} mesh_max={adaptive_bed_mesh_max[0]},{adaptive_bed_mesh_max[1]} ALGORITHM=[bed_mesh_algo] PROBE_COUNT={bed_mesh_probe_count[0]},{bed_mesh_probe_count[1]} ADAPTIVE=0 ADAPTIVE_MARGIN=0
```

### RRF

```gcode
M557 X{adaptive_bed_mesh_min[0]}:{adaptive_bed_mesh_max[0]} Y{adaptive_bed_mesh_min[1]}:{adaptive_bed_mesh_max[1]} P{bed_mesh_probe_count[0]}:{bed_mesh_probe_count[1]}
```

![ABM-Machine-G-code](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Adaptative-Bed-Mesh/ABM-Machine-G-code.png?raw=true)
