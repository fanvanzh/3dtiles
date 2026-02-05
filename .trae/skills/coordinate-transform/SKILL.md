---
name: "coordinate-transform"
description: "提供GIS坐标系统知识，包含ENU/ECEF/WGS84/CGCS2000等坐标系定义及转换方法。在FBX/Shapefile/OSGB转3D Tiles、模型朝向问题、坐标系定义问题时调用。"
---

# 坐标系统转换技能 (Coordinate Transform)

## 概述

本技能提供GIS和3D地理空间领域中常用坐标系统的详细知识，包括坐标系定义、特性及相互转换方法。适用于3D Tiles生产、倾斜摄影处理、BIM模型地理定位等场景。

## 坐标系统分类

### 1. 地心地固坐标系 (ECEF - Earth-Centered Earth-Fixed)

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:4978 |
| **类型** | 地心地固直角坐标系 |
| **原点** | 地球质心 |
| **单位** | 米 (meters) |
| **向上轴** | Z_UP |
| **X轴** | 指向本初子午线 (0°经度) 与赤道交点 |
| **Y轴** | 指向90°东经与赤道交点 |
| **Z轴** | 指向北极点 |
| **用途** | 3D Tiles全局坐标系，Cesium默认坐标系 |

**特点**：
- 坐标轴随地球自转而固定在地表
- 适合全球尺度3D数据表达
- 不适合局部测量（坐标值大，精度损失）

### 2. 地理坐标系 (Geographic Coordinate Systems)

#### 2.1 WGS84 (World Geodetic System 1984)

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:4326 |
| **类型** | 地理坐标系 (经纬度) |
| **单位** | 度 (degrees) |
| **坐标范围** | 经度: -180° ~ +180°, 纬度: -90° ~ +90° |
| **向上轴** | Y_UP (经度向东，纬度向北，高度向上) |
| **椭球体** | WGS84椭球 (a=6378137m, f=1/298.257223563) |
| **用途** | GPS定位、全球GIS数据交换 |

#### 2.2 CGCS2000 (中国2000国家大地坐标系)

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:4490 |
| **类型** | 地理坐标系 (经纬度) |
| **单位** | 度 (degrees) |
| **坐标范围** | 经度: -180° ~ +180°, 纬度: -90° ~ +90° |
| **向上轴** | Y_UP |
| **椭球体** | CGCS2000椭球 (a=6378137m, f=1/298.257222101) |
| **用途** | 中国境内测绘、国土调查 |

**注意**：CGCS2000与WGS84差异极小（厘米级），但在高精度应用中需区分。

### 3. 投影坐标系 (Projected Coordinate Systems)

#### 3.1 Web Mercator (网络墨卡托)

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:3857 |
| **类型** | 圆柱投影坐标系 |
| **单位** | 米 (meters) |
| **坐标范围** | X: -20037508.34 ~ +20037508.34, Y: 同X |
| **向上轴** | Z_UP (X向东，Y向北，Z向上) |
| **原点** | 经纬度 (0°, 0°) 对应投影坐标 (0, 0) |
| **用途** | Web地图 (Google Maps, OpenStreetMap, 高德, 百度) |

**特点**：
- 保持角度不变形，适合导航
- 高纬度地区面积变形严重
- 不支持纬度85°以上区域

#### 3.2 CGCS2000 3度带高斯-克吕格投影

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:4548 - EPSG:4554 (各中央经线不同) |
| **类型** | 横轴墨卡托投影 |
| **单位** | 米 (meters) |
| **分带方式** | 3度经度分带 |
| **向上轴** | Z_UP (X向东，Y向北，Z向上) |
| **False Easting** | 500,000 米 |
| **False Northing** | 0 米 |
| **中央经线** | 根据EPSG不同 (如EPSG:4548为117°E) |
| **用途** | 中国境内大比例尺测绘 |

**常见EPSG编码**：
| EPSG | 中央经线 | 覆盖范围 |
|------|----------|----------|
| EPSG:4548 | 117°E | 中国中部 |
| EPSG:4549 | 120°E | 中国东部 |
| EPSG:4550 | 123°E | 中国东北部 |
| EPSG:4551 | 126°E | 中国东北部 |
| EPSG:4552 | 129°E | 中国东北部 |
| EPSG:4553 | 132°E | 中国东北部 |
| EPSG:4554 | 135°E | 中国东北部 |

