#!/usr/bin/env python3
"""
3D Tiles 格式比对功能测试脚本
测试 tiles_comparator.py 的所有格式比对功能
"""

import os
import sys
import json
import tempfile
import struct
import shutil
from typing import Dict, List, Any

# 添加当前目录到路径
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)

try:
    from tiles_comparator import TilesComparator, ComparisonReport
    from gltf_comparator import GLTFComparator, ComparisonReport as GLTFComparisonReport
except ImportError as e:
    print(f"错误: 无法导入比较器模块: {e}")
    sys.exit(1)


class TilesComparatorTest:
    """3D Tiles 格式比对测试类"""

    def __init__(self):
        self.test_dir = tempfile.mkdtemp(prefix='tiles_test_')
        self.passed = 0
        self.failed = 0
        self.skipped = 0

    def cleanup(self):
        """清理测试目录"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)

    def create_test_tileset(self, filename: str, version: str = "1.0", asset_name: str = "Test") -> str:
        """创建测试用的 tileset.json 文件"""
        tileset = {
            "asset": {
                "version": version,
                "tilesetVersion": "1.0.0",
                "name": asset_name
            },
            "geometricError": 500.0,
            "root": {
                "boundingVolume": {
                    "box": [
                        0, 0, 0,
                        100, 0, 0,
                        0, 100, 0,
                        0, 0, 100
                    ]
                },
                "geometricError": 100.0,
                "refine": "ADD",
                "children": [
                    {
                        "boundingVolume": {
                            "box": [
                                0, 0, 0,
                                50, 0, 0,
                                0, 50, 0,
                                0, 0, 50
                            ]
                        },
                        "geometricError": 10.0,
                        "content": {
                            "uri": "child.b3dm"
                        }
                    }
                ]
            }
        }

        filepath = os.path.join(self.test_dir, filename)
        with open(filepath, 'w') as f:
            json.dump(tileset, f, indent=2)
        return filepath

    def create_test_b3dm(self, filename: str, gltf_data: bytes = None) -> str:
        """创建测试用的 B3DM 文件"""
        if gltf_data is None:
            gltf_data = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "test"}]}'

        b3dm_magic = b'b3dm'
        version = 1
        byte_length = 28 + len(gltf_data)
        feature_table_json_byte_length = 0
        feature_table_binary_byte_length = 0
        batch_table_json_byte_length = 0
        batch_table_binary_byte_length = 0

        header = struct.pack('<4sIIIIII',
                            b3dm_magic,
                            version,
                            byte_length,
                            feature_table_json_byte_length,
                            feature_table_binary_byte_length,
                            batch_table_json_byte_length,
                            batch_table_binary_byte_length)

        filepath = os.path.join(self.test_dir, filename)
        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(gltf_data)
        return filepath

    def create_test_i3dm(self, filename: str, gltf_data: bytes = None) -> str:
        """创建测试用的 I3DM 文件"""
        if gltf_data is None:
            gltf_data = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "instance"}]}'

        i3dm_magic = b'i3dm'
        version = 1
        byte_length = 32 + len(gltf_data)
        feature_table_json_byte_length = 0
        feature_table_binary_byte_length = 0
        batch_table_json_byte_length = 0
        batch_table_binary_byte_length = 0
        gltf_format = 0

        header = struct.pack('<4sIIIIIII',
                            i3dm_magic,
                            version,
                            byte_length,
                            feature_table_json_byte_length,
                            feature_table_binary_byte_length,
                            batch_table_json_byte_length,
                            batch_table_binary_byte_length,
                            gltf_format)

        filepath = os.path.join(self.test_dir, filename)
        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(gltf_data)
        return filepath

    def create_test_pnts(self, filename: str) -> str:
        """创建测试用的 PNTS 文件"""
        pnts_magic = b'pnts'
        version = 1
        feature_table_json_byte_length = 0
        feature_table_binary_byte_length = 16
        batch_table_json_byte_length = 0
        batch_table_binary_byte_length = 0
        byte_length = 28 + feature_table_json_byte_length + feature_table_binary_byte_length + batch_table_json_byte_length + batch_table_binary_byte_length

        header = struct.pack('<4sIIIIII',
                            pnts_magic,
                            version,
                            byte_length,
                            feature_table_json_byte_length,
                            feature_table_binary_byte_length,
                            batch_table_json_byte_length,
                            batch_table_binary_byte_length)

        feature_table_binary = struct.pack('<II', 1, 1)

        filepath = os.path.join(self.test_dir, filename)
        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(feature_table_binary)
        return filepath

    def create_test_cmpt(self, filename: str, inner_tile: bytes = None) -> str:
        """创建测试用的 CMPT 文件"""
        if inner_tile is None:
            inner_tile = b'inner_tile_data'

        cmpt_magic = b'cmpt'
        version = 1
        byte_length = 16 + len(inner_tile)
        tiles_length = 1

        header = struct.pack('<4sIII',
                            cmpt_magic,
                            version,
                            byte_length,
                            tiles_length)

        tile_header = struct.pack('<IIII', 0, len(inner_tile), 0, 0)

        filepath = os.path.join(self.test_dir, filename)
        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(tile_header)
            f.write(inner_tile)
        return filepath

    def test_tileset_comparison(self):
        """测试 tileset.json 比对"""
        print("\n" + "="*60)
        print("测试 1: tileset.json 比对")
        print("="*60)

        comparator = TilesComparator()

        file1 = self.create_test_tileset("tileset1.json", "1.0", "Test1")
        file2 = self.create_test_tileset("tileset2.json", "1.0", "Test1")

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 tileset.json 被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 tileset.json 被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

        file3 = self.create_test_tileset("tileset3.json", "1.0", "Test2")

        try:
            report = comparator.compare(file1, file3)

            if not report.identical:
                print("✅ 通过: 不同的 tileset.json 被正确识别为不一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 不同的 tileset.json 被错误识别为一致")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_b3dm_comparison(self):
        """测试 B3DM 比对"""
        print("\n" + "="*60)
        print("测试 2: B3DM 比对")
        print("="*60)

        comparator = TilesComparator()

        gltf_data = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "test"}]}'
        file1 = self.create_test_b3dm("tile1.b3dm", gltf_data)
        file2 = self.create_test_b3dm("tile2.b3dm", gltf_data)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 B3DM 文件被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 B3DM 文件被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

        gltf_data2 = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "different"}]}'
        file3 = self.create_test_b3dm("tile3.b3dm", gltf_data2)

        try:
            report = comparator.compare(file1, file3)

            if not report.identical:
                print("✅ 通过: 不同的 B3DM 文件被正确识别为不一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 不同的 B3DM 文件被错误识别为一致")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_i3dm_comparison(self):
        """测试 I3DM 比对"""
        print("\n" + "="*60)
        print("测试 3: I3DM 比对")
        print("="*60)

        comparator = TilesComparator()

        gltf_data = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "instance"}]}'
        file1 = self.create_test_i3dm("instance1.i3dm", gltf_data)
        file2 = self.create_test_i3dm("instance2.i3dm", gltf_data)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 I3DM 文件被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 I3DM 文件被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

        gltf_data2 = b'{"asset": {"version": "2.0"}, "scenes": [{"nodes": [0]}], "nodes": [{"name": "different"}]}'
        file3 = self.create_test_i3dm("instance3.i3dm", gltf_data2)

        try:
            report = comparator.compare(file1, file3)

            if not report.identical:
                print("✅ 通过: 不同的 I3DM 文件被正确识别为不一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 不同的 I3DM 文件被错误识别为一致")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_pnts_comparison(self):
        """测试 PNTS 比对"""
        print("\n" + "="*60)
        print("测试 4: PNTS 比对")
        print("="*60)

        comparator = TilesComparator()

        file1 = self.create_test_pnts("points1.pnts")
        file2 = self.create_test_pnts("points2.pnts")

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 PNTS 文件被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 PNTS 文件被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_cmpt_comparison(self):
        """测试 CMPT 比对"""
        print("\n" + "="*60)
        print("测试 5: CMPT 比对")
        print("="*60)

        comparator = TilesComparator()

        inner_tile = b'inner_tile_data'
        file1 = self.create_test_cmpt("composite1.cmpt", inner_tile)
        file2 = self.create_test_cmpt("composite2.cmpt", inner_tile)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 CMPT 文件被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 CMPT 文件被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_float_tolerance(self):
        """测试浮点数容差"""
        print("\n" + "="*60)
        print("测试 6: 浮点数容差")
        print("="*60)

        comparator = TilesComparator(float_tolerance=0.01)

        tileset1 = {
            "asset": {"version": "1.0"},
            "geometricError": 100.0,
            "root": {
                "boundingVolume": {"box": [0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100]},
                "geometricError": 10.0
            }
        }

        tileset2 = {
            "asset": {"version": "1.0"},
            "geometricError": 100.005,
            "root": {
                "boundingVolume": {"box": [0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100]},
                "geometricError": 10.001
            }
        }

        file1 = os.path.join(self.test_dir, "tileset_float1.json")
        file2 = os.path.join(self.test_dir, "tileset_float2.json")

        with open(file1, 'w') as f:
            json.dump(tileset1, f)
        with open(file2, 'w') as f:
            json.dump(tileset2, f)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 浮点数在容差范围内被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 浮点数在容差范围内被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_ignore_fields(self):
        """测试忽略字段"""
        print("\n" + "="*60)
        print("测试 7: 忽略字段")
        print("="*60)

        comparator = TilesComparator(ignore_fields={'timestamp', 'created'})

        tileset1 = {
            "asset": {"version": "1.0"},
            "geometricError": 100.0,
            "root": {
                "boundingVolume": {"box": [0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100]},
                "geometricError": 10.0
            }
        }

        tileset2 = {
            "asset": {"version": "1.0"},
            "geometricError": 100.0,
            "timestamp": "2024-01-01",
            "created": "2024-01-01T00:00:00Z",
            "root": {
                "boundingVolume": {"box": [0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100]},
                "geometricError": 10.0
            }
        }

        file1 = os.path.join(self.test_dir, "tileset_ignore1.json")
        file2 = os.path.join(self.test_dir, "tileset_ignore2.json")

        with open(file1, 'w') as f:
            json.dump(tileset1, f)
        with open(file2, 'w') as f:
            json.dump(tileset2, f)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 忽略字段功能正常工作")
                self.passed += 1
            else:
                print(f"❌ 失败: 忽略字段功能未正常工作")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def test_gltf_comparison(self):
        """测试 glTF 比对"""
        print("\n" + "="*60)
        print("测试 8: glTF 比对")
        print("="*60)

        comparator = GLTFComparator()

        gltf1 = {
            "asset": {"version": "2.0"},
            "scenes": [{"nodes": [0]}],
            "nodes": [{"name": "test"}]
        }

        gltf2 = {
            "asset": {"version": "2.0"},
            "scenes": [{"nodes": [0]}],
            "nodes": [{"name": "test"}]
        }

        file1 = os.path.join(self.test_dir, "model1.gltf")
        file2 = os.path.join(self.test_dir, "model2.gltf")

        with open(file1, 'w') as f:
            json.dump(gltf1, f)
        with open(file2, 'w') as f:
            json.dump(gltf2, f)

        try:
            report = comparator.compare(file1, file2)

            if report.identical:
                print("✅ 通过: 相同的 glTF 文件被正确识别为一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 相同的 glTF 文件被错误识别为不一致")
                print(f"   差异: {report.differences}")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

        gltf3 = {
            "asset": {"version": "2.0"},
            "scenes": [{"nodes": [0]}],
            "nodes": [{"name": "different"}]
        }

        file3 = os.path.join(self.test_dir, "model3.gltf")

        with open(file3, 'w') as f:
            json.dump(gltf3, f)

        try:
            report = comparator.compare(file1, file3)

            if not report.identical:
                print("✅ 通过: 不同的 glTF 文件被正确识别为不一致")
                self.passed += 1
            else:
                print(f"❌ 失败: 不同的 glTF 文件被错误识别为一致")
                self.failed += 1

        except Exception as e:
            print(f"❌ 失败: 异常 - {str(e)}")
            self.failed += 1

    def run_all_tests(self):
        """运行所有测试"""
        print("\n" + "#"*70)
        print("# 3D Tiles 格式比对功能测试")
        print("#"*70)

        self.test_tileset_comparison()
        self.test_b3dm_comparison()
        self.test_i3dm_comparison()
        self.test_pnts_comparison()
        self.test_cmpt_comparison()
        self.test_float_tolerance()
        self.test_ignore_fields()
        self.test_gltf_comparison()

        print("\n" + "="*70)
        print("测试总结")
        print("="*70)
        print(f"总计: {self.passed + self.failed + self.skipped} 个测试")
        print(f"  ✅ 通过: {self.passed}")
        print(f"  ❌ 失败: {self.failed}")
        print(f"  ⏭️  跳过: {self.skipped}")
        print("="*70 + "\n")

        return self.failed == 0


def main():
    test = TilesComparatorTest()

    try:
        success = test.run_all_tests()
        test.cleanup()

        if success:
            print("🎉 所有测试通过!")
            sys.exit(0)
        else:
            print("⚠️  部分测试失败")
            sys.exit(1)
    except Exception as e:
        print(f"❌ 测试执行异常: {str(e)}")
        test.cleanup()
        sys.exit(1)


if __name__ == '__main__':
    main()
