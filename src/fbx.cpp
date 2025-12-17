#include "fbx.h"
#include "extern.h"
#include <iostream>

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Image>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/UserDataContainer>
#include <osgDB/ReadFile>

#include <ufbx.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <osgDB/ReaderWriter>
#include <osgDB/Registry>

// Helper functions
static std::string hash_bytes(const void* data, size_t len) {
  std::ostringstream oss;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; ++i) h = (h ^ p[i]) * 16777619u;
  oss << std::hex << h;
  return oss.str();
}

static osg::Matrixd ufbx_matrix_to_osg(const ufbx_matrix &m) {
  // ufbx_matrix stores an affine 4x3 in column form; map to OSG 4x4.
  // ufbx: cols[0] = (m00, m10, m20) -> X axis
  //       cols[1] = (m01, m11, m21) -> Y axis
  //       cols[2] = (m02, m12, m22) -> Z axis
  //       cols[3] = (m03, m13, m23) -> Translation
  // OSG (row-major v*M): Row 0 = X axis, Row 3 = Translation
  return osg::Matrixd(
      m.m00, m.m10, m.m20, 0.0,
      m.m01, m.m11, m.m21, 0.0,
      m.m02, m.m12, m.m22, 0.0,
      m.m03, m.m13, m.m23, 1.0);
}

static std::string ufbx_string_to_std(const ufbx_string &s) {
  if (s.data == nullptr || s.length == 0) {
    return std::string();
  }
  return std::string(s.data, s.length);
}

static std::filesystem::path resolve_texture_path(const std::string &fbxPath,
                                                  const ufbx_texture *tex) {
  if (!tex) {
    return {};
  }

  std::string path = ufbx_string_to_std(tex->filename);
  if (path.empty()) {
    path = ufbx_string_to_std(tex->relative_filename);
  }
  if (path.empty()) {
    path = ufbx_string_to_std(tex->absolute_filename);
  }
  if (path.empty()) {
    return {};
  }

  std::filesystem::path p(path);
  if (p.is_absolute() && std::filesystem::exists(p)) {
      return p;
  }

  // Try relative to FBX file
  if (!fbxPath.empty()) {
      std::filesystem::path fbxDir = std::filesystem::path(fbxPath).parent_path();
      std::filesystem::path relPath = fbxDir / p;
      if (std::filesystem::exists(relPath)) {
          return relPath;
      }
      // Try just filename in FBX dir
      std::filesystem::path filename = p.filename();
      std::filesystem::path flatPath = fbxDir / filename;
      if (std::filesystem::exists(flatPath)) {
          return flatPath;
      }
  }

  return {};
}

