# 本地化指南

## 介绍

本页面介绍 OrcaSlicer 的本地化（l10n）工作流程。目前有 3 种参与本地化的方式：

- [非开发人员](#非开发人员)：无需任何编程知识。
- [开发人员](#开发人员)：需要具备使用 GNU gettext 的经验。
- [翻译审校人员](#翻译审校人员)：审校和修正机器翻译的字符串。

## 非开发人员

> [!IMPORTANT]
> 开发团队不推荐使用这种方式，因为合并到官方翻译的流程比较麻烦。

如果你不是开发人员，最简单的方式是使用 PoEdit 直接编辑 .po 文件。步骤如下：

1. 在仓库中找到你想翻译的语言对应的 .po 文件（位于 `localization/i18n/<lang>/`）。
2. 在本地编辑 .po 文件。
3. 提交一个拉取请求，将你修改后的 .po 文件包含在内。

> [!TIP]
> 推荐使用 [PoEdit](https://poedit.net/) 编辑 .po 文件。它是免费的，可在所有平台上运行，并提供了完善的翻译工作界面。

> [!WARNING]
> 在 PR 期间原文（源字符串）发生变更时，很容易产生合并冲突。请将你的 PR 控制在较小的规模。

### 添加新语言

如果你想在 OrcaSlicer 中添加一门新语言，你需要在你的 fork 中创建一个新的翻译目录，然后将 .pot 文件（翻译模板）复制为该语言的 .po 文件。

1. Fork 仓库。
2. 创建新目录：`localization/i18n/<lang>/`，其中 `<lang>` 是语言的 ISO 639-1 代码（例如法语为 `fr`）。
3. 将 `localization/i18n/en/OrcaSlicer.pot` 复制到新目录并重命名为 `OrcaSlicer.po`。
4. 翻译 .po 文件中的字符串。你需要翻译 .pot 模板中的所有字符串。
5. 提交一个拉取请求，将新的翻译文件包含在内。

> [!NOTE]
> 新语言只有在有一定数量的翻译完成后才会被合并。请确保你的翻译覆盖了大部分字符串。

## 开发人员

OrcaSlicer 使用 GNU gettext 进行本地化。翻译源文件（.po/.pot）位于 `localization/i18n/` 目录。

如果你想更新翻译文件，请执行以下命令：

**Windows:**

```powershell
./scripts/run_gettext.bat
```

**Linux/macOS:**

```bash
./scripts/run_gettext.sh
```

> [!IMPORTANT]
> 你需要安装 GNU gettext 才能运行这些脚本。Windows 用户可以运行 `_deps\build\gettext\bin\gettext.env` 来使用 OrcaSlicer 依赖构建中包含的 gettext。你也可以从 [此处](https://mlocati.github.io/articles/gettext-on-windows-mingw.html) 安装它。

这些脚本会从源代码中提取可翻译的字符串，并更新 .pot 模板文件以及所有语言的 .po 文件。

如果你想更新特定语言的 .po 文件，可以运行：

```bash
./scripts/run_gettext.sh <lang>
```

例如，更新法语翻译：

```bash
./scripts/run_gettext.sh fr
```

### 新字符串翻译工作流程

1. 在源代码中添加新字符串（参考[下文](#向应用程序添加新字符串)）。
2. 运行 gettext 脚本更新 .pot 和 .po 文件。
3. 在 Crowdin 上翻译新字符串（推荐），或手动编辑 .po 文件。
4. 在发布新版本之前，会将 Crowdin 翻译同步到仓库。

### 向应用程序添加新字符串

OrcaSlicer 有两种类型的可翻译字符串：

#### 标准 UI 字符串

大多数 UI 字符串使用 `_()` 宏进行标记。例如：

```cpp
SetLabel(_("Slice plate"));
```

这会告诉 gettext 提取该字符串进行翻译。你不需要做其他任何事——`run_gettext` 脚本会自动处理其余部分。

#### 参数描述

打印参数（及其描述）存储在 `src/libslic3r/Config.cpp` 中。它们使用 `translate()` 函数进行标记。`translate()` 函数会返回一个键值（左值）和翻译（右值）的向量。

要为参数添加翻译，你需要在 `Config.cpp` 中添加 `translate()` 调用。请注意，这在代码中是一个两步过程：

1. 运行 gettext 脚本，提取新的可翻译字符串。
2. 将新键添加到 `translate()` 函数中（在提取出的字符串列表中找到它）。

> [!TIP]
> 由于参数字符串有很多，手动完成第二步很容易出错。推荐使用 Crowdin。

### Crowdin

OrcaSlicer 的翻译在 [Crowdin](https://crowdin.com/project/orcaslicer) 上进行。开发团队会定期将 Crowdin 的翻译同步到仓库。

如果你想参与翻译，请加入 Crowdin 项目。欢迎所有语言的帮助。

> [!NOTE]
> 非 Crowdin 的翻译（例如直接向仓库提交 .po 文件的 PR）仍然会被接受，但需要更长的时间才能合并。

## 翻译审校人员

OrcaSlicer 使用机器翻译来提供新语言的初始翻译。这些翻译随后由人工审校。

如果你想成为审校人员，请加入 [Discord](https://discord.gg/7zF9NYvdz) 服务器并联系开发团队。

## 需要避免的事项

- **不要修改 .pot 文件中的字符串顺序。** 这会导致难以合并的冲突。
- **不要翻译变量名或格式化字符串。** 例如 `%%s` 或 `%d` 应保持原样。
- **不要在字符串中添加换行符，除非原文中也有换行符。**
- **避免机器翻译习语。** 如果源字符串包含习语或幽默表达，请尽量在目标语言中找到等效表达，而不是逐字翻译。
- **长度限制。** 某些 UI 元素（如按钮）的空间有限。请尽量保持翻译简短，与原文长度相近。

## 反馈

如果你对本地化有任何疑问，请加入 [Discord](https://discord.gg/7zF9NYvdz)。
