#include "FBXPipeline.h"
#include "extern.h"
#include "GeoTransform.h"
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Material>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>

// Use existing tinygltf if possible, or include it
#include <osgDB/ReaderWriter>
#include <osgDB/Registry>
#include <tiny_gltf.h>
#include <nlohmann/json.hpp>
#include "mesh_processor.h"
#include <osg/Texture>
#include <osg/Image>
#include "lod_pipeline.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Constants
const uint32_t B3DM_MAGIC = 0x6D643362;
const uint32_t I3DM_MAGIC = 0x6D643369;

struct B3dmHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t byteLength;
    uint32_t featureTableJSONByteLength;
    uint32_t featureTableBinaryByteLength;
    uint32_t batchTableJSONByteLength;
    uint32_t batchTableBinaryByteLength;
};

struct I3dmHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t byteLength;
    uint32_t featureTableJSONByteLength;
    uint32_t featureTableBinaryByteLength;
    uint32_t batchTableJSONByteLength;
    uint32_t batchTableBinaryByteLength;
    uint32_t gltfFormat; // 0: uri, 1: embedded
};

// Helper to check point in box
bool isPointInBox(const osg::Vec3d& p, const osg::BoundingBox& b) {
    return p.x() >= b.xMin() && p.x() <= b.xMax() &&
           p.y() >= b.yMin() && p.y() <= b.yMax() &&
           p.z() >= b.zMin() && p.z() <= b.zMax();
}

FBXPipeline::FBXPipeline(const PipelineSettings& s) : settings(s) {
}

FBXPipeline::~FBXPipeline() {
    if (loader) delete loader;
    if (rootNode) delete rootNode;
}

void FBXPipeline::run() {
    LOG_I("Starting FBXPipeline...");

    loader = new FBXLoader(settings.inputPath);
    loader->load();
    LOG_I("FBX Loaded. Mesh Pool Size: %zu", loader->meshPool.size());

    // Lambda to generate LOD settings chain
    auto generateLODChain = [&](const PipelineSettings& cfg) -> LODPipelineSettings {
        LODPipelineSettings lodOps;
        lodOps.enable_lod = cfg.enableLOD;

        SimplificationParams simTemplate;
        simTemplate.enable_simplification = true;
        simTemplate.target_error = 0.01f; // Base error

        DracoCompressionParams dracoTemplate;
        dracoTemplate.enable_compression = cfg.enableDraco;

        // Use build_lod_levels from lod_pipeline.h
        // Ratios are expected to be e.g. [1.0, 0.5, 0.25]
        lodOps.levels = build_lod_levels(
            cfg.lodRatios,
            simTemplate.target_error,
            simTemplate,
            dracoTemplate,
            false // draco_for_lod0
        );

        return lodOps;
    };

    // Simplification Step (Only if LOD is NOT enabled, otherwise we do it per-level or later)
    if (settings.enableSimplify && !settings.enableLOD) {
        LOG_I("Simplifying meshes (Global)...");
        SimplificationParams simParams;
        simParams.enable_simplification = true;
        simParams.target_ratio = 0.5f; // Default ratio
        simParams.target_error = 1e-2f;

        for (auto& pair : loader->meshPool) {
            if (pair.second.geometry) {
                simplify_mesh_geometry(pair.second.geometry.get(), simParams);
            }
        }
    } else if (settings.enableLOD) {
        // If LOD is enabled, we prepare the settings
        LODPipelineSettings lodSettings = generateLODChain(settings);
        LOG_I("LOD Enabled. Generated %zu LOD levels configuration.", lodSettings.levels.size());
        // Actual HLOD generation implementation would go here or in buildOctree/processNode
    }

    rootNode = new OctreeNode();

    osg::BoundingBox globalBounds;
    for (auto& pair : loader->meshPool) {
        MeshInstanceInfo& info = pair.second;
        if (!info.geometry) continue;

        osg::BoundingBox geomBox = info.geometry->getBoundingBox();
        for (size_t i = 0; i < info.transforms.size(); ++i) {
            const auto& mat = info.transforms[i];

            // Expand global bounds
            osg::BoundingBox instBox;
            for (int k = 0; k < 8; ++k) instBox.expandBy(geomBox.corner(k) * mat);
            globalBounds.expandBy(instBox);

            // Add to root node content initially
            InstanceRef ref;
            ref.meshInfo = &info;
            ref.transformIndex = (int)i;
            rootNode->content.push_back(ref);
        }
    }
    rootNode->bbox = globalBounds;

    LOG_I("Building Octree...");
    buildOctree(rootNode);

    LOG_I("Processing Nodes and Generating Tiles...");
    json rootJson = processNode(rootNode, settings.outputPath);

    LOG_I("Writing tileset.json...");
    writeTilesetJson(settings.outputPath, globalBounds, rootJson);

    LOG_I("FBXPipeline Finished.");
}

