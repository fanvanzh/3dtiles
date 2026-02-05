# 3D Tiles回归测试框架 - 完整方案

## 一、项目概述

### 1.1 项目背景
3dtiles工程是一个将多种3D数据格式（OSGB、FBX、Shapefile、倾斜摄影等）转换为3D Tiles格式的工具。为了确保代码重构不会破坏数据的一致性，需要建立一个完整的回归测试框架。

### 1.2 项目目标
- **数据一致性验证**：确保重构前后生成的3D Tiles数据完全一致
- **渲染问题检测**：在不使用可视化工具的情况下，检测可能导致渲染失败的问题
- **自动化测试**：提供完整的自动化测试流程，支持CI/CD集成
- **多格式支持**：支持glTF/GLB、B3DM、I3DM、CMPT、Tileset.json等3D Tiles规范中的所有格式

### 1.3 技术栈
- **语言**：Python 3.8+
- **框架**：自定义Python框架
- **官方验证工具**：
  - gltf-validator（gltf-validator）
  - 3d-tiles-validator
- **依赖库**：
  - pydantic（数据验证）
  - rich（终端美化）

## 二、测试数据分布

### 2.1 数据目录结构
```
data/
├── FBX/                    # FBX模型文件
│   ├── 20260128_114012_012.fbx
│   ├── AS-501生产运维楼.fbx
│   ├── AS-501生产运维楼-m.fbx
│   ├── TZCS_0829.FBX
│   ├── taizhou-indoor.fbx
│   └── 仪征_D1_DC_AS_CD-三维视图.fbx
├── Oblique/                # 倾斜摄影数据
│   ├── OSGBny/            # 完整倾斜摄影目录
│   │   ├── Data/
│   │   │   ├── Tile_+005_+033/
│   │   │   ├── Tile_+006_+005/
│   │   │   ├── Tile_+006_+006/
│   │   │   ├── Tile_+006_+007/
│   │   │   ├── Tile_+006_+009/
│   │   │   └── Tile_+032_+031/
│   │   └── metadata.xml
│   └── part/              # 部分倾斜摄影目录
│       └── Data/
├── OSGB/                   # 单个OSGB文件
│   ├── bench.osgb
│   ├── power_tower.osgb
│   ├── power_tower2.osgb
│   └── street_lamp.osgb
└── SHP/                    # Shapefile文件
    ├── 1758464001_building/
    │   ├── 1758464001_building.shp
    │   ├── 1758464001_building.dbf
    │   ├── 1758464001_building.prj
    │   └── 1758464001_building.shx
    ├── building_cgcs2000/
    │   └── building_cgcs2000.shp
    ├── building_epsg4548/
    │   └── building_epsg4548.shp
    └── nanjing_buildings/
        └── 南京市_百度建筑.shp
```

### 2.2 数据类型统计
- **FBX文件**：6个
- **倾斜摄影目录**：2个（OSGBny完整、part部分）
- **OSGB文件**：4个
- **Shapefile**：4个（不同坐标系）

## 三、测试套件设计

### 3.1 测试套件分类

#### 3.1.1 冒烟测试（smoke）
**目的**：快速验证核心功能，确保基本转换流程正常

**测试用例**（4个）：
1. **osgb_to_gltf_bench** - 单个OSGB转glTF
   - 输入：data/OSGB/bench.osgb
   - 输出：bench.glb
   - 验证：几何体、材质

2. **shapefile_to_3dtiles** - Shapefile转3dtiles
   - 输入：data/SHP/1758464001_building/1758464001_building.shp
   - 输出：tileset.json
   - 验证：tileset结构、包围体

3. **oblique_to_3dtiles** - 倾斜摄影转3dtiles
   - 输入：data/Oblique/OSGBny
   - 输出：tileset.json
   - 验证：瓦片层级、LOD结构

