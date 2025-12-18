# Introduction

[![glTF status](https://img.shields.io/badge/glTF-2%2E0-green.svg?style=flat)](https://github.com/KhronosGroup/glTF)
[![Linux Status](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml)
[![Windows Status](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml)
[![macOS ARM64 Status](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-arm64.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-arm64.yml)
[![macOS Intel Status](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-intel.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-intel.yml)

## Features

- **Osgb to 3D-Tiles**: Convert OpenSceneGraph Binary format to 3D-Tiles format for efficient geospatial data visualization
- **FBX to 3D-Tiles**: Convert FBX models to 3D-Tiles format with embedded textures
- **Shapefile to 3D-Tiles**: Transform Esri Shapefile data to 3D-Tiles for web-based 3D visualization
- **Multi-platform Support**: Fully supported on Linux, macOS (Intel & Apple Silicon), and Windows
- **Hybrid Stack**: Combines Rust for CLI/data handling with C++ for high-performance 3D processing

## Build Status by Platform

| Platform | Arch | Status | Notes |
|----------|------|--------|-------|
| **Linux** | x64 | ![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml/badge.svg) | Ubuntu 24.04 LTS |
| **Windows** | x64 | ![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml/badge.svg) | Windows Latest |
| **macOS** | ARM64 (M1+) | ![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-arm64.yml/badge.svg) | macOS 15 (Sequoia) |
| **macOS** | Intel | ![Build](https://github.com/fanvanzh/3dtiles/actions/workflows/macOS-intel.yml/badge.svg) | macOS 14+ |

## Quick Links

- [Complete Build Instructions](./README_EN.md#build)
- [Usage Guide](./README_EN.md#usage)
- [Contributing Guide](https://github.com/fanvanzh/3dtiles/wiki/How-to-debug)
- [Pre-built Windows Binary](https://github.com/fanvanzh/3dtiles/releases/download/v0.4/3dtile.zip)
- [Docker Image](https://hub.docker.com/r/winner1/3dtiles)

# Build

## Prerequisites

This project uses a hybrid Rust and C++ stack with vcpkg for dependency management.

### 0. Install Basic Tools

Before proceeding, ensure you have these basic tools installed:

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
- Install Git: Download from https://git-scm.com/download/win
- curl is available in PowerShell 3.0+ (built-in on Windows 10+)
- Or install via Chocolatey: `choco install git`

### 1. Install Rust Toolchain

**Linux/macOS:**
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

**Windows:**
```cmd
# Download and run rustup installer
from https://rustup.rs/
or use package manager:
choco install rustup
```

Verify installation:
```bash
rustc --version
cargo --version
```

### 2. Setup vcpkg

**One-time setup:**
```bash
# Clone vcpkg repository
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
./bootstrap-vcpkg.sh          # Linux/macOS
# or
.\\bootstrap-vcpkg.bat        # Windows

# (Optional) Add vcpkg to PATH or set VCPKG_ROOT environment variable
export VCPKG_ROOT="$(pwd)"    # Linux/macOS
# or
set VCPKG_ROOT=<path-to-vcpkg>  # Windows
```

### 3. Clone the Repository

```bash
git clone --recursive https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
```

## Linux (Ubuntu 24.04+)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential libgl1-mesa-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev

# Build
cargo build --release -vv
```

## Linux (CentOS/RHEL)

```bash
# Install dependencies
sudo yum groupinstall -y "Development Tools"
sudo yum install -y gcc-c++ cmake automake libtool openscenegraph-devel gdal-devel libGL-devel libGLU-devel libX11-devel libXrandr-devel libXi-devel

# Build
cargo build --release -vv
```

## macOS (Intel and Apple Silicon)

**Install Homebrew (if not already installed):**
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**Install dependencies:**
```bash
brew install automake libtool

# Build
cargo build --release -vv
```

## Windows

```cmd
# Build (vcpkg and Rust toolchain must be installed)
cargo build --release -vv
```

### Environment Setup (Windows)

Set environment variables for Windows builds:

**Using Command Prompt (CMD):**
```cmd
set VCPKG_ROOT=<path-to-vcpkg>
set VCPKG_TRIPLET=x64-windows
```

**Using PowerShell:**
```powershell
$env:VCPKG_ROOT="<path-to-vcpkg>"
$env:VCPKG_TRIPLET="x64-windows"
```

**Permanent setup (optional):**

For CMD, add to system environment variables via:
- Settings → System → Advanced system settings → Environment Variables

Or use PowerShell:
```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT","<path-to-vcpkg>","User")
[Environment]::SetEnvironmentVariable("VCPKG_TRIPLET","x64-windows","User")
```

## Docker

```bash
# Build Docker image with default tag (3dtiles:latest)
./build-dockerfile.sh

# Build with custom tag
./build-dockerfile.sh 3dtiles:dev

# Build with registry path
./build-dockerfile.sh myregistry/3dtiles:v1.0
```

# Usage

## ① Command Line

```sh
_3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② Examples

### Basic Conversion

```sh
# from osgb dataset
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"offset\": 0}"
# use pbr-texture
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"pbr\": true}"

# from single shp file
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single fbx file
_3dtile.exe -f fbx -i E:\Data\model.fbx -o E:\Data\output --enable-texture-compress

# from single osgb file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.osgb -o E:\Data\TT\001.glb

# from single obj file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
_3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```

### Advanced Options with Optimization Flags

```sh
# OSGB: Enable mesh simplification (reduces polygon count)
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-simplify

# OSGB: Enable Draco mesh compression (reduces file size, slower processing)
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-draco

# OSGB: Enable texture compression to KTX2 format (reduces texture size)
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path --enable-texture-compress

# OSGB: Enable all optimizations for maximum compression
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path \
  --enable-simplify --enable-draco --enable-texture-compress

# Shapefile: Only supports mesh simplification
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa \
  --height height --enable-simplify
```

## ③ Command-Line Flags

### Required Options

- `-f, --format <FORMAT>` - Input data format
  Available formats: `osgb`, `shape`, `gltf`, `b3dm`, `fbx`

- `-i, --input <PATH>` - Input file or directory path

- `-o, --output <DIR>` - Output directory path

### Optional Flags

- `-c, --config <JSON>` - JSON configuration string (optional)
  Example: `{"x": 120, "y": 30, "offset": 0, "max_lvl": 20, "pbr": true}`

- `--height <FIELD>` - Height attribute field name (required for shapefile conversion)

- `-v, --verbose` - Enable verbose output for debugging

### Optimization Flags (New)

**These flags are disabled by default. Enable them to optimize output at the cost of processing time.**

- `--enable-lod` - Enable LOD (Level of Detail)
  Generates multiple detail levels for adaptive distance-based rendering.
  - **Applies to:** Shapefile format
  - **Default configuration:** Generates 3 levels `[1.0, 0.5, 0.25]`
    - LOD0: 100% detail (highest quality)
    - LOD1: 50% detail
    - LOD2: 25% detail (coarsest)
  - **Interaction with other flags:**
    - With `--enable-simplify`: Each LOD level will be simplified
    - With `--enable-draco`: LOD0 stays uncompressed, LOD1/LOD2 will be compressed
    - Without `--enable-simplify`: Generates multiple LOD levels without simplification
  - **Recommended combination:** `--enable-lod --enable-simplify --enable-draco`
  - **Use case:** Large-scale scenes requiring distance-based dynamic loading

- `--enable-simplify` - Enable mesh simplification
  Reduces polygon count while preserving visual quality. Uses meshoptimizer library for vertex cache optimization, overdraw reduction, and adaptive simplification.
  - **Applies to:** OSGB, FBX, and Shapefile formats
  - **Impact:** Smaller file size, faster rendering, longer processing time
  - **Use case:** Large datasets where render performance is critical
  - **With LOD:** Controls whether simplification is applied to each LOD level

- `--enable-draco` - Enable Draco mesh compression
  Applies Google Draco compression to geometry data (vertices, normals, indices).
  - **Applies to:** OSGB, FBX, and Shapefile formats
  - **Impact:** 3-6x smaller geometry size, slower processing and decoding
  - **Note:** Requires a Draco-compatible viewer (e.g., CesiumJS with Draco loader)

- `--enable-texture-compress` - Enable KTX2 texture compression
  Compresses textures to KTX2 (Basis Universal) format.
  - **Applies to:** OSGB and FBX formats
  - **Impact:** Significantly reduces GPU memory usage and download size
  - **Note:** Requires a viewer with KTX2 support

- `--enable-pbr` - Enable PBR material support
  Generates glTF 2.0 PBR materials instead of legacy technique-based materials.
  - **Applies to:** OSGB and FBX formats
  - **Default:** Disabled (uses KHR_techniques_webgl or unlit materials)
  - **Use case:** Bandwidth-constrained scenarios, web streaming
  - **With LOD:**
    - Non-LOD mode: Compresses all output
    - LOD mode: LOD0 stays uncompressed, LOD1/LOD2 are compressed
  - **Note:** Requires client-side Draco decoder support

- `--enable-texture-compress` - Enable texture compression (KTX2)
  Converts textures to KTX2 format with GPU-friendly compression.
  - **Applies to:** OSGB format only
  - **Impact:** Faster GPU upload, smaller texture size
  - **Use case:** GPU memory optimization, faster texture loading
  - **Note:** Requires KTX2-compatible renderer

### Format Support Matrix

| Optimization Flag | OSGB | Shapefile | GLTF | B3DM |
|-------------------|------|-----------|------|------|
| `--enable-lod` | ❌ | ✅ | ❌ | ❌ |
| `--enable-simplify` | ✅ | ✅ | ❌ | ❌ |
| `--enable-draco` | ✅ | ✅ | ❌ | ❌ |
| `--enable-texture-compress` | ✅ | ❌ | ❌ | ❌ |

### Flag Combinations

```sh
# OSGB: Best for web streaming (maximum compression)
--enable-simplify --enable-draco --enable-texture-compress

# OSGB or Shapefile: Best for render performance (simplification only)
--enable-simplify

# OSGB: Best for bandwidth optimization (compression only)
--enable-draco --enable-texture-compress

# Shapefile: LOD mode (recommended configuration)
--enable-lod --enable-simplify --enable-draco

# Shapefile: LOD only without simplification
--enable-lod

# Shapefile: LOD + simplification without compression
--enable-lod --enable-simplify

# Shapefile: Draco compression only (without LOD)
--enable-draco

# Shapefile: Simplification + Draco compression (without LOD)
--enable-simplify --enable-draco
```

## Development

### Generate compile_commands.json for IDE support

`cargo build` drives CMake and auto-exports the compile database to `build/compile_commands.json` (handled in `build.rs`). Point VS Code C/C++/clangd to that file.

Manual refresh (optional):
```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cargo build -vv
```
If you also want a root-level symlink: `ln -sf build/compile_commands.json compile_commands.json`

### Build with strict warning checks (matching CI)

```bash
# Treat all warnings as errors (matches GitHub Actions behavior)
RUSTFLAGS="-D warnings" cargo build --release -vv
```

### Debug in VSCode

This project includes pre-configured VSCode debug configurations for multi-platform debugging:

#### Prerequisites

- Install **C/C++ extension** for VSCode
- Install **CodeLLDB extension** (for macOS debugging)
- On Linux: ensure `gdb` is installed (`sudo apt-get install gdb`)
- On macOS: Xcode Command Line Tools with `lldb` debugger
- On Windows: Visual Studio Build Tools with MSVC compiler

#### Available Debug Configurations

Press `F5` or go to **Run → Start Debugging** to select a debug configuration:

**Linux (using gdb):**
- **Debug (Linux, gdb)**: Debug OSGB to 3D-Tiles conversion
- **Debug SHP (Linux, gdb)**: Debug Shapefile to 3D-Tiles conversion

**macOS (using lldb):**
- **Debug (macOS, lldb)**: Debug OSGB conversion with pre-configured test data paths
- **Debug SHP (macOS, lldb)**: Debug Shapefile conversion

**Windows (using MSVC):**
- **Debug (Windows, MSVC)**: Debug OSGB conversion
- **Debug SHP (Windows, MSVC)**: Debug Shapefile conversion

#### Configuration Details

The debug configurations are defined in `.vscode/launch.json` and include:

- **Pre-build step**: Automatically builds the debug binary before launching debugger
- **Arguments**: Pre-configured command-line arguments for conversion (format, input, output paths)
- **Source languages**: Includes both Rust and C++ code
- **Working directory**: Set to project root

#### Customizing Debug Arguments

To change input/output paths for debugging, edit the `args` array in `.vscode/launch.json`:

```json
"args": [
  "-f", "osgb",              // Format: osgb, shape, gltf, b3dm, etc.
  "-i", "/path/to/input",   // Input file/directory path
  "-o", "/path/to/output"   // Output directory path
]
```

#### Debugging Tips

1. **Set Breakpoints**: Click on line numbers in any `.rs` or `.cpp` file to set breakpoints
2. **Step Through Code**: Use F10 (step over) or F11 (step into)
3. **Inspect Variables**: Hover over variables to see their values, or use the Debug Console
4. **View Call Stack**: Check the Call Stack panel to understand execution flow
5. **Debug Output**: Monitor console output to see conversion progress and errors

#### Build Task

The `.vscode/tasks.json` file defines a `cargo build` task that:
- Runs `cargo build` to compile debug binaries
- Supports multi-threaded problem matching with Rust compiler output
- Can be manually invoked with `Ctrl+Shift+B`

To debug release builds instead, modify the task in `.vscode/tasks.json`:

```json
"args": ["build", "--release"]
```
