# STL 转换

OrcaSlicer 切片主要依赖 STL 网格，但 STL 文件可能存在若干局限。

通常，STL 文件的多边形数量较少，这会对打印质量产生不利影响。
相比之下，使用 STEP 文件可获得更高质量的网格，更精确地还原原始设计。但请注意，高多边形 STL 和 STEP 文件都会增加切片时间。

![stl-transformation-smooth-rough](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/stl-transformation-smooth-rough.png?raw=true)

## 导入 STEP 文件

此设置决定 STEP 文件如何转换为 STL 文件，会在 STEP 文件导入过程中显示。

如果你打开 STEP 文件时没有看到它，请查看下方的[不再显示](#不再显示)。

![stl-transformation](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/stl-transformation.png?raw=true)

### 参数：

转换使用[线性偏移和角度偏移（Linear Deflection and Angular Deflection）](https://dev.opencascade.org/doc/overview/html/occt_user_guides__mesh.html)参数来控制网格质量。
网格越精细，对原始表面的还原越精确，但文件大小和处理时间也会增加。

![stl-transformation-params](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/stl-transformation-params.png?raw=true)

- **线性偏移（Linear Deflection）：** 指定原始表面与其多边形近似之间允许的最大距离。值越低，网格越贴合原始曲面。
- **角度偏移（Angular Deflection）：** 定义实际表面与其镶嵌网格之间允许的最大角度差。角度偏移值越小，网格越精确。

#### 将复合体与复合实体拆分为多个对象

启用此选项会将导入的 3D 文件拆分为独立对象。这对调整单个对象位置、微调打印设置或通过简化来优化模型特别有用。

![stl-transformation-split](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/stl-transformation-split.png?raw=true)

#### 不再显示

此选项会在打开 STEP 文件时隐藏 STL 转换对话框。
要恢复该对话框，请前往"首选项"（Ctrl + P）> "显示 STEP 网格参数设置对话框"。

![stl-transformation-enable](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/stl-transformation-enable.png?raw=true)

## 简化模型

处理高多边形数量的模型时，"简化模型"选项可以显著降低复杂度并帮助缩短切片时间。

此功能尤其适合提升切片器性能，或出于艺术或技术原因获得特定的多面化外观。

要使用"简化模型"选项，请在"准备"菜单中右键点击要简化的对象。

![simplify-menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/STL-Transformation/simplify-menu.png?raw=true)

建议在运行简化过程时启用"显示线框"选项，以便目视检查结果。但请小心：过度激进的简化可能导致明显的细节丢失、振纹增加或其他打印问题。

### 你可以使用以下选项简化模型

- **细节级别：** 从五个预设选项中选择，控制简化模型的细节水平。此设置可以在网格保真度与性能之间取得平衡。
- **抽稀比例：** 调整原始模型与简化模型多边形数量的比例。例如，抽稀比例为 0.5 将生成多边形数量约为原始一半的模型。
