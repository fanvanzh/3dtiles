# 3D Tiles回归测试框架 - 快速参考

## 测试用例总览

### 测试套件统计

| 测试套件 | 测试用例数 | P0 | P1 | P2 | 预计时间 |
|---------|-----------|----|----|----|---------|
| smoke | 4 | 4 | 0 | 0 | < 5分钟 |
| osgb_conversion | 4 | 1 | 3 | 0 | < 2分钟 |
| shapefile_conversion | 4 | 1 | 2 | 1 | < 3分钟 |
| fbx_conversion | 6 | 1 | 2 | 3 | < 6分钟 |
| oblique_conversion | 2 | 1 | 1 | 0 | < 10分钟 |
| **总计** | **20** | **8** | **8** | **4** | **< 30分钟** |

### 冒烟测试（smoke）- 4个测试用例

| 测试用例 | 输入 | 输出 | 优先级 | 超时 | 验证内容 |
|---------|------|------|-------|------|---------|
| osgb_to_gltf_bench | data/OSGB/bench.osgb | bench.glb | P0 | 60s | 几何体、材质 |
| shapefile_to_3dtiles | data/SHP/1758464001_building/1758464001_building.shp | tileset.json | P0 | 60s | tileset结构、包围体 |
| oblique_to_3dtiles | data/Oblique/OSGBny | tileset.json | P0 | 300s | 瓦片层级、LOD结构 |
| fbx_to_3dtiles | data/FBX/taizhou-indoor.fbx | tileset.json | P0 | 120s | tileset结构、地理坐标 |

### OSGB转换测试（osgb_conversion）- 4个测试用例

| 测试用例 | 输入 | 输出 | 优先级 | 超时 |
|---------|------|------|-------|------|
| osgb_to_gltf_bench | data/OSGB/bench.osgb | bench.glb | P0 | 60s |
| osgb_to_gltf_power_tower | data/OSGB/power_tower.osgb | power_tower.glb | P1 | 60s |
| osgb_to_gltf_power_tower2 | data/OSGB/power_tower2.osgb | power_tower2.glb | P1 | 60s |
| osgb_to_gltf_street_lamp | data/OSGB/street_lamp.osgb | street_lamp.glb | P1 | 60s |

### Shapefile转换测试（shapefile_conversion）- 4个测试用例

| 测试用例 | 输入 | 输出 | 优先级 | 超时 | 坐标系 |
|---------|------|------|-------|------|-------|
| shapefile_to_3dtiles_1758464001 | data/SHP/1758464001_building/1758464001_building.shp | tileset.json | P0 | 60s | - |
| shapefile_to_3dtiles_cgcs2000 | data/SHP/building_cgcs2000/building_cgcs2000.shp | tileset.json | P1 | 60s | CGCS2000 |
| shapefile_to_3dtiles_epsg4548 | data/SHP/building_epsg4548/building_epsg4548.shp | tileset.json | P1 | 60s | EPSG4548 |
| shapefile_to_3dtiles_nanjing | data/SHP/nanjing_buildings/南京市_百度建筑.shp | tileset.json | P2 | 120s | - |

### FBX转换测试（fbx_conversion）- 6个测试用例

| 测试用例 | 输入 | 输出 | 优先级 | 超时 |
|---------|------|------|-------|------|
| fbx_to_3dtiles_taizhou_indoor | data/FBX/taizhou-indoor.fbx | tileset.json | P0 | 120s |
| fbx_to_3dtiles_as501 | data/FBX/AS-501生产运维楼.fbx | tileset.json | P1 | 120s |
| fbx_to_3dtiles_as501_m | data/FBX/AS-501生产运维楼-m.fbx | tileset.json | P1 | 120s |
| fbx_to_3dtiles_tzcs | data/FBX/TZCS_0829.FBX | tileset.json | P2 | 120s |
| fbx_to_3dtiles_yizheng | data/FBX/仪征_D1_DC_AS_CD-三维视图.fbx | tileset.json | P2 | 120s |
| fbx_to_3dtiles_20260128 | data/FBX/20260128_114012_012.fbx | tileset.json | P2 | 120s |

### 倾斜摄影转换测试（oblique_conversion）- 2个测试用例

| 测试用例 | 输入 | 输出 | 优先级 | 超时 |
|---------|------|------|-------|------|
| oblique_to_3dtiles_osgbny | data/Oblique/OSGBny | tileset.json | P0 | 300s |
| oblique_to_3dtiles_part | data/Oblique/part | tileset.json | P1 | 300s |

## 常用命令

### 生成基线

```bash
# 生成所有测试套件的基线
python3 -m 3dtiles_regression generate --suite all

# 生成冒烟测试套件的基线
python3 -m 3dtiles_regression generate --suite smoke

# 生成指定测试用例的基线
python3 -m 3dtiles_regression generate --test osgb_to_gltf_bench

# 强制覆盖已存在的基线
python3 -m 3dtiles_regression generate --suite smoke --force

# 显示详细信息
python3 -m 3dtiles_regression generate --suite smoke --verbose
```

### 运行测试