void FBXPipeline::buildOctree(OctreeNode* node) {
    if (node->depth >= settings.maxDepth || node->content.size() <= settings.maxItemsPerTile) {
        return;
    }

    // Split bbox into 8 children
    osg::Vec3d center = node->bbox.center();
    osg::Vec3d min = node->bbox._min;
    osg::Vec3d max = node->bbox._max;

    // 8 quadrants
    osg::BoundingBox boxes[8];
    // Bottom-Left-Back (0) to Top-Right-Front (7)
    // x: left/right, y: back/front, z: bottom/top

    // min, center, max combinations
    // 0: min.x, min.y, min.z -> center.x, center.y, center.z
    boxes[0] = osg::BoundingBox(min.x(), min.y(), min.z(), center.x(), center.y(), center.z());
    boxes[1] = osg::BoundingBox(center.x(), min.y(), min.z(), max.x(), center.y(), center.z());
    boxes[2] = osg::BoundingBox(min.x(), center.y(), min.z(), center.x(), max.y(), center.z());
    boxes[3] = osg::BoundingBox(center.x(), center.y(), min.z(), max.x(), max.y(), center.z());
    boxes[4] = osg::BoundingBox(min.x(), min.y(), center.z(), center.x(), center.y(), max.z());
    boxes[5] = osg::BoundingBox(center.x(), min.y(), center.z(), max.x(), center.y(), max.z());
    boxes[6] = osg::BoundingBox(min.x(), center.y(), center.z(), center.x(), max.y(), max.z());
    boxes[7] = osg::BoundingBox(center.x(), center.y(), center.z(), max.x(), max.y(), max.z());

    for (int i = 0; i < 8; ++i) {
        OctreeNode* child = new OctreeNode();
        child->bbox = boxes[i];
        child->depth = node->depth + 1;
        node->children.push_back(child);
    }

    // Distribute content
    // For simplicity, if an object center is in box, it goes there.
    // Or strictly by bounding box intersection.
    // Here we use center point for distribution to avoid duplication across nodes (unless we want loose octree)
    std::vector<InstanceRef> remaining;
    for (const auto& ref : node->content) {
        osg::Matrixd mat = ref.meshInfo->transforms[ref.transformIndex];
        osg::BoundingBox meshBox = ref.meshInfo->geometry->getBoundingBox();
        osg::Vec3d meshCenter = meshBox.center() * mat;

        bool placed = false;
        for (auto child : node->children) {
            if (isPointInBox(meshCenter, child->bbox)) {
                child->content.push_back(ref);
                placed = true;
                break;
            }
        }
        if (!placed) {
            // Should not happen if box covers all, but maybe precision issues
            // Keep in current node? Or force into one?
            // Let's keep in current node if not fitting children (e.g. exactly on boundary?)
            // Or just put in first matching
            node->children[0]->content.push_back(ref);
        }
    }
    node->content.clear(); // Moved to children

    // Recurse
    for (auto child : node->children) {
        if (!child->content.empty()) {
            buildOctree(child);
        }
    }

    // Prune empty children
    auto it = std::remove_if(node->children.begin(), node->children.end(), [](OctreeNode* c){
        if (c->content.empty() && c->children.empty()) {
            delete c;
            return true;
        }
        return false;
    });
    node->children.erase(it, node->children.end());
}