4. **fbx_to_3dtiles** - FBX转3dtiles
   - 输入：data/FBX/taizhou-indoor.fbx
   - 输出：tileset.json
   - 验证：tileset结构、地理坐标

**预计时间**：< 5分钟

#### 3.1.2 OSGB转换测试（osgb_conversion）
**目的**：验证各种OSGB模型的转换

**测试用例**（4个）：
1. osgb_to_gltf_bench - bench模型（P0）
2. osgb_to_gltf_power_tower - 电力塔模型（P1）
3. osgb_to_gltf_power_tower2 - 电力塔模型2（P1）
4. osgb_to_gltf_street_lamp - 路灯模型（P1）

**预计时间**：< 2分钟

#### 3.1.3 Shapefile转换测试（shapefile_conversion）
**目的**：验证不同坐标系的Shapefile转换

**测试用例**（4个）：
1. shapefile_to_3dtiles_1758464001 - 1758464001建筑轮廓（P0）
2. shapefile_to_3dtiles_cgcs2000 - CGCS2000坐标系建筑（P1）
3. shapefile_to_3dtiles_epsg4548 - EPSG4548坐标系建筑（P1）
4. shapefile_to_3dtiles_nanjing - 南京市建筑（P2）

**预计时间**：< 3分钟

#### 3.1.4 FBX转换测试（fbx_conversion）
**目的**：验证各种FBX模型的转换

**测试用例**（6个）：
1. fbx_to_3dtiles_taizhou_indoor - 台州室内模型（P0）
2. fbx_to_3dtiles_as501 - AS-501生产运维楼（P1）
3. fbx_to_3dtiles_as501_m - AS-501生产运维楼简化版（P1）
4. fbx_to_3dtiles_tzcs - TZCS模型（P2）
5. fbx_to_3dtiles_yizheng - 仪征三维视图模型（P2）
6. fbx_to_3dtiles_20260128 - 20260128模型（P2）

**预计时间**：< 6分钟

#### 3.1.5 倾斜摄影转换测试（oblique_conversion）
**目的**：验证倾斜摄影数据的转换

**测试用例**（2个）：
1. oblique_to_3dtiles_osgbny - OSGBny完整目录（P0）
2. oblique_to_3dtiles_part - part部分目录（P1）

**预计时间**：< 10分钟

### 3.2 测试用例总数
- **总测试用例数**：20个
- **P0优先级**：6个（核心功能）
- **P1优先级**：8个（重要功能）
- **P2优先级**：6个（次要功能）

## 四、验证策略

### 4.1 验证层级

#### 4.1.1 格式验证（Format Validation）
**目的**：验证文件格式是否符合规范

**工具**：
- gltf-validator（用于glTF/GLB文件）
- 3d-tiles-validator（用于tileset.json文件）

**验证内容**：
- 文件头部格式
- 必需字段
- 数据类型
- 数组长度

#### 4.1.2 结构验证（Structural Validation）
**目的**：验证数据结构的正确性

**验证内容**：
- JSON结构完整性
- 引用有效性（buffer、bufferView、accessor等）
- 层级结构正确性
- 必需属性存在性

#### 4.1.3 内容验证（Content Validation）
**目的**：验证数据内容的合理性

**验证内容**：
- 几何体数据（顶点、索引）
- 材质数据（PBR参数）
- 纹理数据（UV坐标）
- 法线数据

#### 4.1.4 性能验证（Performance Validation）
**目的**：验证性能指标

**验证内容**：
- 文件大小
- 顶点数量
- 三角形数量
- 纹理数量

#### 4.1.5 一致性验证（Consistency Validation）
**目的**：验证与基线的一致性

**验证内容**：
- 字节级比较（严格模式）
- 浮点数容差比较（宽松模式）
- 结构一致性检查

### 4.2 预渲染验证（Pre-Rendering Validation）

#### 4.2.1 glTF/GLB致命问题检测
**问题类型**：

