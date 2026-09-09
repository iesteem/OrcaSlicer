# 如何构建

本页提供在不同操作系统（包括 Windows、macOS 和 Linux）上从源码构建 OrcaSlicer 的详细说明，涵盖各平台的工具要求、安装命令和构建步骤。

无论你是贡献者还是只想构建一个自定义版本，本指南都能帮助你成功编译 OrcaSlicer。

- [Windows 64 位](#windows-64-位)
  - [Windows 所需工具](#windows-所需工具)
  - [Windows 构建步骤](#windows-构建步骤)
- [MacOS 64 位](#macos-64-位)
  - [MacOS 所需工具](#macos-所需工具)
  - [MacOS 构建步骤](#macos-构建步骤)
  - [在 Xcode 中调试](#在-xcode-中调试)
- [Linux](#linux)
  - [使用 Docker](#使用-docker)
    - [Docker 依赖](#docker-依赖)
    - [Docker 步骤](#docker-步骤)
  - [故障排除](#故障排除)
  - [Linux 构建](#linux-构建)
    - [依赖](#依赖)
      - [各发行版通用依赖](#各发行版通用依赖)
      - [特定发行版的额外依赖](#特定发行版的额外依赖)
    - [Linux 构建步骤](#linux-构建步骤)
- [便携式用户配置](#便携式用户配置)
  - [示例目录结构](#示例目录结构)

## Windows 64 位

如何在 Windows 64 位上使用 Visual Studio 2022 构建。

### Windows 所需工具

- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/) 或 Visual Studio 2019
  ```shell
  winget install --id=Microsoft.VisualStudio.2022.Professional -e
  ```
- [CMake（版本 3.31）](https://cmake.org/) — **⚠️ 必须使用 3.31.x 版本**
  ```shell
  winget install --id=Kitware.CMake -v "3.31.6" -e
  ```
- [Strawberry Perl](https://strawberryperl.com/)
  ```shell
  winget install --id=StrawberryPerl.StrawberryPerl -e
  ```
- [Git](https://git-scm.com/)
  ```shell
  winget install --id=Git.Git -e
  ```
- [git-lfs](https://git-lfs.com/)
  ```shell
  winget install --id=GitHub.GitLFS -e
  ```

> [!TIP]
> GitHub Desktop（可选）：Git 和 Git LFS 的图形界面客户端，已内置这两个工具。
> ```shell
> winget install --id=GitHub.GitHubDesktop -e
> ```

### Windows 构建步骤

1. 克隆仓库：
   - 如果使用 GitHub Desktop，直接在图形界面中克隆仓库。
   - 如果使用命令行：
     1. 克隆仓库：
     ```shell
     git clone https://github.com/SoftFever/OrcaSlicer
     ```
     2. 运行 lfs 下载 Windows 上的工具：
     ```shell
     git lfs pull
     ```
2. 打开对应的命令提示符：
   - Visual Studio 2019：
     打开 **x64 Native Tools Command Prompt for VS 2019** 并运行：
     ```shell
     build_release.bat
     ```
   - Visual Studio 2022：
     打开 **x64 Native Tools Command Prompt for VS 2022** 并运行：
     ```shell
     build_release_vs2022.bat
     ```

> [!NOTE]
> 如果遇到问题，可以尝试从你的 Vcpkg 库中卸载 ZLIB。

3. 如果成功，你将在以下位置找到 VS 2022 解决方案文件：
   ```shell
   build\OrcaSlicer.sln
   ```

> [!IMPORTANT]
> 请确认实际使用的是 CMake 3.31.x 版本。运行 `cmake --version` 并验证其返回 **3.31.x** 版本。
> 如果看到较旧的版本（例如 3.29），很可能是系统 PATH 中存在另一份 CMake（例如来自 Strawberry Perl）。
> 你可以运行 where cmake 检查生效的路径，并调整**系统环境变量** > PATH 的顺序，确保正确的 CMake（例如 C:\Program Files\CMake\bin）排在 C:\Strawberry\c\bin 等其他路径之前。

> [!NOTE]
> 如果构建失败，尝试删除 `build/` 和 `deps/build/` 目录以清除缓存的构建数据。清理后重新构建通常足以解决大多数问题。

## MacOS 64 位

如何在 MacOS 64 位上使用 Xcode 构建。

### MacOS 所需工具

- Xcode
- CMake（必须使用 3.31.x 版本）
- Git
- gettext
- libtool
- automake
- autoconf
- texinfo

> [!TIP]
> 大部分工具可以通过以下命令安装：
> ```shell
> brew install gettext libtool automake autoconf texinfo
> ```

Homebrew 目前只提供最新版本的 CMake（例如 **4.X**），不兼容本项目。要安装所需的 **3.31.X** 版本，请按以下步骤操作：

1. 从 [https://cmake.org/download/](https://cmake.org/download/) 下载 CMake **3.31.7**。
2. 安装应用程序（拖入 `/Applications`）。
3. 在你的 shell 配置文件（`~/.zshrc` 或 `~/.bash_profile`）中添加以下一行：

```sh
export PATH="/Applications/CMake.app/Contents/bin:$PATH"
```

4. 重启终端并检查版本：

```sh
cmake --version
```

5. 确认它显示 **3.31.x** 版本。

> [!IMPORTANT]
> 如果你最近升级了 Xcode，请务必至少打开一次 Xcode 并安装所需的 macOS 构建支持组件。

### MacOS 构建步骤

1. 克隆仓库：
   ```shell
   git clone https://github.com/SoftFever/OrcaSlicer
   cd OrcaSlicer
   ```
2. 构建应用程序：
   ```shell
   ./build_release_macos.sh
   ```
3. 打开应用程序：
   ```shell
   open build/arm64/OrcaSlicer/OrcaSlicer.app
   ```

### 在 Xcode 中调试

直接在 Xcode 中构建和调试：

1. 打开 Xcode 工程：
   ```shell
   open build/arm64/OrcaSlicer.xcodeproj
   ```
2. 在菜单栏中：
   - **Product > Scheme > OrcaSlicer**
   - **Product > Scheme > Edit Scheme...**
     - 在 **Run > Info** 下，将 **Build Configuration** 设置为 `RelWithDebInfo`
     - 在 **Run > Options** 下，取消勾选 **Allow debugging when browsing versions**
   - **Product > Run**

## Linux

Linux 上有两种方式：[使用 Docker](#使用-docker)（推荐）或在系统上[直接构建](#linux-构建)。

### 使用 Docker

如何使用 Docker 构建并运行 OrcaSlicer。

#### Docker 依赖

- Docker
- Git

#### Docker 步骤

```shell
git clone https://github.com/SoftFever/OrcaSlicer && cd OrcaSlicer && ./scripts/DockerBuild.sh && ./scripts/DockerRun.sh
```

### 故障排除

`scripts/DockerRun.sh` 脚本中包含若干被注释掉的选项，可以帮助解决常见问题。以下是各项说明：

- `xhost +local:docker`：如果遇到 "Authorization required, but no authorization protocol specified" 错误，在执行 `scripts/DockerRun.sh` 之前先在终端运行此命令。它授予 Docker 容器与 X 显示服务器交互的权限。
- `-h $HOSTNAME`：强制容器的主机名与你工作站的主机名一致。在某些网络配置中可能有用。
- `-v /tmp/.X11-unix:/tmp/.X11-unix`：将 X11 Unix 套接字挂载到容器中，帮助解决 X 显示问题。
- `--net=host`：使用宿主机的网络栈，对打印机 Wi-Fi 连接和 D-Bus 通信有益。
- `--ipc host`：解决某些 X 安装中阻止与共享内存套接字通信的权限问题。
- `-u $USER`：以你工作站的用户名运行容器，有助于保持文件权限一致。
- `-v $HOME:/home/$USER`：将你的主目录挂载到容器中，方便加载和保存文件。
- `-e DISPLAY=$DISPLAY`：将你的 X 显示编号传递给容器，启用图形界面。
- `--privileged=true`：授予容器提升的权限，libGL 和 D-Bus 功能可能需要。
- `-ti`：为容器附加 TTY，支持与 OrcaSlicer 的命令行交互。
- `--rm`：容器退出后自动删除，保持系统干净。
- `orcaslicer $*`：将 `scripts/DockerRun.sh` 脚本的任何额外参数直接传递给容器内的 OrcaSlicer 可执行文件。

根据需要取消注释并使用这些选项，通常可以解决与显示授权、网络和文件权限相关的问题。

### Linux 构建

如何在 Linux 上构建 OrcaSlicer。

#### 依赖

构建系统支持多种 Linux 发行版，包括 Ubuntu/Debian 和 Arch Linux。所有必需的依赖都会尽可能由提供的 shell 脚本自动安装，但某些依赖可能需要手动安装。

> [!NOTE]
> 目前不支持 Fedora 等发行版，但你可以尝试手动安装下列所需的依赖进行构建。

##### 各发行版通用依赖

- autoconf / automake
- cmake
- curl / libcurl4-openssl-dev
- dbus-devel / libdbus-1-dev
- eglexternalplatform-dev / eglexternalplatform-devel
- extra-cmake-modules
- file
- gettext
- git
- glew-devel / libglew-dev
- gstreamer-devel / libgstreamerd-3-dev
- gtk3-devel / libgtk-3-dev
- libmspack-dev / libmspack-devel
- libsecret-devel / libsecret-1-dev
- libspnav-dev / libspnav-devel
- libssl-dev / openssl-devel
- libtool
- libudev-dev
- mesa-libGLU-devel
- ninja-build
- texinfo
- webkit2gtk-devel / libwebkit2gtk-4.0-dev 或 libwebkit2gtk-4.1-dev
- wget

##### 特定发行版的额外依赖

- **Ubuntu 22.x/23.x**：libfuse-dev、m4
- **Arch Linux**：mesa、wayland-protocols

#### Linux 构建步骤

1. **安装系统依赖：**
   ```shell
   ./build_linux.sh -u
   ```

2. **构建依赖：**
   ```shell
   ./build_linux.sh -d
   ```

3. **构建 OrcaSlicer：**
   ```shell
   ./build_linux.sh -s
   ```

4. **构建 AppImage（可选）：**
   ```shell
   ./build_linux.sh -i
   ```

5. **一体化构建（推荐）：**
   ```shell
   ./build_linux.sh -dsi
   ```

**其他构建选项：**

- `-b`：以调试模式构建
- `-c`：强制清理构建
- `-C`：启用 ANSI 彩色编译输出（仅 GNU/Clang）
- `-j N`：限制使用 N 个核心构建（适合低内存系统）
- `-1`：限制单核构建
- `-l`：使用 Clang 代替 GCC
- `-p`：禁用预编译头（提高 boost ccache 命中率）
- `-r`：跳过内存和磁盘检查（适合低内存系统）

> [!NOTE]
> 构建脚本会自动检测你的 Linux 发行版，并使用相应的包管理器（apt、pacman）安装依赖。

> [!TIP]
> 首次构建时，先用 `./build_linux.sh -u` 安装依赖，再用 `./build_linux.sh -dsi` 构建全部内容。

> [!WARNING]
> 如果编译期间遇到内存问题，使用 `-j 1` 或 `-1` 限制并行编译，或使用 `-r` 跳过内存检查。

---

## 便携式用户配置

如果你希望 OrcaSlicer 使用自定义的用户配置文件夹（例如便携式安装），只需在 OrcaSlicer 可执行文件旁边放置一个名为 `data_dir` 的文件夹。OrcaSlicer 会自动将该文件夹用作其配置目录。

这样可以实现多个相互独立、各自包含用户数据的安装。

> [!TIP]
> 此功能在你想从 U 盘运行 OrcaSlicer 或保持不同配置文件相互隔离时特别有用。

### 示例目录结构

```shell
OrcaSlicer.exe
data_dir/
```

无需重新编译或修改任何设置——只要 `data_dir` 与可执行文件位于同一目录，即可开箱即用。
