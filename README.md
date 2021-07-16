**English | [简体中文](#简介)** 

# Introduction

[![glTF status](https://img.shields.io/badge/glTF-2%2E0-green.svg?style=flat)](https://github.com/KhronosGroup/glTF)
[![Action status](https://github.com/fanvanzh/3dtiles/actions/workflows/rust.yml/badge.svg)](https://github.com/fanvanzh/3dtiles/actions/workflows/rust.yml)

[![Build status](https://ci.appveyor.com/api/projects/status/lyhf989tnt9jhi9y?svg=true)](https://ci.appveyor.com/project/fanvanzh/3dtiles)



Tools for 3D-Tiles convertion.

This is a `RUST language` project with cpp lib to handle osgb data.

Tools provided are as follow：

- `Osgb(OpenSceneGraph Binary)` to `3D-Tiles`: convert huge of osgb file to 3D-Tiles.

- `Esri Shapefile` to `3D-Tiles`: convert shapefile to 3D-Tiles.

- `Fbx` to `3D-Tiles`: convert fbx file to 3D-Tiles, include auto_lod\texture convertion etc.



You may intereted in: 

- [How to build this project?](https://github.com/fanvanzh/3dtiles/wiki/How-to-build)

- [How to debug?](https://github.com/fanvanzh/3dtiles/wiki/How-to-debug)

- [Download Windows Pre-build](https://ci.appveyor.com/api/projects/fanvanzh/3dtiles/artifacts/3dtiles.zip?branch=master)

# Usage

## ① Command Line

```sh
3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② Examples

```sh
# from osgb dataset
3dtile.exe -f osgb -i E:\Data\hgc -o E:\Data\hgc_test
3dtile.exe -f osgb -i E:\Data\dayanta -o E:\Data\dayanta_test -c "{\"offset\": 0}"

# from single shp file
3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single osgb file to glb file
3dtile.exe -f gltf -i E:\Data\TT\001.osgb -o E:\Data\TT\001.glb

# from single obj file to glb file
3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```

## ③ Paramters

To Translate.

# Data Requirements & Announcement

To Translate.





---

**[English](#Introduction) | 简体中文**

<h1 id="intro">简介</h1>

3D-Tile 转换工具集，高效快速的 3D-Tiles 生产工具，极度节省你的处理时间。

这是一个混合了 c 和 c++ 库（主要是 osgb）的 Rust 项目。

提供了如下的子工具：

- `Osgb(OpenSceneGraph Binary)` 转 `3D-Tiles`

- `Esri Shapefile` 转 `3D-Tiles`

- `Fbx` 转 `3D-Tiles`
- ...

# 用法说明

## ① 命令行格式

```sh
3dtile.exe [FLAGS] [OPTIONS] --format <FORMAT> --input <PATH> --output <DIR>
```

## ② 示例命令

```sh
# from osgb dataset
3dtile.exe -f osgb -i E:\Data\hgc -o E:\Data\hgc_test
3dtile.exe -f osgb -i E:\Data\dayanta -o E:\Data\dayanta_test -c "{\"offset\": 0}"

# from single shp file
3dtile.exe -f shape -i E:\Data\aa.shp -o E:\Data\aa --height height

# from single osgb file to glb file
3dtile.exe -f gltf -i E:\Data\TT\001.osgb -o E:\Data\TT\001.glb

# from single obj file to glb file
3dtile.exe -f gltf -i E:\Data\TT\001.obj -o E:\Data\TT\001.glb

# convert single b3dm file to glb file
3dtile.exe -f b3dm -i E:\Data\aa.b3dm -o E:\Data\aa.glb
```

## ③ 参数说明

- `-c, --config <JSON>` 在命令行传入 json 配置的字符串，json 内容为选配，可部分实现

  json 示例：

  ``` json
  {
    "x": 120,
  	"y": 30,
  	"offset": 0 , // 模型最低面地面距离
  	"max_lvl" : 20 // 处理切片模型到20级停止
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

仅支持 WGS84 坐标系（EPSG:4326）的矢量数据。

### ③ 通用模型转 glTF：

支持 osg、osgb、obj、fbx、3ds 等单一通用模型数据转为 gltf、glb 格式。

转出格式为 2.0 的gltf，可在以下网址验证查看： https://pissang.github.io/clay-viewer/editor/

### ④ B3dm 单文件转 glb

支持将 b3dm 单个文件转成 glb 格式，便于调试程序和测试数据



---

# Who use / Who star

- NASA JPL (gkjohnson)
- AnalyticalGraphicsInc (kring)
- NVIDIA (Vinjn Zhang)
- Ubisoft (Cmdu76)
- Baidu (hinikai)
- Esri (suny323)
- Geostar (hekaikai\shitao1988)

- MapTalks (brucin\fuzhenn\axmand)
- Alibaba (luxueyan)
- Tencent (NichoZhang)
- Data Cloud Co- Ltd (liujin834)
- Tsinghua University (DeZhao-Zhang)
- Peking University (CHRIS-WiNG\Weizhen-Fang)
- Wuhan University (chenguanzhou)
- Guangzhou University (LreeLenn)
- Hopkins University (AndrewAnnex)

- 中国铁道科学设计研究院
- 上海华东设计研究院
- 江苏省测绘研究所
- 宁波市测绘设计研究院
- 合肥火星科技有限公司 (muyao1987)
- 北京西部数据科技 (vtxf\elfc2000)

# About author

作者不是专业搞三维GIS的，因偶尔有个需求要展示 3D-Tiles，一时找不到工具，就写了个轮子，代码多有纰漏，仅供参考。