```bash
# 运行冒烟测试（快速验证）
python3 -m 3dtiles_regression run --suite smoke --mode fast

# 运行核心功能测试（严格模式）
python3 -m 3dtiles_regression run --suite osgb_conversion --mode strict

# 运行所有测试
python3 -m 3dtiles_regression run --suite all --mode relaxed

# 并发执行（加速测试）
python3 -m 3dtiles_regression run --suite core --parallel 4

# 按优先级过滤
python3 -m 3dtiles_regression run --suite all --priority P0

# 指定输出目录
python3 -m 3dtiles_regression run --suite smoke --output /path/to/output

# 显示详细信息
python3 -m 3dtiles_regression run --suite smoke --verbose
```

### 验证数据

```bash
# 验证所有测试套件
python3 -m 3dtiles_regression validate --suite all

# 验证指定测试套件
python3 -m 3dtiles_regression validate --suite smoke

# 使用宽松模式验证
python3 -m 3dtiles_regression validate --suite all --mode relaxed

# 不使用官方验证器
python3 -m 3dtiles_regression validate --suite all --no-official
```

### 查看报告

测试完成后，报告会生成在输出目录：

- **JSON报告**：`test_data/test_output/current/report.json`
- **HTML报告**：`test_data/test_output/current/report.html`

直接在浏览器中打开HTML报告即可查看详细的测试结果和验证问题。

## 验证模式对比

| 模式 | 浮点容差 | 官方验证器 | 内容验证 | 字节级比较 | 适用场景 |
|-----|---------|-----------|---------|-----------|---------|
| strict | 1e-9 | 是 | 是 | 是 | 发布前验证 |
| relaxed | 1e-4 | 是 | 是 | 是 | 开发阶段验证 |
| fast | 1e-2 | 否 | 否 | 否 | 快速反馈 |

## 执行配置

| 配置 | 测试套件 | 验证模式 | 并发数 | 超时 | 预计时间 | 适用场景 |
|-----|---------|---------|-------|------|---------|---------|
| local_dev | smoke | fast | 1 | 60s | < 1分钟 | 本地开发 |
| pre_commit | smoke, osgb_conversion | strict | 2 | 180s | < 3分钟 | 提交前 |
| ci_standard | smoke, osgb_conversion, shapefile_conversion, fbx_conversion | strict | 4 | 300s | < 5分钟 | CI标准 |
| ci_full | 所有测试套件 | relaxed | 4 | 600s | < 15分钟 | CI完整 |
| release | 所有测试套件 | strict | 2 | 900s | < 30分钟 | 发布前 |

## 预渲染验证问题

### glTF/GLB致命问题

| 问题代码 | 严重程度 | 渲染影响 | 修复建议 |
|---------|---------|---------|---------|
| INVALID_BUFFER_ACCESSOR | ERROR | 会导致缓冲区访问错误，可能崩溃 | 检查bufferView的byteOffset和byteLength |
| INVALID_INDICES | ERROR | 会导致渲染错误或崩溃 | 检查indices数量是否为3的倍数 |
| MISSING_REQUIRED_FIELD | ERROR | 会导致加载失败 | 添加缺失的必需字段 |
| INVALID_MATERIAL_REFERENCE | ERROR | 会导致材质无法加载 | 检查material索引是否有效 |

### tileset.json致命问题

| 问题代码 | 严重程度 | 渲染影响 | 修复建议 |
|---------|---------|---------|---------|
| MISSING_REFINE_PROPERTY | ERROR | Cesium可能无法正确渲染或崩溃 | 在root tile中添加'refine': 'ADD' |
| INVALID_BOUNDING_VOLUME | ERROR | Cesium无法正确剔除瓦片 | 检查boundingVolume.box格式是否正确 |
| INVALID_GEOMETRIC_ERROR | ERROR | Cesium无法正确计算LOD | 检查geometricError是否为非负数 |
| INVALID_TILE_HIERARCHY | ERROR | Cesium无法正确计算LOD | 检查子瓦片的geometricError是否小于父瓦片 |
| MISSING_CONTENT_FILE | ERROR | Cesium无法加载瓦片内容 | 确保content.uri指向的文件存在 |

## 项目结构

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

test_config.json                   # 测试配置文件
requirements.txt                   # Python依赖
QUICKSTART.md                      # 快速开始指南
COMPLETE_SOLUTION.md               # 完整方案文档
example_usage.py                   # 使用示例
```

## 快速开始

### 1. 安装依赖

```bash
cd /Users/wallance/Developer/cim/thirdparty/3dtiles/.trae/skills/3dtiles-regression-validation
pip install -r requirements.txt
```

### 2. 安装官方验证器（可选，推荐）

```bash
npm install -g gltf-validator
npm install -g 3d-tiles-validator
```

### 3. 生成基线

```bash
python3 -m 3dtiles_regression generate --suite smoke
```

### 4. 运行测试

```bash
python3 -m 3dtiles_regression run --suite smoke --mode fast
```

### 5. 查看报告

在浏览器中打开 `test_data/test_output/current/report.html`

## 常见问题

### Q: 官方验证器未找到？

**A**: 有以下几种解决方案：

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

### Q: 转换超时？

**A**: 检查以下几点：

1. 输入文件是否过大
2. 调整测试用例的超时设置（在test_config.json中）
3. 优化转换参数
4. 检查可执行文件是否正常工作

### Q: 验证失败但转换成功？

**A**: 查看详细报告：

1. 查看HTML报告中的验证问题
2. 重点关注预渲染验证中的致命问题
3. 检查渲染影响说明
4. 根据修复建议修复问题
