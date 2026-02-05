# 3D Tiles回归测试框架 - 快速开始指南

## 安装

### 1. 安装Python依赖

```bash
cd /Users/wallance/Developer/cim/thirdparty/3dtiles/.trae/skills/3dtiles-regression-validation
pip install -r requirements.txt
```

### 2. 安装官方验证器（可选，推荐）

```bash
# 安装gltf-validator
npm install -g gltf-validator

# 安装3d-tiles-validator
npm install -g 3d-tiles-validator
```

如果不想全局安装，可以使用npx：
```bash
# 框架会自动使用npx运行，无需全局安装
```

## 快速开始

### 1. 生成基线数据

```bash
# 生成所有测试套件的基线
python3 -m 3dtiles_regression generate --suite all

# 生成冒烟测试套件的基线
python3 -m 3dtiles_regression generate --suite smoke

# 生成指定测试用例的基线
python3 -m 3dtiles_regression generate --test osgb_to_gltf

# 强制覆盖已存在的基线
python3 -m 3dtiles_regression generate --suite smoke --force
```

### 2. 运行回归测试

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

### 3. 查看报告

测试完成后，会在输出目录生成两种格式的报告：

- **JSON报告**：`test_data/test_output/current/report.json`
- **HTML报告**：`test_data/test_output/current/report.html`

直接在浏览器中打开HTML报告即可查看详细的测试结果和验证问题。

## 预定义执行配置

### 本地开发 - 快速验证
```bash
python3 -m 3dtiles_regression run smoke --mode fast
```
预计时间：< 1分钟

### 提交前 - 验证关键功能
```bash
python3 -m 3dtiles_regression run core --mode strict --priority P0
```
预计时间：< 3分钟

### CI标准 - 核心功能测试
```bash
python3 -m 3dtiles_regression run core --mode strict
```
预计时间：< 5分钟

### CI完整 - 包含优化参数
```bash
python3 -m 3dtiles_regression run core optimization --mode relaxed
```
预计时间：< 15分钟

### 发布前 - 完整测试
```bash
python3 -m 3dtiles_regression run core optimization combination export --mode strict
```
预计时间：< 30分钟

## 验证模式说明

### Strict模式（严格模式）
- **用途**：发布前验证
- **浮点容差**：1e-9
- **内容验证**：启用
- **官方验证器**：启用
- **字节级比较**：启用

### Relaxed模式（宽松模式）
- **用途**：开发阶段验证
- **浮点容差**：1e-4
- **内容验证**：启用
- **官方验证器**：启用
- **字节级比较**：启用

### Fast模式（快速模式）
- **用途**：快速反馈
- **浮点容差**：1e-2
- **内容验证**：禁用
- **官方验证器**：禁用
- **字节级比较**：禁用

## 预渲染验证

框架会在数据生成阶段检测可能导致渲染失败的问题，包括：

### glTF/GLB致命问题
- Invalid buffer accessors（缓冲区访问器错误）
- Invalid indices（索引错误）
- Missing required fields（缺失必需字段）
- Invalid material references（材质引用错误）

### tileset.json致命问题
- Missing 'refine' property（缺失refine属性）
- Invalid boundingVolume（包围体错误）
- Invalid geometricError（几何误差错误）
- Invalid tile hierarchy（瓦片层级错误）
- Missing content files（内容文件缺失）

每个问题都会标注：
- **渲染影响**：说明可能导致的具体渲染问题
- **修复建议**：提供修复建议（对于致命问题）

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
```

## 更多信息

详细文档请参考：[README.md](README.md)