1. **Invalid buffer accessors**
   - 描述：缓冲区访问器错误
   - 渲染影响：会导致缓冲区访问错误，可能崩溃
   - 检测方法：检查bufferView的byteOffset和byteLength是否在buffer范围内

2. **Invalid indices**
   - 描述：索引错误
   - 渲染影响：会导致渲染错误或崩溃
   - 检测方法：检查indices数量是否为3的倍数

3. **Missing required fields**
   - 描述：缺失必需字段
   - 渲染影响：会导致加载失败
   - 检测方法：检查asset、scenes、accessors等必需字段

4. **Invalid material references**
   - 描述：材质引用错误
   - 渲染影响：会导致材质无法加载
   - 检测方法：检查material索引是否有效

#### 4.2.2 tileset.json致命问题检测
**问题类型**：

1. **Missing 'refine' property**
   - 描述：缺失refine属性
   - 渲染影响：Cesium可能无法正确渲染或崩溃
   - 修复建议：在root tile中添加'refine': 'ADD'

2. **Invalid boundingVolume**
   - 描述：包围体错误
   - 渲染影响：Cesium无法正确剔除瓦片
   - 检测方法：检查boundingVolume.box格式是否正确

3. **Invalid geometricError**
   - 描述：几何误差错误
   - 渲染影响：Cesium无法正确计算LOD
   - 检测方法：检查geometricError是否为非负数

4. **Invalid tile hierarchy**
   - 描述：瓦片层级错误
   - 渲染影响：Cesium无法正确计算LOD
   - 检测方法：检查子瓦片的geometricError是否小于父瓦片

5. **Missing content files**
   - 描述：内容文件缺失
   - 渲染影响：Cesium无法加载瓦片内容
   - 检测方法：检查content.uri指向的文件是否存在

### 4.3 验证模式

#### 4.3.1 Strict模式（严格模式）
**用途**：发布前验证

**配置**：
- 浮点容差：1e-9
- 使用官方验证器：是
- 内容验证：启用
- 字节级比较：启用

**适用场景**：
- 代码发布前
- 重要功能验证
- 数据质量保证

#### 4.3.2 Relaxed模式（宽松模式）
**用途**：开发阶段验证

**配置**：
- 浮点容差：1e-4
- 使用官方验证器：是
- 内容验证：启用
- 字节级比较：启用

**适用场景**：
- 日常开发
- 功能迭代
- 性能优化

#### 4.3.3 Fast模式（快速模式）
**用途**：快速反馈

**配置**：
- 浮点容差：1e-2
- 使用官方验证器：否
- 内容验证：禁用
- 字节级比较：禁用

**适用场景**：
- 冒烟测试
- 快速验证
- CI快速检查

## 五、框架架构

### 5.1 模块设计

```
3dtiles_regression/
├── __init__.py                    # 包初始化
├── config.py                      # 配置解析模块
├── converter.py                   # 格式转换器
├── validators/                     # 验证器模块
│   ├── __init__.py
│   ├── official_tools.py         # 官方工具集成
│   └── pre_rendering_validator.py # 预渲染验证器
├── runner.py                     # 测试运行器
├── reporter.py                    # 报告生成器
└── cli.py                        # 命令行入口
```

### 5.2 核心模块说明

#### 5.2.1 Config模块（config.py）
**职责**：加载和解析测试配置

**主要类**：
- `Config`：配置管理器
- `TestCase`：测试用例数据类
- `TestSuite`：测试套件数据类
- `ValidationMode`：验证模式数据类

**主要功能**：
- 加载test_config.json
- 解析测试套件
- 解析验证模式
- 解析执行配置

#### 5.2.2 Converter模块（converter.py）
**职责**：调用_3dtile可执行文件进行格式转换

**主要类**：
- `Converter`：格式转换器
- `ConversionResult`：转换结果数据类
- `ConversionStatus`：转换状态枚举

