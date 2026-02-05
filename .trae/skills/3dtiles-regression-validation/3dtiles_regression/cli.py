"""
命令行接口模块

提供命令行参数解析和子命令处理
"""

import argparse
import sys
from pathlib import Path
from typing import Optional

from .config import Config
from .converter import Converter
from .validators import OfficialValidator, PreRenderingValidator
from .runner import TestRunner
from .reporter import ReportGenerator


def main():
    """主入口函数"""
    parser = argparse.ArgumentParser(
        description="3D Tiles回归测试框架",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--version",
        action="version",
        version="3dtiles-regression 2.0.0"
    )

    subparsers = parser.add_subparsers(dest="command", help="可用命令")

    # generate子命令
    generate_parser = subparsers.add_parser("generate", help="生成基线数据")
    generate_parser.add_argument(
        "--suite",
        type=str,
        default="all",
        help="测试套件名称（默认: all）"
    )
    generate_parser.add_argument(
        "--test",
        type=str,
        help="测试用例名称"
    )
    generate_parser.add_argument(
        "--force",
        action="store_true",
        help="强制覆盖已存在的基线"
    )
    generate_parser.add_argument(
        "--verbose",
        action="store_true",
        help="显示详细信息"
    )

    # run子命令
    run_parser = subparsers.add_parser("run", help="运行回归测试")
    run_parser.add_argument(
        "--suite",
        type=str,
        default="all",
        help="测试套件名称（默认: all）"
    )
    run_parser.add_argument(
        "--test",
        type=str,
        help="测试用例名称"
    )
    run_parser.add_argument(
        "--priority",
        type=str,
        choices=["P0", "P1", "P2"],
        help="按优先级过滤"
    )
    run_parser.add_argument(
        "--mode",
        type=str,
        choices=["strict", "relaxed", "fast"],
        default="strict",
        help="验证模式（默认: strict）"
    )
    run_parser.add_argument(
        "--parallel",
        type=int,
        default=1,
        help="并发数（默认: 1）"
    )
    run_parser.add_argument(
        "--report",
        type=str,
        choices=["json", "html", "both"],
        default="both",
        help="报告格式（默认: both）"
    )
    run_parser.add_argument(
        "--output",
        type=str,
        help="输出目录"
    )
    run_parser.add_argument(
        "--verbose",
        action="store_true",
        help="显示详细信息"
    )

    # validate子命令
    validate_parser = subparsers.add_parser("validate", help="验证数据")
    validate_parser.add_argument(
        "--suite",
        type=str,
        default="all",
        help="测试套件名称（默认: all）"
    )
    validate_parser.add_argument(
        "--test",
        type=str,
        help="测试用例名称"
    )
    validate_parser.add_argument(
        "--mode",
        type=str,
        choices=["strict", "relaxed", "fast"],
        default="strict",
        help="验证模式（默认: strict）"
    )
    validate_parser.add_argument(
        "--use-official",
        action="store_true",
        default=True,
        help="使用官方验证器"
    )
    validate_parser.add_argument(
        "--no-official",
        action="store_true",
        help="不使用官方验证器"
    )
    validate_parser.add_argument(
        "--verbose",
        action="store_true",
        help="显示详细信息"
    )

    # report子命令
    report_parser = subparsers.add_parser("report", help="生成报告")
    report_parser.add_argument(
        "--format",
        type=str,
        choices=["json", "html"],
        default="both",
        help="报告格式（默认: both）"
    )
    report_parser.add_argument(
        "--output",
        type=str,
        help="输出目录"
    )
    report_parser.add_argument(
        "--compare",
        type=str,
        nargs=2,
        metavar=("BASELINE", "CURRENT"),
        help="比较两次运行结果"
    )

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    # 执行命令
    if args.command == "generate":
        cmd_generate(args)
    elif args.command == "run":
        cmd_run(args)
    elif args.command == "validate":
        cmd_validate(args)
    elif args.command == "report":
        cmd_report(args)


def cmd_generate(args):
    """执行generate命令"""
    print("[INFO] 加载配置文件...")
    config = Config()
    config.load()

    print(f"[INFO] 测试套件: {args.suite}")

    # 初始化组件
    converter = Converter(
        config.executable,
        config.project_root
    )

    # 获取测试套件
    if args.suite == "all":
        test_suites = config.get_all_suites()
    else:
        suite = config.get_suite(args.suite)
        if suite is None:
            print(f"[ERROR] 测试套件不存在: {args.suite}")
            sys.exit(1)
        test_suites = [suite]

    # 生成基线
    print("[INFO] 生成基线数据...")
    for suite in test_suites:
        print(f"[INFO] 测试套件: {suite.name} ({len(suite.tests)}个测试用例)")

        for test_case in suite.tests:
            if args.verbose:
                print(f"[INFO] 生成基线: {test_case.name}")

            # 检查是否已存在
            baseline_dir = config.baseline_dir / suite.name / test_case.name
            baseline_file = baseline_dir / test_case.output

            if baseline_file.exists() and not args.force:
                if args.verbose:
                    print(f"[INFO] 跳过已存在的基线: {test_case.name}")
                continue

            # 执行转换
            result = converter.convert(
                test_case.__dict__,
                baseline_dir,
                timeout=test_case.timeout,
                verbose=args.verbose
            )

            if result.success:
                print(f"[SUCCESS] {test_case.name} 基线生成成功 (耗时: {result.duration_ms/1000:.1f}s)")
            else:
                print(f"[FAILED] {test_case.name} 基线生成失败: {result.error_message}")

    print("[INFO] 基线生成完成")


