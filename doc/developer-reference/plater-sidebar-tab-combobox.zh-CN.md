# 应用结构概览

编写中...

> [!WARNING]
> !! 不完整，可能不准确，正随新信息更新 !!

## [`Plater`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/slic3r/GUI/Plater.hpp)

指整个应用程序。整个视图、文件加载、项目保存与加载都由这个类管理。该类包含模型查看器、侧边栏、G-code 查看器及其他一切的成员。

## [`Sidebar`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/slic3r/GUI/Plater.hpp)

指应用程序窗口中的侧边栏。

![full-sidebar](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/full-sidebar.png?raw=true)

## [`ComboBox`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/slic3r/GUI/Widgets/ComboBox.hpp)

可以查看和选择预设的下拉菜单。

![combobox](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/combobox.png?raw=true)

## [`Tab`](https://github.com/SoftFever/OrcaSlicer/blob/main/src/slic3r/GUI/Tab.hpp)

指各种设置窗口。例如编辑打印机或耗材预设的弹窗，以及编辑工艺预设和对象列表的区域。这四者分别由 `TabPrinter`、`TabFilament`、`TabPrint` 和 `TabPrintModel` 管理。

![tab-popup](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/tab-popup.png?raw=true)
