# 坐标系统 API 文档

## 概述

本文档描述3D Tiles项目中坐标系统相关的核心API。坐标系统模块位于 `src/core/coordinate/` 目录下，提供统一的坐标转换和变换矩阵构建功能。

## 架构

```
src/core/coordinate/
├── coordinate_system.h/cpp    - 坐标系统定义
├── coordinate_converter.h/cpp - 坐标转换算法
├── enu_context.h/cpp          - ENU上下文管理
└── transform_builder.h/cpp    - 变换矩阵构建
```

## 核心类

### 1. CoordinateSystem

坐标系统定义结构体，描述坐标系统的类型、基准面、EPSG代码等属性。

#### 枚举类型

**CoordinateType** - 坐标系统类型
```cpp
enum class CoordinateType {
    CARTESIAN,   // 笛卡尔坐标系
    ECEF,        // 地心地固坐标系
    GEOGRAPHIC,  // 地理坐标系（经纬度）
    PROJECTED,   // 投影坐标系
    LOCAL        // 局部坐标系
};
```

**UpAxis** - 上轴方向
```cpp
enum class UpAxis {
    X_UP,  // X轴向上
    Y_UP,  // Y轴向上（如FBX）
    Z_UP   // Z轴向上（3D Tiles标准）
};
```

**GeographicDatum** - 地理基准面
```cpp
enum class GeographicDatum {
    UNKNOWN,  // 未知
    WGS84,    // WGS84
    CGCS2000  //  CGCS2000
};
```

#### 主要属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `type` | CoordinateType | 坐标系统类型 |
| `datum` | GeographicDatum | 地理基准面 |
| `epsgCode` | std::string | EPSG代码（如"EPSG:4326"） |
| `upAxis` | UpAxis | 上轴方向 |
| `unitFactor` | double | 单位转换因子 |
| `isMeterUnit` | bool | 是否为米单位 |
| `center` | std::optional<glm::dvec3> | 中心点坐标 |

#### 主要方法

```cpp
// 判断坐标系统类型
bool isGeographic() const;  // 是否为地理坐标系
bool isProjected() const;   // 是否为投影坐标系
bool isEcef() const;        // 是否为ECEF坐标系
bool isWgs84() const;       // 是否为WGS84
bool isCgcs2000() const;    // 是否为CGCS2000

// 从Shapefile创建坐标系统
static CoordinateSystem fromShapefile(
    const OGRSpatialReference* spatialRef,
    const glm::dvec3& refPoint
);

// 获取中心点
glm::dvec3 getCenter() const;

// 转换为字符串表示
std::string toString() const;
```

---

### 2. CoordinateConverter

坐标转换器，提供各种坐标系统之间的转换功能。

#### WGS84 常量

```cpp
static constexpr double WGS84_A = 6378137.0;                    // 长半轴（米）
static constexpr double WGS84_F = 1.0 / 298.257223563;         // 扁率
static constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);  // 第一偏心率平方
static constexpr double WGS84_B = WGS84_A * (1.0 - WGS84_F);   // 短半轴（米）
```

#### 静态方法

**地理坐标转ECEF**
```cpp
static glm::dvec3 geographicToEcef(double lon, double lat, double height);
```
- 输入：经度（度）、纬度（度）、高度（米）
- 输出：ECEF坐标（X, Y, Z），单位米

**ECEF转地理坐标**
```cpp
static void ecefToGeographic(const glm::dvec3& ecef,
                              double& lon, double& lat, double& height);
```
- 输入：ECEF坐标（X, Y, Z），单位米
- 输出：经度（度）、纬度（度）、高度（米）

**计算ENU到ECEF变换矩阵**
```cpp
static glm::dmat4 calcEnuToEcefMatrix(double lon, double lat, double height);
```
- 输入：ENU原点经纬高
- 输出：4x4变换矩阵（列主序）

**投影坐标转地理坐标**
```cpp
static glm::dvec3 projectedToGeographic(
    const glm::dvec3& projCoord,
    const std::string& srsDefinition
);
```
- 支持EPSG代码（如"EPSG:4548"）或WKT字符串
- 使用GDAL进行坐标转换

**地理坐标转局部ENU坐标**
```cpp
static glm::dvec3 geographicToLocalMeter(
    const glm::dvec3& geoCoord,
    const glm::dvec3& centerGeo
);
```
- 使用近似公式进行小范围转换
- 适用于小范围局部坐标计算

---

### 3. ENUContext

ENU（东-北-天）坐标系统上下文。

ENU是局部切平面坐标系：
- **X轴**：指向东
- **Y轴**：指向北
- **Z轴**：指向天（远离地心）

#### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `originEcef` | glm::dvec3 | ENU原点在ECEF坐标系中的位置 |
| `enuToEcefMatrix` | glm::dmat4 | ENU到ECEF的变换矩阵 |
| `ecefToEnuMatrix` | glm::dmat4 | ECEF到ENU的变换矩阵 |
| `originLon` | double | 原点经度（度） |
| `originLat` | double | 原点纬度（度） |
| `originHeight` | double | 原点高度（米） |

#### 方法

**从地理坐标创建**
```cpp
static ENUContext fromGeographic(double lon, double lat, double height);
```

**从ECEF坐标创建**
```cpp
static ENUContext fromEcef(const glm::dvec3& ecefOrigin);
```

