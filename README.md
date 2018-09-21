# 3dtiles

[![Build status](https://ci.appveyor.com/api/projects/status/lyhf989tnt9jhi9y?svg=true)](https://ci.appveyor.com/project/fanvanzh/3dtiles)

The fastest tools for 3dtiles convert in the world!

include these tools：

osgb => 3dtile , convert huge of osgb file to 3dtiles.

shapefile => 3dtile,  convert shape file to 3dtiles.

fbx => 3dtile, convert fbx file to 3dtile, include auto_lod\texture convert etc..

[How to build / 编译指南](https://github.com/fanvanzh/3dtiles/wiki/How-to-build)

[How to debug / vs调试指南](https://github.com/fanvanzh/3dtiles/wiki/How-to-debug)

[Windows pre-build / 预编译下载](https://ci.appveyor.com/api/projects/fanvanzh/3dtiles/artifacts/3dtiles.zip?branch=master)

## 3dtile 转换工具集。
### 世界上最快的 3dtiles 转换工具，极度节省你的处理时间。

### 命令行： 
	3dtile.exe [FLAGS] [OPTIONS] --format <osgb,shape> --input <FILE> --output <FILE>

### 示例：
	3dtile.exe -f osgb -i E:\Data\倾斜摄影\hgc -o E:\Data\倾斜摄影\hgc_test
	
	3dtile.exe -f osgb -i E:\Data\倾斜摄影\dayanta -o E:\Data\倾斜摄影\dayanta_test -c "{\"offset\": 0}"

### 参数说明：

  -c, --config <config>      
```
     {
	
      "x": 120,
      
      "y": 30,
      
      "offset": 0 , // 模型最低面地面距离
      
      "max_lvl" : 20 // 处理切片模型到20级停止
      
     }
```
```			     
  -f, --format <osgb,shape> 
  
  -i, --input <FILE> 
	
  -o, --output <FILE> 
  
  --height, 指定shapefile的高度字段
```
#### 命令行参数详解：
```
-c 在命令行传入 json 配置的字符串, json 内容为选配，可部分实现。

-f 输入数据格式： osgb 为倾斜摄影格式数据。

-i 输入数据的目录，截止到 "\Data" 目录的上一级。

-o 输出目录。最终结果位于输出目录的 "\Data" 目录。

--height 高度字段。指定shapefile中的高度属性字段。
```
### 数据说明：

**1、倾斜摄影数据：**

倾斜摄影数据仅支持 smart3d 格式的 osgb 组织方式， 数据目录必须有一个 `“Data”` 目录的总入口， `“Data”` 目录同级放置一个 `metadata.xml` 文件用来记录模型的位置信息。

每个瓦片目录下，必须有个和目录名同名的 osgb 文件，否则无法识别根节点。

**正确的目录结构如下：**

```
--metadata.xml

--Data\Tile_000_000\Tile_000_000.osgb

```



**2、shapefile 数据：**

目前仅支持 shapefile 的面数据，可用于建筑物轮廓批量生成 3dtile。

shapefile 中需要有字段来表示高度信息。

# Who use / Who star

. AnalyticalGraphicsInc

. Geostar

. MapTalks

. Alibaba

. Guangzhou University

. Hopkins University

. 中国铁道科学设计研究院

. 上海华东设计研究院

. 江苏省测绘研究所

. 宁波市测绘设计研究院

. 合肥火星科技有限公司

. 北京五维智能科技

. 北京西部数据科技

# About author

作者不是专业搞三维GIS的，因偶尔有个需求要展示3dtile，一时找不到工具，就写了个轮子，代码多有纰漏，仅供参考。

QQ：1378240129