// Helper to create StateSet (Member function implementation)
osg::StateSet* FBXLoader::getOrCreateStateSet(const ufbx_material* mat) {
    if (!mat) return nullptr;

    // Check cache
    auto it = materialCache.find(mat);
    if (it != materialCache.end()) {
        return it->second.get();
    }

    osg::StateSet* stateSet = new osg::StateSet;
    osg::Material* material = new osg::Material;

    // Diffuse
    osg::Vec4 diffuse(1, 1, 1, 1);
    if (mat->pbr.base_color.has_value) {
        diffuse.set(mat->pbr.base_color.value_vec4.x, mat->pbr.base_color.value_vec4.y, mat->pbr.base_color.value_vec4.z, mat->pbr.base_color.value_vec4.w);
    } else if (mat->fbx.diffuse_color.has_value) {
        diffuse.set(mat->fbx.diffuse_color.value_vec3.x, mat->fbx.diffuse_color.value_vec3.y, mat->fbx.diffuse_color.value_vec3.z, 1.0f);
        if (mat->fbx.diffuse_factor.has_value) diffuse *= mat->fbx.diffuse_factor.value_real;
    }
    material->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);

    // Specular
    osg::Vec4 specular(0, 0, 0, 1);
    if (mat->fbx.specular_color.has_value) {
        specular.set(mat->fbx.specular_color.value_vec3.x, mat->fbx.specular_color.value_vec3.y, mat->fbx.specular_color.value_vec3.z, 1.0f);
        if (mat->fbx.specular_factor.has_value) specular *= mat->fbx.specular_factor.value_real;
    }
    material->setSpecular(osg::Material::FRONT_AND_BACK, specular);

    // Shininess
    if (mat->fbx.specular_exponent.has_value) {
        material->setShininess(osg::Material::FRONT_AND_BACK, mat->fbx.specular_exponent.value_real);
    }

    // Emission
    osg::Vec4 emission(0, 0, 0, 1);
    if (mat->pbr.emission_color.has_value) {
        emission.set(mat->pbr.emission_color.value_vec3.x, mat->pbr.emission_color.value_vec3.y, mat->pbr.emission_color.value_vec3.z, 1.0f);
    } else if (mat->fbx.emission_color.has_value) {
        emission.set(mat->fbx.emission_color.value_vec3.x, mat->fbx.emission_color.value_vec3.y, mat->fbx.emission_color.value_vec3.z, 1.0f);
        if (mat->fbx.emission_factor.has_value) emission *= mat->fbx.emission_factor.value_real;
    }
    material->setEmission(osg::Material::FRONT_AND_BACK, emission);

    stateSet->setAttributeAndModes(material);

    // Texture
    const ufbx_texture* tex = NULL;
    if (mat->pbr.base_color.texture) tex = mat->pbr.base_color.texture;
    else if (mat->fbx.diffuse_color.texture) tex = mat->fbx.diffuse_color.texture;

    if (tex) {
        osg::ref_ptr<osg::Image> image;

        // 1. Try embedded content
        if (tex->content.data && tex->content.size > 0) {
             std::string ext = "png"; // default fallback
             std::string filename = ufbx_string_to_std(tex->filename);
             if (filename.length() > 4 && filename.find('.') != std::string::npos) {
                 ext = filename.substr(filename.find_last_of('.') + 1);
             }

             // Create stream from memory
             // Note: stringstream copies data, but it's safe.
             // Ideally we'd use a streambuf wrapper around the pointer to avoid copy, but this is cleaner.
             std::string data((const char*)tex->content.data, tex->content.size);
             std::stringstream ss(data);

             osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(ext);
             if (rw) {
                 osgDB::ReaderWriter::ReadResult rr = rw->readImage(ss);
                 if (rr.success()) {
                     image = rr.getImage();
                     if (image) {
                         // Set a filename so we can identify it later if needed (though it won't exist on disk)
                         image->setFileName(filename.empty() ? "embedded.png" : filename);
                     }
                 } else {
                     LOG_E("Failed to decode embedded image: %s", rr.message().c_str());
                 }
             } else {
                 LOG_E("No ReaderWriter for extension: %s", ext.c_str());
             }
        }

        // 2. Try file path if not embedded or failed
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, tex);
            if (!filename.empty()) {
                image = osgDB::readImageFile(filename.string());
            }
        }

        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(0, texture);
        }
    }

    // Cache it
    materialCache[mat] = stateSet;
    return stateSet;
}

FBXLoader::FBXLoader(const std::string &filename) : source_filename(filename), scene(nullptr) {}

FBXLoader::~FBXLoader() {
  if (scene != nullptr) {
    ufbx_free_scene(scene);
    scene = nullptr;
  }
}

std::string FBXLoader::calcMeshHash(const ufbx_mesh *mesh) {
  if (!mesh) return "0";
  // Only hash vertices/indices/faces count
  std::ostringstream oss;
  oss.write((const char*)mesh->vertices.data, mesh->vertices.count * sizeof(ufbx_vec3));
  oss.write((const char*)mesh->vertex_indices.data, mesh->vertex_indices.count * sizeof(uint32_t));
  oss.write((const char*)mesh->faces.data, mesh->faces.count * sizeof(ufbx_face));
  std::string s = oss.str();
  return hash_bytes(s.data(), s.size());
}

std::string FBXLoader::calcMaterialHash(const ufbx_material *mat) {
  if (!mat) return "0";
  // Hash name and main texture path
  std::string name = mat->name.data ? std::string(mat->name.data, mat->name.length) : "";
  std::string tex = mat->pbr.base_color.texture && mat->pbr.base_color.texture->filename.data ? std::string(mat->pbr.base_color.texture->filename.data, mat->pbr.base_color.texture->filename.length) : "";
  std::string s = name + tex;
  return hash_bytes(s.data(), s.size());
}

std::unordered_map<std::string, std::string> FBXLoader::collectNodeAttrs(const ufbx_node *node) {
  std::unordered_map<std::string, std::string> attrs;
  if (!node) return attrs;
  attrs["name"] = node->name.data ? std::string(node->name.data, node->name.length) : "";
  for (size_t i = 0; i < node->props.props.count; ++i) {
    const ufbx_prop &p = node->props.props.data[i];
    if (p.name.data && p.value_str.data) {
      attrs[std::string(p.name.data, p.name.length)] = std::string(p.value_str.data, p.value_str.length);
    }
  }
  return attrs;
}

