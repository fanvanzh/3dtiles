#!/usr/bin/env python3
"""
glTF 文件对比工具
基于 glTF 2.0 规范实现，支持 .gltf 和 .glb 格式
参考: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
"""

import json
import struct
import argparse
import sys
from typing import Dict, List, Any, Tuple, Optional, Set
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path


class ComparisonResult(Enum):
    """比较结果"""
    IDENTICAL = "identical"
    DIFFERENT = "different"
    TOLERANCE_DIFF = "tolerance_diff"


@dataclass
class DiffItem:
    """差异项"""
    path: str
    field: str
    value1: Any
    value2: Any
    result: ComparisonResult
    message: str = ""


@dataclass
class ComparisonReport:
    """比较报告"""
    file1: str
    file2: str
    identical: bool
    differences: List[DiffItem] = field(default_factory=list)
    total_items: int = 0
    matched_items: int = 0
    tolerance_matched: int = 0

    def add_diff(self, diff: DiffItem):
        """添加差异项"""
        self.differences.append(diff)

    def print_summary(self, verbose: bool = False):
        """打印摘要"""
        print(f"\n{'='*60}")
        print(f"glTF 文件对比报告")
        print(f"{'='*60}")
        print(f"文件1: {self.file1}")
        print(f"文件2: {self.file2}")
        print(f"\n比较结果: {'✓ 完全一致' if self.identical else '✗ 存在差异'}")
        print(f"总项目数: {self.total_items}")
        print(f"匹配项数: {self.matched_items}")
        if self.tolerance_matched > 0:
            print(f"容差匹配: {self.tolerance_matched}")
        print(f"差异项数: {len(self.differences)}")

        if self.differences and verbose:
            print(f"\n{'='*60}")
            print(f"详细差异:")
            print(f"{'='*60}")
            for i, diff in enumerate(self.differences, 1):
                icon = "⚠" if diff.result == ComparisonResult.TOLERANCE_DIFF else "✗"
                print(f"\n[{i}] {icon} {diff.path}.{diff.field}")
                print(f"    文件1: {diff.value1}")
                print(f"    文件2: {diff.value2}")
                if diff.message:
                    print(f"    说明: {diff.message}")