#### 3.3 CGCS2000 6度带高斯-克吕格投影

| 属性 | 说明 |
|------|------|
| **EPSG** | EPSG:4491 - EPSG:4506 |
| **类型** | 横轴墨卡托投影 |
| **单位** | 米 (meters) |
| **分带方式** | 6度经度分带 |
| **向上轴** | Z_UP |
| **False Easting** | 500,000 米 |
| **用途** | 中国境内中比例尺测绘 |

### 4. 局部坐标系 (Local Coordinate Systems)

#### 4.1 ENU坐标系 (East-North-Up)

| 属性 | 说明 |
|------|------|
| **类型** | 局部切平面直角坐标系 |
| **原点** | 参考点 (通常为测区中心) |
| **单位** | 米 (meters) |
| **向上轴** | Z_UP (E向东，N向北，U向上) |
| **X轴** | 东方向 (East) |
| **Y轴** | 北方向 (North) |
| **Z轴** | 天顶方向 (Up) |
| **用途** | 局部测量、工程测量、3D Tiles局部坐标 |

**特点**：
- 原点在参考点，坐标值小，精度高
- 适合局部区域（通常几公里范围内）
- 不同参考点的ENU坐标系不同

## 坐标系统转换

### 1. 常用转换路径

```
地理坐标系 (WGS84/CGCS2000)
    ↓ (大地坐标转地心坐标)
ECEF (地心地固坐标系)
    ↓ (局部切平面转换)
ENU (东-北-天坐标系)
    ↓ (3D Tiles坐标系)
3D Tiles 局部坐标
```

### 2. 转换公式

#### 2.1 地理坐标 → ECEF (WGS84)

```cpp
// 输入: lon(经度), lat(纬度), h(高度) - 单位: 度, 度, 米
// 输出: (X, Y, Z) - 单位: 米

const double a = 6378137.0;                  // WGS84长半轴
const double f = 1.0 / 298.257223563;        // WGS84扁率
const double e2 = f * (2.0 - f);             // 第一偏心率平方

double lon_rad = lon * PI / 180.0;
double lat_rad = lat * PI / 180.0;

double sinLat = sin(lat_rad);
double cosLat = cos(lat_rad);
double sinLon = sin(lon_rad);
double cosLon = cos(lon_rad);

// 卯酉圈曲率半径
double N = a / sqrt(1.0 - e2 * sinLat * sinLat);

// ECEF坐标
double X = (N + h) * cosLat * cosLon;
double Y = (N + h) * cosLat * sinLon;
double Z = (N * (1.0 - e2) + h) * sinLat;
```

#### 2.2 ECEF → ENU 变换矩阵

```cpp
// 输入: lon(经度), lat(纬度), h(高度) - 参考点
// 输出: 4x4变换矩阵 (ENU到ECEF)

// 首先计算参考点的ECEF坐标 (X0, Y0, Z0)
// 然后构建ENU基向量:

// 东方向 (East) - 沿纬度圈切线方向
glm::dvec3 east(-sinLon, cosLon, 0.0);

// 北方向 (North) - 沿经线切线方向
glm::dvec3 north(-sinLat * cosLon, -sinLat * sinLon, cosLat);

// 天方向 (Up) - 沿椭球法线方向
glm::dvec3 up(cosLat * cosLon, cosLat * sinLon, sinLat);

// 构建ENU到ECEF的变换矩阵 (列主序)
glm::dmat4 T(1.0);
T[0] = glm::dvec4(east,  0.0);    // 第一列: East
T[1] = glm::dvec4(north, 0.0);    // 第二列: North
T[2] = glm::dvec4(up,    0.0);    // 第三列: Up
T[3] = glm::dvec4(X0, Y0, Z0, 1.0); // 第四列: 平移

// ENU到ECEF: P_ecef = T * P_enu
// ECEF到ENU: P_enu = inverse(T) * P_ecef
```

#### 2.3 投影坐标 → 地理坐标 (GDAL)

