"""
格式转换器模块

负责调用_3dtile可执行文件进行格式转换，支持多种输入输出格式和优化参数
"""

import subprocess
import json
import time
from pathlib import Path
from typing import List, Optional, Dict, Any
from dataclasses import dataclass
from enum import Enum


class ConversionStatus(Enum):
    """转换状态枚举"""
    SUCCESS = "success"
    FAILED = "failed"
    TIMEOUT = "timeout"
    CANCELLED = "cancelled"


@dataclass
class ConversionResult:
    """转换结果数据类"""
    status: ConversionStatus
    success: bool
    duration_ms: int
    output_file: str
    file_size_mb: float
    error_message: Optional[str] = None
    stdout: Optional[str] = None
    stderr: Optional[str] = None
    metrics: Optional[Dict[str, Any]] = None


class Converter:
    """格式转换器"""

    def __init__(self, executable: Path, project_root: Path):
        """
        初始化转换器

        Args:
            executable: _3dtile可执行文件路径
            project_root: 项目根目录
        """
        self.executable = executable
        self.project_root = project_root
        self.data_dir = project_root / "data"

    def convert(
        self,
        test_case: Dict[str, Any],
        output_dir: Path,
        timeout: int = 300,
        verbose: bool = False
    ) -> ConversionResult:
        """
        执行格式转换

        Args:
            test_case: 测试用例配置
            output_dir: 输出目录
            timeout: 超时时间（秒）
            verbose: 是否显示详细信息

        Returns:
            ConversionResult: 转换结果
        """
        start_time = time.time()

        # 构建命令
        cmd = self._build_command(test_case, output_dir)

        if verbose:
            print(f"[INFO] 执行转换命令: {' '.join(cmd)}")

        try:
            # 执行转换
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                cwd=str(self.project_root)
            )

            duration_ms = int((time.time() - start_time) * 1000)

            # 检查返回码
            if result.returncode != 0:
                error_msg = f"转换失败，返回码: {result.returncode}"
                if result.stderr:
                    error_msg += f"\n错误信息: {result.stderr}"
                if result.stdout:
                    error_msg += f"\n输出信息: {result.stdout}"
                return ConversionResult(
                    status=ConversionStatus.FAILED,
                    success=False,
                    duration_ms=duration_ms,
                    output_file="",
                    file_size_mb=0.0,
                    error_message=error_msg,
                    stdout=result.stdout,
                    stderr=result.stderr
                )

            # 检查输出文件
            output_file = output_dir / test_case["output"]
            if not output_file.exists():
                return ConversionResult(
                    status=ConversionStatus.FAILED,
                    success=False,
                    duration_ms=duration_ms,
                    output_file=str(output_file),
                    file_size_mb=0.0,
                    error_message=f"输出文件不存在: {output_file}",
                    stdout=result.stdout,
                    stderr=result.stderr
                )

            # 获取文件大小
            file_size_mb = output_file.stat().st_size / (1024 * 1024)

            # 解析输出获取指标
            metrics = self._parse_metrics(result.stdout)

            return ConversionResult(
                status=ConversionStatus.SUCCESS,
                success=True,
                duration_ms=duration_ms,
                output_file=str(output_file),
                file_size_mb=file_size_mb,
                stdout=result.stdout,
                stderr=result.stderr,
                metrics=metrics
            )

        except subprocess.TimeoutExpired:
            duration_ms = int((time.time() - start_time) * 1000)
            return ConversionResult(
                status=ConversionStatus.TIMEOUT,
                success=False,
                duration_ms=duration_ms,
                output_file="",
                file_size_mb=0.0,
                error_message=f"转换超时（{timeout}秒）"
            )
        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            return ConversionResult(
                status=ConversionStatus.FAILED,
                success=False,
                duration_ms=duration_ms,
                output_file="",
                file_size_mb=0.0,
                error_message=f"转换异常: {str(e)}"
            )

    def _build_command(self, test_case: Dict[str, Any], output_dir: Path) -> List[str]:
        """
        构建转换命令

        Args:
            test_case: 测试用例配置
            output_dir: 输出目录

        Returns:
            List[str]: 命令列表
        """
        cmd = [str(self.executable)]

        # 输入文件
        input_path = self.data_dir / test_case["input"]
        cmd.append("--input")
        cmd.append(str(input_path))

        # 输出文件
        output_file = output_dir / test_case["output"]
        cmd.append("--output")
        cmd.append(str(output_file))

        # 格式参数
        format_type = test_case.get("format", "osgb")
        cmd.append("--format")
        cmd.append(format_type)

        # 其他参数
        args = test_case.get("args", [])
        cmd.extend(args)

        return cmd

    def _parse_metrics(self, stdout: str) -> Dict[str, Any]:
        """
        解析转换输出获取指标

        Args:
            stdout: 标准输出

        Returns:
            Dict[str, Any]: 指标字典
        """
        metrics = {}

        try:
            # 尝试解析JSON输出
            if stdout.strip().startswith('{'):
                data = json.loads(stdout)
                metrics.update(data)
        except json.JSONDecodeError:
            pass

        return metrics
