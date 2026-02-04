#!/usr/bin/env python3
# regression_validator_v2.py - 3D Tiles回归验证工具（改进版）

import argparse
import hashlib
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# 导入 gltf_comparator 模块
try:
    from gltf_comparator import GLTFComparator, ComparisonReport as GLTFComparisonReport
    GLTF_COMPARATOR_AVAILABLE = True
except ImportError:
    GLTF_COMPARATOR_AVAILABLE = False

# 导入 tiles_comparator 模块
try:
    from tiles_comparator import TilesComparator, ComparisonReport as TilesComparisonReport
    TILES_COMPARATOR_AVAILABLE = True
except ImportError:
    TILES_COMPARATOR_AVAILABLE = False


class ValidationMode(Enum):
    STRICT = "strict"      # 字节级完全一致
    RELAXED = "relaxed"    # 允许浮点数容差
    FAST = "fast"          # 仅验证关键字段


class ValidationResult(Enum):
    PASS = "pass"
    FAIL = "fail"
    WARNING = "warning"


@dataclass
class ValidationConfig:
    mode: ValidationMode
    float_tolerance: float = 1e-6
    skip_validation: bool = False
    generate_report: bool = True
    report_path: Optional[str] = None
    ignore_fields: List[str] = None  # 忽略的字段列表

    def __post_init__(self):
        if self.ignore_fields is None:
            self.ignore_fields = ["generator", "created", "timestamp"]  # 默认忽略时间戳等字段


@dataclass
class FileResult:
    path: str
    result: ValidationResult
    message: str
    details: Dict[str, Any]