```cpp
// 使用GDAL进行坐标转换
#include <ogr_spatialref.h>

// 创建源坐标系 (投影坐标系)
OGRSpatialReference srs;
srs.SetFromUserInput("EPSG:4548");  // 或其他EPSG代码

// 创建目标坐标系 (WGS84)
OGRSpatialReference wgs84;
wgs84.SetWellKnownGeogCS("WGS84");

// 设置传统GIS坐标轴顺序 (X=East, Y=North)
srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

// 创建坐标转换器
OGRCoordinateTransformation* transform =
    OGRCreateCoordinateTransformation(&srs, &wgs84);

// 执行转换
double x = 388231.963;  // 投影坐标X (东向)
double y = 3168121.924; // 投影坐标Y (北向)
double z = 0.0;

transform->Transform(1, &x, &y, &z);
// 结果: x=经度(度), y=纬度(度), z=高度

// 清理
OGRCoordinateTransformation::DestroyCT(transform);
```

### 3. 3D Tiles中的坐标转换

#### 3.1 Shapefile → 3D Tiles

```cpp
// 步骤1: 读取Shapefile坐标系
OGRSpatialReference* poSRS = poLayer->GetSpatialRef();

// 步骤2: 获取数据范围并计算中心点
OGREnvelope envelop;
poLayer->GetExtent(&envelop);
double center_x = (envelop.MinX + envelop.MaxX) / 2;
double center_y = (envelop.Miny + envelop.MaxY) / 2;

// 步骤3: 根据坐标系类型处理
if (sourceCS.isGeographic()) {
    // WGS84: 直接使用经纬度
    double lon = center_x;
    double lat = center_y;
    glm::dmat4 transform = make_transform(lon, lat, min_height);
} else if (sourceCS.isProjected()) {
    // 投影坐标: 先转换为WGS84
    glm::dvec3 geo = projectedToGeographic(
        glm::dvec3(center_x, center_y, 0.0), epsgCode);
    glm::dmat4 transform = make_transform(geo.x, geo.y, min_height);
}
```

#### 3.2 FBX → 3D Tiles

```cpp
// FBX通常使用局部坐标系，需要用户提供地理参考点

// 用户提供: 模型原点的经纬度和高度
double origin_lon = 115.8571025816;
double origin_lat = 28.6239248470;
double origin_height = 50.0;

// 构建ENU到ECEF的变换矩阵
glm::dmat4 enuToEcef = CalcEnuToEcefMatrix(origin_lon, origin_lat, origin_height);

// 应用Y-up到Z-up的转换 (如果FBX使用Y-up)
glm::dmat4 yUpToZUp = createYUpToZUpMatrix();

// 最终变换矩阵
glm::dmat4 finalTransform = enuToEcef * yUpToZUp;
```

#### 3.3 OSGB (倾斜摄影) → 3D Tiles

```cpp
// 从metadata.xml解析坐标信息
// 通常包含: SRSOrigin (局部坐标原点), SRS (坐标系)

// 步骤1: 解析SRSOrigin (局部坐标系原点的ECEF坐标)
double srsOriginX, srsOriginY, srsOriginZ;

// 步骤2: 解析坐标系 (可能是ENU或投影坐标系)
std::string srsWKT = parseSRSFromMetadata();

// 步骤3: 构建变换矩阵
if (isENU(srsWKT)) {
    // ENU坐标系: 原点在SRSOrigin
    glm::dmat4 transform = buildEnuTransform(srsOriginX, srsOriginY, srsOriginZ);
} else if (isProjected(srsWKT)) {
    // 投影坐标系: 需要转换为ECEF
    glm::dvec3 geo = projectedToGeographic(localCoord, epsgCode);
    glm::dvec3 ecef = geographicToEcef(geo.x, geo.y, geo.z);
    // ...
}
```

## 常见问题与解决方案

### 问题1: 模型朝向不正确

**原因**：
- ENU坐标系朝向计算错误
- 坐标轴顺序混淆 (X/Y交换)
- 上轴方向错误 (Y-up vs Z-up)