**主要功能**：
- 构建转换命令
- 执行转换
- 处理超时
- 解析转换输出

#### 5.2.3 Validators模块（validators/）
**职责**：验证生成的3D数据

**主要类**：
- `OfficialValidator`：官方验证器集成
- `PreRenderingValidator`：预渲染验证器
- `ValidationResult`：验证结果数据类
- `ValidationIssue`：验证问题数据类

**主要功能**：
- 调用gltf-validator
- 调用3d-tiles-validator
- 检测glTF/GLB致命问题
- 检测tileset.json致命问题
- 生成验证报告

#### 5.2.4 Runner模块（runner.py）
**职责**：执行测试并收集结果

**主要类**：
- `TestRunner`：测试运行器
- `TestResult`：测试结果数据类
- `SuiteResult`：测试套件结果数据类

**主要功能**：
- 执行测试套件
- 并发执行测试
- 超时控制
- 错误处理和重试
- 进度跟踪

#### 5.2.5 Reporter模块（reporter.py）
**职责**：生成测试报告

**主要类**：
- `ReportGenerator`：报告生成器

**主要功能**：
- 生成JSON报告
- 生成HTML报告
- 统计测试结果
- 显示验证问题

#### 5.2.6 CLI模块（cli.py）
**职责**：提供命令行接口

**主要功能**：
- 解析命令行参数
- 提供子命令（generate、run、validate、report）
- 执行相应操作

## 六、执行配置

### 6.1 预定义执行配置

#### 6.1.1 本地开发（local_dev）
**目的**：快速验证

**配置**：
- 测试套件：smoke
- 验证模式：fast
- 并发数：1
- 超时：60秒

**预计时间**：< 1分钟

#### 6.1.2 提交前（pre_commit）
**目的**：验证关键功能

**配置**：
- 测试套件：smoke, osgb_conversion
- 验证模式：strict
- 优先级：P0
- 并发数：2
- 超时：180秒

**预计时间**：< 3分钟

#### 6.1.3 CI标准（ci_standard）
**目的**：核心功能测试

**配置**：
- 测试套件：smoke, osgb_conversion, shapefile_conversion, fbx_conversion
- 验证模式：strict
- 并发数：4
- 超时：300秒

**预计时间**：< 5分钟

#### 6.1.4 CI完整（ci_full）
**目的**：包含倾斜摄影

**配置**：
- 测试套件：所有测试套件
- 验证模式：relaxed
- 并发数：4
- 超时：600秒

**预计时间**：< 15分钟

#### 6.1.5 发布前（release）
**目的**：完整测试

**配置**：
- 测试套件：所有测试套件
- 验证模式：strict
- 并发数：2
- 超时：900秒

**预计时间**：< 30分钟

### 6.2 使用示例

#### 6.2.1 生成基线
```bash
# 生成所有测试套件的基线
python3 -m 3dtiles_regression generate --suite all

# 生成冒烟测试套件的基线
python3 -m 3dtiles_regression generate --suite smoke

# 强制覆盖已存在的基线
python3 -m 3dtiles_regression generate --suite smoke --force
```

#### 6.2.2 运行测试
```bash
# 运行冒烟测试（快速验证）
python3 -m 3dtiles_regression run --suite smoke --mode fast

# 运行核心功能测试（严格模式）
python3 -m 3dtiles_regression run --suite core --mode strict

# 运行所有测试
python3 -m 3dtiles_regression run --suite all --mode relaxed

# 并发执行（加速测试）
python3 -m 3dtiles_regression run --suite core --parallel 4
```

#### 6.2.3 查看报告
测试完成后，会在输出目录生成两种格式的报告：

- **JSON报告**：`test_data/test_output/current/report.json`
- **HTML报告**：`test_data/test_output/current/report.html`

直接在浏览器中打开HTML报告即可查看详细的测试结果和验证问题。

## 七、报告格式

