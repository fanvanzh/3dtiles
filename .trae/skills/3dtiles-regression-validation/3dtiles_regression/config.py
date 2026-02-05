"""
配置解析模块
负责加载和解析test_config.json配置文件
"""

import json
import os
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field


@dataclass
class TestCase:
    """测试用例数据类"""
    name: str
    description: str
    input: str
    format: str
    args: List[str] = field(default_factory=list)
    output: str = "tileset.json"
    priority: str = "P2"
    condition: Optional[str] = None
    expected_outputs: List[str] = field(default_factory=list)
    max_size_mb: Optional[int] = None
    timeout: int = 300
    validation: Dict[str, Any] = field(default_factory=dict)
    notes: Optional[str] = None
    metrics: List[str] = field(default_factory=list)
    expected_behavior: Optional[str] = None

    def get_full_input_path(self, project_root: Path) -> Path:
        """获取完整的输入路径"""
        return project_root / "data" / self.input

    def get_output_filename(self) -> str:
        """获取输出文件名"""
        return self.output

    def should_skip(self, project_root: Path) -> tuple[bool, str]:
        """检查是否应该跳过此测试用例"""
        if self.condition == "manual":
            return True, "手动执行测试"

        input_path = self.get_full_input_path(project_root)

        if self.condition == "dir_exists":
            if not input_path.is_dir():
                return True, f"输入目录不存在: {input_path}"
            return False, ""

        if self.condition == "file_exists":
            if not input_path.is_file():
                return True, f"输入文件不存在: {input_path}"
            return False, ""

        if not input_path.exists():
            return True, f"输入不存在: {input_path}"

        return False, ""


@dataclass
class TestSuite:
    """测试套件数据类"""
    name: str
    description: str
    timeout: int
    ci_required: bool
    tests: List[TestCase] = field(default_factory=list)

    def get_tests_by_priority(self, priority: str) -> List[TestCase]:
        """按优先级获取测试用例"""
        return [t for t in self.tests if t.priority == priority]

    def get_p0_tests(self) -> List[TestCase]:
        """获取P0优先级测试"""
        return self.get_tests_by_priority("P0")

    def get_p1_tests(self) -> List[TestCase]:
        """获取P1优先级测试"""
        return self.get_tests_by_priority("P1")

    def get_p2_tests(self) -> List[TestCase]:
        """获取P2优先级测试"""
        return self.get_tests_by_priority("P2")


@dataclass
class ValidationMode:
    """验证模式数据类"""
    name: str
    description: str
    float_tolerance: float
    ignore_fields: List[str]
    skip_content_validation: bool
    use_official_validator: bool


@dataclass
class SelectionCriteria:
    """选择标准数据类"""
    priority: str
    description: str
    blocking: bool
    ci_required: bool
    notification: str
    examples: List[str]


@dataclass
class ExecutionProfile:
    """执行配置数据类"""
    name: str
    description: str
    command: str
    estimated_time: str