**解决**：
```cpp
// 确保GDAL使用传统GIS坐标轴顺序
srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

// 确保ENU基向量计算正确
glm::dvec3 east(-sinLon, cosLon, 0.0);
glm::dvec3 north(-sinLat * cosLon, -sinLat * sinLon, cosLat);
glm::dvec3 up(cosLat * cosLon, cosLat * sinLon, sinLat);
```

### 问题2: 投影坐标转换后位置错误

**原因**：
- 未正确设置GDAL坐标轴映射策略
- 投影坐标与地理坐标混淆

**解决**：
```cpp
// 投影坐标系中心点需要转换为经纬度后再使用
double center_lon = (minx + maxx) / 2;  // 投影坐标X
double center_lat = (miny + maxy) / 2;  // 投影坐标Y

// 转换为WGS84
glm::dvec3 geo = projectedToGeographic(
    glm::dvec3(center_lon, center_lat, 0.0), "EPSG:4548");

// 使用转换后的经纬度构建变换矩阵
glm::dmat4 transform = make_transform(geo.x, geo.y, height);
```

### 问题3: 3D Tiles层级结构异常

**原因**：
- 四叉树分割阈值设置不当
- 地理坐标系和投影坐标系的单位不同

**解决**：
```cpp
// 根据坐标系类型设置合适的分割阈值
if (sourceCS.isGeographic()) {
    // WGS84: 0.01度约等于1公里
    metric = 0.01;
} else if (sourceCS.isProjected()) {
    // 投影坐标系: 使用米为单位
    metric = 1000.0;  // 1公里
}
```

## 坐标系判断方法

```cpp
// 判断坐标系类型
CoordinateSystem cs;

if (cs.epsgCode == "EPSG:4326") {
    // WGS84地理坐标系
} else if (cs.epsgCode == "EPSG:4490") {
    // CGCS2000地理坐标系
} else if (cs.epsgCode == "EPSG:4978") {
    // ECEF地心地固坐标系
} else if (cs.epsgCode == "EPSG:3857") {
    // Web Mercator投影
} else if (cs.epsgCode.find("EPSG:454") == 0) {
    // CGCS2000 3度带高斯-克吕格投影
} else if (cs.epsgCode.find("EPSG:449") == 0) {
    // CGCS2000 6度带高斯-克吕格投影
}

// 判断坐标系类别
if (cs.isGeographic()) {  // 地理坐标系 (经纬度)
    // WGS84, CGCS2000等
} else if (cs.isProjected()) {  // 投影坐标系 (米)
    // 高斯-克吕格, Web Mercator等
} else if (cs.isCartesian()) {  // 笛卡尔坐标系
    // ECEF等
} else if (cs.isLocal()) {  // 局部坐标系
    // ENU等
}
```

## 5. 笛卡尔局部米坐标系 (Cartesian Local Meter)

### 5.1 定义

| 属性 | 说明 |
|------|------|
| **类型** | 局部笛卡尔直角坐标系 |
| **原点** | 切片中心点 (ENU坐标系原点) |
| **单位** | 米 (meters) |
| **向上轴** | Z_UP (X向东，Y向北，Z向上) |
| **X轴** | 东方向 (East) - 局部东向 |
| **Y轴** | 北方向 (North) - 局部北向 |
| **Z轴** | 高度方向 (Up) - 垂直向上 |
| **用途** | 3D Tiles切片内部顶点坐标、glTF模型坐标 |

### 5.2 与ENU坐标系的关系

笛卡尔局部米坐标系与ENU坐标系**本质上是同一个坐标系**：
- **ENU**: 强调方向 (East-North-Up)
- **笛卡尔局部米**: 强调坐标类型 (笛卡尔坐标 + 米单位)

在3D Tiles中：
- 切片级别使用ENU/笛卡尔局部米坐标系
- 根tileset使用ECEF坐标系 (通过transform矩阵转换)

### 5.3 转换流程

```
地理坐标 (WGS84/CGCS2000)
    ↓ 1. 确定参考点 (切片中心)
    ↓ 2. 地理坐标 → ECEF
ECEF坐标 (全局)
    ↓ 3. ECEF → ENU (局部)
ENU/笛卡尔局部米坐标 (切片局部)
    ↓ 4. 顶点坐标相对于切片中心
3D Tiles切片内部坐标
```

**详细步骤**：