### 7.1 JSON报告格式
```json
{
  "timestamp": "2024-01-01T00:00:00Z",
  "metadata": {
    "validation_mode": "strict",
    "parallel": 4
  },
  "summary": {
    "total_suites": 5,
    "total_tests": 20,
    "total_passed": 18,
    "total_failed": 2,
    "total_skipped": 0,
    "pass_rate": 90.0,
    "total_duration_ms": 300000
  },
  "suites": [
    {
      "name": "smoke",
      "total_tests": 4,
      "passed": 4,
      "failed": 0,
      "skipped": 0,
      "duration_ms": 60000,
      "tests": [...]
    }
  ]
}
```

### 7.2 HTML报告格式
HTML报告包含以下内容：

1. **测试概览**
   - 总测试套件数
   - 总测试数
   - 通过/失败/跳过数量
   - 通过率
   - 总耗时

2. **测试套件详情**
   - 每个测试套件的测试结果
   - 每个测试用例的详细信息
   - 转换结果（时间、文件大小）
   - 验证结果（错误数、警告数）

3. **验证问题详情**
   - 问题代码
   - 严重程度（ERROR/WARNING）
   - 问题描述
   - 渲染影响
   - 修复建议

## 八、CI/CD集成

### 8.1 GitHub Actions示例
```yaml
name: Regression Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Set up Python
      uses: actions/setup-python@v2
      with:
        python-version: '3.8'

    - name: Install dependencies
      run: |
        cd .trae/skills/3dtiles-regression-validation
        pip install -r requirements.txt

    - name: Install Node.js and validators
      run: |
        npm install -g gltf-validator
        npm install -g 3d-tiles-validator

    - name: Build project
      run: cargo build --release

    - name: Run regression tests
      run: |
        cd .trae/skills/3dtiles-regression-validation
        python3 -m 3dtiles_regression run --suite smoke --mode strict

    - name: Upload reports
      uses: actions/upload-artifact@v2
      with:
        name: test-reports
        path: test_data/test_output/current/
```

### 8.2 GitLab CI示例
```yaml
stages:
  - test

regression_test:
  stage: test
  image: python:3.8

  before_script:
    - cd .trae/skills/3dtiles-regression-validation
    - pip install -r requirements.txt
    - npm install -g gltf-validator
    - npm install -g 3d-tiles-validator

  script:
    - cargo build --release
    - python3 -m 3dtiles_regression run --suite smoke --mode strict

  artifacts:
    paths:
      - test_data/test_output/current/
    expire_in: 1 week
```

## 九、常见问题

### 9.1 官方验证器未找到？
**解决方案**：

1. 使用npm全局安装：
```bash
npm install -g gltf-validator
npm install -g 3d-tiles-validator
```

2. 使用npx（无需全局安装）：
```bash
# 框架会自动使用npx运行
```

3. 禁用官方验证器：
```bash
python3 -m 3dtiles_regression run --suite core --no-official
```

### 9.2 转换超时？
**检查要点**：

1. 输入文件是否过大
2. 调整测试用例的超时设置（在test_config.json中）
3. 优化转换参数
4. 检查可执行文件是否正常工作

### 9.3 验证失败但转换成功？
**排查步骤**：

1. 查看详细报告（HTML报告）
2. 重点关注预渲染验证中的致命问题
3. 检查渲染影响说明
4. 根据修复建议修复问题

## 十、总结

本方案提供了一个完整的3D Tiles回归测试框架，具有以下特点：

1. **全面覆盖**：覆盖所有主要数据格式和转换流程
2. **深度验证**：不仅验证格式，还验证渲染相关问题
3. **灵活配置**：支持多种验证模式和执行配置
4. **自动化**：完全自动化，支持CI/CD集成
5. **易用性**：提供清晰的命令行接口和详细的报告

通过本框架，可以确保代码重构不会破坏数据的一致性，并且能够提前发现可能导致渲染失败的问题，大大提高开发效率和代码质量。