class Config:
    """配置管理器"""

    def __init__(self, config_path: Optional[Path] = None):
        """
        初始化配置管理器

        Args:
            config_path: 配置文件路径，默认为test_config.json
        """
        if config_path is None:
            script_dir = Path(__file__).parent
            config_path = script_dir.parent / "test_config.json"

        self.config_path = Path(config_path)
        self.project_root = Path(__file__).parent.parent.parent.parent.parent
        self.baseline_dir = self.project_root / "test_data" / "test_baseline"
        self.test_output_dir = self.project_root / "test_data" / "test_output"
        self.executable = self.project_root / "target" / "debug" / "_3dtile"

        self._data: Optional[Dict] = None
        self._test_suites: Dict[str, TestSuite] = {}
        self._validation_modes: Dict[str, ValidationMode] = {}
        self._selection_criteria: Dict[str, SelectionCriteria] = {}
        self._execution_profiles: Dict[str, ExecutionProfile] = {}

    def load(self) -> None:
        """加载配置文件"""
        if not self.config_path.exists():
            raise FileNotFoundError(f"配置文件不存在: {self.config_path}")

        with open(self.config_path, 'r', encoding='utf-8') as f:
            self._data = json.load(f)

        self._parse_test_suites()
        self._parse_validation_modes()
        self._parse_selection_criteria()
        self._parse_execution_profiles()

    def _parse_test_suites(self) -> None:
        """解析测试套件"""
        suites_data = self._data.get("test_suites", [])

        for suite_data in suites_data:
            suite_name = suite_data.get("name")
            if not suite_name:
                continue

            tests = []
            for test_data in suite_data.get("tests", []):
                test = TestCase(
                    name=test_data.get("name"),
                    description=test_data.get("description"),
                    input=test_data.get("input"),
                    format=test_data.get("format"),
                    args=test_data.get("args", []),
                    output=test_data.get("output", "tileset.json"),
                    priority=test_data.get("priority", "P2"),
                    condition=test_data.get("condition"),
                    expected_outputs=test_data.get("expected_outputs", []),
                    max_size_mb=test_data.get("max_size_mb"),
                    timeout=test_data.get("timeout", 300),
                    validation=test_data.get("validation", {}),
                    notes=test_data.get("notes"),
                    metrics=test_data.get("metrics", []),
                    expected_behavior=test_data.get("expected_behavior")
                )
                tests.append(test)

            suite = TestSuite(
                name=suite_name,
                description=suite_data.get("description"),
                timeout=suite_data.get("timeout", 300),
                ci_required=suite_data.get("ci_required", False),
                tests=tests
            )
            self._test_suites[suite_name] = suite

    def _parse_validation_modes(self) -> None:
        """解析验证模式"""
        modes_data = self._data.get("validation_modes", {})

        for mode_name, mode_data in modes_data.items():
            mode = ValidationMode(
                name=mode_name,
                description=mode_data.get("description"),
                float_tolerance=mode_data.get("float_tolerance", 1e-9),
                ignore_fields=mode_data.get("ignore_fields", []),
                skip_content_validation=not mode_data.get("check_content", True),
                use_official_validator=mode_data.get("use_official_validator", True)
            )
            self._validation_modes[mode_name] = mode

    def _parse_selection_criteria(self) -> None:
        """解析选择标准"""
        criteria_data = self._data.get("selection_criteria", {})

        for priority, criteria in criteria_data.items():
            criterion = SelectionCriteria(
                priority=priority,
                description=criteria.get("description"),
                blocking=criteria.get("blocking", False),
                ci_required=criteria.get("ci_required", False),
                notification=criteria.get("notification", ""),
                examples=criteria.get("examples", [])
            )
            self._selection_criteria[priority] = criterion

    def _parse_execution_profiles(self) -> None:
        """解析执行配置"""
        profiles_data = self._data.get("execution_profiles", {})

        for profile_name, profile in profiles_data.items():
            execution_profile = ExecutionProfile(
                name=profile_name,
                description=profile.get("description"),
                command=profile.get("command"),
                estimated_time=profile.get("estimated_time")
            )
            self._execution_profiles[profile_name] = execution_profile

    @property
    def test_suites(self) -> Dict[str, TestSuite]:
        """获取所有测试套件"""
        return self._test_suites

    @property
    def validation_modes(self) -> Dict[str, ValidationMode]:
        """获取所有验证模式"""
        return self._validation_modes

    @property
    def selection_criteria(self) -> Dict[str, SelectionCriteria]:
        """获取所有选择标准"""
        return self._selection_criteria

    @property
    def execution_profiles(self) -> Dict[str, ExecutionProfile]:
        """获取所有执行配置"""
        return self._execution_profiles

    def get_suite(self, suite_name: str) -> Optional[TestSuite]:
        """获取指定测试套件"""
        return self._test_suites.get(suite_name)

    def get_all_suites(self) -> List[TestSuite]:
        """获取所有测试套件"""
        return list(self._test_suites.values())

    def get_validation_mode(self, mode_name: str) -> Optional[ValidationMode]:
        """获取指定验证模式"""
        return self._validation_modes.get(mode_name)

    def get_all_tests(self) -> List[TestCase]:
        """获取所有测试用例"""
        all_tests = []
        for suite in self._test_suites.values():
            all_tests.extend(suite.tests)
        return all_tests

    def get_tests_by_suite(self, suite_name: str) -> List[TestCase]:
        """获取指定套件的测试用例"""
        suite = self.get_suite(suite_name)
        return suite.tests if suite else []

    def get_tests_by_priority(self, priority: str) -> List[TestCase]:
        """按优先级获取所有测试用例"""
        all_tests = []
        for suite in self._test_suites.values():
            all_tests.extend(suite.get_tests_by_priority(priority))
        return all_tests

    def get_p0_tests(self) -> List[TestCase]:
        """获取所有P0优先级测试"""
        return self.get_tests_by_priority("P0")

    def get_p1_tests(self) -> List[TestCase]:
        """获取所有P1优先级测试"""
        return self.get_tests_by_priority("P1")

    def get_p2_tests(self) -> List[TestCase]:
        """获取所有P2优先级测试"""
        return self.get_tests_by_priority("P2")

    def ensure_directories(self) -> None:
        """确保必要的目录存在"""
        self.baseline_dir.mkdir(parents=True, exist_ok=True)
        self.test_output_dir.mkdir(parents=True, exist_ok=True)

    def __repr__(self) -> str:
        return f"Config(config_path={self.config_path}, project_root={self.project_root})"