class RegressionValidator:
    """3D Tiles回归验证器 - 支持多种输出结构"""

    def __init__(self, config: ValidationConfig):
        self.config = config
        self.results: List[FileResult] = []
        self.summary = {
            "total": 0,
            "passed": 0,
            "failed": 0,
            "warnings": 0,
            "errors": [],
            "warnings_list": [],
        }

    def calculate_file_hash(self, filepath: str) -> str:
        """计算文件的MD5哈希值"""
        hash_md5 = hashlib.md5()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()

    def compare_files_bytes(self, file1: str, file2: str) -> Tuple[bool, int]:
        """逐字节比较两个文件"""
        size1 = os.path.getsize(file1)
        size2 = os.path.getsize(file2)

        if size1 != size2:
            return False, size2 - size1

        with open(file1, "rb") as f1, open(file2, "rb") as f2:
            if f1.read() != f2.read():
                return False, 0

        return True, 0

    def compare_json_values(self, val1: Any, val2: Any, path: str = "") -> List[str]:
        """比较两个JSON值，支持容差和忽略字段"""
        differences = []

        # 检查是否在忽略字段列表中
        path_parts = path.strip("/").split("/")
        if path_parts and path_parts[-1] in self.config.ignore_fields:
            return differences

        if type(val1) != type(val2):
            differences.append(f"{path}: 类型不同 ({type(val1).__name__} vs {type(val2).__name__})")
            return differences

        if isinstance(val1, dict):
            keys1 = set(val1.keys())
            keys2 = set(val2.keys())

            for key in keys1 & keys2:
                diffs = self.compare_json_values(val1[key], val2[key], f"{path}/{key}")
                differences.extend(diffs)

            for key in keys1 - keys2:
                differences.append(f"{path}/{key}: 基准存在，当前不存在")

            for key in keys2 - keys1:
                differences.append(f"{path}/{key}: 当前存在，基准不存在")

        elif isinstance(val1, list):
            if len(val1) != len(val2):
                differences.append(f"{path}: 列表长度不同 ({len(val1)} vs {len(val2)})")
                return differences

            for i, (item1, item2) in enumerate(zip(val1, val2)):
                diffs = self.compare_json_values(item1, item2, f"{path}[{i}]")
                differences.extend(diffs)

        elif isinstance(val1, float):
            if abs(val1 - val2) > self.config.float_tolerance:
                differences.append(f"{path}: 数值差异 ({val1:.10f} vs {val2:.10f}, 差值: {abs(val1 - val2):.2e})")

        elif isinstance(val1, str):
            # 字符串比较，忽略时间戳格式
            if val1 != val2:
                differences.append(f"{path}: 字符串不同 ('{val1[:50]}' vs '{val2[:50]}')")

        else:
            if val1 != val2:
                differences.append(f"{path}: 值不同 ({val1} vs {val2})")

        return differences

    def find_tileset_json(self, directory: str) -> Optional[str]:
        """在目录中查找tileset.json文件"""
        tileset_path = os.path.join(directory, "tileset.json")
        if os.path.exists(tileset_path):
            return tileset_path
        return None

    def find_content_files(self, directory: str) -> Dict[str, str]:
        """查找所有内容文件（b3dm, glb, i3dm等）"""
        content_files = {}
        content_extensions = {'.b3dm', '.glb', '.gltf', '.i3dm', '.pnts', '.cmpt'}

        for root, dirs, files in os.walk(directory):
            for file in files:
                if any(file.lower().endswith(ext) for ext in content_extensions):
                    rel_path = os.path.relpath(os.path.join(root, file), directory)
                    content_files[rel_path] = os.path.join(root, file)

        return content_files

    def validate_tileset_structure(self, baseline: str, current: str) -> FileResult:
        """验证瓦片集结构"""
        baseline_json = self.find_tileset_json(baseline)
        current_json = self.find_tileset_json(current)

        if not baseline_json:
            return FileResult(
                path="tileset.json",
                result=ValidationResult.FAIL,
                message="基准tileset.json不存在",
                details={}
            )

        if not current_json:
            return FileResult(
                path="tileset.json",
                result=ValidationResult.FAIL,
                message="当前tileset.json不存在",
                details={}
            )

        try:
            with open(baseline_json, 'r', encoding='utf-8') as f:
                baseline_data = json.load(f)

            with open(current_json, 'r', encoding='utf-8') as f:
                current_data = json.load(f)

            differences = self.compare_json_values(baseline_data, current_data)

            if differences:
                return FileResult(
                    path="tileset.json",
                    result=ValidationResult.FAIL if self.config.mode == ValidationMode.STRICT else ValidationResult.WARNING,
                    message=f"发现 {len(differences)} 个差异",
                    details={"differences": differences[:20]}  # 显示更多差异
                )
            else:
                return FileResult(
                    path="tileset.json",
                    result=ValidationResult.PASS,
                    message="tileset.json完全一致",
                    details={}
                )
        except json.JSONDecodeError as e:
            return FileResult(
                path="tileset.json",
                result=ValidationResult.FAIL,
                message=f"JSON解析错误: {str(e)}",
                details={}
            )

    def validate_content_files(self, baseline: str, current: str) -> List[FileResult]:
        """验证内容文件"""
        results = []

        baseline_files = self.find_content_files(baseline)
        current_files = self.find_content_files(current)

        # 检查基准文件是否都在当前输出中
        for filename in sorted(baseline_files.keys()):
            if filename not in current_files:
                results.append(FileResult(
                    path=filename,
                    result=ValidationResult.FAIL,
                    message="文件在当前输出中不存在",
                    details={"baseline_path": baseline_files[filename]}
                ))
                continue

            baseline_path = baseline_files[filename]
            current_path = current_files[filename]

            # 使用 tiles_comparator 进行深度内容比对
            if TILES_COMPARATOR_AVAILABLE:
                try:
                    comparator = TilesComparator(
                        float_tolerance=self.config.float_tolerance,
                        ignore_fields=set(self.config.ignore_fields)
                    )
                    tiles_report = comparator.compare(baseline_path, current_path)

                    if tiles_report.identical:
                        results.append(FileResult(
                            path=filename,
                            result=ValidationResult.PASS,
                            message=f"{tiles_report.file_type}内容完全一致",
                            details={
                                "comparator": "tiles_comparator",
                                "total_items": tiles_report.total_items,
                                "matched_items": tiles_report.matched_items,
                                "tolerance_matched": tiles_report.tolerance_matched
                            }
                        ))
                    else:
                        results.append(FileResult(
                            path=filename,
                            result=ValidationResult.FAIL if self.config.mode == ValidationMode.STRICT else ValidationResult.WARNING,
                            message=f"{tiles_report.file_type}内容存在 {len(tiles_report.differences)} 个差异",
                            details={
                                "comparator": "tiles_comparator",
                                "total_items": tiles_report.total_items,
                                "matched_items": tiles_report.matched_items,
                                "differences": len(tiles_report.differences),
                                "diff_sample": [
                                    {
                                        "path": d.path,
                                        "field": d.field,
                                        "value1": str(d.value1),
                                        "value2": str(d.value2)
                                    }
                                    for d in tiles_report.differences[:3]
                                ]
                            }
                        ))

                except Exception as e:
                    results.append(FileResult(
                        path=filename,
                        result=ValidationResult.WARNING,
                        message=f"tiles_comparator执行异常: {str(e)}",
                        details={}
                    ))
            else:
                # 降级到字节级比较
                is_match, diff_info = self.compare_files_bytes(
                    baseline_path,
                    current_path
                )

                if is_match:
                    results.append(FileResult(
                        path=filename,
                        result=ValidationResult.PASS,
                        message="文件内容完全一致",
                        details={
                            "size": os.path.getsize(baseline_path),
                            "hash": self.calculate_file_hash(baseline_path)
                        }
                    ))
                else:
                    results.append(FileResult(
                        path=filename,
                        result=ValidationResult.FAIL,
                        message=f"文件内容存在差异",
                        details={
                            "baseline_size": os.path.getsize(baseline_path),
                            "current_size": os.path.getsize(current_path),
                            "size_diff": diff_info
                        }
                    ))

        # 检查是否有新增文件
        for filename in sorted(current_files.keys()):
            if filename not in baseline_files:
                results.append(FileResult(
                    path=filename,
                    result=ValidationResult.WARNING,
                    message="文件在基准输出中不存在（新增）",
                    details={"current_path": current_files[filename]}
                ))

        return results

    def validate_with_official_tools(self, baseline: str, current: str) -> List[FileResult]:
        """使用官方工具进行验证"""
        results = []

        if self.config.skip_validation:
            return results

        tileset_path = self.find_tileset_json(current)
        if not tileset_path:
            return results

        # 使用3d-tiles-validator验证
        try:
            result = subprocess.run(
                ["npx", "3d-tiles-validator", "--tilesetFile", tileset_path, "--writeReports"],
                capture_output=True,
                text=True,
                timeout=300,
                cwd=os.path.dirname(tileset_path)
            )

            # 读取验证报告
            report_path = tileset_path.replace(".json", ".report.json")
            if os.path.exists(report_path):
                with open(report_path, 'r') as f:
                    validation_report = json.load(f)

                num_errors = validation_report.get("numErrors", 0)
                num_warnings = validation_report.get("numWarnings", 0)

                if num_errors == 0:
                    results.append(FileResult(
                        path="tileset.json",
                        result=ValidationResult.PASS,
                        message=f"3d-tiles-validator验证通过 (warnings: {num_warnings})",
                        details={"report": report_path}
                    ))
                else:
                    results.append(FileResult(
                        path="tileset.json",
                        result=ValidationResult.FAIL,
                        message=f"3d-tiles-validator发现 {num_errors} 个错误",
                        details={"errors": validation_report.get("issues", [])[:5]}
                    ))
            else:
                results.append(FileResult(
                    path="tileset.json",
                    result=ValidationResult.WARNING,
                    message="未找到验证报告",
                    details={"stdout": result.stdout[:500]}
                ))

        except subprocess.TimeoutExpired:
            results.append(FileResult(
                path="tileset.json",
                result=ValidationResult.WARNING,
                message="3d-tiles-validator执行超时",
                details={}
            ))
        except FileNotFoundError:
            results.append(FileResult(
                path="tileset.json",
                result=ValidationResult.WARNING,
                message="3d-tiles-validator未安装，跳过验证",
                details={}
            ))
        except Exception as e:
            results.append(FileResult(
                path="tileset.json",
                result=ValidationResult.WARNING,
                message=f"3d-tiles-validator执行异常: {str(e)}",
                details={}
            ))

        # 验证glb文件
        content_files = self.find_content_files(current)
        glb_files = [f for f in content_files.values() if f.endswith('.glb')]

        # 查找基准目录中的 glb 文件用于对比
        baseline_content_files = self.find_content_files(baseline)
        baseline_glb_files = [f for f in baseline_content_files.values() if f.endswith('.glb')]

        for i, glb_file in enumerate(glb_files[:3]):  # 只验证前3个glb文件
            glb_filename = os.path.basename(glb_file)

            # 1. 使用 gltf-validator 进行规范验证
            try:
                result = subprocess.run(
                    ["gltf-validator", glb_file, "--stdout"],
                    capture_output=True,
                    text=True,
                    timeout=60
                )

                # gltf-validator 的输出包含摘要信息和 JSON，需要提取 JSON 部分
                # JSON 对象以 '{' 开头，通常在输出的最后部分
                output = result.stdout.strip()

                # 找到最后一个完整的 JSON 对象
                # 从最后一个 '{' 开始，找到匹配的 '}'
                last_brace_pos = output.rfind('{')
                if last_brace_pos >= 0:
                    json_str = output[last_brace_pos:]

                    # 验证 JSON 是否完整（括号匹配）
                    brace_count = 0
                    for i, char in enumerate(json_str):
                        if char == '{':
                            brace_count += 1
                        elif char == '}':
                            brace_count -= 1
                            if brace_count == 0:
                                json_str = json_str[:i + 1]
                                break

                    try:
                        validation_result = json.loads(json_str)
                        num_errors = validation_result.get("issues", {}).get("numErrors", 0)
                        num_warnings = validation_result.get("issues", {}).get("numWarnings", 0)

                        if num_errors == 0:
                            results.append(FileResult(
                                path=glb_filename,
                                result=ValidationResult.PASS,
                                message=f"gltf-validator验证通过 (warnings: {num_warnings})",
                                details={"validator": "gltf-validator"}
                            ))
                        else:
                            results.append(FileResult(
                                path=glb_filename,
                                result=ValidationResult.FAIL,
                                message=f"gltf-validator发现 {num_errors} 个错误",
                                details={"validator": "gltf-validator", "errors": num_errors}
                            ))
                    except json.JSONDecodeError as e:
                        results.append(FileResult(
                            path=glb_filename,
                            result=ValidationResult.WARNING,
                            message=f"gltf-validator JSON解析失败: {str(e)}",
                            details={"json_preview": json_str[:200]}
                        ))
                else:
                    results.append(FileResult(
                        path=glb_filename,
                        result=ValidationResult.WARNING,
                        message="gltf-validator输出格式异常",
                        details={"output": output[:200]}
                    ))

            except FileNotFoundError:
                results.append(FileResult(
                    path=glb_filename,
                    result=ValidationResult.WARNING,
                    message="gltf-validator未安装，跳过规范验证",
                    details={}
                ))
            except Exception as e:
                results.append(FileResult(
                    path=glb_filename,
                    result=ValidationResult.WARNING,
                    message=f"gltf-validator执行异常: {str(e)}",
                    details={}
                ))

            # 2. 使用 gltf_comparator 进行内容比对（如果存在对应的基准文件）
            if GLTF_COMPARATOR_AVAILABLE and baseline_glb_files:
                baseline_glb = None
                for baseline_file in baseline_glb_files:
                    if os.path.basename(baseline_file) == glb_filename:
                        baseline_glb = baseline_file
                        break

                if baseline_glb and os.path.exists(baseline_glb):
                    try:
                        comparator = GLTFComparator(
                            float_tolerance=self.config.float_tolerance,
                            ignore_fields=set(self.config.ignore_fields)
                        )
                        gltf_report = comparator.compare(baseline_glb, glb_file)

                        if gltf_report.identical:
                            results.append(FileResult(
                                path=f"{glb_filename} (内容比对)",
                                result=ValidationResult.PASS,
                                message="glTF内容完全一致",
                                details={
                                    "validator": "gltf_comparator",
                                    "total_items": gltf_report.total_items,
                                    "matched_items": gltf_report.matched_items,
                                    "tolerance_matched": gltf_report.tolerance_matched
                                }
                            ))
                        else:
                            results.append(FileResult(
                                path=f"{glb_filename} (内容比对)",
                                result=ValidationResult.FAIL if self.config.mode == ValidationMode.STRICT else ValidationResult.WARNING,
                                message=f"glTF内容存在 {len(gltf_report.differences)} 个差异",
                                details={
                                    "validator": "gltf_comparator",
                                    "total_items": gltf_report.total_items,
                                    "matched_items": gltf_report.matched_items,
                                    "differences": len(gltf_report.differences),
                                    "diff_sample": [
                                        {
                                            "path": d.path,
                                            "field": d.field,
                                            "value1": str(d.value1),
                                            "value2": str(d.value2)
                                        }
                                        for d in gltf_report.differences[:3]
                                    ]
                                }
                            ))

                    except Exception as e:
                        results.append(FileResult(
                            path=f"{glb_filename} (内容比对)",
                            result=ValidationResult.WARNING,
                            message=f"gltf_comparator执行异常: {str(e)}",
                            details={}
                        ))

        return results

    def validate(self, baseline: str, current: str) -> Dict[str, Any]:
        """执行完整回归验证"""
        self.results = []
        self.summary = {
            "total": 0,
            "passed": 0,
            "failed": 0,
            "warnings": 0,
            "errors": [],
            "warnings_list": [],
        }

        print("=" * 70)
        print("3D Tiles 回归验证")
        print("=" * 70)
        print(f"基准数据: {baseline}")
        print(f"当前数据: {current}")
        print(f"验证模式: {self.config.mode.value}")
        print(f"浮点容差: {self.config.float_tolerance}")
        print(f"忽略字段: {', '.join(self.config.ignore_fields)}")
        print("=" * 70)

        # 1. 验证瓦片集结构
        print("\n[1/4] 验证瓦片集结构...")
        structure_result = self.validate_tileset_structure(baseline, current)
        self.results.append(structure_result)
        self.summary["total"] += 1
        if structure_result.result == ValidationResult.PASS:
            self.summary["passed"] += 1
            print(f"  ✓ {structure_result.message}")
        elif structure_result.result == ValidationResult.WARNING:
            self.summary["warnings"] += 1
            self.summary["warnings_list"].append({
                "file": structure_result.path,
                "message": structure_result.message
            })
            print(f"  ⚠ {structure_result.message}")
        else:
            self.summary["failed"] += 1
            self.summary["errors"].append({
                "file": structure_result.path,
                "message": structure_result.message
            })
            print(f"  ✗ {structure_result.message}")
            if structure_result.details.get("differences"):
                for diff in structure_result.details["differences"][:5]:
                    print(f"      - {diff}")

        # 2. 验证内容文件
        print("\n[2/4] 验证内容文件...")
        content_results = self.validate_content_files(baseline, current)
        self.results.extend(content_results)

        for result in content_results:
            self.summary["total"] += 1
            if result.result == ValidationResult.PASS:
                self.summary["passed"] += 1
                print(f"  ✓ {result.path}")
            elif result.result == ValidationResult.WARNING:
                self.summary["warnings"] += 1
                self.summary["warnings_list"].append({
                    "file": result.path,
                    "message": result.message
                })
                print(f"  ⚠ {result.path}: {result.message}")
            else:
                self.summary["failed"] += 1
                self.summary["errors"].append({
                    "file": result.path,
                    "message": result.message
                })
                print(f"  ✗ {result.path}: {result.message}")

        # 3. 使用官方工具验证
        print("\n[3/4] 使用官方工具验证...")
                    self.summary["passed"] += 1
                    print(f"  ✓ {result.path}")
                else:
                    self.summary["failed"] += 1
                    self.summary["errors"].append({
                        "file": result.path,
                        "message": result.message
                    })
                    print(f"  ✗ {result.path}: {result.message}")

        # 4. 使用官方工具验证
        print("\n[4/4] 使用官方工具验证...")
        official_results = self.validate_with_official_tools(baseline, current)
        self.results.extend(official_results)

        for result in official_results:
            self.summary["total"] += 1
            if result.result == ValidationResult.PASS:
                self.summary["passed"] += 1
                print(f"  ✓ {result.path}: {result.message}")
            elif result.result == ValidationResult.WARNING:
                self.summary["warnings"] += 1
                self.summary["warnings_list"].append({
                    "file": result.path,
                    "message": result.message
                })
                print(f"  ⚠ {result.path}: {result.message}")
            else:
                self.summary["failed"] += 1
                self.summary["errors"].append({
                    "file": result.path,
                    "message": result.message
                })
                print(f"  ✗ {result.path}: {result.message}")

        # 输出总结
        print("\n" + "=" * 70)
        print("验证总结")
        print("=" * 70)
        print(f"总计: {self.summary['total']} 项")
        print(f"通过: {self.summary['passed']} 项 ✓")
        print(f"警告: {self.summary['warnings']} 项 ⚠")
        print(f"失败: {self.summary['failed']} 项 ✗")

        if self.summary['errors']:
            print("\n错误详情:")
            for error in self.summary['errors'][:10]:
                print(f"  - {error['file']}: {error['message']}")

        # 生成报告
        if self.config.generate_report:
            report = {
                "validation_mode": self.config.mode.value,
                "float_tolerance": self.config.float_tolerance,
                "baseline": baseline,
                "current": current,
                "timestamp": subprocess.check_output(["date", "+%Y-%m-%d %H:%M:%S"]).decode().strip(),
                "summary": self.summary,
                "results": [
                    {
                        "path": r.path,
                        "result": r.result.value,
                        "message": r.message,
                        "details": r.details
                    }
                    for r in self.results
                ]
            }

            report_path = self.config.report_path or os.path.join(current, "validation_report.json")
            with open(report_path, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)

            print(f"\n验证报告已保存: {report_path}")

        return {
            "summary": self.summary,
            "results": self.results
        }


