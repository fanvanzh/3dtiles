#!/usr/bin/env python3
"""
3D Tiles 综合验证工具
整合回归验证和格式验证功能
支持 gltf-validator、3d-tiles-validator、3d-tiles-tools
"""

import json
import subprocess
import sys
import os
import argparse
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class ValidationLevel(Enum):
    """验证级别"""
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"


class ValidatorType(Enum):
    """验证器类型"""
    GLTF = "gltf-validator"
    TILES = "3d-tiles-validator"
    TOOLS = "3d-tiles-tools"


@dataclass
class ValidationIssue:
    """验证问题"""
    code: str
    message: str
    severity: ValidationLevel
    pointer: str = ""
    validator: ValidatorType = ValidatorType.GLTF


@dataclass
class ValidationResult:
    """验证结果"""
    validator: ValidatorType
    success: bool
    issues: List[ValidationIssue]
    stats: Dict
    raw_output: str = ""


class TilesValidator:
    """3D Tiles综合验证器"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self._check_tools()

    def _check_tools(self):
        """检查验证工具是否安装"""
        self.tools_available = {
            'gltf-validator': self._check_command('gltf-validator --version'),
            '3d-tiles-validator': self._check_command('npx 3d-tiles-validator --version'),
            '3d-tiles-tools': self._check_command('npx 3d-tiles-tools --help')
        }

        if self.verbose:
            print("验证工具状态:")
            for tool, available in self.tools_available.items():
                status = "✓" if available else "✗"
                print(f"  {status} {tool}")

    def _check_command(self, cmd: str) -> bool:
        """检查命令是否可用"""
        try:
            result = subprocess.run(
                cmd.split(),
                capture_output=True,
                timeout=5
            )
            return result.returncode == 0 or b'version' in result.stdout
        except:
            return False

    def validate_gltf(self, file_path: str, validate_resources: bool = True) -> ValidationResult:
        """
        使用 gltf-validator 验证 glTF/glb 文件

        Args:
            file_path: glTF或glb文件路径
            validate_resources: 是否验证嵌入和引用资源

        Returns:
            ValidationResult: 验证结果
        """
        if not self.tools_available['gltf-validator']:
            return ValidationResult(
                validator=ValidatorType.GLTF,
                success=False,
                issues=[ValidationIssue(
                    code="TOOL_NOT_FOUND",
                    message="gltf-validator 未安装。请从 https://github.com/KhronosGroup/glTF-Validator/releases 下载对应系统的预编译二进制文件",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.GLTF
                )],
                stats={}
            )

        cmd = ['gltf-validator', file_path]
        if validate_resources:
            cmd.append('--validate-resources')

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )

            # 解析JSON输出
            try:
                report = json.loads(result.stdout)
                issues = self._parse_gltf_issues(report)
                stats = report.get('stats', {})

                # 检查是否有错误
                num_errors = report.get('issues', {}).get('numErrors', 0)
                success = num_errors == 0

                return ValidationResult(
                    validator=ValidatorType.GLTF,
                    success=success,
                    issues=issues,
                    stats=stats,
                    raw_output=result.stdout
                )
            except json.JSONDecodeError:
                # 非JSON输出，可能是错误信息
                return ValidationResult(
                    validator=ValidatorType.GLTF,
                    success=result.returncode == 0,
                    issues=[],
                    stats={},
                    raw_output=result.stdout + result.stderr
                )

        except subprocess.TimeoutExpired:
            return ValidationResult(
                validator=ValidatorType.GLTF,
                success=False,
                issues=[ValidationIssue(
                    code="TIMEOUT",
                    message="验证超时",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.GLTF
                )],
                stats={}
            )
        except Exception as e:
            return ValidationResult(
                validator=ValidatorType.GLTF,
                success=False,
                issues=[ValidationIssue(
                    code="EXCEPTION",
                    message=str(e),
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.GLTF
                )],
                stats={}
            )

    def _parse_gltf_issues(self, report: Dict) -> List[ValidationIssue]:
        """解析 gltf-validator 报告中的问题"""
        issues = []

        for msg in report.get('messages', []):
            severity = ValidationLevel.INFO
            if msg.get('severity') == 0:
                severity = ValidationLevel.ERROR
            elif msg.get('severity') == 1:
                severity = ValidationLevel.WARNING
            elif msg.get('severity') == 2:
                severity = ValidationLevel.INFO

            issues.append(ValidationIssue(
                code=msg.get('code', 'UNKNOWN'),
                message=msg.get('message', ''),
                severity=severity,
                pointer=msg.get('pointer', ''),
                validator=ValidatorType.GLTF
            ))

        return issues

    def validate_tileset(self, tileset_path: str, write_report: bool = False) -> ValidationResult:
        """
        使用 3d-tiles-validator 验证 tileset.json

        Args:
            tileset_path: tileset.json 文件路径
            write_report: 是否写入验证报告

        Returns:
            ValidationResult: 验证结果
        """
        if not self.tools_available['3d-tiles-validator']:
            return ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="TOOL_NOT_FOUND",
                    message="3d-tiles-validator 未安装，请运行: npm install 3d-tiles-validator",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            )

        cmd = ['npx', '3d-tiles-validator', '--tilesetFile', tileset_path]

        if write_report:
            report_path = tileset_path.replace('.json', '_validation_report.json')
            cmd.extend(['--reportFile', report_path])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60
            )

            # 3d-tiles-validator 输出到 stderr
            output = result.stderr if result.stderr else result.stdout

            # 尝试解析JSON报告
            try:
                report = json.loads(output)
                issues = self._parse_tiles_issues(report)

                # 检查是否有错误
                num_errors = len([i for i in issues if i.severity == ValidationLevel.ERROR])
                success = num_errors == 0

                return ValidationResult(
                    validator=ValidatorType.TILES,
                    success=success,
                    issues=issues,
                    stats=report.get('stats', {}),
                    raw_output=output
                )
            except json.JSONDecodeError:
                # 解析文本输出
                success = result.returncode == 0 and 'Validation succeeded' in output

                return ValidationResult(
                    validator=ValidatorType.TILES,
                    success=success,
                    issues=[],
                    stats={},
                    raw_output=output
                )

        except subprocess.TimeoutExpired:
            return ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="TIMEOUT",
                    message="验证超时",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            )
        except Exception as e:
            return ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="EXCEPTION",
                    message=str(e),
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            )

    def _parse_tiles_issues(self, report: Dict) -> List[ValidationIssue]:
        """解析 3d-tiles-validator 报告中的问题"""
        issues = []

        for issue in report.get('issues', []):
            severity = ValidationLevel.ERROR if issue.get('type') == 'ERROR' else ValidationLevel.WARNING

            issues.append(ValidationIssue(
                code=issue.get('code', 'UNKNOWN'),
                message=issue.get('message', ''),
                severity=severity,
                pointer=issue.get('path', ''),
                validator=ValidatorType.TILES
            ))

        return issues

    def validate_tile_content(self, content_path: str) -> ValidationResult:
        """
        验证单个瓦片内容文件（b3dm, i3dm, pnts等）

        Args:
            content_path: 瓦片内容文件路径

        Returns:
            ValidationResult: 验证结果
        """
        if not self.tools_available['3d-tiles-validator']:
            return ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="TOOL_NOT_FOUND",
                    message="3d-tiles-validator 未安装",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            )

        cmd = ['npx', '3d-tiles-validator', '--tileContentFile', content_path]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )

            output = result.stderr if result.stderr else result.stdout
            success = result.returncode == 0

            return ValidationResult(
                validator=ValidatorType.TILES,
                success=success,
                issues=[],
                stats={},
                raw_output=output
            )

        except Exception as e:
            return ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="EXCEPTION",
                    message=str(e),
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            )

    def validate_directory(self, directory: str, recursive: bool = True) -> Dict[str, List[ValidationResult]]:
        """
        验证整个目录中的所有3D Tiles相关文件

        Args:
            directory: 要验证的目录路径
            recursive: 是否递归验证子目录

        Returns:
            Dict[str, List[ValidationResult]]: 按文件类型分类的验证结果
        """
        results = {
            'tilesets': [],
            'gltf': [],
            'tile_content': [],
            'errors': []
        }

        dir_path = Path(directory)

        if not dir_path.exists():
            results['errors'].append(ValidationResult(
                validator=ValidatorType.TILES,
                success=False,
                issues=[ValidationIssue(
                    code="PATH_NOT_FOUND",
                    message=f"目录不存在: {directory}",
                    severity=ValidationLevel.ERROR,
                    validator=ValidatorType.TILES
                )],
                stats={}
            ))
            return results

        # 查找 tileset.json
        pattern = "**/*.json" if recursive else "*.json"
        for json_file in dir_path.glob(pattern):
            if json_file.name == "tileset.json":
                if self.verbose:
                    print(f"验证 tileset: {json_file}")
                result = self.validate_tileset(str(json_file))
                results['tilesets'].append(result)

        # 查找 glTF/glb 文件
        for ext in ['*.gltf', '*.glb']:
            pattern = f"**/{ext}" if recursive else ext
            for gltf_file in dir_path.glob(pattern):
                if self.verbose:
                    print(f"验证 glTF: {gltf_file}")
                result = self.validate_gltf(str(gltf_file))
                results['gltf'].append(result)

        # 查找瓦片内容文件
        for ext in ['*.b3dm', '*.i3dm', '*.pnts', '*.cmpt']:
            pattern = f"**/{ext}" if recursive else ext
            for content_file in dir_path.glob(pattern):
                if self.verbose:
                    print(f"验证瓦片内容: {content_file}")
                result = self.validate_tile_content(str(content_file))
                results['tile_content'].append(result)

        return results

    def print_validation_report(self, results: Dict[str, List[ValidationResult]], verbose: bool = False):
        """打印验证报告"""
        print("\n" + "="*70)
        print("3D Tiles 验证报告")
        print("="*70)

        total_issues = 0
        total_errors = 0
        total_warnings = 0

        for category, category_results in results.items():
            if category == 'errors':
                continue

            if not category_results:
                continue

            print(f"\n{category.upper()}:")
            print("-" * 70)

            for result in category_results:
                status = "✅" if result.success else "❌"
                print(f"  {status} {result.validator.value}")

                # 统计问题
                errors = [i for i in result.issues if i.severity == ValidationLevel.ERROR]
                warnings = [i for i in result.issues if i.severity == ValidationLevel.WARNING]
                infos = [i for i in result.issues if i.severity == ValidationLevel.INFO]

                total_errors += len(errors)
                total_warnings += len(warnings)
                total_issues += len(result.issues)

                if errors:
                    print(f"     错误: {len(errors)}")
                    if verbose:
                        for issue in errors[:5]:  # 只显示前5个
                            print(f"       - {issue.code}: {issue.message[:80]}")
                        if len(errors) > 5:
                            print(f"       ... 还有 {len(errors) - 5} 个错误")

                if warnings:
                    print(f"     警告: {len(warnings)}")
                    if verbose:
                        for issue in warnings[:3]:  # 只显示前3个
                            print(f"       - {issue.code}: {issue.message[:80]}")

                if infos and verbose:
                    print(f"     信息: {len(infos)}")

        # 打印错误
        if results.get('errors'):
            print("\n错误:")
            print("-" * 70)
            for result in results['errors']:
                for issue in result.issues:
                    print(f"  ❌ {issue.message}")

        # 总结
        print("\n" + "="*70)
        print("总结:")
        print(f"  总问题数: {total_issues}")
        print(f"  错误: {total_errors}")
        print(f"  警告: {total_warnings}")
        print("="*70 + "\n")

        return total_errors == 0


def main():
    parser = argparse.ArgumentParser(description='3D Tiles 综合验证工具')
    parser.add_argument('path', help='要验证的文件或目录路径')
    parser.add_argument('--type', choices=['auto', 'tileset', 'gltf', 'directory'],
                       default='auto', help='验证类型')
    parser.add_argument('--recursive', '-r', action='store_true',
                       help='递归验证目录')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='详细输出')
    parser.add_argument('--report', help='保存报告到文件')

    args = parser.parse_args()

    validator = TilesValidator(verbose=args.verbose)

    # 自动检测类型
    if args.type == 'auto':
        path = Path(args.path)
        if path.is_dir():
            args.type = 'directory'
        elif path.suffix == '.json':
            args.type = 'tileset'
        elif path.suffix in ['.gltf', '.glb']:
            args.type = 'gltf'
        else:
            print(f"无法自动检测类型: {path.suffix}")
            sys.exit(1)

    # 执行验证
    if args.type == 'directory':
        results = validator.validate_directory(args.path, args.recursive)
        success = validator.print_validation_report(results, args.verbose)
    elif args.type == 'tileset':
        result = validator.validate_tileset(args.path, write_report=bool(args.report))
        results = {'tilesets': [result], 'gltf': [], 'tile_content': [], 'errors': []}
        success = validator.print_validation_report(results, args.verbose)
    elif args.type == 'gltf':
        result = validator.validate_gltf(args.path)
        results = {'tilesets': [], 'gltf': [result], 'tile_content': [], 'errors': []}
        success = validator.print_validation_report(results, args.verbose)

    # 保存报告
    if args.report:
        report_data = {
            'path': args.path,
            'type': args.type,
            'results': [
                {
                    'validator': r.validator.value,
                    'success': r.success,
                    'issues': [
                        {
                            'code': i.code,
                            'message': i.message,
                            'severity': i.severity.value,
                            'pointer': i.pointer
                        }
                        for i in r.issues
                    ],
                    'stats': r.stats
                }
                for category, category_results in results.items()
                if category != 'errors'
                for r in category_results
            ]
        }

        with open(args.report, 'w', encoding='utf-8') as f:
            json.dump(report_data, f, indent=2, ensure_ascii=False)

        print(f"报告已保存: {args.report}")

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