class GLTFComparator:
    """glTF 文件比较器"""

    def __init__(self, float_tolerance: float = 1e-6, ignore_fields: Optional[Set[str]] = None):
        self.float_tolerance = float_tolerance
        self.ignore_fields = ignore_fields or {
            'generator', 'version', 'extensionsUsed', 'extensionsRequired'
        }

    def compare(self, file1: str, file2: str) -> ComparisonReport:
        """
        比较两个 glTF 文件

        Args:
            file1: 第一个 glTF 文件路径
            file2: 第二个 glTF 文件路径

        Returns:
            ComparisonReport: 比较报告
        """
        gltf1 = self._load_gltf(file1)
        gltf2 = self._load_gltf(file2)

        report = ComparisonReport(file1=file1, file2=file2, identical=True)
        self._compare_objects(gltf1, gltf2, "", report)

        report.identical = len(report.differences) == 0
        return report

    def _load_gltf(self, file_path: str) -> Dict:
        """
        加载 glTF 文件

        Args:
            file_path: glTF 文件路径

        Returns:
            解析后的 glTF JSON 对象
        """
        path = Path(file_path)

        if path.suffix.lower() == '.glb':
            return self._load_glb(file_path)
        elif path.suffix.lower() == '.gltf':
            return self._load_gltf_json(file_path)
        else:
            raise ValueError(f"不支持的文件格式: {path.suffix}")

    def _load_gltf_json(self, file_path: str) -> Dict:
        """加载 .gltf JSON 格式文件"""
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    def _load_glb(self, file_path: str) -> Dict:
        """
        加载 .glb 二进制格式文件

        GLB 文件结构 (根据 glTF 2.0 规范):
        - 12-byte header: magic (4) + version (4) + length (4)
        - JSON chunk: chunkLength (4) + chunkType (4) + JSON data
        - Binary chunk: chunkLength (4) + chunkType (4) + binary data
        """
        with open(file_path, 'rb') as f:
            header = f.read(12)
            magic, version, length = struct.unpack('<III', header)

            if magic != 0x46546C67:
                raise ValueError(f"无效的 GLB 文件: magic number 不匹配")

            if version != 2:
                raise ValueError(f"不支持的 GLB 版本: {version}")

            json_chunk_length = struct.unpack('<I', f.read(4))[0]
            json_chunk_type = struct.unpack('<I', f.read(4))[0]

            if json_chunk_type != 0x4E4F534A:
                raise ValueError("JSON chunk type 不匹配")

            json_data = f.read(json_chunk_length).decode('utf-8')
            gltf_data = json.loads(json_data)

            binary_chunk_length = struct.unpack('<I', f.read(4))[0]
            binary_chunk_type = struct.unpack('<I', f.read(4))[0]

            if binary_chunk_type != 0x004E4942:
                raise ValueError("Binary chunk type 不匹配")

            binary_data = f.read(binary_chunk_length)

            gltf_data['_binary_data'] = binary_data

            return gltf_data

    def _compare_objects(self, obj1: Any, obj2: Any, path: str, report: ComparisonReport):
        """
        递归比较两个对象

        Args:
            obj1: 第一个对象
            obj2: 第二个对象
            path: 当前路径
            report: 比较报告
        """
        report.total_items += 1

        if type(obj1) != type(obj2):
            report.add_diff(DiffItem(
                path=path,
                field="type",
                value1=type(obj1).__name__,
                value2=type(obj2).__name__,
                result=ComparisonResult.DIFFERENT,
                message="类型不匹配"
            ))
            return

        if isinstance(obj1, dict):
            self._compare_dicts(obj1, obj2, path, report)
        elif isinstance(obj1, list):
            self._compare_lists(obj1, obj2, path, report)
        elif isinstance(obj1, (int, str, bool)) or obj1 is None:
            if obj1 != obj2:
                report.add_diff(DiffItem(
                    path=path,
                    field="value",
                    value1=obj1,
                    value2=obj2,
                    result=ComparisonResult.DIFFERENT
                ))
        elif isinstance(obj1, float):
            if not self._float_equal(obj1, obj2):
                result = ComparisonResult.TOLERANCE_DIFF if self._float_tolerance_equal(obj1, obj2) else ComparisonResult.DIFFERENT
                if result == ComparisonResult.TOLERANCE_DIFF:
                    report.tolerance_matched += 1
                    report.matched_items += 1
                else:
                    report.add_diff(DiffItem(
                        path=path,
                        field="value",
                        value1=obj1,
                        value2=obj2,
                        result=result,
                        message=f"差异: {abs(obj1 - obj2):.2e}"
                    ))
            else:
                report.matched_items += 1
        elif isinstance(obj1, bytes):
            if obj1 != obj2:
                report.add_diff(DiffItem(
                    path=path,
                    field="bytes",
                    value1=f"<{len(obj1)} bytes>",
                    value2=f"<{len(obj2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="二进制数据不匹配"
                ))
        else:
            report.matched_items += 1

    def _compare_dicts(self, dict1: Dict, dict2: Dict, path: str, report: ComparisonReport):
        """比较两个字典"""
        keys1 = set(dict1.keys())
        keys2 = set(dict2.keys())

        for key in self.ignore_fields:
            keys1.discard(key)
            keys2.discard(key)

        only_in_1 = keys1 - keys2
        only_in_2 = keys2 - keys1

        for key in only_in_1:
            report.add_diff(DiffItem(
                path=path,
                field=key,
                value1=dict1[key],
                value2="<missing>",
                result=ComparisonResult.DIFFERENT,
                message="仅在文件1中存在"
            ))

        for key in only_in_2:
            report.add_diff(DiffItem(
                path=path,
                field=key,
                value1="<missing>",
                value2=dict2[key],
                result=ComparisonResult.DIFFERENT,
                message="仅在文件2中存在"
            ))

        common_keys = keys1 & keys2
        for key in sorted(common_keys):
            new_path = f"{path}.{key}" if path else key
            self._compare_objects(dict1[key], dict2[key], new_path, report)

    def _compare_lists(self, list1: List, list2: List, path: str, report: ComparisonReport):
        """比较两个列表"""
        if len(list1) != len(list2):
            report.add_diff(DiffItem(
                path=path,
                field="length",
                value1=len(list1),
                value2=len(list2),
                result=ComparisonResult.DIFFERENT,
                message="数组长度不匹配"
            ))
            min_len = min(len(list1), len(list2))
        else:
            min_len = len(list1)

        for i in range(min_len):
            new_path = f"{path}[{i}]"
            self._compare_objects(list1[i], list2[i], new_path, report)

    def _float_equal(self, val1: float, val2: float) -> bool:
        """判断两个浮点数是否完全相等"""
        return val1 == val2

    def _float_tolerance_equal(self, val1: float, val2: float) -> bool:
        """判断两个浮点数是否在容差范围内相等"""
        return abs(val1 - val2) <= self.float_tolerance


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='glTF 文件对比工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 基本用法
  python3 gltf_comparator.py file1.glb file2.glb

  # 使用容差
  python3 gltf_comparator.py file1.glb file2.glb --tolerance 1e-4

  # 详细输出
  python3 gltf_comparator.py file1.glb file2.glb --verbose

  # 忽略特定字段
  python3 gltf_comparator.py file1.glb file2.glb --ignore generator version
        """
    )

    parser.add_argument('file1', help='第一个 glTF 文件路径')
    parser.add_argument('file2', help='第二个 glTF 文件路径')
    parser.add_argument('--tolerance', type=float, default=1e-6,
                        help='浮点数容差 (默认: 1e-6)')
    parser.add_argument('--ignore', nargs='*', default=[],
                        help='要忽略的字段 (默认: generator, version, extensionsUsed, extensionsRequired)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='显示详细差异')
    parser.add_argument('--output', '-o', help='输出报告到 JSON 文件')

    args = parser.parse_args()

    if not Path(args.file1).exists():
        print(f"错误: 文件不存在: {args.file1}", file=sys.stderr)
        sys.exit(1)

    if not Path(args.file2).exists():
        print(f"错误: 文件不存在: {args.file2}", file=sys.stderr)
        sys.exit(1)

    ignore_fields = set(args.ignore) if args.ignore else None
    comparator = GLTFComparator(float_tolerance=args.tolerance, ignore_fields=ignore_fields)

    try:
        report = comparator.compare(args.file1, args.file2)
        report.print_summary(verbose=args.verbose)

        if args.output:
            output_data = {
                'file1': report.file1,
                'file2': report.file2,
                'identical': report.identical,
                'total_items': report.total_items,
                'matched_items': report.matched_items,
                'tolerance_matched': report.tolerance_matched,
                'differences': [
                    {
                        'path': diff.path,
                        'field': diff.field,
                        'value1': str(diff.value1),
                        'value2': str(diff.value2),
                        'result': diff.result.value,
                        'message': diff.message
                    }
                    for diff in report.differences
                ]
            }
            with open(args.output, 'w', encoding='utf-8') as f:
                json.dump(output_data, f, indent=2, ensure_ascii=False)
            print(f"\n报告已保存到: {args.output}")

        sys.exit(0 if report.identical else 1)

    except Exception as e:
        print(f"错误: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(2)


if __name__ == '__main__':
    main()
