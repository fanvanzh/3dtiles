---
name: "3dtiles-regression-validation"
description: "3D Tiles重构回归验证与数据质量验证技能。确保重构前后的代码生成完全一致的3D Tiles数据，支持多格式验证、字节级比较、结构一致性检查，集成gltf-validator、3d-tiles-validator、3d-tiles-tools进行深度验证。当用户需要进行代码重构、验证数据质量或调试格式错误时调用。"
---

# 3D Tiles 重构回归验证与数据质量验证技能

## 概述

本技能提供完整的3D Tiles验证解决方案，包含两大核心功能：

1. **回归验证**: 确保代码重构后生成的瓦片数据与重构前完全一致
2. **数据质量验证**: 使用官方工具验证3D Tiles数据格式规范性和质量

通过多层次验证策略，从文件结构到二进制内容，实现全方位的验证覆盖。

## 适用场景

- **代码重构**: 重构C++/Rust代码后验证输出一致性
- **依赖升级**: 升级第三方库（如OSG、GDAL）后验证兼容性
- **功能优化**: 优化算法后确保结果不变
- **数据质量检查**: 验证生成的3D Tiles是否符合规范
- **格式错误调试**: 发现和修复数据格式问题
- **CI/CD集成**: 自动化回归测试和质量检查

## 文件结构

```
.trae/skills/3dtiles-regression-validation/
├── SKILL.md                          # 本文件
├── test_config.json                  # 测试配置（v2.0）
├── regression_validator_v2.py        # 回归验证脚本
├── tiles_validator.py                # 数据质量验证脚本
├── run_tests.py                      # 测试执行器
├── generate_baseline.sh              # 基准数据生成脚本
└── run_regression_test.sh            # 综合测试工作流
```

## 快速开始

### 1. 安装依赖

#### gltf-validator（glTF验证工具）