```cpp
// 步骤1: 确定切片中心点 (地理坐标)
double center_lon = (min_lon + max_lon) / 2;
double center_lat = (min_lat + max_lat) / 2;
double center_height = 0.0;

// 步骤2: 切片中心点 → ECEF
glm::dvec3 center_ecef = geographicToEcef(center_lon, center_lat, center_height);

// 步骤3: 构建ENU到ECEF的变换矩阵
glm::dmat4 enu_to_ecef = CalcEnuToEcefMatrix(center_lon, center_lat, center_height);

// 步骤4: 计算ECEF到ENU的逆变换
glm::dmat4 ecef_to_enu = glm::inverse(enu_to_ecef);

// 步骤5: 将顶点从地理坐标转换为ENU/笛卡尔局部米坐标
// 输入: 顶点地理坐标 (lon, lat, height)
glm::dvec3 vertex_ecef = geographicToEcef(vertex_lon, vertex_lat, vertex_height);
glm::dvec4 vertex_enu = ecef_to_enu * glm::dvec4(vertex_ecef, 1.0);

// vertex_enu.x, vertex_enu.y, vertex_enu.z 即为笛卡尔局部米坐标
// 这些坐标将直接存储在B3DM/glTF中
```

### 5.4 3D Tiles中的坐标层级

```
根tileset.json (ECEF坐标系)
    ├── transform: ENU→ECEF矩阵 (将局部坐标转换到全局)
    └── root.boundingVolume.box (ECEF坐标)

    子tileset.json (笛卡尔局部米坐标系)
        ├── boundingVolume.box (局部坐标，中心在原点)
        └── content.b3dm
            └── glTF顶点坐标 (笛卡尔局部米，相对于切片中心)
```

**关键点**：
- 每个切片的顶点坐标都是相对于切片中心的**局部坐标**
- 切片中心通过父tileset的transform矩阵定位到ECEF全局坐标系
- 这种设计避免了大坐标值导致的精度损失

## 坐标问题定位工具与方法

### 工具1: GDAL命令行工具

#### 1.1 查看Shapefile坐标系

```bash
# 查看Shapefile的完整信息，包括坐标系
ogrinfo -so your_data.shp layer_name

# 示例输出关键信息:
# Layer SRS WKT: 坐标系WKT描述
# EPSG: 从WKT解析的EPSG代码
# Extent: 数据范围
```

**实际案例**：
```bash
$ ogrinfo -so building.shp building

INFO: Open of `building.shp' using driver `ESRI Shapefile' successful.

Layer name: building
Geometry: Polygon
Feature Count: 39
Extent: (387939.205682, 3167858.813433) - (388524.719888, 3168385.033921)
Layer SRS WKT:
PROJCRS["CGCS2000 / 3-degree Gauss-Kruger CM 117E",
    BASEGEOGCRS["China Geodetic Coordinate System 2000", ...]
    ...
]
```

**分析**：
- `Extent`数值较大 (数十万) → 投影坐标系 (米)
- `Layer SRS WKT`包含 `Gauss-Kruger` → 高斯-克吕格投影
- `CGCS2000` → 中国2000坐标系
- 中央经线117°E → 对应EPSG:4548

#### 1.2 坐标系转换验证

```bash
# 将Shapefile从源坐标系转换为WGS84，验证坐标是否正确
ogr2ogr -f "ESRI Shapefile" -t_srs EPSG:4326 output_wgs84.shp input_projected.shp

# 查看转换后的坐标范围
ogrinfo -so output_wgs84.shp output_wgs84
```

#### 1.3 投影坐标转经纬度

```bash
# 使用cs2cs进行单点转换
echo "388231.963 3168121.924" | cs2cs EPSG:4548 EPSG:4326

