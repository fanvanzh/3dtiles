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
├── test_config.json                  # 测试配置（v3.0）
├── 3dtiles_regression/               # Python包
│   ├── __init__.py                   # 包初始化
│   ├── __main__.py                   # 模块入口
│   ├── cli.py                        # 命令行入口
│   ├── config.py                     # 配置解析
│   ├── converter.py                  # 格式转换
│   ├── runner.py                     # 测试运行器
│   ├── reporter.py                   # 报告生成
│   └── validators/                   # 验证器模块
│       ├── __init__.py               # 验证器包初始化
│       ├── official_tools.py         # 官方工具集成
│       └── pre_rendering_validator.py # 预渲染验证
├── requirements.txt                  # Python依赖
├── README.md                         # 详细使用文档
├── QUICKSTART.md                     # 快速开始指南
├── COMPLETE_SOLUTION.md              # 完整解决方案文档
└── QUICK_REFERENCE.md                # 快速参考手册
```

## 快速开始

### 1. 安装依赖

#### Python依赖

```bash
# 安装Python依赖
pip install -r requirements.txt
```

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
# 生成所有测试套件的基准数据
python3 -m 3dtiles_regression generate --suite all

# 生成指定测试套件的基准数据
python3 -m 3dtiles_regression generate --suite smoke

# 生成指定测试用例的基准数据
python3 -m 3dtiles_regression generate --test osgb_to_gltf_bench

# 强制覆盖已存在的基准数据
python3 -m 3dtiles_regression generate --suite smoke --force

# 显示详细信息
python3 -m 3dtiles_regression generate --suite smoke --verbose
```

#### 运行回归测试

```bash
# 运行所有测试套件
python3 -m 3dtiles_regression run --suite all

# 运行指定测试套件
python3 -m 3dtiles_regression run --suite smoke

# 运行指定测试用例
python3 -m 3dtiles_regression run --test osgb_to_gltf_bench

# 按优先级运行
python3 -m 3dtiles_regression run --priority P0

# 指定验证模式
python3 -m 3dtiles_regression run --suite smoke --mode strict

# 并发执行
python3 -m 3dtiles_regression run --suite smoke --parallel 4

# 显示详细信息
python3 -m 3dtiles_regression run --suite smoke --verbose
```

#### 生成报告

```bash
# 生成HTML报告
python3 -m 3dtiles_regression report --format html --output ./reports

# 生成JSON报告
python3 -m 3dtiles_regression report --format json --output ./reports

# 生成所有格式报告
python3 -m 3dtiles_regression report --format both --output ./reports
```

## 框架架构

### 核心模块

#### config.py - 配置解析

负责解析test_config.json配置文件，提供测试用例和验证模式的访问接口。

**主要功能:**
- 解析测试套件配置
- 解析验证模式配置
- 提供配置查询接口

**关键类:**
- `Config`: 配置管理类
- `TestSuite`: 测试套件数据类
- `TestCase`: 测试用例数据类
- `ValidationMode`: 验证模式数据类

#### converter.py - 格式转换

负责调用转换工具执行3D格式转换。

**主要功能:**
- 执行OSGB到glTF转换
- 执行Shapefile到3D Tiles转换
- 执行FBX到3D Tiles转换
- 处理转换输出

**关键类:**
- `Converter`: 格式转换器类
- `ConversionResult`: 转换结果数据类

#### runner.py - 测试运行器

负责执行测试用例、验证输出和生成测试结果。

**主要功能:**
- 执行测试用例
- 调用验证器进行验证
- 收集测试结果
- 生成测试报告

**关键类:**
- `TestRunner`: 测试运行器类
- `TestResult`: 测试结果数据类

#### reporter.py - 报告生成

负责生成HTML和JSON格式的测试报告。

**主要功能:**
- 生成HTML报告
- 生成JSON报告
- 聚合测试结果
- 生成统计信息

**关键类:**
- `Reporter`: 报告生成器类
- `TestReport`: 测试报告数据类

#### validators/pre_rendering_validator.py - 预渲染验证

负责在不使用可视化工具的情况下检测渲染问题。

**主要功能:**
- 验证glTF文件渲染准备
- 验证tileset.json渲染准备
- 检测常见的渲染问题
- 生成预渲染验证报告

**关键类:**
- `PreRenderingValidator`: 预渲染验证器类
- `PreRenderingResult`: 预渲染验证结果数据类

**验证内容:**
- 几何数据完整性
- 材质数据有效性
- 纹理数据可用性
- 边界体积正确性
- 变换矩阵有效性

