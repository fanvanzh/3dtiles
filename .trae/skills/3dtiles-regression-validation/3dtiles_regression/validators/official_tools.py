"""
官方验证工具集成模块

负责调用gltf-validator和3d-tiles-validator的CLI版本进行验证
"""

import subprocess
import json
from pathlib import Path
from typing import Dict, Any, Optional
from dataclasses import dataclass
from enum import Enum


class ValidationStatus(Enum):
    """验证状态枚举"""
    PASSED = "passed"
    FAILED = "failed"
    ERROR = "error"


@dataclass
class ValidationResult:
    """验证结果数据类"""
    status: ValidationStatus
    passed: bool
    errors: int
    warnings: int
    report_file: Optional[str] = None
    report_data: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None


class OfficialValidator:
    """官方验证器集成"""

    def __init__(self, use_npx: bool = False):
        """
        初始化官方验证器

        Args:
            use_npx: 是否使用npx运行（无需全局安装）
        """
        self.use_npx = use_npx
        self.gltf_validator_cmd = ["gltf-validator"] if use_npx else ["gltf-validator"]
        self.tiles_validator_cmd = ["3d-tiles-validator"] if use_npx else ["3d-tiles-validator"]

    def validate_gltf(
        self,
        file_path: Path,
        output_dir: Path,
        verbose: bool = False
    ) -> ValidationResult:
        """
        使用gltf-validator验证glTF/GLB文件

        Args:
            file_path: glTF/GLB文件路径
            output_dir: 输出目录
            verbose: 是否显示详细信息

        Returns:
            ValidationResult: 验证结果
        """
        # 检查文件是否存在
        if not file_path.exists():
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message=f"文件不存在: {file_path}"
            )

        # 构建输出报告路径
        report_file = output_dir / f"{file_path.stem}_gltf_validation.json"

        # 构建命令
        cmd = self.gltf_validator_cmd + [
            str(file_path),
            "--output", "json",
            "--output-file", str(report_file)
        ]

        if verbose:
            print(f"[INFO] 执行gltf-validator: {' '.join(cmd)}")

        try:
            # 执行验证
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60
            )

            # 检查返回码
            if result.returncode != 0:
                return ValidationResult(
                    status=ValidationStatus.ERROR,
                    passed=False,
                    errors=0,
                    warnings=0,
                    error_message=f"gltf-validator执行失败: {result.stderr}"
                )

            # 解析报告
            if not report_file.exists():
                return ValidationResult(
                    status=ValidationStatus.ERROR,
                    passed=False,
                    errors=0,
                    warnings=0,
                    error_message=f"验证报告未生成: {report_file}"
                )

            with open(report_file, 'r', encoding='utf-8') as f:
                report_data = json.load(f)

            # 统计错误和警告
            errors = report_data.get("numErrors", 0)
            warnings = report_data.get("numWarnings", 0)

            return ValidationResult(
                status=ValidationStatus.PASSED if errors == 0 else ValidationStatus.FAILED,
                passed=errors == 0,
                errors=errors,
                warnings=warnings,
                report_file=str(report_file),
                report_data=report_data
            )

        except subprocess.TimeoutExpired:
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message="gltf-validator执行超时"
            )
        except Exception as e:
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message=f"gltf-validator执行异常: {str(e)}"
            )

    def validate_tileset(
        self,
        file_path: Path,
        output_dir: Path,
        verbose: bool = False
    ) -> ValidationResult:
        """
        使用3d-tiles-validator验证tileset.json文件

        Args:
            file_path: tileset.json文件路径
            output_dir: 输出目录
            verbose: 是否显示详细信息

        Returns:
            ValidationResult: 验证结果
        """
        # 检查文件是否存在
        if not file_path.exists():
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message=f"文件不存在: {file_path}"
            )

        # 构建输出报告路径
        report_file = output_dir / f"{file_path.stem}_tiles_validation.json"

        # 构建命令
        cmd = self.tiles_validator_cmd + [
            str(file_path),
            "--output", "json",
            "--output-file", str(report_file)
        ]

        if verbose:
            print(f"[INFO] 执行3d-tiles-validator: {' '.join(cmd)}")

        try:
            # 执行验证
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60
            )

            # 检查返回码
            if result.returncode != 0:
                return ValidationResult(
                    status=ValidationStatus.ERROR,
                    passed=False,
                    errors=0,
                    warnings=0,
                    error_message=f"3d-tiles-validator执行失败: {result.stderr}"
                )

            # 解析报告
            if not report_file.exists():
                return ValidationResult(
                    status=ValidationStatus.ERROR,
                    passed=False,
                    errors=0,
                    warnings=0,
                    error_message=f"验证报告未生成: {report_file}"
                )

            with open(report_file, 'r', encoding='utf-8') as f:
                report_data = json.load(f)

            # 统计错误和警告
            errors = report_data.get("numErrors", 0)
            warnings = report_data.get("numWarnings", 0)

            return ValidationResult(
                status=ValidationStatus.PASSED if errors == 0 else ValidationStatus.FAILED,
                passed=errors == 0,
                errors=errors,
                warnings=warnings,
                report_file=str(report_file),
                report_data=report_data
            )

        except subprocess.TimeoutExpired:
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message="3d-tiles-validator执行超时"
            )
        except Exception as e:
            return ValidationResult(
                status=ValidationStatus.ERROR,
                passed=False,
                errors=0,
                warnings=0,
                error_message=f"3d-tiles-validator执行异常: {str(e)}"
            )