# 输出: 115.8570962067 28.6239243154 0.0000000000
# 验证: 经度115.85°E, 纬度28.62°N → 中国江西省区域 ✓
```

### 工具2: 倾斜摄影metadata.xml解析

#### 2.1 metadata.xml结构

```xml
<?xml version="1.0" encoding="utf-8"?>
<ModelMetadata version="1.0">
    <!-- 坐标系信息 -->
    <SRS>EPSG:4548</SRS>  <!-- 或WKT描述 -->
    <SRSOrigin>388231.963,3168121.924,0</SRSOrigin>

    <!-- 数据范围 -->
    <SRSExtent>
        <MinX>387939.205682</MinX>
        <MinY>3167858.813433</MinY>
        <MaxX>388524.719888</MaxX>
        <MaxY>3168385.033921</MaxY>
    </SRSExtent>

    <!-- 其他元数据 -->
    <Texture>
        <ColorFormat>jpg</ColorFormat>
    </Texture>
</ModelMetadata>
```

#### 2.2 解析步骤

```cpp
// 步骤1: 解析SRS (Spatial Reference System)
std::string srs = parseXmlElement(metadata, "SRS");
// 可能是: "EPSG:4548" 或完整WKT

// 步骤2: 解析SRSOrigin (局部坐标系原点)
std::string origin_str = parseXmlElement(metadata, "SRSOrigin");
// 格式: "X,Y,Z" 或 "X Y Z"
std::vector<double> origin = parseCoordinates(origin_str);
double origin_x = origin[0];  // 388231.963
double origin_y = origin[1];  // 3168121.924
double origin_z = origin[2];  // 0.0

// 步骤3: 判断坐标系类型并处理
if (srs.find("EPSG:") != std::string::npos) {
    // EPSG代码直接可用
    std::string epsg = extractEPSG(srs);

    if (isGeographicEPSG(epsg)) {
        // 地理坐标系: SRSOrigin就是经纬度
        double lon = origin_x;
        double lat = origin_y;
        double height = origin_z;
    } else if (isProjectedEPSG(epsg)) {
        // 投影坐标系: 需要转换为地理坐标
        glm::dvec3 geo = projectedToGeographic(
            glm::dvec3(origin_x, origin_y, origin_z), epsg);
        double lon = geo.x;
        double lat = geo.y;
        double height = geo.z;
    }
} else {
    // WKT格式: 需要解析
    OGRSpatialReference srs_obj;
    srs_obj.importFromWkt(srs.c_str());
    // ...进一步处理
}
```

#### 2.3 常见问题定位

**问题**: metadata.xml中的SRSOrigin是投影坐标，但程序按地理坐标处理

**症状**: 3D Tiles位置完全错误 (可能在非洲或海洋中)

**排查**：
```bash
# 1. 查看metadata.xml中的SRS
$ cat metadata.xml | grep -A5 "<SRS>"
<SRS>EPSG:4548</SRS>

# 2. 查看SRSOrigin
$ cat metadata.xml | grep "<SRSOrigin>"
<SRSOrigin>388231.963,3168121.924,0</SRSOrigin>

# 3. 判断: EPSG:4548是投影坐标系，但数值看起来不像经纬度
# 388度经度无效，所以一定是投影坐标

# 4. 验证转换
$ echo "388231.963 3168121.924" | cs2cs EPSG:4548 EPSG:4326
115.8570962067 28.6239243154 0.0000000000

# 结论: SRSOrigin需要转换为经纬度后再使用
```

### 工具3: FBX转换坐标原点计算

#### 3.1 用户提供的信息

```cpp
// 用户提供的模型定位信息
struct ModelLocation {
    double longitude;   // 经度 (度)
    double latitude;    // 纬度 (度)
    double altitude;    // 海拔高度 (米)
    double heading;     // 航向角 (度，可选)
    double pitch;       // 俯仰角 (度，可选)
    double roll;        // 横滚角 (度，可选)
};

ModelLocation loc = {
    115.8571025816,  // 经度
    28.6239248470,   // 纬度
    50.0,            // 海拔
    0.0, 0.0, 0.0    // 无旋转
};
```

#### 3.2 计算3D Tiles坐标

```cpp
// 步骤1: 地理坐标 → ECEF
glm::dvec3 ecef = geographicToEcef(loc.longitude, loc.latitude, loc.altitude);
// 结果: ecef = (-2443593.8, 5041986.5, 3037379.7)

// 步骤2: 构建ENU到ECEF的变换矩阵
glm::dmat4 enu_to_ecef = CalcEnuToEcefMatrix(
    loc.longitude, loc.latitude, loc.altitude);