#### validators/official_tools.py - 官方工具集成

负责集成和调用官方验证工具。

**主要功能:**
- 调用gltf-validator
- 调用3d-tiles-validator
- 解析验证结果
- 整合验证报告

**关键类:**
- `OfficialValidator`: 官方验证器类
- `GltfValidationResult`: glTF验证结果数据类
- `TilesValidationResult`: Tiles验证结果数据类

#### cli.py - 命令行入口

提供命令行接口，支持多种操作模式。

**主要命令:**
- `generate`: 生成基准数据
- `run`: 运行回归测试
- `validate`: 数据质量验证
- `report`: 生成测试报告

### 验证模式

| 模式 | 说明 | 浮点容差 | 官方验证 | 内容验证 | 字节比较 |
|------|------|----------|----------|----------|----------|
| **strict** | 严格模式 | 1e-9 | ✓ | ✓ | ✓ |
| **relaxed** | 宽松模式 | 1e-4 | ✓ | ✓ | ✓ |
| **fast** | 快速模式 | 1e-2 | ✗ | ✗ | ✗ |

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

### 3d-tiles-validator

#### 安装

```bash
# 通过npm安装
npm install -g 3d-tiles-validator

# 验证安装
npx 3d-tiles-validator --version
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

## 预渲染验证详解

### 验证原理

预渲染验证通过分析3D数据的结构和内容，在不使用可视化工具的情况下检测可能导致渲染问题的因素。

### 验证内容

#### glTF文件验证

- **几何数据验证**
  - 顶点数量有效性
  - 三角形数量有效性
  - 索引有效性
  - 顶点属性完整性

- **材质数据验证**
  - 材质定义完整性
  - 纹理引用有效性
  - 着色器参数有效性

- **纹理数据验证**
  - 纹理尺寸有效性
  - 纹理格式支持性
  - 纹理引用有效性

- **场景图验证**
  - 节点层次结构
  - 变换矩阵有效性
  - 边界体积正确性

#### tileset.json验证

- **瓦片结构验证**
  - 瓦片层次结构
  - 瓦片引用有效性
  - 瓦片内容类型

- **边界体积验证**
  - 边界体积格式正确性
  - 边界体积数值有效性
  - 边界体积包含关系

- **几何误差验证**
  - 几何误差数值有效性
  - 几何误差层次一致性

### 常见渲染问题检测

| 问题类型 | 检测方法 | 严重性 |
|----------|----------|--------|
| INVALID_TRIANGLE_COUNT | 三角形数量与顶点数量不匹配 | ERROR |
| INVALID_BOUNDING_VOLUME_FORMAT | 边界体积格式错误 | ERROR |
| INVALID_GEOMETRIC_ERROR | 几何误差值无效 | ERROR |
| MISSING_TEXTURE | 纹理文件缺失 | WARNING |
| INVALID_MATERIAL | 材质定义无效 | WARNING |
| INVALID_TRANSFORM | 变换矩阵无效 | ERROR |

## 常见错误修复

### 预渲染验证常见问题

| 错误代码 | 问题描述 | 解决方案 |
|----------|----------|----------|
| INVALID_TRIANGLE_COUNT | 三角形数量与顶点数量不匹配 | 检查索引数组和顶点数组的长度 |
| INVALID_BOUNDING_VOLUME_FORMAT | 边界体积格式错误 | 确保边界体积使用正确的格式（box、sphere、region） |
| INVALID_GEOMETRIC_ERROR | 几何误差值无效 | 确保geometricError为非负数 |
| MISSING_TEXTURE | 纹理文件缺失 | 检查纹理路径是否正确 |
| INVALID_MATERIAL | 材质定义无效 | 检查材质定义是否符合规范 |
| INVALID_TRANSFORM | 变换矩阵无效 | 检查变换矩阵是否可逆 |

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

### 框架使用常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| "测试套件不存在: xxx" | test_config.json中未定义该测试套件 | 检查test_config.json中的test_suites配置 |
| "验证模式不存在: xxx" | test_config.json中未定义该验证模式 | 检查test_config.json中的validation_modes配置 |
| "配置文件不存在" | test_config.json路径错误 | 确保test_config.json在正确的位置 |
| "输入文件不存在" | 测试用例的输入路径错误 | 检查test_config.json中的input路径配置 |
| "输出文件不存在" | 转换失败或输出路径错误 | 检查转换工具是否正常工作 |

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
          pip install -r .trae/skills/3dtiles-regression-validation/requirements.txt

      - name: Run data quality validation
        run: |
          python3 -m 3dtiles_regression validate --suite smoke --mode strict

      - name: Run regression tests
        run: |
          python3 -m 3dtiles_regression run --suite core --mode strict --priority P0 P1

      - name: Generate test reports
        if: always()
        run: |
          python3 -m 3dtiles_regression report --format both --output ./reports

      - name: Upload test results
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: test-results
          path: ./reports/
```