// Helper to append geometry to tinygltf model
void appendGeometryToModel(tinygltf::Model& model, const std::vector<InstanceRef>& instances, const PipelineSettings& settings, json* batchTableJson, int* batchIdCounter, const SimplificationParams& simParams, osg::BoundingBox* outBox = nullptr) {
    if (instances.empty()) return;

    // Ensure model has at least one buffer
    if (model.buffers.empty()) {
        model.buffers.push_back(tinygltf::Buffer());
    }
    tinygltf::Buffer& buffer = model.buffers[0];

    // Group instances by material
    struct GeomInst {
        osg::Geometry* geom;
        osg::Matrixd matrix;
        int originalBatchId;
    };
    std::map<const osg::StateSet*, std::vector<GeomInst>> materialGroups;

    for (const auto& ref : instances) {
        osg::Geometry* geom = ref.meshInfo->geometry.get();
        if (!geom) continue;
        osg::StateSet* ss = geom->getStateSet();
        materialGroups[ss].push_back({geom, ref.meshInfo->transforms[ref.transformIndex], *batchIdCounter});
        (*batchIdCounter)++;
    }

    // Process each material group
    for (auto& pair : materialGroups) {
        // Prepare merged data
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<unsigned int> indices; // Use uint32 for safety
        std::vector<float> batchIds; // Use float for BATCHID attribute (vec1)

        double minPos[3] = {1e30, 1e30, 1e30};
        double maxPos[3] = {-1e30, -1e30, -1e30};

        for (const auto& inst : pair.second) {
            osg::ref_ptr<osg::Geometry> processedGeom = inst.geom;
            if (simParams.enable_simplification) {
                processedGeom = (osg::Geometry*)inst.geom->clone(osg::CopyOp::DEEP_COPY_ALL);
                simplify_mesh_geometry(processedGeom.get(), simParams);
            }

            osg::Vec3Array* v = dynamic_cast<osg::Vec3Array*>(processedGeom->getVertexArray());
            osg::Vec3Array* n = dynamic_cast<osg::Vec3Array*>(processedGeom->getNormalArray());
            osg::Vec2Array* t = dynamic_cast<osg::Vec2Array*>(processedGeom->getTexCoordArray(0));

            if (!v || v->empty()) continue;

            uint32_t baseIndex = (uint32_t)(positions.size() / 3);

            for (unsigned int i = 0; i < v->size(); ++i) {
                osg::Vec3d p = (*v)[i];
                p = p * inst.matrix;

                // Input is Y-up from ufbx (due to opts.target_axes = ufbx_axes_right_handed_y_up).
                // We want to convert to ENU (East-North-Up) coordinates for 3D Tiles (Z-up).
                // X (Right) -> East (+X)
                // Y (Up)    -> Up   (+Z)
                // Z (Back)  -> South (-Y) or North (+Y)?
                // usually OpenGL -Z is Forward (North). So +Z is Back (South).
                // So Z -> -Y.

                float px = (float)p.x();
                float py = (float)-p.z();
                float pz = (float)p.y();

                positions.push_back(px);
                positions.push_back(py);
                positions.push_back(pz);

                // Compute bounds in ENU (Z-up) for tileset.json
                float enu_x = px;
                float enu_y = py;
                float enu_z = pz;

                if (enu_x < minPos[0]) minPos[0] = enu_x;
                if (enu_y < minPos[1]) minPos[1] = enu_y;
                if (enu_z < minPos[2]) minPos[2] = enu_z;
                if (enu_x > maxPos[0]) maxPos[0] = enu_x;
                if (enu_y > maxPos[1]) maxPos[1] = enu_y;
                if (enu_z > maxPos[2]) maxPos[2] = enu_z;

                // Accumulate to global outBox if provided
                if (outBox) {
                    outBox->expandBy(osg::Vec3d(enu_x, enu_y, enu_z));
                }

                if (n && i < n->size()) {
                    osg::Vec3d nm = (*n)[i];
                    nm = osg::Matrix::transform3x3(osg::Matrix::inverse(inst.matrix), nm);
                    nm.normalize();
                    // Rotate normal same as position
                    normals.push_back((float)nm.x());
                    normals.push_back((float)-nm.z());
                    normals.push_back((float)nm.y());
                } else {
                    normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f); // Up is Z (ENU)
                }

                if (t && i < t->size()) {
                    texcoords.push_back((float)(*t)[i].x());
                    texcoords.push_back((float)(*t)[i].y());
                } else {
                    texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                }

                // Batch ID (optional, for picking)
                batchIds.push_back((float)inst.originalBatchId);
            }

            // Indices
            for (unsigned int k = 0; k < processedGeom->getNumPrimitiveSets(); ++k) {
                osg::PrimitiveSet* ps = processedGeom->getPrimitiveSet(k);
                if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) continue;

                const osg::DrawElementsUShort* deus = dynamic_cast<const osg::DrawElementsUShort*>(ps);
                const osg::DrawElementsUInt* deui = dynamic_cast<const osg::DrawElementsUInt*>(ps);

                if (deus) {
                    for (unsigned int idx = 0; idx < deus->size(); ++idx) indices.push_back(baseIndex + (*deus)[idx]);
                } else if (deui) {
                    for (unsigned int idx = 0; idx < deui->size(); ++idx) indices.push_back(baseIndex + (*deui)[idx]);
                } else {
                    // DrawArrays?
                    // Not handled for now
                }
            }
        }

        if (positions.empty() || indices.empty()) continue;

        // Write to buffer
        size_t posOffset = buffer.data.size();
        size_t posLen = positions.size() * sizeof(float);
        buffer.data.resize(posOffset + posLen);
        memcpy(buffer.data.data() + posOffset, positions.data(), posLen);

        size_t normOffset = buffer.data.size();
        size_t normLen = normals.size() * sizeof(float);
        buffer.data.resize(normOffset + normLen);
        memcpy(buffer.data.data() + normOffset, normals.data(), normLen);

        size_t texOffset = buffer.data.size();
        size_t texLen = texcoords.size() * sizeof(float);
        buffer.data.resize(texOffset + texLen);
        memcpy(buffer.data.data() + texOffset, texcoords.data(), texLen);

        size_t indOffset = buffer.data.size();
        size_t indLen = indices.size() * sizeof(unsigned int);
        buffer.data.resize(indOffset + indLen);
        memcpy(buffer.data.data() + indOffset, indices.data(), indLen);

        // Batch IDs
        size_t batchOffset = buffer.data.size();
        size_t batchLen = batchIds.size() * sizeof(float);
        buffer.data.resize(batchOffset + batchLen);
        memcpy(buffer.data.data() + batchOffset, batchIds.data(), batchLen);

        // BufferViews
        tinygltf::BufferView bvPos;
        bvPos.buffer = 0;
        bvPos.byteOffset = posOffset;
        bvPos.byteLength = posLen;
        bvPos.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int bvPosIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(bvPos);

        tinygltf::BufferView bvNorm;
        bvNorm.buffer = 0;
        bvNorm.byteOffset = normOffset;
        bvNorm.byteLength = normLen;
        bvNorm.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int bvNormIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(bvNorm);

        tinygltf::BufferView bvTex;
        bvTex.buffer = 0;
        bvTex.byteOffset = texOffset;
        bvTex.byteLength = texLen;
        bvTex.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int bvTexIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(bvTex);

        tinygltf::BufferView bvInd;
        bvInd.buffer = 0;
        bvInd.byteOffset = indOffset;
        bvInd.byteLength = indLen;
        bvInd.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
        int bvIndIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(bvInd);

        tinygltf::BufferView bvBatch;
        bvBatch.buffer = 0;
        bvBatch.byteOffset = batchOffset;
        bvBatch.byteLength = batchLen;
        bvBatch.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int bvBatchIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(bvBatch);

        // Accessors
        tinygltf::Accessor accPos;
        accPos.bufferView = bvPosIdx;
        accPos.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accPos.count = positions.size() / 3;
        accPos.type = TINYGLTF_TYPE_VEC3;
        accPos.minValues = {minPos[0], minPos[1], minPos[2]};
        accPos.maxValues = {maxPos[0], maxPos[1], maxPos[2]};
        int accPosIdx = (int)model.accessors.size();
        model.accessors.push_back(accPos);

        tinygltf::Accessor accNorm;
        accNorm.bufferView = bvNormIdx;
        accNorm.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accNorm.count = normals.size() / 3;
        accNorm.type = TINYGLTF_TYPE_VEC3;
        int accNormIdx = (int)model.accessors.size();
        model.accessors.push_back(accNorm);

        tinygltf::Accessor accTex;
        accTex.bufferView = bvTexIdx;
        accTex.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accTex.count = texcoords.size() / 2;
        accTex.type = TINYGLTF_TYPE_VEC2;
        int accTexIdx = (int)model.accessors.size();
        model.accessors.push_back(accTex);

        tinygltf::Accessor accInd;
        accInd.bufferView = bvIndIdx;
        accInd.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        accInd.count = indices.size();
        accInd.type = TINYGLTF_TYPE_SCALAR;
        int accIndIdx = (int)model.accessors.size();
        model.accessors.push_back(accInd);

        tinygltf::Accessor accBatch;
        accBatch.bufferView = bvBatchIdx;
        accBatch.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accBatch.count = batchIds.size();
        accBatch.type = TINYGLTF_TYPE_SCALAR;
        int accBatchIdx = (int)model.accessors.size();
        model.accessors.push_back(accBatch);

        // Material
        tinygltf::Material mat;
        mat.name = "Default";
        mat.doubleSided = true; // Fix for potential backface culling issues

        const osg::StateSet* stateSet = pair.first;
        std::vector<double> baseColor = {1.0, 1.0, 1.0, 1.0};

        // Check for texture in StateSet
        if (stateSet) {
             // Get Material Color
             const osg::Material* osgMat = dynamic_cast<const osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
             if (osgMat) {
                 osg::Vec4 diffuse = osgMat->getDiffuse(osg::Material::FRONT);
                 baseColor = {diffuse.r(), diffuse.g(), diffuse.b(), diffuse.a()};
             }

             const osg::Texture* tex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
             if (tex && tex->getNumImages() > 0) {
                 const osg::Image* img = tex->getImage(0);
                 if (img) {
                     std::string imgPath = img->getFileName();
                     std::vector<unsigned char> imgData;
                     std::string mimeType = "image/png"; // default

                     bool hasData = false;
                    if (!imgPath.empty() && fs::exists(imgPath)) {
                        std::ifstream file(imgPath, std::ios::binary | std::ios::ate);
                        if (file) {
                            size_t size = file.tellg();
                            imgData.resize(size);
                            file.seekg(0);
                            file.read(reinterpret_cast<char*>(imgData.data()), size);
                            hasData = true;

                            std::string ext = fs::path(imgPath).extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            if (ext == ".jpg" || ext == ".jpeg") mimeType = "image/jpeg";
                        }
                    }

                    // Fallback: If file not found but image data exists (e.g. embedded or generated)
                    if (!hasData && img->data() != nullptr) {
                        std::string ext = "png";
                        if (!imgPath.empty()) {
                            std::string e = fs::path(imgPath).extension().string();
                            if (!e.empty() && e.size() > 1) {
                                ext = e.substr(1); // remove dot
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            }
                        }

                        // Try to write to memory
                        std::stringstream ss;
                        osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(ext);
                        if (rw) {
                            osgDB::ReaderWriter::WriteResult wr = rw->writeImage(*img, ss);
                            if (wr.success()) {
                                std::string s = ss.str();
                                imgData.assign(s.begin(), s.end());
                                hasData = true;
                                if (ext == "jpg" || ext == "jpeg") mimeType = "image/jpeg";
                                else mimeType = "image/png";
                            }
                        }

                        // Retry with PNG if failed
                        if (!hasData && ext != "png") {
                            rw = osgDB::Registry::instance()->getReaderWriterForExtension("png");
                            if (rw) {
                                std::stringstream ss2;
                                osgDB::ReaderWriter::WriteResult wr = rw->writeImage(*img, ss2);
                                if (wr.success()) {
                                    std::string s = ss2.str();
                                    imgData.assign(s.begin(), s.end());
                                    hasData = true;
                                    mimeType = "image/png";
                                }
                            }
                        }
                    }

                    if (hasData) {
                         // Add Image
                         tinygltf::Image gltfImg;
                         gltfImg.mimeType = mimeType;

                         // Create BufferView for image data (Embedded)
                         // Ensure 4-byte alignment before writing image
                         size_t currentSize = buffer.data.size();
                         size_t padding = (4 - (currentSize % 4)) % 4;
                         if (padding > 0) {
                             buffer.data.resize(currentSize + padding);
                             // Fill padding with 0
                             memset(buffer.data.data() + currentSize, 0, padding);
                         }

                         size_t imgOffset = buffer.data.size();
                         size_t imgLen = imgData.size();
                         buffer.data.resize(imgOffset + imgLen);
                         memcpy(buffer.data.data() + imgOffset, imgData.data(), imgLen);

                         tinygltf::BufferView bvImg;
                         bvImg.buffer = 0;
                         bvImg.byteOffset = imgOffset;
                         bvImg.byteLength = imgLen;
                         int bvImgIdx = (int)model.bufferViews.size();
                         model.bufferViews.push_back(bvImg);

                         gltfImg.bufferView = bvImgIdx;
                         int imgIdx = (int)model.images.size();
                         model.images.push_back(gltfImg);

                         // Add Texture
                         tinygltf::Texture gltfTex;
                         gltfTex.source = imgIdx;
                         int texIdx = (int)model.textures.size();
                         model.textures.push_back(gltfTex);

                         // Link to Material
                         mat.pbrMetallicRoughness.baseColorTexture.index = texIdx;

                         // Ensure 4-byte alignment AFTER writing image, so next bufferView starts aligned
                         size_t endSize = buffer.data.size();
                         size_t endPadding = (4 - (endSize % 4)) % 4;
                         if (endPadding > 0) {
                            buffer.data.resize(endSize + endPadding);
                            memset(buffer.data.data() + endSize, 0, endPadding);
                        }
                    }
                }
            }
       }

        mat.pbrMetallicRoughness.baseColorFactor = baseColor;
        if (baseColor[3] < 0.99) {
            mat.alphaMode = "BLEND";
        } else {
            mat.alphaMode = "OPAQUE";
        }

        mat.pbrMetallicRoughness.metallicFactor = 0.0;
        mat.pbrMetallicRoughness.roughnessFactor = 1.0;
        int matIdx = (int)model.materials.size();
        model.materials.push_back(mat);

        // Mesh
        tinygltf::Mesh mesh;
        tinygltf::Primitive prim;
        prim.mode = TINYGLTF_MODE_TRIANGLES;
        prim.indices = accIndIdx;
        prim.attributes["POSITION"] = accPosIdx;
        prim.attributes["NORMAL"] = accNormIdx;
        prim.attributes["TEXCOORD_0"] = accTexIdx;
        prim.attributes["_BATCHID"] = accBatchIdx;
        prim.material = matIdx;
        mesh.primitives.push_back(prim);
        int meshIdx = (int)model.meshes.size();
        model.meshes.push_back(mesh);

        // Node
        tinygltf::Node node;
        node.mesh = meshIdx;
        int nodeIdx = (int)model.nodes.size();
        model.nodes.push_back(node);
    }

    // Scene
    tinygltf::Scene scene;
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        scene.nodes.push_back((int)i);
    }
    model.scenes.push_back(scene);
    model.defaultScene = 0;
}