**验证**
```cpp
bool validate() const;
```
检查变换矩阵是否有效且互为逆矩阵。

**坐标转换**
```cpp
glm::dvec3 enuToEcef(const glm::dvec3& enu) const;
glm::dvec3 ecefToEnu(const glm::dvec3& ecef) const;
```

---

### 4. TransformBuilder

变换矩阵构建器，用于创建3D Tiles所需的各种变换矩阵。

#### 方法

**ENU到ECEF变换**
```cpp
static glm::dmat4 buildEnuToEcefTransform(
    double lon, double lat, double height,
    const glm::dvec3& localCenter = glm::dvec3(0)
);
```

**轴向转换**
```cpp
static glm::dmat4 buildYUpToZUpTransform();  // Y-up转Z-up
static glm::dmat4 buildZUpToYUpTransform();  // Z-up转Y-up
```

**中心偏移**
```cpp
static glm::dmat4 buildCenterOffsetMatrix(const glm::dvec3& center);
```
将坐标平移，使center成为原点。

**单位缩放**
```cpp
static glm::dmat4 buildUnitScaleMatrix(double fromScale, double toScale);
```
例如：fromScale=0.001(毫米), toScale=1.0(米)，得到缩放因子0.001

**完整Tileset变换**
```cpp
static glm::dmat4 buildTilesetTransform(
    const CoordinateSystem& source,
    const glm::dvec3& modelCenter,
    const glm::dvec3& localOffset = glm::dvec3(0)
);
```
构建用于tileset.json的完整变换矩阵。

**矩阵工具**
```cpp
static bool validateTransform(const glm::dmat4& transform);
static bool isIdentity(const glm::dmat4& transform, double tolerance = 1e-10);
static std::vector<double> serializeMatrix(const glm::dmat4& mat);
static glm::dmat4 deserializeMatrix(const std::vector<double>& data);
```

---

## 使用示例

### 示例1：地理坐标转ECEF

```cpp
#include "core/coordinate/coordinate_converter.h"

using namespace Tiles::Core::Geo;

// 北京天安门坐标
double lon = 116.4074;
double lat = 39.9042;
double height = 0.0;

// 转换为ECEF
glm::dvec3 ecef = CoordinateConverter::geographicToEcef(lon, lat, height);
// ecef ≈ (-2175863.5, 4388656.2, 4070111.3)
```

### 示例2：创建ENU上下文

```cpp
#include "core/coordinate/enu_context.h"

// 在指定位置创建ENU坐标系
ENUContext enu = ENUContext::fromGeographic(116.4074, 39.9042, 0.0);

// 验证
if (enu.validate()) {
    // 将ENU坐标转换为ECEF
    glm::dvec3 enuCoord(100.0, 200.0, 50.0);  // 东100m，北200m，高50m
    glm::dvec3 ecefCoord = enu.enuToEcef(enuCoord);
}
```

### 示例3：构建Tileset变换矩阵

```cpp
#include "core/coordinate/coordinate_system.h"
#include "core/coordinate/transform_builder.h"

// 定义源坐标系统
CoordinateSystem source;
source.type = CoordinateType::GEOGRAPHIC;
source.epsgCode = "EPSG:4326";
source.datum = GeographicDatum::WGS84;
source.upAxis = UpAxis::Z_UP;

// 模型中心点
glm::dvec3 modelCenter(116.4074, 39.9042, 0.0);

// 构建变换矩阵
glm::dmat4 transform = TransformBuilder::buildTilesetTransform(source, modelCenter);

// 序列化为tileset.json格式
std::vector<double> matrixArray = TransformBuilder::serializeMatrix(transform);
// matrixArray 包含16个元素，按行主序排列
```

### 示例4：投影坐标转换

```cpp
#include "core/coordinate/coordinate_converter.h"

// CGCS2000 / 3-degree Gauss-Kruger zone 39 (EPSG:4548)
// 投影坐标（米）
glm::dvec3 projCoord(395000.0, 4420000.0, 0.0);

// 转换为地理坐标
glm::dvec3 geoCoord = CoordinateConverter::projectedToGeographic(
    projCoord, "EPSG:4548"
);
// geoCoord ≈ (lon=116.4, lat=39.9, height=0.0)
```

---

## 兼容层

为保持向后兼容，以下文件作为兼容层保留：

- `src/coordinate_transformer.h/cpp` - 委托到新实现
- `src/GeoTransform.h/cpp` - 委托到新实现

这些文件保持原有API不变，内部调用新的核心类实现。

---

## 测试

坐标系统模块包含完整的单元测试：

```bash
# 运行所有坐标系统测试
cd build && ctest -R coordinate --output-on-failure

# 运行特定测试
./tests/unit/test_coordinate_converter
./tests/unit/test_enu_context
./tests/unit/test_transform_builder
```

测试覆盖：
- 地理坐标与ECEF双向转换
- ENU矩阵计算和验证
- 投影坐标转换
- 变换矩阵构建
- 边界条件处理

---

## 依赖

- **glm** - 线性代数库
- **GDAL** - 地理空间数据抽象库（用于投影转换）
- **fmt** - 格式化库

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 2.0 | 2025-02 | 重构为新的核心类架构 |
| 1.0 | - | 原始实现 |