void FBXLoader::load() {
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up; // Convert to glTF/OpenGL standard (Y-up)
    opts.target_unit_meters = 1.0f;
    opts.clean_skin_weights = true;

    // Ensure we handle triangulation if needed (though ufbx does this well by default)
    // opts.generate_indices is NOT a field in ufbx_load_opts. We must call ufbx_generate_indices manually per mesh.

    ufbx_error error;
    scene = ufbx_load_file(source_filename.c_str(), &opts, &error);
    if (!scene) {
        LOG_E("Failed to load FBX: %s", error.description.data);
        return;
    }

    // Log settings for debugging (metadata often contains the original file info)
    if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_X) LOG_I("FBX File Up-Axis: +X");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_X) LOG_I("FBX File Up-Axis: -X");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_Y) LOG_I("FBX File Up-Axis: +Y");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_Y) LOG_I("FBX File Up-Axis: -Y");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_Z) LOG_I("FBX File Up-Axis: +Z");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_Z) LOG_I("FBX File Up-Axis: -Z");
    else LOG_I("FBX File Up-Axis: Unknown");

    if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_X) LOG_I("FBX File Front-Axis: +X");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_X) LOG_I("FBX File Front-Axis: -X");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_Y) LOG_I("FBX File Front-Axis: +Y");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_Y) LOG_I("FBX File Front-Axis: -Y");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_Z) LOG_I("FBX File Front-Axis: +Z");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_Z) LOG_I("FBX File Front-Axis: -Z");

    LOG_I("FBX File Unit Meters: %f", scene->settings.unit_meters);

    // Start loading from the root node
    if (scene->root_node) {
        _root = loadNode(scene->root_node, osg::Matrixd::identity());
    }
}

osg::ref_ptr<osg::Node> FBXLoader::loadNode(ufbx_node *node, const osg::Matrixd &parentXform) {
    if (!node) return nullptr;

    // Check visibility
    if (!node->visible) {
        return nullptr;
    }

    // Use ufbx's precomputed node_to_world if available, or accumulate node_to_parent.
    // ufbx computes node_to_world considering all inheritance rules.
    // However, since we are building an OSG scene graph which is hierarchical,
    // we normally use local transforms (node_to_parent) for the OSG nodes (MatrixTransform).
    // BUT, for flattening meshes into world space for 3D Tiles (B3DM), we need the absolute world transform.

    // For OSG scene graph structure (visualizing locally):
    osg::Matrixd localMatrix = ufbx_matrix_to_osg(node->node_to_parent);

    // For Mesh processing (flattening to world):
    // ufbx provides node_to_world which handles complex inheritance.
    // It is safer to use node_to_world directly for geometry processing than accumulating local matrices manually
    // if there are complex inheritance flags (which ufbx handles).
    osg::Matrixd globalMatrix = ufbx_matrix_to_osg(node->node_to_world);

    osg::ref_ptr<osg::Group> group;
    osg::ref_ptr<osg::MatrixTransform> transform;

    bool hasTransform = !localMatrix.isIdentity();
    if (hasTransform) {
        transform = new osg::MatrixTransform(localMatrix);
        group = transform;
    } else {
        group = new osg::Group;
    }

    std::string name = ufbx_string_to_std(node->name);
    group->setName(name);

    // Process Meshes
    if (node->mesh) {
        ufbx_mesh *mesh = node->mesh;

        // Apply Geometry Transform (Pivot/Offset specific to the mesh attachment)
        // ufbx provides geometry_to_node which transforms mesh to node local space.
        // Or geometry_to_world directly.
        // Let's check if geometry_to_world is available or compute it.
        // geometry_to_world = geometry_to_node * node_to_world

        osg::Matrixd geomToNode = ufbx_matrix_to_osg(node->geometry_to_node);
        osg::Matrixd meshGlobalMatrix = geomToNode * globalMatrix;

        osg::ref_ptr<osg::Geode> geode = processMesh(node, mesh, meshGlobalMatrix);
        if (geode) {
            group->addChild(geode);
        }
    }

    // Process Children
    // We pass globalMatrix just in case, but actually if we switch to using node->node_to_world for everyone,
    // we don't strictly need to pass accumulated parentXform down for calculation,
    // BUT loadNode signature expects it.
    // The recursive call logic here is fine for scene graph building.

    for (size_t i = 0; i < node->children.count; ++i) {
        // For the child, the parent transform is THIS node's global transform?
        // Wait, 'parentXform' passed to loadNode is the parent's world transform.
        // 'globalMatrix' computed here IS this node's world transform.
        // So passing 'globalMatrix' to children is correct for accumulation if we were doing it manually.
        // But since we use ufbx's node_to_world, the child will just use its own node_to_world.

        osg::ref_ptr<osg::Node> childNode = loadNode(node->children.data[i], globalMatrix);
        if (childNode) {
            group->addChild(childNode);
        }
    }

    return group;
}