json FBXPipeline::processNode(OctreeNode* node, const std::string& parentPath) {
    json nodeJson;
    nodeJson["refine"] = "REPLACE";

    osg::BoundingBox tightBox;
    bool hasTightBox = false;

    // 2. Content
    if (!node->content.empty()) {
        std::string tileName = "tile_" + std::to_string(node->depth) + "_" + std::to_string((uintptr_t)node); // Unique name

        // Create content
        // For B3DM
        auto result = createB3DM(node->content, parentPath, tileName);
        std::string contentUrl = result.first;
        osg::BoundingBox cBox = result.second;

        if (!contentUrl.empty()) {
            nodeJson["content"] = {{"uri", contentUrl}};
            if (cBox.valid()) {
                tightBox.expandBy(cBox);
                hasTightBox = true;
            }
        }
    }

    // 3. Children
    if (!node->children.empty()) {
        nodeJson["children"] = json::array();
        for (auto child : node->children) {
            json childJson = processNode(child, parentPath);
            nodeJson["children"].push_back(childJson);

            // Expand tightBox by child's bounding volume
            try {
                auto& cBoxJson = childJson["boundingVolume"]["box"];
                if (cBoxJson.is_array() && cBoxJson.size() == 12) {
                    double cx = cBoxJson[0];
                    double cy = cBoxJson[1];
                    double cz = cBoxJson[2];
                    double dx = cBoxJson[3];
                    double dy = cBoxJson[7];
                    double dz = cBoxJson[11];

                    // Reconstruct box (approximate AABB from OBB)
                    tightBox.expandBy(osg::Vec3d(cx - dx, cy - dy, cz - dz));
                    tightBox.expandBy(osg::Vec3d(cx + dx, cy + dy, cz + dz));
                    hasTightBox = true;
                }
            } catch (...) {}
        }
    }

    // 1. Calculate Geometric Error and Bounding Volume
    double diagonal = 0.0;
    if (hasTightBox) {
        double dx = tightBox.xMax() - tightBox.xMin();
        double dy = tightBox.yMax() - tightBox.yMin();
        double dz = tightBox.zMax() - tightBox.zMin();
        diagonal = std::sqrt(dx*dx + dy*dy + dz*dz);

        double cx = tightBox.center().x();
        double cy = tightBox.center().y();
        double cz = tightBox.center().z();
        double hx = (tightBox.xMax() - tightBox.xMin()) / 2.0;
        double hy = (tightBox.yMax() - tightBox.yMin()) / 2.0;
        double hz = (tightBox.zMax() - tightBox.zMin()) / 2.0;

        // Add small padding (10%) to avoid near-plane culling or precision issues
        hx *= 1.1;
        hy *= 1.1;
        hz *= 1.1;

        nodeJson["boundingVolume"] = {
            {"box", {
                cx, cy, cz,
                hx, 0, 0,
                0, hy, 0,
                0, 0, hz
            }}
        };
    } else {
        // Fallback: Transform node->bbox from Y-up to Z-up
        double cx = node->bbox.center().x();
        double cy = node->bbox.center().y();
        double cz = node->bbox.center().z();

        double extentX = (node->bbox.xMax() - node->bbox.xMin()) / 2.0;
        double extentY = (node->bbox.yMax() - node->bbox.yMin()) / 2.0;
        double extentZ = (node->bbox.zMax() - node->bbox.zMin()) / 2.0;

        // Add padding to fallback as well
        extentX *= 1.1;
        extentY *= 1.1;
        extentZ *= 1.1;

        diagonal = std::sqrt(extentX*extentX*4 + extentY*extentY*4 + extentZ*extentZ*4);

        nodeJson["boundingVolume"] = {
            {"box", {
                cx, -cz, cy,           // Center transformed
                extentX, 0, 0,         // X axis
                0, extentZ, 0,         // Y axis (was Z)
                0, 0, extentY          // Z axis (was Y)
            }}
        };
    }

    // Use a small non-zero error even for leaves to prevent aggressive culling in some viewers,
    // or keep 0.0 if strict compliance is desired.
    // However, if it is a root node (diagonal large), 0.0 might be confusing.
    // For now, we stick to 0.0 for leaves but ensure diagonal is correct for parents.
    nodeJson["geometricError"] = node->isLeaf() ? 0.0 : diagonal;

    return nodeJson;
}

