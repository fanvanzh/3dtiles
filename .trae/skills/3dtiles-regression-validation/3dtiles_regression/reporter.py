"""
报告生成器模块

负责生成JSON和HTML格式的详细报告
"""

import json
from pathlib import Path
from typing import Dict, Any, List, Optional
from datetime import datetime

from .runner import SuiteResult, TestResult, TestStatus


class ReportGenerator:
    """报告生成器"""
    
    def __init__(self):
        """初始化报告生成器"""
        pass
    
    def generate_json(
        self,
        suite_results: List[SuiteResult],
        output_path: Path,
        metadata: Optional[Dict[str, Any]] = None
    ):
        """
        生成JSON格式报告
        
        Args:
            suite_results: 测试套件结果列表
            output_path: 输出文件路径
            metadata: 元数据
        """
        report = {
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "metadata": metadata or {},
            "summary": self._generate_summary(suite_results),
            "suites": []
        }
        
        for suite_result in suite_results:
            suite_report = {
                "name": suite_result.suite_name,
                "total_tests": suite_result.total_tests,
                "passed": suite_result.passed,
                "failed": suite_result.failed,
                "skipped": suite_result.skipped,
                "duration_ms": suite_result.duration_ms,
                "tests": []
            }
            
            for test_result in suite_result.test_results:
                test_report = self._generate_test_report(test_result)
                suite_report["tests"].append(test_report)
            
            report["suites"].append(suite_report)
        
        # 写入文件
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
    
    def generate_html(
        self,
        suite_results: List[SuiteResult],
        output_path: Path,
        metadata: Optional[Dict[str, Any]] = None
    ):
        """
        生成HTML格式报告
        
        Args:
            suite_results: 测试套件结果列表
            output_path: 输出文件路径
            metadata: 元数据
        """
        summary = self._generate_summary(suite_results)
        
        html = self._generate_html_header()
        html += self._generate_summary_html(summary, metadata)
        html += self._generate_suites_html(suite_results)
        html += self._generate_html_footer()
        
        # 写入文件
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html)
    
    def _generate_summary(self, suite_results: List[SuiteResult]) -> Dict[str, Any]:
        """生成摘要信息"""
        total_tests = sum(r.total_tests for r in suite_results)
        total_passed = sum(r.passed for r in suite_results)
        total_failed = sum(r.failed for r in suite_results)
        total_skipped = sum(r.skipped for r in suite_results)
        total_duration_ms = sum(r.duration_ms for r in suite_results)
        
        return {
            "total_suites": len(suite_results),
            "total_tests": total_tests,
            "total_passed": total_passed,
            "total_failed": total_failed,
            "total_skipped": total_skipped,
            "pass_rate": (total_passed / total_tests * 100) if total_tests > 0 else 0,
            "total_duration_ms": total_duration_ms
        }
    
    def _generate_test_report(self, test_result: TestResult) -> Dict[str, Any]:
        """生成测试报告"""
        report = {
            "name": test_result.test_name,
            "status": test_result.status.value,
            "timestamp": test_result.timestamp,
            "duration_ms": test_result.duration_ms
        }
        
        if test_result.conversion:
            report["conversion"] = {
                "success": test_result.conversion.success,
                "duration_ms": test_result.conversion.duration_ms,
                "output_file": test_result.conversion.output_file,
                "file_size_mb": test_result.conversion.file_size_mb
            }
        
        if test_result.validation:
            report["validation"] = test_result.validation
        
        if test_result.error_message:
            report["error_message"] = test_result.error_message
        
        if test_result.notes:
            report["notes"] = test_result.notes
        
        return report
    
    def _generate_html_header(self) -> str:
        """生成HTML头部"""
        return """<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>3D Tiles回归测试报告</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #2c3e50;
            border-bottom: 2px solid #3498db;
            padding-bottom: 10px;
        }
        h2 {
            color: #34495e;
            margin-top: 30px;
        }
        .summary {
            background: #ecf0f1;
            padding: 20px;
            border-radius: 5px;
            margin-top: 20px;
        }
        .summary-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 15px;
        }
        .summary-item {
            background: white;
            padding: 15px;
            border-radius: 5px;
            text-align: center;
        }
        .summary-item .label {
            color: #7f8c8d;
            font-size: 14px;
            margin-bottom: 5px;
        }
        .summary-item .value {
            font-size: 24px;
            font-weight: bold;
        }
        .passed { color: #27ae60; }
        .failed { color: #e74c3c; }
        .skipped { color: #f39c12; }
        .test-case {
            margin-bottom: 20px;
            padding: 15px;
            border: 1px solid #ddd;
            border-radius: 5px;
            border-left-width: 5px;
        }
        .test-case.passed { border-left-color: #27ae60; }
        .test-case.failed { border-left-color: #e74c3c; }
        .test-case.skipped { border-left-color: #f39c12; }
        .test-case h3 {
            margin-top: 0;
            color: #2c3e50;
        }
        .metrics {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
            margin-top: 10px;
        }
        .metric {
            background: #f8f9fa;
            padding: 8px 12px;
            border-radius: 3px;
            font-size: 14px;
        }
        .metric strong {
            color: #2c3e50;
        }
        .issue {
            margin-left: 20px;
            padding: 10px;
            border-left: 3px solid #ddd;
            margin-top: 10px;
        }
        .issue-error {
            border-left-color: #e74c3c;
            background: #fee;
        }
        .issue-warning {
            border-left-color: #f39c12;
            background: #fff8e1;
        }
        .issue strong {
            display: block;
            margin-bottom: 5px;
        }
        .issue em {
            color: #7f8c8d;
            font-size: 13px;
        }
        .suite {
            margin-top: 30px;
            border: 1px solid #ddd;
            border-radius: 5px;
            overflow: hidden;
        }
        .suite-header {
            background: #3498db;
            color: white;
            padding: 15px;
        }
        .suite-content {
            padding: 15px;
        }
        .timestamp {
            color: #7f8c8d;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
"""
    
    def _generate_html_footer(self) -> str:
        """生成HTML尾部"""
        return """
    </div>
</body>
</html>
"""
    
    def _generate_summary_html(
        self,
        summary: Dict[str, Any],
        metadata: Optional[Dict[str, Any]]
    ) -> str:
        """生成摘要HTML"""
        html = """
        <h1>3D Tiles回归测试报告</h1>
        <p class="timestamp">生成时间: """ + datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC") + """</p>
        
        <div class="summary">
            <h2>测试概览</h2>
            <div class="summary-grid">
                <div class="summary-item">
                    <div class="label">总测试套件</div>
                    <div class="value">""" + str(summary["total_suites"]) + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">总测试数</div>
                    <div class="value">""" + str(summary["total_tests"]) + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">通过</div>
                    <div class="value passed">""" + str(summary["total_passed"]) + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">失败</div>
                    <div class="value failed">""" + str(summary["total_failed"]) + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">跳过</div>
                    <div class="value skipped">""" + str(summary["total_skipped"]) + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">通过率</div>
                    <div class="value">""" + f"{summary['pass_rate']:.1f}%" + """</div>
                </div>
                <div class="summary-item">
                    <div class="label">总耗时</div>
                    <div class="value">""" + f"{summary['total_duration_ms']/1000:.1f}s" + """</div>
                </div>
            </div>
        </div>
"""
        return html
    
    def _generate_suites_html(self, suite_results: List[SuiteResult]) -> str:
        """生成测试套件HTML"""
        html = ""
        
        for suite_result in suite_results:
            html += f"""
        <div class="suite">
            <div class="suite-header">
                <h3>{suite_result.suite_name}</h3>
                <p>通过: {suite_result.passed} | 失败: {suite_result.failed} | 跳过: {suite_result.skipped} | 耗时: {suite_result.duration_ms/1000:.1f}s</p>
            </div>
            <div class="suite-content">
"""
            
            for test_result in suite_result.test_results:
                html += self._generate_test_html(test_result)
            
            html += """
            </div>
        </div>
"""
        
        return html
    
    def _generate_test_html(self, test_result: TestResult) -> str:
        """生成测试HTML"""
        status_class = test_result.status.value
        status_symbol = "✓" if test_result.status == TestStatus.PASSED else "✗" if test_result.status == TestStatus.FAILED else "⊘"
        
        html = f"""
                <div class="test-case {status_class}">
                    <h3>{test_result.test_name} {status_symbol}</h3>
"""
        
        if test_result.conversion:
            html += f"""
                    <div class="metrics">
                        <div class="metric"><strong>转换时间:</strong> {test_result.conversion.duration_ms/1000:.1f}s</div>
                        <div class="metric"><strong>输出文件:</strong> {Path(test_result.conversion.output_file).name} ({test_result.conversion.file_size_mb:.2f}MB)</div>
                    </div>
"""
        
        if test_result.validation:
            validation = test_result.validation
            pre_rendering = validation.get("pre_rendering", {})
            official = validation.get("official", {})
            
            html += f"""
                    <div class="metrics">
                        <div class="metric"><strong>验证结果:</strong> {validation.get('errors', 0)} errors, {validation.get('warnings', 0)} warnings</div>
                    </div>
"""
            
            # 显示预渲染验证问题
            if pre_rendering.get("issues"):
                for issue in pre_rendering["issues"]:
                    severity_class = "issue-error" if issue["severity"] == "error" else "issue-warning"
                    html += f"""
                    <div class="issue {severity_class}">
                        <strong>[{issue['severity'].upper()}] {issue['code']}</strong>
                        {issue['message']}<br>
                        <em>渲染影响: {issue['rendering_impact']}</em>
"""
                    if issue.get("fix"):
                        html += f"""                        <em>修复建议: {issue['fix']}</em>"""
                    
                    html += """
                    </div>
"""
        
        if test_result.error_message:
            html += f"""
                    <div class="issue issue-error">
                        <strong>错误信息</strong>
                        {test_result.error_message}
                    </div>
"""
        
        if test_result.notes:
            html += f"""
                    <p><em>备注: {test_result.notes}</em></p>
"""
        
        html += """
                </div>
"""
        
        return html