// 步骤3: 如果FBX使用Y-up坐标系，添加Y-up到Z-up的转换
glm::dmat4 yup_to_zup = glm::dmat4(
    1.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0
);

// 步骤4: 组合变换矩阵 (注意顺序: 先Y-up转Z-up，再ENU转ECEF)
glm::dmat4 final_transform = enu_to_ecef * yup_to_zup;

// 步骤5: 将变换矩阵写入tileset.json
tileset["root"]["transform"] = flattenMatrix(final_transform);
```

#### 3.3 验证计算结果

```cpp
// 验证: 将局部坐标(0,0,0)转换回ECEF，应该等于原点ECEF坐标
glm::dvec4 local_origin(0.0, 0.0, 0.0, 1.0);
glm::dvec4 ecef_check = final_transform * local_origin;

// ecef_check应该约等于步骤1计算的ecef
// 如果差异很大，说明变换矩阵计算错误

// 验证ENU基向量正交性
glm::dvec3 east = glm::dvec3(final_transform[0]);
glm::dvec3 north = glm::dvec3(final_transform[1]);
glm::dvec3 up = glm::dvec3(final_transform[2]);

// 点积应该接近0 (正交)
assert(abs(glm::dot(east, north)) < 1e-10);
assert(abs(glm::dot(east, up)) < 1e-10);
assert(abs(glm::dot(north, up)) < 1e-10);

// 长度应该接近1 (单位向量)
assert(abs(glm::length(east) - 1.0) < 1e-10);
assert(abs(glm::length(north) - 1.0) < 1e-10);
assert(abs(glm::length(up) - 1.0) < 1e-10);
```

### 工具4: 3D Tiles坐标验证

#### 4.1 验证tileset.json

```bash
# 1. 检查transform矩阵是否合理
# 平移部分(最后4个值)应该在地球半径范围内 (约6371km)
# 如果数值是几百或几千，可能是单位错误

# 2. 使用Cesium验证
# 在Cesium中加载tileset，检查:
# - 位置是否正确
# - 朝向是否正确 (东向是否指向东)
# - 高度是否正确

# 3. 使用3D Tiles Inspector工具
# 查看bounding volume是否正确包围模型
```

#### 4.2 调试输出

```cpp
// 在代码中添加调试输出，追踪坐标转换过程

// 1. 输入坐标
fprintf(stderr, "[DEBUG] Input: lon=%.10f, lat=%.10f, h=%.3f\n",
        longitude, latitude, height);

// 2. ECEF坐标
glm::dvec3 ecef = geographicToEcef(longitude, latitude, height);
fprintf(stderr, "[DEBUG] ECEF: X=%.3f, Y=%.3f, Z=%.3f\n",
        ecef.x, ecef.y, ecef.z);

// 3. 变换矩阵
fprintf(stderr, "[DEBUG] Transform matrix:\n");
for (int i = 0; i < 4; i++) {
    fprintf(stderr, "  %.6f %.6f %.6f %.6f\n",
            transform[0][i], transform[1][i],
            transform[2][i], transform[3][i]);
}

// 4. 验证: 将(0,0,0)转换到ECEF
glm::dvec4 test(0.0, 0.0, 0.0, 1.0);
glm::dvec4 result = transform * test;
fprintf(stderr, "[DEBUG] Origin in ECEF: %.3f, %.3f, %.3f\n",
        result.x, result.y, result.z);
```

## 参考资源

- [EPSG注册表](https://epsg.org/)
- [GDAL坐标系文档](https://gdal.org/tutorials/osr_api_tut.html)
- [Cesium坐标系](https://cesium.com/learn/cesiumjs-learn/cesiumjs-coordinate-systems/)
- [3D Tiles规范](https://github.com/CesiumGS/3d-tiles/tree/main/specification)
- [PROJ坐标转换](https://proj.org/)

- [EPSG注册表](https://epsg.org/)
- [GDAL坐标系文档](https://gdal.org/tutorials/osr_api_tut.html)
- [Cesium坐标系](https://cesium.com/learn/cesiumjs-learn/cesiumjs-coordinate-systems/)
- [3D Tiles规范](https://github.com/CesiumGS/3d-tiles/tree/main/specification)