osg::ref_ptr<osg::Geode> FBXLoader::processMesh(ufbx_node *node, ufbx_mesh *mesh, const osg::Matrixd &globalXform) {
    if (!mesh) return nullptr;

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    // 1. Check Cache
    auto it = meshCache.find(mesh);
    if (it != meshCache.end()) {
        for (const auto& part : it->second) {
            geode->addDrawable(part.geometry);

            // Update MeshPool
            MeshKey key;
            key.geomHash = part.geomHash;
            key.matHash = part.matHash;

            if (meshPool.find(key) != meshPool.end()) {
                 meshPool[key].transforms.push_back(globalXform);
                 meshPool[key].nodeNames.push_back(ufbx_string_to_std(node->name));
                 meshPool[key].nodeAttrs.push_back(collectNodeAttrs(node));
            } else {
                 MeshInstanceInfo info;
                 info.key = key;
                 info.geometry = part.geometry;
                 info.transforms.push_back(globalXform);
                 info.nodeNames.push_back(ufbx_string_to_std(node->name));
                 info.nodeAttrs.push_back(collectNodeAttrs(node));
                 meshPool[key] = info;
            }
        }
        return geode;
    }

    // 2. Not in cache, process mesh
    // Use ufbx_generate_indices to handle vertex deduplication

    // Calculate Mesh Hash (Base geometry hash)
    std::string meshHash = calcMeshHash(mesh);

    // We need to flatten the indexed ufbx data into "wedge" arrays first,
    // because ufbx_generate_indices expects flat arrays of size num_indices.
    // It will then reorder/compact these arrays in-place.

    size_t num_indices = mesh->num_indices;
    if (num_indices == 0) return nullptr;

    std::vector<ufbx_vec3> tempPos;
    std::vector<ufbx_vec3> tempNorm;
    std::vector<ufbx_vec2> tempUV;
    std::vector<ufbx_vec4> tempColor;

    // Helper to resize if exists
    if (mesh->vertex_position.exists) tempPos.resize(num_indices);
    if (mesh->vertex_normal.exists) tempNorm.resize(num_indices);
    if (mesh->vertex_uv.exists) tempUV.resize(num_indices);
    if (mesh->vertex_color.exists) tempColor.resize(num_indices);

    // Flatten data
    for (size_t i = 0; i < num_indices; ++i) {
        if (mesh->vertex_position.exists) {
            tempPos[i] = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
        }
        if (mesh->vertex_normal.exists) {
            tempNorm[i] = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
        }
        if (mesh->vertex_uv.exists) {
            tempUV[i] = ufbx_get_vertex_vec2(&mesh->vertex_uv, i);
        }
        if (mesh->vertex_color.exists) {
            tempColor[i] = ufbx_get_vertex_vec4(&mesh->vertex_color, i);
        }
    }

    // Prepare streams for ufbx_generate_indices
    std::vector<ufbx_vertex_stream> streams;
    ufbx_vertex_stream s;

    if (!tempPos.empty()) {
        s.data = tempPos.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec3);
        streams.push_back(s);
    }
    if (!tempNorm.empty()) {
        s.data = tempNorm.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec3);
        streams.push_back(s);
    }
    if (!tempUV.empty()) {
        s.data = tempUV.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec2);
        streams.push_back(s);
    }
    if (!tempColor.empty()) {
        s.data = tempColor.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec4);
        streams.push_back(s);
    }

    // Generate indices
    std::vector<uint32_t> generated_indices(num_indices);
    ufbx_error error;
    size_t num_vertices = ufbx_generate_indices(streams.data(), streams.size(), generated_indices.data(), num_indices, nullptr, &error);

    if (num_vertices == 0 && num_indices > 0) {
        return nullptr;
    }

    // Copy to OSG arrays (only the unique vertices)
    osg::ref_ptr<osg::Vec3Array> osgPos = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec3Array> osgNorm = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec2Array> osgUV = new osg::Vec2Array;
    osg::ref_ptr<osg::Vec4Array> osgColor = new osg::Vec4Array;

    osgPos->reserve(num_vertices);
    if (!tempNorm.empty()) osgNorm->reserve(num_vertices);
    if (!tempUV.empty()) osgUV->reserve(num_vertices);
    if (!tempColor.empty()) osgColor->reserve(num_vertices);

    for(size_t i=0; i<num_vertices; ++i) {
        if (!tempPos.empty()) osgPos->push_back(osg::Vec3(tempPos[i].x, tempPos[i].y, tempPos[i].z));
        if (!tempNorm.empty()) osgNorm->push_back(osg::Vec3(tempNorm[i].x, tempNorm[i].y, tempNorm[i].z));
        if (!tempUV.empty()) osgUV->push_back(osg::Vec2(tempUV[i].x, tempUV[i].y));
        if (!tempColor.empty()) osgColor->push_back(osg::Vec4(tempColor[i].x, tempColor[i].y, tempColor[i].z, tempColor[i].w));
    }

    // Scratch buffer for triangulation
    std::vector<uint32_t> triIndices;
    triIndices.resize(mesh->max_face_triangles * 3);

    // Split by material parts
    std::vector<CachedPart> cachedParts;

    size_t num_parts = mesh->material_parts.count > 0 ? mesh->material_parts.count : 1;
    size_t faceOffset = 0;

    for (size_t partIndex = 0; partIndex < num_parts; ++partIndex) {
        // Identify faces for this part
        std::vector<uint32_t> partFaces;
        size_t matIndex = 0;

        if (mesh->material_parts.count > 0) {
            ufbx_mesh_part *part = &mesh->material_parts.data[partIndex];
            matIndex = part->index;
            if (part->num_faces == 0) continue;

            if (part->face_indices.count > 0) {
                partFaces.resize(part->face_indices.count);
                for(size_t i=0; i<part->face_indices.count; ++i) {
                    partFaces[i] = part->face_indices.data[i];
                }
            } else {
                // Contiguous range
                partFaces.resize(part->num_faces);
                for(size_t i=0; i<part->num_faces; ++i) partFaces[i] = (uint32_t)(faceOffset + i);
                faceOffset += part->num_faces;
            }
        } else {
            // No parts, process all faces
            partFaces.resize(mesh->num_faces);
            for(size_t i=0; i<mesh->num_faces; ++i) partFaces[i] = (uint32_t)i;
        }

        if (partFaces.empty()) continue;

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setVertexArray(osgPos);
        if (osgNorm->size() > 0) geometry->setNormalArray(osgNorm, osg::Array::BIND_PER_VERTEX);
        if (osgUV->size() > 0) geometry->setTexCoordArray(0, osgUV, osg::Array::BIND_PER_VERTEX);
        if (osgColor->size() > 0) geometry->setColorArray(osgColor, osg::Array::BIND_PER_VERTEX);

        osg::ref_ptr<osg::DrawElementsUInt> drawElements = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);

        for (uint32_t faceIdx : partFaces) {
            ufbx_face face = mesh->faces.data[faceIdx];

            // Triangulate using ufbx
            size_t numTris = ufbx_triangulate_face(triIndices.data(), triIndices.size(), mesh, face);

            for (size_t t = 0; t < numTris; ++t) {
                for (size_t v = 0; v < 3; ++v) {
                    uint32_t wedgeIdx = triIndices[t * 3 + v];
                    // Map wedge index to unique index
                    uint32_t uniqueIdx = generated_indices[wedgeIdx];
                    drawElements->push_back(uniqueIdx);
                }
            }
        }

        geometry->addPrimitiveSet(drawElements);

        // Material
        if (mesh->materials.count > matIndex) {
             ufbx_material *mat = mesh->materials.data[matIndex];
             geometry->setStateSet(getOrCreateStateSet(mat));
        }

        geode->addDrawable(geometry);

        // Cache Info
        CachedPart cpart;
        cpart.geometry = geometry;
        cpart.geomHash = meshHash;
        cpart.matHash = calcMaterialHash(mesh->materials.count > matIndex ? mesh->materials.data[matIndex] : nullptr);
        cachedParts.push_back(cpart);

        // MeshPool Update
        MeshInstanceInfo info;
        info.key.geomHash = cpart.geomHash;
        info.key.matHash = cpart.matHash;
        info.geometry = geometry;
        info.transforms.push_back(globalXform);
        info.nodeNames.push_back(ufbx_string_to_std(node->name));
        info.nodeAttrs.push_back(collectNodeAttrs(node));

        if (meshPool.find(info.key) != meshPool.end()) {
             meshPool[info.key].transforms.push_back(globalXform);
             meshPool[info.key].nodeNames.push_back(ufbx_string_to_std(node->name));
             meshPool[info.key].nodeAttrs.push_back(collectNodeAttrs(node));
        } else {
             meshPool[info.key] = info;
        }
    }

    // Store in Cache
    meshCache[mesh] = cachedParts;

    return geode;
}