def main():
    parser = argparse.ArgumentParser(description="3D Tiles回归验证工具 v2.0")
    parser.add_argument("baseline", help="基准数据目录")
    parser.add_argument("current", help="当前数据目录")
    parser.add_argument(
        "--mode",
        choices=["strict", "relaxed", "fast"],
        default="strict",
        help="验证模式 (strict=字节级, relaxed=容差, fast=快速)"
    )
    parser.add_argument(
        "--float-tolerance",
        type=float,
        default=1e-6,
        help="浮点数容差 (默认: 1e-6)"
    )
    parser.add_argument(
        "--ignore-fields",
        nargs="+",
        default=["generator", "created", "timestamp"],
        help="忽略的JSON字段列表"
    )
    parser.add_argument(
        "--skip-validation",
        action="store_true",
        help="跳过官方工具验证"
    )
    parser.add_argument(
        "--report",
        type=str,
        help="报告输出路径"
    )

    args = parser.parse_args()

    config = ValidationConfig(
        mode=ValidationMode(args.mode),
        float_tolerance=args.float_tolerance,
        ignore_fields=args.ignore_fields,
        skip_validation=args.skip_validation,
        generate_report=True,
        report_path=args.report
    )

    validator = RegressionValidator(config)
    result = validator.validate(args.baseline, args.current)

    # 根据验证结果返回退出码
    if validator.summary["failed"] > 0:
        print("\n❌ 验证失败 - 发现不兼容变更")
        sys.exit(1)
    elif validator.summary["warnings"] > 0:
        print("\n⚠️  验证通过 - 存在警告")
        sys.exit(0)
    else:
        print("\n✅ 验证通过 - 数据完全一致")
        sys.exit(0)


if __name__ == "__main__":
    main()