std::pair<std::string, osg::BoundingBox> FBXPipeline::createB3DM(const std::vector<InstanceRef>& instances, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams) {
    // 1. Create GLB (TinyGLTF)
    tinygltf::Model model;
    tinygltf::Asset asset;
    asset.version = "2.0";
    asset.generator = "FBX23DTiles";
    model.asset = asset;

    json batchTableJson;
    int batchIdCounter = 0;
    osg::BoundingBox contentBox;

    appendGeometryToModel(model, instances, settings, &batchTableJson, &batchIdCounter, simParams, &contentBox);

    // 2. Create B3DM wrapping GLB
    std::string filename = tileName + ".b3dm";
    std::string fullPath = (fs::path(tilePath) / filename).string();

    std::ofstream outfile(fullPath, std::ios::binary);
    if (!outfile) {
        LOG_E("Failed to create B3DM file: %s", fullPath.c_str());
        return {"", contentBox};
    }

    // Serialize GLB to memory
    tinygltf::TinyGLTF gltf;
    std::stringstream ss;
    gltf.WriteGltfSceneToStream(&model, ss, false, true); // pretty=false, binary=true
    std::string glbData = ss.str();

    // Create Feature Table JSON
    json featureTable;

    // For single instance (or merged mesh), if we have only 1 batch ID (0),
    // we can simplify by setting BATCH_LENGTH to 0, which implies no batching.
    // This avoids issues with _BATCHID attribute requirement if BATCH_LENGTH > 0.
    // However, if we do have multiple batches, we set it.
    if (batchIdCounter <= 1) {
        featureTable["BATCH_LENGTH"] = 0;
    } else {
        featureTable["BATCH_LENGTH"] = batchIdCounter;
    }

    std::string featureTableString = featureTable.dump();

    // Calculate Padding
    // Header (28 bytes) + FeatureTableJSON (N bytes) + Padding (P bytes)
    // Spec: "The byte length of the Feature Table JSON ... must be aligned to 8 bytes."
    // This strictly means featureTableJSONByteLength % 8 == 0.

    size_t jsonLen = featureTableString.size();
    size_t padding = (8 - (jsonLen % 8)) % 8;
    for(size_t i=0; i<padding; ++i) {
        featureTableString += " ";
    }

    // Now featureTableString.size() is multiple of 8.
    // Header is 28 bytes.
    // 28 + featureTableString.size() is 4 mod 8.
    // So GLB starts at 4 mod 8. This is valid for GLB (4-byte alignment).

    // Also, align GLB data size to 8 bytes (Spec recommendation for B3DM body end? No, just good practice)
    // Actually, B3DM byteLength doesn't need to be aligned, but let's align it to 8 bytes for safety.
    size_t glbLen = glbData.size();
    size_t glbPadding = (8 - (glbLen % 8)) % 8;
    for(size_t i=0; i<glbPadding; ++i) {
        glbData += '\0';
    }

    // Write header
    B3dmHeader header;
    header.magic = B3DM_MAGIC;
    header.version = 1;
    header.featureTableJSONByteLength = (uint32_t)featureTableString.size();
    header.featureTableBinaryByteLength = 0;
    header.batchTableJSONByteLength = 0;
    header.batchTableBinaryByteLength = 0;
    header.byteLength = (uint32_t)(sizeof(B3dmHeader) + featureTableString.size() + glbData.size());

    outfile.write(reinterpret_cast<const char*>(&header), sizeof(B3dmHeader));
    outfile.write(featureTableString.c_str(), featureTableString.size());
    outfile.write(glbData.data(), glbData.size());
    outfile.close();

    return {filename, contentBox};
}

