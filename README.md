**English | [简体中文](#简介)**

# Introduction

[![glTF status](https://img.shields.io/badge/glTF-2%2E0-green.svg?style=flat)](https://github.com/KhronosGroup/glTF)
[![Action status](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/linux.yml)
[![Action status](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/windows.yml)

3D-Tiles convertion:

- `Osgb(OpenSceneGraph Binary)` to `3D-Tiles`: convert huge of osgb file to 3D-Tiles.

- `Esri Shapefile` to `3D-Tiles`: convert shapefile to 3D-Tiles.

You may intereted in:

- [How to build?](https://github.com/fanvanzh/3dtiles/wiki/How-to-build)

- [How to debug?](https://github.com/fanvanzh/3dtiles/wiki/How-to-debug)

- [Download Windows Pre-build](https://github.com/fanvanzh/3dtiles/releases/download/v0.4/3dtile.zip)

- [Docker Image](https://hub.docker.com/r/winner1/3dtiles)

# Build
## Ubuntu
```
sudo apt-get update
sudo apt-get install -y g++ libgdal-dev libopenscenegraph-dev cargo
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
cargo build --release
```
## Centos
```
sudo yum install -y gdal-devel cargo g++
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install osg:x64-linux-release
cargo build --release
```
## Windows
```
curl -O https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe
.\rustup-init.exe -y
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install osg:x64-windows-release
.\vcpkg\vcpkg install gdal:x64-windows-release
cargo build --release
```
# Usage

## ① Command Line

```sh
_3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② Examples

```sh
# from osgb dataset
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"offset\": 0}"
# use pbr-texture
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"pbr\": true}"

# from single shp file
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single osgb file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.osgb -o E:\Data\TT\001.glb

# from single obj file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
_3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```
---

**[English](#Introduction) | 简体中文**

<h1 id="intro">简介</h1>

3D-Tile 转换工具集，高效快速的 3D-Tiles 生产工具。

提供了如下的子工具：

- `Osgb(OpenSceneGraph Binary)` 转 `3D-Tiles`

- `Esri Shapefile` 转 `3D-Tiles`

# 生成compile_commands.json便于vscode索引C++头文件
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 编译
## Ubuntu
```
sudo apt-get update
sudo apt-get install -y g++ libgdal-dev libopenscenegraph-dev cargo
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
cargo build --release
```
## Centos
```
sudo yum install -y gdal-devel cargo g++
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install osg:x64-linux-release
cargo build --release
```
## Windows
```
curl -O https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe
.\rustup-init.exe -y
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install osg:x64-windows-release
.\vcpkg\vcpkg install gdal:x64-windows-release
cargo build --release
```
## MacOS
```
#install brew first
brew install rust gdal open-scene-graph
git clone https://github.com/fanvanzh/3dtiles.git
cd 3dtiles
cargo build --release
```
# 使用说明

## ① 命令行格式

```sh
_3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② 示例命令

```sh
# from osgb dataset
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path
_3dtile.exe -f osgb -i E:\osgb_path -o E:\out_path -c "{\"offset\": 0}"

# from single shp file
_3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single osgb file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.osgb -o E:\Data\TT\001.glb

# from single obj file to glb file
_3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
_3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```

## ③ 参数说明

- `-c, --config <JSON>` 在命令行传入 json 配置的字符串，json 内容为选配，可部分实现

  json 示例：

  ``` json
  {
    "x": 120,
    "y": 30,
    "offset": 0,
    "max_lvl" : 20
  }
  ```


- `-f, --format <FORMAT>` 输入数据格式。

  `FORMAT` 可选：osgb, shape, gltf, b3dm

  `osgb` 为倾斜摄影格式数据, `shape` 为 Shapefile 面数据, `gltf` 为单一通用模型转gltf, `b3dm` 为单个3dtile二进制数据转gltf

- `-i, --input <PATH>` 输入数据的目录，osgb数据截止到 `<DIR>/Data` 目录的上一级，其他格式具体到文件名。

- `-o, --output <DIR>` 输出目录。输出的数据文件位于 `<DIR>/Data` 目录。

- `--height` 高度字段。指定shapefile中的高度属性字段，此项为转换 shp 时的必须参数。



# 数据要求及说明

### ① 倾斜摄影数据

倾斜摄影数据仅支持 smart3d 格式的 osgb 组织方式：

- 数据目录必须有一个 `“Data”` 目录的总入口；
- `“Data”` 目录同级放置一个 `metadata.xml` 文件用来记录模型的位置信息；
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