### GitLab CI示例

```yaml
# .gitlab-ci.yml
stages:
  - validate
  - test
  - report

variables:
  PYTHON_VERSION: "3.10"

validate:
  stage: validate
  script:
    - pip install -r .trae/skills/3dtiles-regression-validation/requirements.txt
    - npm install -g gltf-validator 3d-tiles-validator
    - python3 -m 3dtiles_regression validate --suite smoke --mode strict

test:
  stage: test
  script:
    - python3 -m 3dtiles_regression run --suite core --mode strict --priority P0 P1
  artifacts:
    when: always
    paths:
      - ./reports/

report:
  stage: report
  script:
    - python3 -m 3dtiles_regression report --format both --output ./reports
  artifacts:
    when: always
    paths:
      - ./reports/
```

### Jenkins Pipeline示例

```groovy
pipeline {
    agent any

    stages {
        stage('Install Dependencies') {
            steps {
                sh 'npm install -g gltf-validator 3d-tiles-validator'
                sh 'pip install -r .trae/skills/3dtiles-regression-validation/requirements.txt'
            }
        }

        stage('Validate Data Quality') {
            steps {
                sh 'python3 -m 3dtiles_regression validate --suite smoke --mode strict'
            }
        }

        stage('Run Regression Tests') {
            steps {
                sh 'python3 -m 3dtiles_regression run --suite core --mode strict --priority P0 P1'
            }
        }

        stage('Generate Reports') {
            steps {
                sh 'python3 -m 3dtiles_regression report --format both --output ./reports'
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: './reports/**/*', allowEmptyArchive: true
        }
    }
}
```

## 重构工作流程

### 推荐的重构验证流程

```
1. 准备阶段
   ├── 确保所有测试数据可用
   ├── 安装验证工具
   │   ├── npm install -g gltf-validator 3d-tiles-validator
   │   └── pip install -r requirements.txt
   ├── 构建当前版本（重构前）
   └── 生成基准数据
       └── python3 -m 3dtiles_regression generate --suite core

2. 重构阶段
   ├── 进行代码重构
   ├── 确保代码编译通过
   └── 运行代码格式化工具

3. 验证阶段
   ├── 构建新版本（重构后）
   ├── 运行数据质量验证
   │   └── python3 -m 3dtiles_regression validate --suite core --mode strict
   ├── 运行回归测试
   │   └── python3 -m 3dtiles_regression run --suite core --mode strict
   └── 生成测试报告
       └── python3 -m 3dtiles_regression report --format both --output ./reports

4. 问题处理
   ├── 如果验证失败，分析差异原因
   ├── 判断是否为预期内的变化
   ├── 修复代码或更新基准数据
   │   └── python3 -m 3dtiles_regression generate --suite core --force
   └── 重新运行验证
```

### 日常开发流程

```
1. 开发新功能
   ├── 编写代码
   ├── 运行单元测试
   └── 运行冒烟测试
       └── python3 -m 3dtiles_regression run --suite smoke --mode fast

2. 提交前验证
   ├── 运行核心测试套件
   │   └── python3 -m 3dtiles_regression run --suite core --mode relaxed
   ├── 生成测试报告
   │   └── python3 -m 3dtiles_regression report --format html --output ./reports
   └── 检查测试结果

3. 发布前验证
   ├── 运行完整测试套件
   │   └── python3 -m 3dtiles_regression run --suite all --mode strict
   ├── 生成详细报告
   │   └── python3 -m 3dtiles_regression report --format both --output ./reports
   └── 审查所有测试结果
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

### v4.0 (2025-02-05)
- 完全重构为Python框架
- 新增预渲染验证功能，无需可视化工具即可检测渲染问题
- 集成官方验证工具（gltf-validator、3d-tiles-validator）
- 新增命令行接口（CLI）
- 支持多种验证模式（strict、relaxed、fast）
- 支持并发测试执行
- 生成HTML和JSON格式的详细报告
- 完善的配置管理系统
- 支持测试套件和测试用例的灵活配置
- 新增冒烟测试套件，包含OSGB、Shapefile、FBX转换测试

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