std::string FBXPipeline::createI3DM(MeshInstanceInfo* meshInfo, const std::vector<int>& transformIndices, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams) {
    return "";
}

void FBXPipeline::writeTilesetJson(const std::string& basePath, const osg::BoundingBox& globalBounds, const nlohmann::json& rootContent) {
    json tileset;
    tileset["asset"] = {
        {"version", "1.0"},
        {"gltfUpAxis", "Z"} // OSG/FBX usually Z-up or we converted
    };

    // Use geometric error from root content if available, otherwise fallback to global bounds
    double geometricError = 0.0;
    if (rootContent.contains("geometricError")) {
        geometricError = rootContent["geometricError"];
    } else {
        double dx = globalBounds.xMax() - globalBounds.xMin();
        double dy = globalBounds.yMax() - globalBounds.yMin();
        double dz = globalBounds.zMax() - globalBounds.zMin();
        geometricError = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    tileset["geometricError"] = geometricError;

    // Root
    tileset["root"] = rootContent;

    // Force geometric error for root node if it's 0.0 (which happens if it's a leaf)
    // A root node with 0.0 geometric error might cause visibility issues in some viewers
    // if the screen space error calculation behaves unexpectedly.
    // We set it to the calculated diagonal size to ensure it passes the SSE check initially.
    if (tileset["root"]["geometricError"] == 0.0) {
         double diag = 0.0;
         if (rootContent["boundingVolume"].contains("box")) {
            auto& box = rootContent["boundingVolume"]["box"];
             if (box.is_array() && box.size() == 12) {
                // box is center(3) + x_axis(3) + y_axis(3) + z_axis(3)
                // Half axes vectors (assuming AABB structure as generated in processNode)
                double hx = box[3]; // box[3], box[4], box[5]
                double hy = box[7]; // box[6], box[7], box[8]
                double hz = box[11]; // box[9], box[10], box[11]

                // If it's not strictly AABB (rotated), we should compute length of vectors
                // But our processNode generates diagonal matrices for axes.
                double xlen = std::sqrt(double(box[3])*double(box[3]) + double(box[4])*double(box[4]) + double(box[5])*double(box[5]));
                double ylen = std::sqrt(double(box[6])*double(box[6]) + double(box[7])*double(box[7]) + double(box[8])*double(box[8]));
                double zlen = std::sqrt(double(box[9])*double(box[9]) + double(box[10])*double(box[10]) + double(box[11])*double(box[11]));

                // Diagonal of the box (2 * half_diagonal)
                diag = 2.0 * std::sqrt(xlen*xlen + ylen*ylen + zlen*zlen);
             }
         }

         if (diag > 0.0) {
            tileset["root"]["geometricError"] = diag;
            LOG_I("Forcing root geometric error to %f (calculated from root box)", diag);
         } else {
             // Fallback to globalBounds if box missing (unlikely)
             // globalBounds is Y-up from FBX, but size is same
             double dx = globalBounds.xMax() - globalBounds.xMin();
             double dy = globalBounds.yMax() - globalBounds.yMin();
             double dz = globalBounds.zMax() - globalBounds.zMin();
             double fallbackDiag = std::sqrt(dx*dx + dy*dy + dz*dz);
             tileset["root"]["geometricError"] = fallbackDiag;
             LOG_I("Forcing root geometric error to %f (calculated from global bounds)", fallbackDiag);
         }
    }

    // Add Transform if geolocation is provided
    if (settings.longitude != 0.0 || settings.latitude != 0.0 || settings.height != 0.0) {
        // Calculate ENU to ECEF matrix
        glm::dmat4 enuToEcef = GeoTransform::CalcEnuToEcefMatrix(settings.longitude, settings.latitude, settings.height);

        // GLM is column-major, 3D Tiles expects column-major array of 16 numbers
        const double* m = (const double*)&enuToEcef;
        tileset["root"]["transform"] = {
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]
        };
    }

    std::string s = tileset.dump(4);
    std::ofstream out(fs::path(basePath) / "tileset.json");
    out << s;
    out.close();
}

