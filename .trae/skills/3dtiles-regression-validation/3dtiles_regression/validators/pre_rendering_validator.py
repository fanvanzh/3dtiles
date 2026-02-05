"""
预渲染验证器模块

在数据生成阶段检测可能导致渲染失败的问题，包括：
- glTF/GLB格式问题（致命问题和警告问题）
- tileset.json格式问题（致命问题和警告问题）
- b3dm/i3dm格式问题
- CMPT格式问题
"""

import json
import math
import struct
from pathlib import Path
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum


class IssueSeverity(Enum):
    """问题严重程度"""
    ERROR = "error"
    WARNING = "warning"


@dataclass
class ValidationIssue:
    """验证问题数据类"""
    code: str
    severity: IssueSeverity
    message: str
    rendering_impact: str
    fix: Optional[str] = None


@dataclass
class PreRenderingValidationResult:
    """预渲染验证结果"""
    passed: bool
    errors: int
    warnings: int
    issues: List[ValidationIssue]


class PreRenderingValidator:
    """预渲染验证器"""

    def __init__(self):
        """初始化预渲染验证器"""
        self.issues: List[ValidationIssue] = []

    def validate_gltf_for_rendering(self, file_path: Path) -> PreRenderingValidationResult:
        """
        验证glTF/GLB文件，检测可能导致渲染失败的问题

        Args:
            file_path: glTF/GLB文件路径

        Returns:
            PreRenderingValidationResult: 验证结果
        """
        self.issues = []

        if file_path.suffix == ".glb":
            glb_data = self._parse_glb(file_path)
            if glb_data is None:
                return PreRenderingValidationResult(
                    passed=False,
                    errors=len([i for i in self.issues if i.severity == IssueSeverity.ERROR]),
                    warnings=len([i for i in self.issues if i.severity == IssueSeverity.WARNING]),
                    issues=self.issues
                )
            gltf_json = glb_data.json
            binary_data = glb_data.binary_chunk
        else:
            with open(file_path, 'r', encoding='utf-8') as f:
                gltf_json = json.load(f)
            binary_data = None

        # 1. 致命问题检查
        self._check_fatal_gltf_issues(gltf_json, binary_data)

        # 2. 警告问题检查
        self._check_warning_gltf_issues(gltf_json, binary_data)

        # 3. 渲染兼容性检查
        self._check_rendering_compatibility(gltf_json)

        return PreRenderingValidationResult(
            passed=len([i for i in self.issues if i.severity == IssueSeverity.ERROR]) == 0,
            errors=len([i for i in self.issues if i.severity == IssueSeverity.ERROR]),
            warnings=len([i for i in self.issues if i.severity == IssueSeverity.WARNING]),
            issues=self.issues
        )

    def validate_tileset_for_rendering(self, file_path: Path) -> PreRenderingValidationResult:
        """
        验证tileset.json，检测可能导致渲染失败的问题

        Args:
            file_path: tileset.json文件路径

        Returns:
            PreRenderingValidationResult: 验证结果
        """
        self.issues = []

        with open(file_path, 'r', encoding='utf-8') as f:
            tileset = json.load(f)

        # 1. 致命问题检查
        self._check_fatal_tileset_issues(tileset, file_path.parent)

        # 2. 警告问题检查
        self._check_warning_tileset_issues(tileset)

        return PreRenderingValidationResult(
            passed=len([i for i in self.issues if i.severity == IssueSeverity.ERROR]) == 0,
            errors=len([i for i in self.issues if i.severity == IssueSeverity.ERROR]),
            warnings=len([i for i in self.issues if i.severity == IssueSeverity.WARNING]),
            issues=self.issues
        )

    def _parse_glb(self, file_path: Path) -> Optional[Any]:
        """
        解析GLB文件

        Args:
            file_path: GLB文件路径

        Returns:
            解析结果（包含JSON和binary_chunk）
        """
        try:
            with open(file_path, 'rb') as f:
                # 读取header
                magic = f.read(4)
                if magic != b'glTF':
                    self.issues.append(ValidationIssue(
                        code="INVALID_GLB_MAGIC",
                        severity=IssueSeverity.ERROR,
                        message=f"Invalid GLB magic number: {magic}",
                        rendering_impact="Will cause loader to fail"
                    ))
                    return None

                version = struct.unpack('<I', f.read(4))[0]
                if version != 2:
                    self.issues.append(ValidationIssue(
                        code="UNSUPPORTED_GLB_VERSION",
                        severity=IssueSeverity.ERROR,
                        message=f"Unsupported GLB version: {version}",
                        rendering_impact="Cesium may not load this version"
                    ))
                    return None

                length = struct.unpack('<I', f.read(4))[0]

                # 读取chunks
                json_chunk = None
                binary_chunk = None

                while f.tell() < length:
                    chunk_length = struct.unpack('<I', f.read(4))[0]
                    chunk_type = f.read(4)
                    chunk_data = f.read(chunk_length)

                    if chunk_type == b'JSON':
                        json_chunk = chunk_data.decode('utf-8')
                    elif chunk_type == b'BIN\x00':
                        binary_chunk = chunk_data

                if json_chunk is None:
                    self.issues.append(ValidationIssue(
                        code="MISSING_JSON_CHUNK",
                        severity=IssueSeverity.ERROR,
                        message="GLB file missing JSON chunk",
                        rendering_impact="Will cause loader to fail"
                    ))
                    return None

                gltf_json = json.loads(json_chunk)

                class GLBData:
                    def __init__(self, json_data, binary):
                        self.json = json_data
                        self.binary_chunk = binary

                return GLBData(gltf_json, binary_chunk)

        except Exception as e:
            self.issues.append(ValidationIssue(
                code="GLB_PARSE_ERROR",
                severity=IssueSeverity.ERROR,
                message=f"Failed to parse GLB file: {str(e)}",
                rendering_impact="Will cause loader to fail"
            ))
            return None

    def _check_fatal_gltf_issues(self, gltf_json: dict, binary_data: Optional[bytes]):
        """检查致命的glTF问题"""

        # 检查必需字段 - glTF 2.0 规范中只有 asset 是必需的
        if "asset" not in gltf_json:
            self.issues.append(ValidationIssue(
                code="MISSING_REQUIRED_FIELD",
                severity=IssueSeverity.ERROR,
                message="Missing required field: asset",
                rendering_impact="Will cause loader to fail"
            ))
        elif "version" not in gltf_json["asset"]:
            self.issues.append(ValidationIssue(
                code="MISSING_ASSET_VERSION",
                severity=IssueSeverity.ERROR,
                message="Missing required field: asset.version",
                rendering_impact="Will cause loader to fail"
            ))

        # 检查buffer和bufferView的完整性
        if "buffers" in gltf_json and "bufferViews" in gltf_json:
            for i, buffer_view in enumerate(gltf_json["bufferViews"]):
                buffer_idx = buffer_view.get("buffer")
                if buffer_idx is None or buffer_idx >= len(gltf_json["buffers"]):
                    self.issues.append(ValidationIssue(
                        code="INVALID_BUFFER_REFERENCE",
                        severity=IssueSeverity.ERROR,
                        message=f"bufferView[{i}] references invalid buffer: {buffer_idx}",
                        rendering_impact="Will cause buffer access error during rendering"
                    ))
                    continue

                # 检查byteOffset
                byte_offset = buffer_view.get("byteOffset", 0)
                byte_length = buffer_view.get("byteLength", 0)
                buffer = gltf_json["buffers"][buffer_idx] if buffer_idx < len(gltf_json["buffers"]) else None

                if buffer and byte_offset + byte_length > buffer.get("byteLength", 0):
                    self.issues.append(ValidationIssue(
                        code="BUFFER_VIEW_OUT_OF_BOUNDS",
                        severity=IssueSeverity.ERROR,
                        message=f"bufferView[{i}] exceeds buffer bounds: offset={byte_offset}, length={byte_length}, buffer_size={buffer['byteLength']}",
                        rendering_impact="Will cause buffer access error during rendering"
                    ))

        # 检查accessor的有效性
        if "accessors" in gltf_json:
            # glTF 2.0 规范定义的有效 accessor types
            valid_accessor_types = {"SCALAR", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3", "MAT4"}
            # glTF 2.0 规范定义的有效 component types
            valid_component_types = {5120, 5121, 5122, 5123, 5125, 5126}
            component_type_names = {
                5120: "BYTE",
                5121: "UNSIGNED_BYTE",
                5122: "SHORT",
                5123: "UNSIGNED_SHORT",
                5125: "UNSIGNED_INT",
                5126: "FLOAT"
            }

            for i, accessor in enumerate(gltf_json["accessors"]):
                buffer_view_idx = accessor.get("bufferView")
                if buffer_view_idx is not None:
                    if buffer_view_idx >= len(gltf_json.get("bufferViews", [])):
                        self.issues.append(ValidationIssue(
                            code="INVALID_BUFFER_VIEW_REFERENCE",
                            severity=IssueSeverity.ERROR,
                            message=f"accessor[{i}] references invalid bufferView: {buffer_view_idx}",
                            rendering_impact="Will cause rendering error"
                        ))
                        continue

                # 检查count
                count = accessor.get("count", 0)
                if count <= 0:
                    self.issues.append(ValidationIssue(
                        code="INVALID_ACCESSOR_COUNT",
                        severity=IssueSeverity.ERROR,
                        message=f"accessor[{i}] has invalid count: {count}",
                        rendering_impact="Will cause rendering error"
                    ))

                # 检查type
                accessor_type = accessor.get("type")
                if accessor_type is None:
                    self.issues.append(ValidationIssue(
                        code="MISSING_ACCESSOR_TYPE",
                        severity=IssueSeverity.ERROR,
                        message=f"accessor[{i}] missing required field: type",
                        rendering_impact="Will cause rendering error"
                    ))
                elif accessor_type not in valid_accessor_types:
                    self.issues.append(ValidationIssue(
                        code="INVALID_ACCESSOR_TYPE",
                        severity=IssueSeverity.ERROR,
                        message=f"accessor[{i}] has invalid type: {accessor_type} (must be one of: {', '.join(sorted(valid_accessor_types))})",
                        rendering_impact="Will cause rendering error"
                    ))

                # 检查componentType
                component_type = accessor.get("componentType")
                if component_type is None:
                    self.issues.append(ValidationIssue(
                        code="MISSING_ACCESSOR_COMPONENT_TYPE",
                        severity=IssueSeverity.ERROR,
                        message=f"accessor[{i}] missing required field: componentType",
                        rendering_impact="Will cause rendering error"
                    ))
                elif component_type not in valid_component_types:
                    self.issues.append(ValidationIssue(
                        code="INVALID_ACCESSOR_COMPONENT_TYPE",
                        severity=IssueSeverity.ERROR,
                        message=f"accessor[{i}] has invalid componentType: {component_type} (must be one of: {', '.join(f'{k} ({v})' for k, v in component_type_names.items())})",
                        rendering_impact="Will cause rendering error"
                    ))

        # 检查mesh的indices和primitive mode
        if "meshes" in gltf_json:
            for i, mesh in enumerate(gltf_json["meshes"]):
                for j, primitive in enumerate(mesh.get("primitives", [])):
                    accessor_idx = primitive.get("indices")

                    if accessor_idx is not None and accessor_idx >= len(gltf_json.get("accessors", [])):
                        self.issues.append(ValidationIssue(
                            code="INVALID_INDICES_REFERENCE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] references invalid accessor: {accessor_idx}",
                            rendering_impact="Will cause rendering crash"
                        ))
                        continue

                    # 检查primitive mode
                    mode = primitive.get("mode", 4)  # 默认是 TRIANGLES (4)

                    # 只有 TRIANGLES 模式 (4) 需要检查 indices 数量是否为 3 的倍数
                    if accessor_idx is not None and accessor_idx < len(gltf_json.get("accessors", [])):
                        accessor = gltf_json["accessors"][accessor_idx]
                        count = accessor.get("count", 0)

                        if mode == 4:  # TRIANGLES
                            if count % 3 != 0:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_TRIANGLE_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid triangle count: {count} (must be multiple of 3 for TRIANGLES mode)",
                                    rendering_impact="Will cause rendering error"
                                ))
                        elif mode == 5:  # TRIANGLE_STRIP
                            if count < 3:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_TRIANGLE_STRIP_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid triangle strip count: {count} (must be at least 3 for TRIANGLE_STRIP mode)",
                                    rendering_impact="Will cause rendering error"
                                ))
                        elif mode == 6:  # TRIANGLE_FAN
                            if count < 3:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_TRIANGLE_FAN_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid triangle fan count: {count} (must be at least 3 for TRIANGLE_FAN mode)",
                                    rendering_impact="Will cause rendering error"
                                ))
                        elif mode == 1:  # LINES
                            if count % 2 != 0:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_LINES_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid lines count: {count} (must be multiple of 2 for LINES mode)",
                                    rendering_impact="Will cause rendering error"
                                ))
                        elif mode == 3:  # LINE_STRIP
                            if count < 2:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_LINE_STRIP_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid line strip count: {count} (must be at least 2 for LINE_STRIP mode)",
                                    rendering_impact="Will cause rendering error"
                                ))
                        elif mode == 2:  # LINE_LOOP
                            if count < 2:
                                self.issues.append(ValidationIssue(
                                    code="INVALID_LINE_LOOP_COUNT",
                                    severity=IssueSeverity.ERROR,
                                    message=f"mesh[{i}].primitive[{j}] has invalid line loop count: {count} (must be at least 2 for LINE_LOOP mode)",
                                    rendering_impact="Will cause rendering error"
                                ))

        # 检查material引用
        if "meshes" in gltf_json and "materials" in gltf_json:
            for i, mesh in enumerate(gltf_json["meshes"]):
                for j, primitive in enumerate(mesh.get("primitives", [])):
                    material_idx = primitive.get("material")

                    if material_idx is not None and material_idx >= len(gltf_json["materials"]):
                        self.issues.append(ValidationIssue(
                            code="INVALID_MATERIAL_REFERENCE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] references invalid material: {material_idx}",
                            rendering_impact="Will cause rendering error or fallback to default material"
                        ))

        # 检查primitive attributes - glTF 2.0 规范要求
        if "meshes" in gltf_json:
            for i, mesh in enumerate(gltf_json["meshes"]):
                for j, primitive in enumerate(mesh.get("primitives", [])):
                    attributes = primitive.get("attributes")

                    # attributes 是必需的
                    if attributes is None:
                        self.issues.append(ValidationIssue(
                            code="MISSING_PRIMITIVE_ATTRIBUTES",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] missing required field: attributes",
                            rendering_impact="Will cause rendering error"
                        ))
                        continue

                    # POSITION 属性是必需的
                    position_accessor_idx = attributes.get("POSITION")
                    if position_accessor_idx is None:
                        self.issues.append(ValidationIssue(
                            code="MISSING_POSITION_ATTRIBUTE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] missing required attribute: POSITION",
                            rendering_impact="Will cause rendering error"
                        ))
                        continue

                    # 检查 POSITION 引用是否有效
                    if position_accessor_idx >= len(gltf_json.get("accessors", [])):
                        self.issues.append(ValidationIssue(
                            code="INVALID_POSITION_REFERENCE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] POSITION references invalid accessor: {position_accessor_idx}",
                            rendering_impact="Will cause rendering error"
                        ))
                        continue

                    # 检查 POSITION accessor 的 type 和 componentType
                    position_accessor = gltf_json["accessors"][position_accessor_idx]
                    position_type = position_accessor.get("type")
                    position_component_type = position_accessor.get("componentType")

                    if position_type != "VEC3":
                        self.issues.append(ValidationIssue(
                            code="INVALID_POSITION_TYPE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] POSITION accessor has invalid type: {position_type} (must be VEC3)",
                            rendering_impact="Will cause rendering error"
                        ))

                    if position_component_type not in [5126, 5123, 5122, 5121, 5120]:
                        self.issues.append(ValidationIssue(
                            code="INVALID_POSITION_COMPONENT_TYPE",
                            severity=IssueSeverity.ERROR,
                            message=f"mesh[{i}].primitive[{j}] POSITION accessor has invalid componentType: {position_component_type} (must be FLOAT, UNSIGNED_SHORT, SHORT, UNSIGNED_BYTE, or BYTE)",
                            rendering_impact="Will cause rendering error"
                        ))

                    # 检查其他属性引用是否有效
                    for attr_name, attr_accessor_idx in attributes.items():
                        if attr_name == "POSITION":
                            continue

                        if attr_accessor_idx >= len(gltf_json.get("accessors", [])):
                            self.issues.append(ValidationIssue(
                                code="INVALID_ATTRIBUTE_REFERENCE",
                                severity=IssueSeverity.ERROR,
                                message=f"mesh[{i}].primitive[{j}] attribute '{attr_name}' references invalid accessor: {attr_accessor_idx}",
                                rendering_impact="Will cause rendering error"
                            ))

    def _check_warning_gltf_issues(self, gltf_json: dict, binary_data: Optional[bytes]):
        """检查警告的glTF问题"""

        # 检查PBR参数
        if "materials" in gltf_json:
            for i, material in enumerate(gltf_json["materials"]):
                pbr = material.get("pbrMetallicRoughness", {})

                # 检查metallicFactor
                metallic_factor = pbr.get("metallicFactor", 0.0)
                if not (0.0 <= metallic_factor <= 1.0):
                    self.issues.append(ValidationIssue(
                        code="INVALID_METALLIC_FACTOR",
                        severity=IssueSeverity.WARNING,
                        message=f"material[{i}] has invalid metallicFactor: {metallic_factor} (must be in [0, 1])",
                        rendering_impact="May cause incorrect material appearance"
                    ))

                # 检查roughnessFactor
                roughness_factor = pbr.get("roughnessFactor", 1.0)
                if not (0.0 <= roughness_factor <= 1.0):
                    self.issues.append(ValidationIssue(
                        code="INVALID_ROUGHNESS_FACTOR",
                        severity=IssueSeverity.WARNING,
                        message=f"material[{i}] has invalid roughnessFactor: {roughness_factor} (must be in [0, 1])",
                        rendering_impact="May cause incorrect material appearance"
                    ))

        # 检查纹理
        if "textures" in gltf_json and "images" in gltf_json:
            for i, texture in enumerate(gltf_json["textures"]):
                source_idx = texture.get("source")
                if source_idx is None:
                    self.issues.append(ValidationIssue(
                        code="MISSING_TEXTURE_SOURCE",
                        severity=IssueSeverity.WARNING,
                        message=f"texture[{i}] missing source",
                        rendering_impact="Texture will not render"
                    ))
                    continue

                if source_idx >= len(gltf_json["images"]):
                    self.issues.append(ValidationIssue(
                        code="INVALID_TEXTURE_SOURCE",
                        severity=IssueSeverity.WARNING,
                        message=f"texture[{i}] references invalid image: {source_idx}",
                        rendering_impact="Texture will not render"
                    ))

    def _check_rendering_compatibility(self, gltf_json: dict):
        """检查渲染兼容性"""

        # 检查glTF版本
        if "asset" in gltf_json:
            version = gltf_json["asset"].get("version", "")
            if not version.startswith("2."):
                self.issues.append(ValidationIssue(
                    code="UNSUPPORTED_GLT_VERSION",
                    severity=IssueSeverity.ERROR,
                    message=f"Unsupported glTF version: {version}",
                    rendering_impact="Cesium may not load this version"
                ))

        # 检查是否有支持的扩展
        if "extensionsUsed" in gltf_json:
            supported_extensions = [
                "KHR_materials_pbrSpecularGlossiness",
                "KHR_materials_unlit",
                "KHR_materials_transmission",
                "KHR_texture_transform",
                "KHR_draco_mesh_compression"
            ]

            for ext in gltf_json["extensionsUsed"]:
                if ext not in supported_extensions:
                    self.issues.append(ValidationIssue(
                        code="UNSUPPORTED_EXTENSION",
                        severity=IssueSeverity.WARNING,
                        message=f"Extension '{ext}' may not be supported by all renderers",
                        rendering_impact="May fail to load in some renderers"
                    ))

    def _check_fatal_tileset_issues(self, tileset: dict, base_dir: Path):
        """检查致命的tileset问题"""

        # 检查root tile
        root = tileset.get("root")
        if root is None:
            self.issues.append(ValidationIssue(
                code="MISSING_ROOT_TILE",
                severity=IssueSeverity.ERROR,
                message="tileset.json missing 'root' tile",
                rendering_impact="Cesium cannot load this tileset"
            ))
            return

        # 检查root tile的refine属性
        if "refine" not in root:
            self.issues.append(ValidationIssue(
                code="MISSING_REFINE_PROPERTY",
                severity=IssueSeverity.ERROR,
                message="Root tile missing 'refine' property",
                rendering_impact="Cesium may not render correctly or may crash",
                fix="Add 'refine': 'ADD' to root tile"
            ))

        # 检查boundingVolume
        if "boundingVolume" not in root:
            self.issues.append(ValidationIssue(
                code="MISSING_BOUNDING_VOLUME",
                severity=IssueSeverity.ERROR,
                message="Root tile missing 'boundingVolume'",
                rendering_impact="Cesium may not cull tiles correctly",
                fix="Add 'boundingVolume' with [min, max] values"
            ))
        else:
            bounding_volume = root["boundingVolume"]
            self._validate_bounding_volume(bounding_volume)

        # 检查geometricError
        if "geometricError" not in root:
            self.issues.append(ValidationIssue(
                code="MISSING_GEOMETRIC_ERROR",
                severity=IssueSeverity.ERROR,
                message="Root tile missing 'geometricError'",
                rendering_impact="Cesium may not calculate LOD correctly",
                fix="Add 'geometricError' value"
            ))
        else:
            geometric_error = root["geometricError"]
            if geometric_error < 0:
                self.issues.append(ValidationIssue(
                    code="INVALID_GEOMETRIC_ERROR",
                    severity=IssueSeverity.ERROR,
                    message=f"geometricError is negative: {geometric_error}",
                    rendering_impact="Cesium may not calculate LOD correctly",
                    fix="geometricError must be non-negative"
                ))

        # 检查content引用
        if "content" in root:
            content_uri = root["content"].get("uri", "")
            if content_uri:
                content_path = base_dir / content_uri
                if not content_path.exists():
                    self.issues.append(ValidationIssue(
                        code="MISSING_CONTENT_FILE",
                        severity=IssueSeverity.ERROR,
                        message=f"Content file not found: {content_uri}",
                        rendering_impact="Cesium will fail to load tile content",
                        fix=f"Ensure file exists: {content_path}"
                    ))

        # 递归检查子瓦片
        if "children" in root:
            self._validate_tile_hierarchy(root, base_dir)

    def _validate_bounding_volume(self, bounding_volume: dict):
        """验证boundingVolume - 3D Tiles 支持 box/sphere/region 三种类型"""
        if not bounding_volume:
            self.issues.append(ValidationIssue(
                code="EMPTY_BOUNDING_VOLUME",
                severity=IssueSeverity.ERROR,
                message="boundingVolume is empty",
                rendering_impact="Cesium may not cull tiles correctly"
            ))
            return

        # 检查是否有至少一种有效的 bounding volume 类型
        has_valid_type = False

        if "box" in bounding_volume:
            has_valid_type = True
            box = bounding_volume["box"]
            if not isinstance(box, list) or len(box) != 12:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_BOX_FORMAT",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.box must be an array of 12 numbers [centerX, centerY, centerZ, dirXx, dirXy, dirXz, dirYx, dirYy, dirYz, dirZx, dirZy, dirZz], got length {len(box) if isinstance(box, list) else 'not a list'}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))
                return

            # 检查数值是否有效
            for i, value in enumerate(box):
                if not isinstance(value, (int, float)):
                    self.issues.append(ValidationIssue(
                        code="INVALID_BOUNDING_VOLUME_VALUE",
                        severity=IssueSeverity.ERROR,
                        message=f"boundingVolume.box[{i}] is not a number: {value}",
                        rendering_impact="Cesium may not cull tiles correctly"
                    ))
                elif isinstance(value, float):
                    if value != value:  # 检查NaN
                        self.issues.append(ValidationIssue(
                            code="INVALID_BOUNDING_VOLUME_NAN",
                            severity=IssueSeverity.ERROR,
                            message=f"boundingVolume.box[{i}] is NaN",
                            rendering_impact="Cesium may not cull tiles correctly"
                        ))

        if "sphere" in bounding_volume:
            has_valid_type = True
            sphere = bounding_volume["sphere"]
            if not isinstance(sphere, list) or len(sphere) != 4:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_SPHERE_FORMAT",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.sphere must be an array of 4 numbers [centerX, centerY, centerZ, radius], got length {len(sphere) if isinstance(sphere, list) else 'not a list'}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))
                return

            # 检查半径是否非负
            radius = sphere[3]
            if radius < 0:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_SPHERE_RADIUS",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.sphere radius must be non-negative, got: {radius}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            # 检查数值是否有效
            for i, value in enumerate(sphere):
                if not isinstance(value, (int, float)):
                    self.issues.append(ValidationIssue(
                        code="INVALID_BOUNDING_VOLUME_VALUE",
                        severity=IssueSeverity.ERROR,
                        message=f"boundingVolume.sphere[{i}] is not a number: {value}",
                        rendering_impact="Cesium may not cull tiles correctly"
                    ))
                elif isinstance(value, float):
                    if value != value:  # 检查NaN
                        self.issues.append(ValidationIssue(
                            code="INVALID_BOUNDING_VOLUME_NAN",
                            severity=IssueSeverity.ERROR,
                            message=f"boundingVolume.sphere[{i}] is NaN",
                            rendering_impact="Cesium may not cull tiles correctly"
                        ))

        if "region" in bounding_volume:
            has_valid_type = True
            region = bounding_volume["region"]
            if not isinstance(region, list) or len(region) != 6:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_FORMAT",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region must be an array of 6 numbers [west, south, east, north, minHeight, maxHeight], got length {len(region) if isinstance(region, list) else 'not a list'}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))
                return

            west, south, east, north, min_height, max_height = region

            # 检查经纬度范围（弧度制）
            if not (-math.pi <= west <= math.pi):
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_WEST",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region west must be in [-π, π] radians, got: {west}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if not (-math.pi / 2 <= south <= math.pi / 2):
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_SOUTH",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region south must be in [-π/2, π/2] radians, got: {south}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if not (-math.pi <= east <= math.pi):
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_EAST",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region east must be in [-π, π] radians, got: {east}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if not (-math.pi / 2 <= north <= math.pi / 2):
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_NORTH",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region north must be in [-π/2, π/2] radians, got: {north}",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if west > east:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_LONGITUDE",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region west ({west}) must be <= east ({east})",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if south > north:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_LATITUDE",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region south ({south}) must be <= north ({north})",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

            if min_height > max_height:
                self.issues.append(ValidationIssue(
                    code="INVALID_BOUNDING_VOLUME_REGION_HEIGHT",
                    severity=IssueSeverity.ERROR,
                    message=f"boundingVolume.region minHeight ({min_height}) must be <= maxHeight ({max_height})",
                    rendering_impact="Cesium may not cull tiles correctly"
                ))

        if not has_valid_type:
            self.issues.append(ValidationIssue(
                code="INVALID_BOUNDING_VOLUME_TYPE",
                severity=IssueSeverity.ERROR,
                message=f"boundingVolume must contain at least one of: box, sphere, region. Got keys: {list(bounding_volume.keys())}",
                rendering_impact="Cesium may not cull tiles correctly"
            ))

    def _validate_tile_hierarchy(self, tile: dict, base_dir: Path):
        """验证瓦片层级结构"""
        if "children" not in tile:
            return

        for i, child_ref in enumerate(tile["children"]):
            if "content" not in child_ref and "children" not in child_ref:
                self.issues.append(ValidationIssue(
                    code="EMPTY_TILE",
                    severity=IssueSeverity.WARNING,
                    message=f"Tile[{i}] has no content or children",
                    rendering_impact="Cesium may skip this tile"
                ))

            # 检查LOD结构
            parent_geometric_error = tile.get("geometricError", 0)
            child_geometric_error = child_ref.get("geometricError", 0)

            if child_geometric_error > parent_geometric_error:
                self.issues.append(ValidationIssue(
                    code="INVALID_LOD_STRUCTURE",
                    severity=IssueSeverity.ERROR,
                    message=f"Child tile has larger geometricError than parent: {child_geometric_error} > {parent_geometric_error}",
                    rendering_impact="Cesium may not calculate LOD correctly",
                    fix="Child tiles should have smaller geometricError than parent"
                ))

            # 递归检查
            self._validate_tile_hierarchy(child_ref, base_dir)

    def _check_warning_tileset_issues(self, tileset: dict):
        """检查警告的tileset问题"""

        # 检查transform
        root = tileset.get("root", {})
        if "transform" in root:
            transform = root["transform"]
            # 检查transform矩阵是否奇异
            # 简化检查：检查是否有0或接近0的缩放因子
            if isinstance(transform, list) and len(transform) >= 10:
                scale_x = transform[0]
                if abs(scale_x) < 1e-6:
                    self.issues.append(ValidationIssue(
                        code="NEAR_ZERO_SCALE",
                        severity=IssueSeverity.WARNING,
                        message=f"Transform has near-zero scale factor: {scale_x}",
                        rendering_impact="May cause rendering artifacts"
                    ))