从 [GitHub Releases](https://github.com/KhronosGroup/glTF-Validator/releases) 下载对应操作系统的预编译二进制文件：

**macOS (Apple Silicon - M1/M2/M3):**
```bash
# 下载最新版本
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-macos-arm64.zip

# 解压
unzip gltf-validator.zip

# 移动到系统路径
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator

# 验证安装
gltf-validator --version
```

**macOS (Intel):**
```bash
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-macos-amd64.zip
unzip gltf-validator.zip
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator
```

**Linux:**
```bash
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-linux-amd64.zip
unzip gltf-validator.zip
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator
```

**Windows:**
1. 下载 `gltf_validator-2.0.0-dev.3.10-win64.zip`
2. 解压到任意目录
3. 将目录添加到系统 PATH 环境变量

#### 3d-tiles-validator 和 3d-tiles-tools

```bash
# 通过 npm 安装
npm install -g 3d-tiles-validator 3d-tiles-tools

# 验证安装
npx 3d-tiles-validator --version
npx 3d-tiles-tools --help
```

#### 完整验证

```bash
# 验证所有工具已安装
gltf-validator --version
npx 3d-tiles-validator --version
npx 3d-tiles-tools --help
```

### 2. 数据质量验证（独立使用）

```bash
# 验证单个 tileset.json
python3 tiles_validator.py ./output/tileset.json --type tileset

# 验证单个 glb 文件
python3 tiles_validator.py ./output/tile.b3dm --type gltf

# 验证整个目录
python3 tiles_validator.py ./output --type directory --recursive --verbose

# 保存验证报告
python3 tiles_validator.py ./output --report validation_report.json
```

### 3. 回归测试（完整流程）

```bash
# 使用综合脚本
cd .trae/skills/3dtiles-regression-validation
./run_regression_test.sh --all

# 或使用Python测试执行器
python3 run_tests.py core --mode strict

# 生成基准数据
./generate_baseline.sh --suite core

# 运行特定测试套件
python3 run_tests.py optimization --mode relaxed
```

## 验证工具详解

### tiles_validator.py - 数据质量验证

#### 命令行选项

```bash
# 基本用法
python3 tiles_validator.py <路径> [选项]

# 选项说明
--type {auto,tileset,gltf,directory}  # 验证类型（默认自动检测）
--recursive, -r                       # 递归验证目录
--verbose, -v                         # 详细输出
--report <文件>                        # 保存报告到JSON文件
```

#### 使用示例

```bash
# 验证 tileset.json
python3 tiles_validator.py ./output/tileset.json

# 验证 glb 文件
python3 tiles_validator.py ./output/tile.b3dm --type gltf

# 递归验证整个目录
python3 tiles_validator.py ./output -r -v

# 验证并保存报告
python3 tiles_validator.py ./output -r --report report.json
```

#### 支持的验证内容

| 文件类型 | 验证工具 | 验证内容 |
|----------|----------|----------|
| tileset.json | 3d-tiles-validator | 结构完整性、边界体积、几何误差、引用有效性 |
| .gltf/.glb | gltf-validator | 格式规范、资源完整性、扩展支持、性能统计 |
| .b3dm/.i3dm/.pnts | 3d-tiles-validator | 头部信息、Feature Table、Batch Table、glTF内容 |

#### 验证报告解读

```json
{
  "validator": "3d-tiles-validator",
  "success": true,
  "issues": [
    {
      "code": "TEXTURE_SIZE_NOT_POWER_OF_TWO",
      "message": "Texture size is not a power of two: 513x513",
      "severity": "warning",
      "pointer": "/materials/0/pbrMetallicRoughness/baseColorTexture"
    }
  ],
  "stats": {
    "totalVertices": 15000,
    "totalTriangles": 12000,
    "totalBufferSize": 5242880
  }
}
```

### regression_validator_v2.py - 回归验证

#### 验证模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| **strict** | 字节级逐字节比较 | 确保完全一致的输出 |
| **relaxed** | 允许浮点数容差 | 允许微小数值差异 |
| **fast** | 仅验证关键字段 | 快速验证结构完整性 |

#### 命令行选项

```bash
# 严格模式（默认）
python3 regression_validator_v2.py <基准目录> <当前输出目录> --mode strict

# 宽松模式（允许浮点误差）
python3 regression_validator_v2.py <基准> <当前> \
    --mode relaxed \
    --float-tolerance 1e-4

# 快速模式
python3 regression_validator_v2.py <基准> <当前> --mode fast

# 自定义忽略字段
python3 regression_validator_v2.py <基准> <当前> \
    --ignore-fields generator created timestamp version

# 跳过官方工具验证
python3 regression_validator_v2.py <基准> <当前> --skip-validation

# 指定报告输出路径
python3 regression_validator_v2.py <基准> <当前> --report ./my_report.json
```

### run_tests.py - 测试执行器

#### 测试套件

| 套件 | 描述 | 用例数 | CI必需 | 超时 |
|------|------|--------|--------|------|
| smoke | 冒烟测试 | 1 | ✓ | 60s |
| core | 核心功能测试 | 8 | ✓ | 300s |
| optimization | 优化参数测试 | 10 | ✓ | 600s |
| combination | 参数组合测试 | 6 | ✗ | 900s |
| export | 导出功能测试 | 3 | ✗ | 300s |
| performance | 性能测试 | 3 | ✗ | 1800s |
| edge_cases | 边界情况测试 | 3 | ✗ | 300s |

#### 使用示例

```bash
# 运行核心测试套件
python3 run_tests.py core --mode strict

# 运行所有测试
python3 run_tests.py all --mode relaxed

# 只运行P0优先级测试
python3 run_tests.py core --priority P0

# 运行P0和P1测试
python3 run_tests.py core --priority P0 P1

# 指定输出目录
python3 run_tests.py core --output ./my_test_output
```

## 官方验证工具详解

### gltf-validator

#### 安装

从 [GitHub Releases](https://github.com/KhronosGroup/glTF-Validator/releases) 下载预编译二进制文件：

**macOS (Apple Silicon):**
```bash
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-macos-arm64.zip
unzip gltf-validator.zip
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator
```

**macOS (Intel):**
```bash
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-macos-amd64.zip
unzip gltf-validator.zip
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator
```

**Linux:**
```bash
curl -L -o gltf-validator.zip https://github.com/KhronosGroup/glTF-Validator/releases/download/v2.0.0-dev.3.10/gltf_validator-2.0.0-dev.3.10-linux-amd64.zip
unzip gltf-validator.zip
sudo mv gltf_validator /usr/local/bin/gltf-validator
sudo chmod +x /usr/local/bin/gltf-validator
```

**Windows:**
1. 下载 `gltf_validator-2.0.0-dev.3.10-win64.zip`
2. 解压并将目录添加到 PATH

#### 基本用法
```bash
# 验证单个文件
gltf-validator model.glb

# 验证目录
gltf-validator /path/to/directory/

# 输出报告
gltf-validator model.glb -o report.json --validate-resources

# 多线程验证
gltf-validator /path/to/models/ -o ./reports/ --threads 4 --all
```

#### 配置文件示例
```yaml
# gltf-validator-config.yaml
validateResources: true
writeTimestamp: true
mode: strict

ignoredIssues:
  - TEXTURE_SIZE_NOT_POWER_OF_TWO

enabledExtensions:
  - KHR_lights_punctual
  - KHR_materials_transmission
```

### 3d-tiles-validator

#### 安装
```bash
npm install 3d-tiles-validator
```

#### 基本用法
```bash
# 验证 tileset.json
npx 3d-tiles-validator --tilesetFile tileset.json

# 验证目录
npx 3d-tiles-validator --tilesetsDirectory ./output/ --writeReports

# 验证瓦片内容
npx 3d-tiles-validator --tileContentFile model.b3dm

# 输出报告
npx 3d-tiles-validator --tilesetFile tileset.json --reportFile report.json
```

#### 支持的内容类型

| 类型 | 描述 |
|------|------|
| CONTENT_TYPE_B3DM | Batched 3D Model |
| CONTENT_TYPE_I3DM | Instanced 3D Model |
| CONTENT_TYPE_PNTS | Point Cloud |
| CONTENT_TYPE_CMPT | Composite |
| CONTENT_TYPE_GLB | glTF Binary |
| CONTENT_TYPE_TILESET | Tileset |

### 3d-tiles-tools

#### 安装
```bash
npm install -g 3d-tiles-tools
```

#### 常用命令

```bash
# 验证瓦片集
npx 3d-tiles-tools validate -i tileset.json

# 优化瓦片集
npx 3d-tiles-tools optimize -i ./input/ -o ./optimized/ --draco

# 升级版本
npx 3d-tiles-tools upgrade -i ./tileset.json -o ./upgraded/ --targetVersion 1.1

# Gzip压缩
npx 3d-tiles-tools gzip -i ./input/ -o ./output/

# 合并瓦片集
npx 3d-tiles-tools combine -i ./input/ -o ./output/

# 分析瓦片集
npx 3d-tiles-tools analyze -i /path/to/tiles/
```

## 常见错误修复

### glTF验证常见问题

| 错误代码 | 问题描述 | 解决方案 |
|----------|----------|----------|
| TEXTURE_SIZE_NOT_POWER_OF_TWO | 纹理尺寸不是2的幂次 | 调整纹理尺寸为256x256、512x512等 |
| NORMAL_NOT_NORMALIZED | 法向量未归一化 | 确保法向量长度为1 |
| ACCESSOR_OUT_OF_BOUNDS | 访问器超出缓冲区范围 | 检查bufferView和accessor参数 |
| MESH_PRIMITIVE_UNEQUAL_ACCESSOR_COUNT | 访问器计数不匹配 | 检查顶点属性数组长度 |

### 3D Tiles验证常见问题

| 错误代码 | 问题描述 | 解决方案 |
|----------|----------|----------|
| INVALID_BOUNDING_VOLUME | 边界体积无效 | 检查box、sphere或region参数 |
| INVALID_GEOMETRIC_ERROR | 几何误差值无效 | 确保geometricError为非负数 |
| MISSING_REQUIRED_PROPERTY | 缺少必需属性 | 检查tileset.json结构完整性 |
| INVALID_TILE_CONTENT | 瓦片内容无效 | 检查B3DM/glb文件格式 |

## 集成到CI/CD

### GitHub Actions示例

```yaml
# .github/workflows/regression-test.yml
name: Regression Test

on: [push, pull_request]

jobs:
  regression:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          npm install -g gltf-validator 3d-tiles-validator
          cargo build --release

      - name: Run data quality validation
        run: |
          python3 .trae/skills/3dtiles-regression-validation/tiles_validator.py \
            ./test_output --type directory --recursive

      - name: Run regression tests
        run: |
          python3 .trae/skills/3dtiles-regression-validation/run_tests.py \
            core --mode strict --priority P0 P1

      - name: Upload test results
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: test-results
          path: test_output/
```

## 重构工作流程

### 推荐的重构验证流程

```
1. 准备阶段
   ├── 确保所有测试数据可用
   ├── 安装验证工具
   ├── 构建当前版本（重构前）
   └── 运行 ./generate_baseline.sh --suite core

2. 重构阶段
   ├── 进行代码重构
   ├── 确保代码编译通过
   └── 运行代码格式化工具

3. 验证阶段
   ├── 构建新版本（重构后）
   ├── 运行数据质量验证
   │   └── python3 tiles_validator.py ./output -r
   ├── 运行回归测试
   │   └── python3 run_tests.py core --mode strict
   └── 检查验证报告

4. 问题处理
   ├── 如果验证失败，分析差异原因
   ├── 判断是否为预期内的变化
   └── 修复代码或更新基准数据
```

## 最佳实践

1. **定期更新基准数据**: 当功能变更是预期行为时，重新生成基准数据
2. **多模式验证**: 先用fast模式快速验证，再用strict模式详细验证
3. **版本控制**: 将基准数据纳入版本控制或作为CI/CD产物保存
4. **差异分析**: 验证失败时仔细分析报告，区分预期变更和回归错误
5. **自动化**: 集成到CI/CD流程，每次PR自动运行回归测试
6. **分层验证**: 先验证数据质量，再进行回归比较
7. **工具链整合**: 结合gltf-validator、3d-tiles-validator和自定义验证

## 技术细节

### 浮点数比较

使用相对容差和绝对容差结合的方式：
```python
if abs(val1 - val2) > float_tolerance:
    # 报告差异
```

### B3DM文件解析

B3DM文件结构验证：
- Magic: `b'b3dm'`
- Version: 1
- Feature Table JSON 长度
- Feature Table 二进制长度

### JSON比较

递归比较JSON对象，支持：
- 嵌套对象
- 数组
- 基本类型
- 自定义忽略字段

## 更新日志

### v3.0 (2025-02-04)
- 合并3dtiles-validation技能
- 新增tiles_validator.py数据质量验证工具
- 新增run_tests.py测试执行器
- 更新test_config.json v2.0，包含34个测试用例
- 完善官方验证工具文档
- 添加常见错误修复指南

### v2.0 (2025-02-04)
- 改进文件查找逻辑，支持多种输出结构
- 添加B3DM内部结构验证
- 增强错误报告和诊断信息
- 支持更多配置选项
- 改进输出格式和颜色显示

### v1.0
- 初始版本
- 基本字节级比较
- JSON结构验证
- 官方工具集成