// C-API Implementation
extern "C" void* fbx23dtile(
    const char* in_path,
    const char* out_path,
    double* box_ptr,
    int* len,
    int max_lvl,
    bool enable_texture_compress,
    bool enable_meshopt,
    bool enable_draco,
    double longitude,
    double latitude,
    double height
) {
    std::string input(in_path);
    std::string output(out_path);

    PipelineSettings settings;
    settings.inputPath = input;
    settings.outputPath = output;
    settings.maxDepth = max_lvl > 0 ? max_lvl : 5;
    settings.enableTextureCompress = enable_texture_compress;
    settings.enableDraco = enable_draco;
    settings.enableSimplify = enable_meshopt;
    settings.enableLOD = false; // HLOD not yet implemented
    settings.longitude = longitude;
    settings.latitude = latitude;
    settings.height = height;

    FBXPipeline pipeline(settings);
    pipeline.run();

    fs::path tilesetPath = fs::path(output) / "tileset.json";
    if (!fs::exists(tilesetPath)) {
        LOG_E("Failed to generate tileset.json at %s", tilesetPath.string().c_str());
        return nullptr;
    }

    std::ifstream t(tilesetPath);
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string jsonStr = buffer.str();

    // Parse json to get bounding box
    try {
        json root = json::parse(jsonStr);
        auto& box = root["root"]["boundingVolume"]["box"];
        if (box.is_array() && box.size() == 12) {
            double cx = box[0];
            double cy = box[1];
            double cz = box[2];
            double hx = box[3];
            double hy = box[7];
            double hz = box[11];

            double max[3] = {cx + hx, cy + hy, cz + hz};
            double min[3] = {cx - hx, cy - hy, cz - hz};

            memcpy(box_ptr, max, 3 * sizeof(double));
            memcpy(box_ptr + 3, min, 3 * sizeof(double));
        }
    } catch (const std::exception& e) {
        LOG_E("Failed to parse tileset.json: %s", e.what());
    }

    void* str = malloc(jsonStr.length() + 1);
    if (str) {
        memcpy(str, jsonStr.c_str(), jsonStr.length());
        ((char*)str)[jsonStr.length()] = '\0';
        *len = (int)jsonStr.length();
    }

    return str;
}
