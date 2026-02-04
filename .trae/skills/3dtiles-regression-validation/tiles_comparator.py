#!/usr/bin/env python3
"""
3D Tiles 格式比对工具
支持 tileset.json、B3DM、I3DM、PNTS、CMPT 格式的内容比对
参考: https://github.com/CesiumGS/3d-tiles/tree/main/specification
"""

import json
import struct
import argparse
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
import os


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
    file_type: str
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
        print(f"3D Tiles 文件对比报告")
        print(f"{'='*60}")
        print(f"文件1: {self.file1}")
        print(f"文件2: {self.file2}")
        print(f"文件类型: {self.file_type}")
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


class TilesComparator:
    """3D Tiles 格式比较器"""

    def __init__(self, float_tolerance: float = 1e-6, ignore_fields: Optional[set] = None):
        self.float_tolerance = float_tolerance
        self.ignore_fields = ignore_fields or {'generator', 'created', 'timestamp', 'version'}

    def compare(self, file1: str, file2: str) -> ComparisonReport:
        """
        比较两个 3D Tiles 文件

        Args:
            file1: 第一个文件路径
            file2: 第二个文件路径

        Returns:
            ComparisonReport: 比较报告
        """
        file_type = self._detect_file_type(file1)
        report = ComparisonReport(
            file1=file1,
            file2=file2,
            file_type=file_type,
            identical=True
        )

        if file_type == 'tileset':
            self._compare_tileset(file1, file2, report)
        elif file_type == 'b3dm':
            self._compare_b3dm(file1, file2, report)
        elif file_type == 'i3dm':
            self._compare_i3dm(file1, file2, report)
        elif file_type == 'pnts':
            self._compare_pnts(file1, file2, report)
        elif file_type == 'cmpt':
            self._compare_cmpt(file1, file2, report)
        else:
            raise ValueError(f"不支持的文件类型: {file_type}")

        report.identical = len(report.differences) == 0
        return report

    def _detect_file_type(self, file_path: str) -> str:
        """检测文件类型"""
        path = Path(file_path)
        suffix = path.suffix.lower()

        if suffix == '.json':
            return 'tileset'
        elif suffix == '.b3dm':
            return 'b3dm'
        elif suffix == '.i3dm':
            return 'i3dm'
        elif suffix == '.pnts':
            return 'pnts'
        elif suffix == '.cmpt':
            return 'cmpt'
        else:
            raise ValueError(f"未知的文件扩展名: {suffix}")

    def _compare_tileset(self, file1: str, file2: str, report: ComparisonReport):
        """比较 tileset.json 文件"""
        with open(file1, 'r', encoding='utf-8') as f:
            data1 = json.load(f)
        with open(file2, 'r', encoding='utf-8') as f:
            data2 = json.load(f)

        self._compare_json_objects(data1, data2, "", report)

    def _compare_b3dm(self, file1: str, file2: str, report: ComparisonReport):
        """
        比较 B3DM 文件

        B3DM 文件结构:
        - Header (28 bytes): magic (4) + version (4) + byteLength (4) +
                           featureTableJSONByteLength (4) + featureTableBinaryByteLength (4) +
                           batchTableJSONByteLength (4) + batchTableBinaryByteLength (4)
        - Feature Table JSON
        - Feature Table Binary
        - Batch Table JSON
        - Batch Table Binary
        - glTF Binary
        """
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            # 读取并比较头部
            header1 = f1.read(28)
            header2 = f2.read(28)

            if header1 != header2:
                report.add_diff(DiffItem(
                    path="header",
                    field="bytes",
                    value1=f"<{len(header1)} bytes>",
                    value2=f"<{len(header2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="B3DM 头部不匹配"
                ))

            # 解析头部信息
            magic1, version1, byte_length1, ft_json_len1, ft_bin_len1, bt_json_len1, bt_bin_len1 = \
                struct.unpack('<4sIIIIII', header1)

            if magic1 != b'b3dm':
                report.add_diff(DiffItem(
                    path="header",
                    field="magic",
                    value1=magic1,
                    value2="b'b3dm'",
                    result=ComparisonResult.DIFFERENT,
                    message="无效的 B3DM magic number"
                ))

            # 比较 Feature Table JSON
            ft_json1 = f1.read(ft_json_len1).decode('utf-8')
            ft_json2 = f2.read(ft_json_len1).decode('utf-8')

            if ft_json1 != ft_json2:
                self._compare_json_objects(
                    json.loads(ft_json1),
                    json.loads(ft_json2),
                    "featureTable",
                    report
                )

            # 跳过 Feature Table Binary
            f1.read(ft_bin_len1)
            f2.read(ft_bin_len1)

            # 比较 Batch Table JSON
            bt_json1 = f1.read(bt_json_len1).decode('utf-8')
            bt_json2 = f2.read(bt_json_len1).decode('utf-8')

            if bt_json1 != bt_json2:
                self._compare_json_objects(
                    json.loads(bt_json1),
                    json.loads(bt_json2),
                    "batchTable",
                    report
                )

            # 跳过 Batch Table Binary
            f1.read(bt_bin_len1)
            f2.read(bt_bin_len1)

            # 比较 glTF Binary 部分
            glb_data1 = f1.read()
            glb_data2 = f2.read()

            if glb_data1 != glb_data2:
                report.add_diff(DiffItem(
                    path="glb",
                    field="binary",
                    value1=f"<{len(glb_data1)} bytes>",
                    value2=f"<{len(glb_data2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="glTF 二进制数据不匹配"
                ))

    def _compare_i3dm(self, file1: str, file2: str, report: ComparisonReport):
        """
        比较 I3DM 文件

        I3DM 文件结构与 B3DM 类似，但包含实例化信息
        """
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            # 读取并比较头部
            header1 = f1.read(32)
            header2 = f2.read(32)

            if header1 != header2:
                report.add_diff(DiffItem(
                    path="header",
                    field="bytes",
                    value1=f"<{len(header1)} bytes>",
                    value2=f"<{len(header2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="I3DM 头部不匹配"
                ))

            magic1, version1, byte_length1, ft_json_len1, ft_bin_len1, bt_json_len1, bt_bin_len1, gltf_format = \
                struct.unpack('<4sIIIIIII', header1)

            if magic1 != b'i3dm':
                report.add_diff(DiffItem(
                    path="header",
                    field="magic",
                    value1=magic1,
                    value2="b'i3dm'",
                    result=ComparisonResult.DIFFERENT,
                    message="无效的 I3DM magic number"
                ))

            # I3DM 的其余部分与 B3DM 类似
            ft_json1 = f1.read(ft_json_len1).decode('utf-8')
            ft_json2 = f2.read(ft_json_len1).decode('utf-8')

            if ft_json1 != ft_json2:
                self._compare_json_objects(
                    json.loads(ft_json1),
                    json.loads(ft_json2),
                    "featureTable",
                    report
                )

            f1.read(ft_bin_len1)
            f2.read(ft_bin_len1)

            bt_json1 = f1.read(bt_json_len1).decode('utf-8')
            bt_json2 = f2.read(bt_json_len1).decode('utf-8')

            if bt_json1 != bt_json2:
                self._compare_json_objects(
                    json.loads(bt_json1),
                    json.loads(bt_json2),
                    "batchTable",
                    report
                )

            f1.read(bt_bin_len1)
            f2.read(bt_bin_len1)

            glb_data1 = f1.read()
            glb_data2 = f2.read()

            if glb_data1 != glb_data2:
                report.add_diff(DiffItem(
                    path="glb",
                    field="binary",
                    value1=f"<{len(glb_data1)} bytes>",
                    value2=f"<{len(glb_data2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="glTF 二进制数据不匹配"
                ))

    def _compare_pnts(self, file1: str, file2: str, report: ComparisonReport):
        """
        比较 PNTS 文件

        PNTS 文件结构:
        - Header (28 bytes): magic (4) + version (4) + byteLength (4) +
                           featureTableJSONByteLength (4) + featureTableBinaryByteLength (4) +
                           batchTableJSONByteLength (4) + batchTableBinaryByteLength (4)
        - Feature Table JSON
        - Feature Table Binary (point data)
        - Batch Table JSON (optional)
        - Batch Table Binary (optional)
        """
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            header1 = f1.read(28)
            header2 = f2.read(28)

            if header1 != header2:
                report.add_diff(DiffItem(
                    path="header",
                    field="bytes",
                    value1=f"<{len(header1)} bytes>",
                    value2=f"<{len(header2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="PNTS 头部不匹配"
                ))

            magic1, version1, byte_length1, ft_json_len1, ft_bin_len1, bt_json_len1, bt_bin_len1 = \
                struct.unpack('<4sIIIIII', header1)

            if magic1 != b'pnts':
                report.add_diff(DiffItem(
                    path="header",
                    field="magic",
                    value1=magic1,
                    value2="b'pnts'",
                    result=ComparisonResult.DIFFERENT,
                    message="无效的 PNTS magic number"
                ))

            ft_json1 = f1.read(ft_json_len1).decode('utf-8')
            ft_json2 = f2.read(ft_json_len1).decode('utf-8')

            if ft_json1 != ft_json2:
                self._compare_json_objects(
                    json.loads(ft_json1),
                    json.loads(ft_json2),
                    "featureTable",
                    report
                )

            ft_bin1 = f1.read(ft_bin_len1)
            ft_bin2 = f2.read(ft_bin_len1)

            if ft_bin1 != ft_bin2:
                report.add_diff(DiffItem(
                    path="featureTableBinary",
                    field="bytes",
                    value1=f"<{len(ft_bin1)} bytes>",
                    value2=f"<{len(ft_bin2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="Feature Table 二进制数据不匹配"
                ))

            if bt_json_len1 > 0:
                bt_json1 = f1.read(bt_json_len1).decode('utf-8')
                bt_json2 = f2.read(bt_json_len1).decode('utf-8')

                if bt_json1 != bt_json2:
                    self._compare_json_objects(
                        json.loads(bt_json1),
                        json.loads(bt_json2),
                        "batchTable",
                        report
                    )

            if bt_bin_len1 > 0:
                bt_bin1 = f1.read(bt_bin_len1)
                bt_bin2 = f2.read(bt_bin_len1)

                if bt_bin1 != bt_bin2:
                    report.add_diff(DiffItem(
                        path="batchTableBinary",
                        field="bytes",
                        value1=f"<{len(bt_bin1)} bytes>",
                        value2=f"<{len(bt_bin2)} bytes>",
                        result=ComparisonResult.DIFFERENT,
                        message="Batch Table 二进制数据不匹配"
                    ))

    def _compare_cmpt(self, file1: str, file2: str, report: ComparisonReport):
        """
        比较 CMPT 文件

        CMPT 文件结构:
        - Header (16 bytes): magic (4) + version (4) + byteLength (4) + tilesLength (4)
        - Tiles: 可变数量的内嵌瓦片
        """
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            header1 = f1.read(16)
            header2 = f2.read(16)

            if header1 != header2:
                report.add_diff(DiffItem(
                    path="header",
                    field="bytes",
                    value1=f"<{len(header1)} bytes>",
                    value2=f"<{len(header2)} bytes>",
                    result=ComparisonResult.DIFFERENT,
                    message="CMPT 头部不匹配"
                ))

            magic1, version1, byte_length1, tiles_length1 = \
                struct.unpack('<4sIII', header1)

            if magic1 != b'cmpt':
                report.add_diff(DiffItem(
                    path="header",
                    field="magic",
                    value1=magic1,
                    value2="b'cmpt'",
                    result=ComparisonResult.DIFFERENT,
                    message="无效的 CMPT magic number"
                ))

            if tiles_length1 != tiles_length1:
                report.add_diff(DiffItem(
                    path="header",
                    field="tilesLength",
                    value1=tiles_length1,
                    value2=tiles_length1,
                    result=ComparisonResult.DIFFERENT,
                    message="瓦片数量不匹配"
                ))

            # 比较每个内嵌瓦片
            for i in range(tiles_length1):
                tile_byte_length1 = struct.unpack('<I', f1.read(4))[0]
                tile_byte_length2 = struct.unpack('<I', f2.read(4))[0]

                tile_data1 = f1.read(tile_byte_length1)
                tile_data2 = f2.read(tile_byte_length2)

                if tile_data1 != tile_data2:
                    report.add_diff(DiffItem(
                        path=f"tiles[{i}]",
                        field="binary",
                        value1=f"<{tile_byte_length1} bytes>",
                        value2=f"<{tile_byte_length2} bytes>",
                        result=ComparisonResult.DIFFERENT,
                        message=f"内嵌瓦片 {i} 数据不匹配"
                    ))

    def _compare_json_objects(self, obj1: Any, obj2: Any, path: str, report: ComparisonReport):
        """递归比较 JSON 对象"""
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
            else:
                report.matched_items += 1
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
            self._compare_json_objects(dict1[key], dict2[key], new_path, report)

    def _compare_lists(self, list1: List, list2: List, path: str, report: ComparisonReport):
        """比较两个列表"""
        if len(list1) != len(list2):
            report.add_diff(DiffItem(
                path=path,
                field="length",
                value1=len(list1),
                value2=len(list2),
                result=ComparisonResult.DIFFERENT,
                message="列表长度不匹配"
            ))

        min_len = min(len(list1), len(list2))
        for i in range(min_len):
            new_path = f"{path}[{i}]"
            self._compare_json_objects(list1[i], list2[i], new_path, report)

    def _float_equal(self, val1: float, val2: float) -> bool:
        """判断两个浮点数是否完全相等"""
        return val1 == val2

    def _float_tolerance_equal(self, val1: float, val2: float) -> bool:
        """判断两个浮点数是否在容差范围内相等"""
        return abs(val1 - val2) <= self.float_tolerance


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='3D Tiles 格式比对工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 比较 tileset.json
  python3 tiles_comparator.py tileset1.json tileset2.json

  # 比较 B3DM 文件
  python3 tiles_comparator.py model1.b3dm model2.b3dm

  # 使用容差
  python3 tiles_comparator.py file1.b3dm file2.b3dm --tolerance 1e-4

  # 详细输出
  python3 tiles_comparator.py file1.b3dm file2.b3dm --verbose

  # 输出 JSON 报告
  python3 tiles_comparator.py file1.b3dm file2.b3dm --output report.json
        """
    )

    parser.add_argument('file1', help='第一个文件路径')
    parser.add_argument('file2', help='第二个文件路径')
    parser.add_argument('--tolerance', type=float, default=1e-6,
                        help='浮点数容差 (默认: 1e-6)')
    parser.add_argument('--ignore', nargs='*', default=[],
                        help='要忽略的字段 (默认: generator, created, timestamp, version)')
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
    comparator = TilesComparator(float_tolerance=args.tolerance, ignore_fields=ignore_fields)

    try:
        report = comparator.compare(args.file1, args.file2)
        report.print_summary(verbose=args.verbose)

        if args.output:
            output_data = {
                'file1': report.file1,
                'file2': report.file2,
                'file_type': report.file_type,
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
