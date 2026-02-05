"""
测试运行器模块

负责并发执行测试、超时控制、错误处理和重试、进度跟踪
"""

import time
from pathlib import Path
from typing import List, Dict, Any, Optional
from dataclasses import dataclass, field
from enum import Enum
from concurrent.futures import ThreadPoolExecutor, as_completed
import json

from .config import Config, TestCase, ValidationMode
from .converter import Converter, ConversionResult, ConversionStatus
from .validators import OfficialValidator, PreRenderingValidator


class TestStatus(Enum):
    """测试状态枚举"""
    PENDING = "pending"
    RUNNING = "running"
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"


@dataclass
class TestResult:
    """测试结果数据类"""
    test_name: str
    status: TestStatus
    timestamp: str
    duration_ms: int
    conversion: Optional[ConversionResult] = None
    validation: Optional[Dict[str, Any]] = None
    comparison: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    notes: Optional[str] = None


@dataclass
class SuiteResult:
    """测试套件结果数据类"""
    suite_name: str
    total_tests: int
    passed: int
    failed: int
    skipped: int
    duration_ms: int
    test_results: List[TestResult] = field(default_factory=list)


class TestRunner:
    """测试运行器"""

    def __init__(
        self,
        config: Config,
        converter: Converter,
        official_validator: Optional[OfficialValidator] = None,
        pre_rendering_validator: Optional[PreRenderingValidator] = None
    ):
        """
        初始化测试运行器

        Args:
            config: 配置对象
            converter: 格式转换器
            official_validator: 官方验证器（可选）
            pre_rendering_validator: 预渲染验证器（可选）
        """
        self.config = config
        self.converter = converter
        self.official_validator = official_validator or OfficialValidator()
        self.pre_rendering_validator = pre_rendering_validator or PreRenderingValidator()

    def run_suite(
        self,
        suite_name: str,
        output_dir: Path,
        baseline_dir: Optional[Path] = None,
        mode: str = "strict",
        parallel: int = 1,
        verbose: bool = False
    ) -> SuiteResult:
        """
        运行测试套件

        Args:
            suite_name: 测试套件名称
            output_dir: 输出目录
            baseline_dir: 基线目录（可选）
            mode: 验证模式（strict/relaxed/fast）
            parallel: 并发数
            verbose: 是否显示详细信息

        Returns:
            SuiteResult: 测试套件结果
        """
        start_time = time.time()

        # 获取测试套件
        suite = self.config.get_suite(suite_name)
        if suite is None:
            raise ValueError(f"测试套件不存在: {suite_name}")

        # 获取验证模式
        validation_mode = self.config.get_validation_mode(mode)
        if validation_mode is None:
            raise ValueError(f"验证模式不存在: {mode}")

        # 创建输出目录
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        test_results = []

        # 并发执行测试
        if parallel > 1:
            test_results = self._run_parallel(
                suite.tests,
                output_dir,
                baseline_dir,
                validation_mode,
                parallel,
                verbose
            )
        else:
            test_results = self._run_sequential(
                suite.tests,
                output_dir,
                baseline_dir,
                validation_mode,
                verbose
            )

        # 统计结果
        duration_ms = int((time.time() - start_time) * 1000)
        passed = sum(1 for r in test_results if r.status == TestStatus.PASSED)
        failed = sum(1 for r in test_results if r.status == TestStatus.FAILED)
        skipped = sum(1 for r in test_results if r.status == TestStatus.SKIPPED)

        return SuiteResult(
            suite_name=suite_name,
            total_tests=len(test_results),
            passed=passed,
            failed=failed,
            skipped=skipped,
            duration_ms=duration_ms,
            test_results=test_results
        )

    def _run_sequential(
        self,
        test_cases: List[TestCase],
        output_dir: Path,
        baseline_dir: Optional[Path],
        validation_mode: ValidationMode,
        verbose: bool
    ) -> List[TestResult]:
        """顺序执行测试"""
        results = []

        for test_case in test_cases:
            result = self._run_test(
                test_case,
                output_dir,
                baseline_dir,
                validation_mode,
                verbose
            )
            results.append(result)

        return results

    def _run_parallel(
        self,
        test_cases: List[TestCase],
        output_dir: Path,
        baseline_dir: Optional[Path],
        validation_mode: ValidationMode,
        parallel: int,
        verbose: bool
    ) -> List[TestResult]:
        """并发执行测试"""
        results = []

        with ThreadPoolExecutor(max_workers=parallel) as executor:
            # 提交所有测试
            future_to_test = {
                executor.submit(
                    self._run_test,
                    test_case,
                    output_dir,
                    baseline_dir,
                    validation_mode,
                    verbose
                ): test_case for test_case in test_cases
            }

            # 收集结果
            for future in as_completed(future_to_test):
                test_case = future_to_test[future]
                try:
                    result = future.result()
                    results.append(result)
                except Exception as e:
                    results.append(TestResult(
                        test_name=test_case.name,
                        status=TestStatus.FAILED,
                        timestamp=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                        duration_ms=0,
                        error_message=str(e)
                    ))

        return results

    def _run_test(
        self,
        test_case: TestCase,
        output_dir: Path,
        baseline_dir: Optional[Path],
        validation_mode: ValidationMode,
        verbose: bool
    ) -> TestResult:
        """
        运行单个测试

        Args:
            test_case: 测试用例
            output_dir: 输出目录
            baseline_dir: 基线目录
            validation_mode: 验证模式
            verbose: 是否显示详细信息

        Returns:
            TestResult: 测试结果
        """
        start_time = time.time()
        timestamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

        if verbose:
            print(f"[INFO] 执行测试: {test_case.name} [{test_case.priority}]")

        # 检查条件
        if test_case.condition == "dir_exists":
            input_path = test_case.get_full_input_path(self.config.project_root)
            if not input_path.exists() or not input_path.is_dir():
                return TestResult(
                    test_name=test_case.name,
                    status=TestStatus.SKIPPED,
                    timestamp=timestamp,
                    duration_ms=0,
                    notes=f"输入目录不存在: {test_case.input}"
                )

        # 创建测试输出目录
        test_output_dir = output_dir / test_case.name
        test_output_dir.mkdir(parents=True, exist_ok=True)

        # 执行转换
        conversion_result = self.converter.convert(
            test_case.__dict__,
            test_output_dir,
            timeout=test_case.timeout,
            verbose=verbose
        )

        if not conversion_result.success:
            return TestResult(
                test_name=test_case.name,
                status=TestStatus.FAILED,
                timestamp=timestamp,
                duration_ms=conversion_result.duration_ms,
                conversion=conversion_result,
                error_message=conversion_result.error_message
            )

        # 执行验证
        validation_result = self._validate(
            test_case,
            test_output_dir,
            validation_mode,
            verbose
        )

        # 检查验证结果
        if validation_result["passed"]:
            return TestResult(
                test_name=test_case.name,
                status=TestStatus.PASSED,
                timestamp=timestamp,
                duration_ms=conversion_result.duration_ms,
                conversion=conversion_result,
                validation=validation_result
            )
        else:
            return TestResult(
                test_name=test_case.name,
                status=TestStatus.FAILED,
                timestamp=timestamp,
                duration_ms=conversion_result.duration_ms,
                conversion=conversion_result,
                validation=validation_result,
                error_message=f"验证失败: {validation_result['errors']} errors, {validation_result['warnings']} warnings"
            )

    def _validate(
        self,
        test_case: TestCase,
        test_output_dir: Path,
        validation_mode: ValidationMode,
        verbose: bool
    ) -> Dict[str, Any]:
        """
        执行验证

        Args:
            test_case: 测试用例
            test_output_dir: 测试输出目录
            validation_mode: 验证模式
            verbose: 是否显示详细信息

        Returns:
            Dict[str, Any]: 验证结果
        """
        validation_result = {
            "passed": True,
            "errors": 0,
            "warnings": 0,
            "official": {},
            "pre_rendering": {}
        }

        # 获取输出文件
        output_file = test_output_dir / test_case.output
        if not output_file.exists():
            validation_result["passed"] = False
            validation_result["errors"] += 1
            return validation_result

        # 如果输出是目录，查找实际的 tileset.json 文件
        actual_output_file = output_file
        if output_file.is_dir():
            actual_output_file = output_file / "tileset.json"
            if not actual_output_file.exists():
                validation_result["passed"] = False
                validation_result["errors"] += 1
                return validation_result

        # 预渲染验证
        if actual_output_file.suffix in [".glb", ".gltf"]:
            pre_rendering_result = self.pre_rendering_validator.validate_gltf_for_rendering(actual_output_file)
        elif actual_output_file.suffix == ".json":
            pre_rendering_result = self.pre_rendering_validator.validate_tileset_for_rendering(actual_output_file)
        else:
            pre_rendering_result = None

        if pre_rendering_result:
            validation_result["pre_rendering"] = {
                "errors": pre_rendering_result.errors,
                "warnings": pre_rendering_result.warnings,
                "issues": [
                    {
                        "code": issue.code,
                        "severity": issue.severity.value,
                        "message": issue.message,
                        "rendering_impact": issue.rendering_impact,
                        "fix": issue.fix
                    }
                    for issue in pre_rendering_result.issues
                ]
            }
            validation_result["errors"] += pre_rendering_result.errors
            validation_result["warnings"] += pre_rendering_result.warnings

        # 官方验证器验证
        if validation_mode.use_official_validator:
            if actual_output_file.suffix in [".glb", ".gltf"]:
                official_result = self.official_validator.validate_gltf(
                    actual_output_file,
                    test_output_dir,
                    verbose=verbose
                )
                validation_result["official"]["gltf_validator"] = {
                    "success": official_result.passed,
                    "errors": official_result.errors,
                    "warnings": official_result.warnings
                }
                validation_result["errors"] += official_result.errors
                validation_result["warnings"] += official_result.warnings
            elif actual_output_file.suffix == ".json":
                official_result = self.official_validator.validate_tileset(
                    actual_output_file,
                    test_output_dir,
                    verbose=verbose
                )
                validation_result["official"]["tiles_validator"] = {
                    "success": official_result.passed,
                    "errors": official_result.errors,
                    "warnings": official_result.warnings
                }
                validation_result["errors"] += official_result.errors
                validation_result["warnings"] += official_result.warnings

        validation_result["passed"] = validation_result["errors"] == 0

        return validation_result
