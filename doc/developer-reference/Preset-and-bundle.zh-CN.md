# Preset 与 Bundle

本页解释代码中的 3 个类。

## [`Preset`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/libslic3r/Preset.hpp)

顾名思义，这个类处理各种事物的预设。它定义了一个 `Type` 枚举，基本告诉你该预设包含哪种数据。下面解释其中几个及对应的 UI 元素。

> [!WARNING]
> 代码库中存在大量过时和遗留的代码。

- `TYPE_PRINT`： 指工艺预设。它叫 'Print' 可能是遗留代码的原因。

![process-preset-full](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/process-preset-full.png?raw=true)

- `TYPE_FILAMENT`： 顾名思义，这是耗材预设。

![filament-preset](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/filament-preset.png?raw=true)

- `TYPE_PRINTER`： 打印机预设。

![printer-preset](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/printer-preset.png?raw=true)

还有其他预设类型，但有些属于 SLA——那是遗留代码，因为 SLA 打印机已不再支持。以上 3 种是重要类型。

## [`PresetBundle`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/libslic3r/PresetBundle.hpp)

这是一个包含若干种 `PresetCollection` 的包。一个 bundle 拥有某些打印机、耗材和工艺（TYPE_PRINT）的预设。

`PresetCollection            prints`\
`PresetCollection            filaments`\
`PrinterPresetCollection     printers`

它们分别包含一系列工艺、耗材和打印机预设。\

> [!IMPORTANT]
> bundle 中的打印机、耗材和工艺并不要求彼此兼容。实际上所有已保存的预设都存储在一个 `PresetBundle` 中。`PresetBundle` 在启动时加载。为特定打印机显示的耗材和工艺列表是 `filaments` 和 `prints` 这两个 `PresetCollection` 的子集。

## [`PresetCollection`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/libslic3r/Preset.hpp)

`PrinterPresetCollection` 是从 `PresetCollection` 派生的类。

它们包含一系列预设。预设可以是任何类型。\
这里值得注意的函数有：

`get_edited_preset()`： 返回当前选中的预设以及用户所做的修改。\
`get_selected_preset()`： 返回当前选中的预设，不含用户所做的修改。
