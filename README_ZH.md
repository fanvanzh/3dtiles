# 简介

3D-Tile 转换工具集，高效快速的 3D-Tiles 生产工具。

## 主要功能

- **Osgb 转 3D-Tiles**: 将 OpenSceneGraph 二进制格式转换为 3D-Tiles 格式，为地理空间数据提供高效可视化
- **Shapefile 转 3D-Tiles**: 将 Esri Shapefile 数据转换为 3D-Tiles，供网页基 3D 可视化使用
- **FBX 转 3D-Tiles**: 将 FBX 模型转换为 3D-Tiles 格式
- **多平台支持**: 完整支持 Linux, macOS (Intel & Apple Silicon) 和 Windows
- **混合栈**: 使用 Rust 处理 CLI/数据，C++ 处理高性能 3D 处理

## 构建状态汇总

| 平台 | 架构 | 状态 | 说明 |
|----------|------|--------|-------|
| **Linux** | x64 | [![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml) | Ubuntu 24.04 LTS |
| **Windows** | x64 | [![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml) | Windows 最新版 |
| **macOS** | ARM64 (M1+) | [![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-arm64.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-arm64.yml) | macOS 15 (Sequoia) |
| **macOS** | Intel | [![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-intel.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-intel.yml) | macOS 14+ |

## 快速链接

- [完整编译指南](./README_ZH.md#编译)
- [使用指南](./README_ZH.md#使用说明)
- [调试指南](https://github.com/fanvanzh/3dtiles/wiki/How-to-debug)
- [Windows 预编译版](https://github.com/fanvanzh/3dtiles/releases/download/v0.4/3dtile.zip)
- [Docker 镜像](https://hub.docker.com/r/winner1/3dtiles)

# 编译

## 前置要求

本项目采用 Rust 和 C++ 混合栈，使用 vcpkg 进行依赖管理。

### 0. 安装基本工具

在按照下面3步之前，请确保已经安装了以下基本工具：

**Linux (Ubuntu 24.04+):**
```bash
sudo apt-get update
sudo apt-get install -y git curl build-essential
```

**Linux (CentOS/RHEL):**
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y git curl
```

**macOS:**
```bash
brew install git curl
```

**Windows:**
- 安装 Git: https://git-scm.com/download/win
- curl 在 PowerShell 3.0+ 中内置（Windows 10+ 自有）
- 或使用 Chocolatey 安装：`choco install git`

### 1. 安装 Rust 工具

**Linux/macOS:**
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

**Windows:**
```cmd
# 下载并运行 rustup 安装程序
https://rustup.rs/
或使用包管理器：
choco install rustup
```

验证安装：
```bash
rustc --version
cargo --version
```

### 2. 设置 vcpkg

**一次性设置：**
```bash
# 克隆 vcpkg 仓库
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 引导 vcpkg
./bootstrap-vcpkg.sh          # Linux/macOS
# 或
.\\bootstrap-vcpkg.bat        # Windows

# （可选）设置 VCPKG_ROOT 环境变量
export VCPKG_ROOT="$(pwd)"    # Linux/macOS
# 或
set VCPKG_ROOT=<vcpkg 路径>  # Windows
```

### 3. 克隆仓库

```bash
git clone --recursive https://github.com/fanvanzh/3dtiles.git
cd 3dtiles

# 如果忘记添加 --recursive 参数，或者更新代码后，请手动初始化子模块：
git submodule update --init --recursive
```

## Linux (Ubuntu 24.04+)

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y build-essential libgl1-mesa-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev

# 编译
cargo build --release -vv
```

## Linux (CentOS/RHEL)

```bash
# 安装依赖
sudo yum groupinstall -y "Development Tools"
sudo yum install -y gcc-c++ cmake automake libtool openscenegraph-devel gdal-devel libGL-devel libGLU-devel libX11-devel libXrandr-devel libXi-devel

# 编译
cargo build --release -vv
```

## macOS (Intel 和 Apple Silicon)

**安装 Homebrew（如果尚未安装）:**
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**安装依赖:**
```bash
brew install automake libtool

# 编译
cargo build --release -vv
```

## Windows

```cmd
# 编译（需要提前安装 vcpkg 和 Rust 工具链）
cargo build --release -vv
```

### 环境变量设置 (Windows)

**使用命令提示符 (CMD):**
```cmd
set VCPKG_ROOT=<vcpkg 路径>
set VCPKG_TRIPLET=x64-windows
```

**使用 PowerShell:**
```powershell
$env:VCPKG_ROOT="<vcpkg 路径>"
$env:VCPKG_TRIPLET="x64-windows"
```

**永久设置（可选）:**

使用 CMD，通过以下方式添加到系统环境变量：
- 设置 → 系统 → 高级系统设置 → 环境变量

或使用 PowerShell：
```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT","<vcpkg 路径>","User")
[Environment]::SetEnvironmentVariable("VCPKG_TRIPLET","x64-windows","User")
```

## Docker

```bash
# 构建 Docker 镜像，使用默认标签 (3dtiles:latest)
./build-dockerfile.sh

# 使用自定义标签构建
./build-dockerfile.sh 3dtiles:dev

# 使用完整的镜像仓库路径构建
./build-dockerfile.sh myregistry/3dtiles:v1.0
```

## 开发

### 生成 compile_commands.json 用于 IDE 支持

`cargo build` 会驱动 CMake，并自动在 `build/compile_commands.json` 下导出编译数据库（由 `build.rs` 复制好路径）。VSCode 的 C/C++/clangd 统一指向该文件即可。

手动刷新（可选）：
```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cargo build -vv
```
若需要在仓库根暴露软链，执行：`ln -sf build/compile_commands.json compile_commands.json`

### 使用严格警告检查进行构建（匹配 CI）

```bash
# 将所有警告视为错误（匹配 GitHub Actions 行为）
RUSTFLAGS="-D warnings" cargo build --release -vv
```

### 在 VSCode 中进行调试

本项目包含预配置的 VSCode 调试配置，支持多平台调试：

#### 前置要求

- 安装 **C/C++ 扩展** for VSCode
- 安装 **CodeLLDB 扩展** (用于 macOS 调试)
- Linux: 确保已安装 `gdb` (`sudo apt-get install gdb`)
- macOS: 安装 Xcode 命令行工具并配置 `lldb` 调试器
- Windows: 安装 Visual Studio Build Tools 及 MSVC 编译器

#### 可用的调试配置

按 `F5` 或进入 **Run → Start Debugging** 选择调试配置：

**Linux (使用 gdb):**
- **Debug (Linux, gdb)**: 调试 OSGB 转 3D-Tiles 转换
- **Debug SHP (Linux, gdb)**: 调试 Shapefile 转 3D-Tiles 转换

**macOS (使用 lldb):**
- **Debug (macOS, lldb)**: 调试 OSGB 转换，包含预配置的测试数据路径
- **Debug SHP (macOS, lldb)**: 调试 Shapefile 转换

**Windows (使用 MSVC):**
- **Debug (Windows, MSVC)**: 调试 OSGB 转换
- **Debug SHP (Windows, MSVC)**: 调试 Shapefile 转换

#### 配置详情

调试配置定义在 `.vscode/launch.json` 中，包括：

- **预启动任务**: 在启动调试器前自动构建调试二进制文件
- **命令行参数**: 预配置的转换参数（格式、输入输出路径等）
- **源代码语言**: 包括 Rust 和 C++ 代码
- **工作目录**: 设置为项目根目录

#### 自定义调试参数

要修改调试时的输入/输出路径，编辑 `.vscode/launch.json` 中的 `args` 数组：

```json
"args": [
  "-f", "osgb",              // 格式: osgb, shape, gltf, b3dm 等
  "-i", "/path/to/input",   // 输入文件/目录路径
  "-o", "/path/to/output"   // 输出目录路径
]
```

#### 调试技巧

1. **设置断点**: 在任何 `.rs` 或 `.cpp` 文件的行号处点击设置断点
2. **逐步执行**: 使用 F10（单步跳过）或 F11（单步进入）
3. **检查变量**: 将鼠标悬停在变量上查看值，或使用调试控制台
4. **查看调用栈**: 在调用栈面板中了解执行流程
5. **调试输出**: 监控控制台输出以查看转换进度和错误信息

#### 构建任务

`.vscode/tasks.json` 文件定义了 `cargo build` 任务，可以：
- 运行 `cargo build` 编译调试二进制文件
- 支持与 Rust 编译器输出的多线程问题匹配
- 可通过 `Ctrl+Shift+B` 手动调用

若要调试 release 构建，请修改 `.vscode/tasks.json` 中的任务配置：

```json
"args": ["build", "--release"]
```

# 使用说明

## ① 命令行格式

```sh
_3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② 示例命令

### 基础转换

```sh
# from osgb dataset
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"offset\": 0}"

# from single shp file
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single bbj file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
_3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```

### 使用优化参数的高级选项

```sh
# OSGB：启用网格简化（减少多边形数量）
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-simplify

# OSGB：启用 Draco 网格压缩（减小文件大小，处理较慢）
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-draco

# OSGB：启用纹理压缩为 KTX2 格式（减小纹理大小）
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-texture-compress

# OSGB：启用所有优化以实现最大压缩
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path \
  --enable-simplify --enable-draco --enable-texture-compress

# Shapefile：仅支持网格简化
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa \
  --height height --enable-simplify
```

## ③ 参数说明

### 必需选项

- `-f, --format <FORMAT>` 输入数据格式
  可选格式：`osgb`, `shape`, `gltf`, `b3dm`, `fbx`
  - `osgb` 为倾斜摄影格式数据
  - `shape` 为 Shapefile 面数据
  - `gltf` 为单一通用模型转 gltf
  - `b3dm` 为单个 3dtile 二进制数据转 gltf
  - `fbx` 为 FBX 模型数据

- `-i, --input <PATH>` 输入数据的目录或文件路径
  osgb 数据截止到 `<DIR>/Data` 目录的上一级，其他格式具体到文件名

- `-o, --output <DIR>` 输出目录
  输出的数据文件位于 `<DIR>/Data` 目录

### 可选参数

- `-c, --config <JSON>` 在命令行传入 JSON 配置字符串（可选）
  示例：`{"x": 120, "y": 30, "offset": 0, "max_lvl": 20, "pbr": true}`

  JSON 配置说明：
  ```json
  {
    "x": 120,        // 中心点经度
    "y": 30,         // 中心点纬度
    "offset": 0,     // 高度偏移
    "max_lvl": 20,   // 最大层级
    "pbr": true      // 启用 PBR 材质
  }
  ```

- `--height <FIELD>` 高度字段名称
  指定 shapefile 中的高度属性字段，转换 shp 时的必需参数

- `-v, --verbose` 启用详细输出用于调试

### 优化参数（新增）

**这些参数默认禁用。启用它们可以优化输出，但会增加处理时间。**

- `--enable-lod` 启用 LOD（多级细节）
  生成多个不同细节级别的模型，适应不同的视距。
  - **适用于：** Shapefile 格式
  - **默认配置：** 生成 3 个级别 `[1.0, 0.5, 0.25]`
    - LOD0: 100% 细节（最高质量）
    - LOD1: 50% 细节
    - LOD2: 25% 细节（最粗略）
  - **与其他参数的关系：**
    - 可与 `--enable-simplify` 组合：每个 LOD 级别都会应用简化
    - 可与 `--enable-draco` 组合：LOD0 不压缩，LOD1/LOD2 会压缩
    - 不使用 `--enable-simplify` 时，仅生成多个 LOD 级别但不简化
  - **推荐组合：** `--enable-lod --enable-simplify --enable-draco`
  - **使用场景：** 大范围场景浏览，需要根据视距动态加载不同细节

- `--enable-simplify` 启用网格简化
  在保持视觉质量的同时减少多边形数量。使用 meshoptimizer 库进行顶点缓存优化、过度绘制减少和自适应简化。
  - **适用于：** OSGB 和 Shapefile 格式
  - **影响：** 文件更小，渲染更快，处理时间更长
  - **使用场景：** 大型数据集，渲染性能要求高的场景
  - **与 LOD 的关系：** 在 LOD 模式下，此参数控制是否对每个 LOD 级别应用简化

- `--enable-draco` 启用 Draco 网格压缩
  对几何数据（顶点、法线、索引）应用 Google Draco 压缩。
  - **适用于：** OSGB 和 Shapefile 格式
  - **影响：** 几何体积减小 3-6 倍，处理和解码较慢
  - **使用场景：** 带宽受限场景，Web 流式传输
  - **与 LOD 的关系：**
    - 非 LOD 模式：压缩所有输出
    - LOD 模式：LOD0 不压缩，LOD1/LOD2 压缩
  - **注意：** 需要客户端支持 Draco 解码器

- `--enable-texture-compress` 启用纹理压缩（KTX2）
  将纹理转换为 KTX2 格式，使用 GPU 友好的压缩。
  - **适用于：** 仅 OSGB 格式
  - **影响：** GPU 上传更快，纹理体积更小
  - **使用场景：** GPU 内存优化，纹理加载更快
  - **注意：** 需要支持 KTX2 的渲染器

### 格式支持矩阵

| 优化参数 | OSGB | Shapefile | GLTF | B3DM | FBX |
|-------------------|------|-----------|------|------|-----|
| `--enable-lod` | ❌ | ✅ | ❌ | ❌ | ❌ |
| `--enable-simplify` | ✅ | ✅ | ❌ | ❌ | ✅ |
| `--enable-draco` | ✅ | ✅ | ❌ | ❌ | ✅ |
| `--enable-texture-compress` | ✅ | ❌ | ❌ | ❌ | ✅ |

### 参数组合建议

```sh
# OSGB：最适合 Web 流式传输（最大压缩）
--enable-simplify --enable-draco --enable-texture-compress

# OSGB 或 Shapefile：最适合渲染性能（仅简化）
--enable-simplify

# OSGB：最适合带宽优化（仅压缩）
--enable-draco --enable-texture-compress

# Shapefile：LOD 模式（推荐配置）
--enable-lod --enable-simplify --enable-draco

# Shapefile：仅 LOD 不简化
--enable-lod

# Shapefile：LOD + 简化但不压缩
--enable-lod --enable-simplify

# Shapefile：独立使用 Draco 压缩（无 LOD）
--enable-draco

# Shapefile：简化 + Draco 压缩（无 LOD）
--enable-simplify --enable-draco
```

## ④ 预览

项目根目录提供了一个简单的 `index.html` 查看器，用于预览导出的 3D Tiles 数据。

1.  在项目根目录启动一个本地 HTTP 服务器：
    ```bash
    python3 -m http.server 3000
    ```
2.  打开浏览器访问：[http://localhost:3000](http://localhost:3000)
3.  查看器将自动加载 `output` 目录中的 3D Tiles 数据。

# 数据要求及说明

### ① 倾斜摄影数据

倾斜摄影数据仅支持 smart3d 格式的 osgb 组织方式：

- 数据目录必须有一个 `"Data"` 目录的总入口；
- `"Data"` 目录同级放置一个 `metadata.xml` 文件用来记录模型的位置信息；
- 每个瓦片目录下，必须有个和目录名同名的 osgb 文件，否则无法识别根节点；

正确的目录结构示意：

```
- Your-data-folder
  ├ metadata.xml
  └ Data\Tile_000_000\Tile_000_000.osgb
```

### ② Shapefile

目前仅支持 Shapefile 的面数据，可用于建筑物轮廓批量生成 3D-Tiles.

Shapefile 中需要有字段来表示高度信息。

### ③ B3dm 单文件转 glb

支持将 b3dm 单个文件转成 glb 格式，便于调试程序和测试数据
