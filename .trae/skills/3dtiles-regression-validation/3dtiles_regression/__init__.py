"""
3D Tiles回归测试框架

为3dtiles工程提供完整的Python回归测试框架，用于代码重构前后的数据一致性验证
"""

__version__ = "2.0.0"

from .config import Config, TestCase, TestSuite, ValidationMode
from .converter import Converter, ConversionResult, ConversionStatus
from .validators import (
    OfficialValidator,
    ValidationResult,
    ValidationStatus,
    PreRenderingValidator,
    PreRenderingValidationResult,
    ValidationIssue,
    IssueSeverity
)
from .runner import TestRunner, TestResult, SuiteResult, TestStatus
from .reporter import ReportGenerator

__all__ = [
    # Config
    "Config",
    "TestCase",
    "TestSuite",
    "ValidationMode",
    # Converter
    "Converter",
    "ConversionResult",
    "ConversionStatus",
    # Validators
    "OfficialValidator",
    "ValidationResult",
    "ValidationStatus",
    "PreRenderingValidator",
    "PreRenderingValidationResult",
    "ValidationIssue",
    "IssueSeverity",
    # Runner
    "TestRunner",
    "TestResult",
    "SuiteResult",
    "TestStatus",
    # Reporter
    "ReportGenerator",
]