def cmd_run(args):
    """执行run命令"""
    print("[INFO] 加载配置文件...")
    config = Config()
    config.load()

    print(f"[INFO] 测试套件: {args.suite}")
    print(f"[INFO] 验证模式: {args.mode}")

    # 初始化组件
    converter = Converter(
        config.executable,
        config.project_root
    )

    official_validator = OfficialValidator()
    pre_rendering_validator = PreRenderingValidator()

    test_runner = TestRunner(
        config,
        converter,
        official_validator,
        pre_rendering_validator
    )

    # 获取测试套件
    if args.suite == "all":
        test_suites = config.get_all_suites()
    else:
        suite = config.get_suite(args.suite)
        if suite is None:
            print(f"[ERROR] 测试套件不存在: {args.suite}")
            sys.exit(1)
        test_suites = [suite]

    # 确定输出目录
    if args.output:
        output_dir = Path(args.output)
    else:
        output_dir = config.test_output_dir / "current"

    # 运行测试
    print("[INFO] 执行测试...")
    suite_results = []

    for suite in test_suites:
        print(f"[INFO] 测试套件: {suite.name} ({len(suite.tests)}个测试用例)")

        suite_result = test_runner.run_suite(
            suite.name,
            output_dir,
            baseline_dir=config.baseline_dir,
            mode=args.mode,
            parallel=args.parallel,
            verbose=args.verbose
        )

        suite_results.append(suite_result)

        # 打印结果
        print(f"[INFO] 测试完成: {suite_result.passed}/{suite_result.total_tests} 通过")

    # 生成报告
    if args.report in ["json", "both"]:
        json_report_path = output_dir / "report.json"
        print(f"[INFO] 生成JSON报告: {json_report_path}")
        reporter = ReportGenerator()
        reporter.generate_json(
            suite_results,
            json_report_path,
            metadata={
                "validation_mode": args.mode,
                "parallel": args.parallel
            }
        )

    if args.report in ["html", "both"]:
        html_report_path = output_dir / "report.html"
        print(f"[INFO] 生成HTML报告: {html_report_path}")
        reporter = ReportGenerator()
        reporter.generate_html(
            suite_results,
            html_report_path,
            metadata={
                "validation_mode": args.mode,
                "parallel": args.parallel
            }
        )

    # 打印摘要
    total_tests = sum(r.total_tests for r in suite_results)
    total_passed = sum(r.passed for r in suite_results)
    total_failed = sum(r.failed for r in suite_results)

    print(f"\n[SUMMARY] 总测试数: {total_tests}")
    print(f"[SUMMARY] 通过: {total_passed} ({total_passed/total_tests*100:.1f}%)" if total_tests > 0 else "[SUMMARY] 通过: 0")
    print(f"[SUMMARY] 失败: {total_failed}")

    if total_failed > 0:
        sys.exit(1)


def cmd_validate(args):
    """执行validate命令"""
    print("[INFO] 加载配置文件...")
    config = Config()
    config.load()

    print(f"[INFO] 测试套件: {args.suite}")
    print(f"[INFO] 验证模式: {args.mode}")

    # 初始化组件
    converter = Converter(
        config.executable,
        config.project_root
    )

    use_official = args.use_official and not args.no_official
    official_validator = OfficialValidator() if use_official else None
    pre_rendering_validator = PreRenderingValidator()

    test_runner = TestRunner(
        config,
        converter,
        official_validator,
        pre_rendering_validator
    )

    # 获取测试套件
    if args.suite == "all":
        test_suites = config.get_all_suites()
    else:
        suite = config.get_suite(args.suite)
        if suite is None:
            print(f"[ERROR] 测试套件不存在: {args.suite}")
            sys.exit(1)
        test_suites = [suite]

    # 运行验证
    print("[INFO] 执行验证...")
    suite_results = []

    for suite in test_suites:
        print(f"[INFO] 测试套件: {suite.name} ({len(suite.tests)}个测试用例)")

        suite_result = test_runner.run_suite(
            suite.name,
            config.test_output_dir / "validation",
            baseline_dir=None,
            mode=args.mode,
            parallel=1,
            verbose=args.verbose
        )

        suite_results.append(suite_result)

        # 打印结果
        print(f"[INFO] 验证完成: {suite_result.passed}/{suite_result.total_tests} 通过")

    # 打印摘要
    total_tests = sum(r.total_tests for r in suite_results)
    total_passed = sum(r.passed for r in suite_results)
    total_failed = sum(r.failed for r in suite_results)

    print(f"\n[SUMMARY] 总测试数: {total_tests}")
    print(f"[SUMMARY] 通过: {total_passed} ({total_passed/total_tests*100:.1f}%)" if total_tests > 0 else "[SUMMARY] 通过: 0")
    print(f"[SUMMARY] 失败: {total_failed}")

    if total_failed > 0:
        sys.exit(1)


def cmd_report(args):
    """执行report命令"""
    print("[INFO] 生成报告...")

    # TODO: 实现报告生成逻辑
    print("[INFO] 报告生成功能待实现")


if __name__ == "__main__":
    main()
