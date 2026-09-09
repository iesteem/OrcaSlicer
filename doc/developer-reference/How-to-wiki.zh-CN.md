# 如何为 Wiki 做贡献

本指南介绍如何为 OrcaSlicer wiki 做贡献。

OrcaSlicer 使用 GitHub 的 wiki 功能，允许用户和开发者协作创建和编辑文档。

我们鼓励开发者和用户通过更新现有页面和添加新内容来做贡献。这有助于保持文档准确且有用。

添加新功能时，请考虑更新 wiki，让用户可以获得最新指引。

- [Wiki 结构](#wiki-结构)
  - [Home](#home)
    - [索引与导航](#索引与导航)
  - [文件命名与组织](#文件命名与组织)
- [Orca 到 Wiki 的跳转](#orca-到-wiki-的跳转)
- [格式与风格](#格式与风格)
  - [Markdown 格式](#markdown-格式)
  - [提示与标注](#提示与标注)
- [图片](#图片)
  - [图片命名](#图片命名)
  - [图片放置](#图片放置)
  - [链接图片](#链接图片)
    - [示例](#示例)
    - [避免以下做法](#避免以下做法)
    - [调整图片大小](#调整图片大小)
  - [图片裁剪与高亮](#图片裁剪与高亮)
  - [推荐格式](#推荐格式)
- [内容结构](#内容结构)
- [命令与代码块](#命令与代码块)
- [外部链接](#外部链接)

## Wiki 结构

每个 wiki 页面是位于仓库 `doc` 目录下的 Markdown 文件。wiki 按覆盖项目不同领域的章节组织。

### Home

Home 页面是 OrcaSlicer wiki 的起点。你可以从那里导航到与项目相关的章节和主题。

创建新页面或章节时，请在 Home 页面的相应类别下添加链接。
Home 页面目前按以下顶级条目组织内容：

- ![printer](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/printer.svg?raw=true) [Printer Settings（打印机设置）](home#printer-settings)
- ![filament](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/filament.svg?raw=true) [Material Settings（材料设置）](home#material-settings)
- ![process](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/process.svg?raw=true) [Process Settings（工艺设置）](home#process-settings)
- ![tab_3d_active](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/tab_3d_active.svg?raw=true) [Prepare（准备）](home#prepare)
- ![tab_calibration_active](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/tab_calibration_active.svg?raw=true) [Calibrations（校准）](home#calibrations)
- ![im_code](https://github.com/SoftFever/OrcaSlicer/blob/main/resources/images/im_code.svg?raw=true) [Developer Section（开发者专区）](home#developer-section)

每个章节可以有多篇覆盖特定主题的页面。例如 [Process Settings](home#process-settings) 章节包含 [quality](home#quality-settings)、[support](home#support-settings) 和 [others](home#others-settings) 等页面。

#### 索引与导航

GitHub Wiki 使用文件名作为页面标识。链接到某个页面时，使用不带 `.md` 扩展名的文件名。如果文件位于子目录中，链接中**不要包含子目录**；直接从 Home 页面链接到文件名。

例如，若你添加了 `doc/calibration/flow-rate-calib.md`，这样链接：

```markdown
[Flow Rate Calibration](flow-rate-calib)
```

对于较长的页面，请在顶部包含目录以帮助读者快速找到章节。

```markdown
- [Wiki Structure](#wiki-structure)
  - [Home](#home)
    - [Index and Navigation](#index-and-navigation)
  - [File Naming and Organization](#file-naming-and-organization)
- [Formatting and Style](#formatting-and-style)
```

> [!NOTE]
> 若要添加新章节，请遵循现有结构，并确认它确实不属于任何现有类别。然后在 Home 页面相应添加链接。

### 文件命名与组织

创建新页面时，请遵循以下文件命名约定：

- 使用唯一文件名以避免冲突。
- 使用能反映页面内容的描述性名称。
- 文件名使用 kebab-case（例如 `How-to-wiki.md`）。
- 若页面属于某章节，添加表明归属的后缀（例如校准页面应以 `-calib.md` 结尾，如 `flow-rate-calib.md`）。
- 在适用时将文件放入相应子目录（例如校准相关内容放 `doc/calibration/`）。

## Orca 到 Wiki 的跳转

OrcaSlicer 可以将用户从 GUI 跳转到相应的 wiki 页面，方便查找相关文档。

选项到 wiki 的映射定义在 [src/slic3r/GUI/Tab.cpp](https://github.com/SoftFever/OrcaSlicer/blob/main/src/slic3r/GUI/Tab.cpp)。任何用 `append_single_option_line` 添加的选项都可以通过第二个字符串参数映射到 wiki 页面。

```cpp
optgroup->append_single_option_line("OPTION_NAME"); // 无 wiki 页面/跳转的选项
optgroup->append_single_option_line("OPTION_NAME", "WIKI_PAGE"); // 有 wiki 页面和跳转的选项
```

还可以通过追加片段标识符（例如 `#section-name`）指向 wiki 页面内的特定章节。

示例：

```cpp
optgroup->append_single_option_line("seam_gap","quality_settings_seam"); // wiki 页面及跳转
optgroup->append_single_option_line("seam_slope_type", "quality_settings_seam#scarf-joint-seam"); // wiki 页面并跳转到 `Scarf Joint Seam` 章节
```

## 格式与风格

为 wiki 做贡献时请遵循以下风格和格式约定。

### Markdown 格式

wiki 使用标准 Markdown 语法进行格式化，并力求所有页面风格一致。避免使用原始 HTML 标签，优先使用 Markdown 格式。

确保缩进一致，尤其是代码块和列表。

有关 Markdown 语法的更多信息，请参阅 [GitHub Markdown Guide](https://guides.github.com/features/mastering-markdown/)。

### 提示与标注

使用 GitHub 的 alert 语法添加行内注释和警告：

```markdown
> [!NOTE]
> 读者应当知道的有用信息。

> [!TIP]
> 帮助更轻松做事的建议。

> [!IMPORTANT]
> 实现目标所需的关键信息。

> [!WARNING]
> 用于避免问题的紧急信息。

> [!CAUTION]
> 关于风险或负面后果的警告。
```

> [!NOTE]
> 更多细节请参阅 [GitHub Alerts 文档](https://docs.github.com/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#alerts)。

## 图片

鼓励使用图片来提升 wiki 内容的清晰度和质量。它们有助于阐释概念、提供示例并改善可读性。

> [!CAUTION]
> 除非拥有适当许可，否则不要使用来自第三方来源的图片。

### 图片命名

- 使用能反映图片内容的清晰描述性文件名。
- 对于章节专属图片，包含章节名称或缩写（例如压力推进图片用 `pa-[description].png`）。

### 图片放置

- 通用图片应放在 `doc/images/` 目录。
- 章节专属图片应存放在相应子目录（例如校准内容放 `doc/images/calibration/`）。

> [!TIP]
> 你可以使用 GUI 中用到的 `\resources\images` 图片。

### 链接图片

图片链接始终使用 GitHub raw URL 以确保正确显示：

格式 = `![`文件名`](` + Base URL + 文件名.扩展名 + Raw 标记 + `)`

- Base URL：

  ```markdown
  https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/
  ```

- Raw 标记：

  ```markdown
  ?raw=true
  ```

#### 示例

- 对于 `doc/images/` 下名为 `calibration.png` 的图片：

  ```markdown
  ![calibration](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/calibration.png?raw=true)
  ```

- 对于子目录中的图片如 `doc/images/GUI/combobox.png`：

  ```markdown
  ![combobox](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/GUI/combobox.png?raw=true)
  ```

> [!IMPORTANT]
> 新增或移动的图片在拉取请求合并前可能无法在预览中显示。移动文件后请仔细检查路径并更新链接。

#### 避免以下做法

- 相对路径
- GitHub Assets/user-content/user-images URL
- 来自临时或不可靠主机的外部图片链接
- 包含个人或敏感信息的图片
- 对可用文字表达的内容（如公式或代码）使用图片——应改用 Markdown 语法或 Mermaid/Math 格式。

> [!NOTE]
> 贡献章节专属图片时，请遵循命名约定和目录结构。

#### 调整图片大小

避免手动调整图片大小，让 Wiki 自动处理。

若确需调整大小（例如缩略图），使用以下语法：

HTML 格式 = `<img alt="` + 文件名 + `"` + `src="` + Base URL + 文件名.扩展名 + Raw 标记 + 尺寸限制。
示例：

```html
<img alt="IS_damp_marlin_print_measure" src="https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/InputShaping/IS_damp_marlin_print_measure.jpg?raw=true" height="200">
```

### 图片裁剪与高亮

为确保清晰：

- 裁剪图片以聚焦相关区域。
- 使用简单的标注（箭头、圆圈、矩形）突出重要部分，避免画面过载。

### 推荐格式

- **JPG：** 适合照片。因压缩伪影，避免用于含文字或细节的图片。
- **PNG：** 截图或需要透明度的图片的理想选择。确保在浅色和深色模式下都有足够对比度。
- **SVG：** 尽可能优先使用。SVG 支持主题适配（浅色/深色模式），是图标和示意图的理想选择。

## 内容结构

每个页面应有明确的目标。简短介绍之后，选择适合内容的结构：

- **分步指南：** 用于顺序操作流程（例如校准）。
- **基于 GUI 的参考：** 当不要求顺序时，按 OrcaSlicer 的 UI 描述设置。

示例：先讲**层高**再讲**首层高度**，因为前者是全局的，后者仅适用于第一层。

## 命令与代码块

添加命令或代码块时，请使用 [Markdown 的带语法高亮代码块功能](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-and-highlighting-code-blocks#syntax-highlighting)。

- 使用三重反引号（```）包裹代码块。
- 指定语言以获得正确的高亮和可读性。

````markdown
```json
{
  "key": "value"
}
```
````

```json
{
  "key": "value"
}
```

## 外部链接

链接到外部资源时请谨慎。
确保链接相关且可靠，并在适当之处引用论文或文章。
