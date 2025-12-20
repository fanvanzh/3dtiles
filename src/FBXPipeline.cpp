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
#include <typeinfo>
#include <osg/GL>
#include <cmath>

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
    {
        auto stats = loader->getStats();
        LOG_I("Material dedup: created=%d reused_by_hash=%d pointer_hits=%d unique_statesets=%zu",
              stats.material_created, stats.material_hash_reused, stats.material_ptr_reused, stats.unique_statesets);
        LOG_I("Mesh dedup: geometries_created=%d reused_by_hash=%d mesh_cache_hit_count=%d unique_geometries=%zu",
              stats.geometry_created, stats.geometry_hash_reused, stats.mesh_cache_hit_count, stats.unique_geometries);
    }

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

    // --- 1. Pre-pass: Detect Outliers ---
    osg::Vec3d centroid(0,0,0);
    size_t totalInstanceCount = 0;

    // Track Extrema for Debugging
    struct Extrema { double val; std::string name; osg::Vec3d pos; };
    Extrema minX = {DBL_MAX, "", osg::Vec3d()};
    Extrema maxX = {-DBL_MAX, "", osg::Vec3d()};
    Extrema minY = {DBL_MAX, "", osg::Vec3d()};
    Extrema maxY = {-DBL_MAX, "", osg::Vec3d()};
    Extrema minZ = {DBL_MAX, "", osg::Vec3d()};
    Extrema maxZ = {-DBL_MAX, "", osg::Vec3d()};

    struct VolumeInfo {
        std::string name;
        double volume;
        double dx, dy, dz;
        osg::Vec3d center;
        osg::Vec3d minPt, maxPt;
    };
    std::vector<VolumeInfo> volumeStats;

    LOG_I("--- Analyzing All Processed Nodes (Sorted by Volume) ---");
    {
        osg::Vec3d sumPos(0,0,0);
        for (auto& pair : loader->meshPool) {
            MeshInstanceInfo& info = pair.second;
            if (!info.geometry) continue;
            osg::BoundingBox geomBox = info.geometry->getBoundingBox();
            for (size_t i = 0; i < info.transforms.size(); ++i) {
                osg::Vec3d center = geomBox.center() * info.transforms[i];
                sumPos += center;
                totalInstanceCount++;

                // Track Extrema
                if (center.x() < minX.val) minX = {center.x(), info.nodeNames[i], center};
                if (center.x() > maxX.val) maxX = {center.x(), info.nodeNames[i], center};
                if (center.y() < minY.val) minY = {center.y(), info.nodeNames[i], center};
                if (center.y() > maxY.val) maxY = {center.y(), info.nodeNames[i], center};
                if (center.z() < minZ.val) minZ = {center.z(), info.nodeNames[i], center};
                if (center.z() > maxZ.val) maxZ = {center.z(), info.nodeNames[i], center};

                // Calculate World AABB
                osg::BoundingBox worldBox;
                for(int k=0; k<8; ++k) {
                    worldBox.expandBy(geomBox.corner(k) * info.transforms[i]);
                }
                double dx = worldBox.xMax() - worldBox.xMin();
                double dy = worldBox.yMax() - worldBox.yMin();
                double dz = worldBox.zMax() - worldBox.zMin();
                double vol = dx * dy * dz;

                volumeStats.push_back({
                    (i < info.nodeNames.size()) ? info.nodeNames[i] : "unknown",
                    vol, dx, dy, dz, center,
                    osg::Vec3d(worldBox.xMin(), worldBox.yMin(), worldBox.zMin()),
                    osg::Vec3d(worldBox.xMax(), worldBox.yMax(), worldBox.zMax())
                });
            }
        }

        // Sort by Volume Descending
        std::sort(volumeStats.begin(), volumeStats.end(), [](const VolumeInfo& a, const VolumeInfo& b){
            return a.volume > b.volume;
        });

        // Log
        for(const auto& v : volumeStats) {
            LOG_I("Node: '%s' Vol=%.3f Dim=(%.2f, %.2f, %.2f) Center=(%.2f, %.2f, %.2f) Min=(%.2f, %.2f, %.2f) Max=(%.2f, %.2f, %.2f)",
                  v.name.c_str(), v.volume, v.dx, v.dy, v.dz,
                  v.center.x(), v.center.y(), v.center.z(),
                  v.minPt.x(), v.minPt.y(), v.minPt.z(),
                  v.maxPt.x(), v.maxPt.y(), v.maxPt.z());
        }

        if (totalInstanceCount > 0) {
            centroid = sumPos / (double)totalInstanceCount;
        }
    }

    LOG_I("--- Scene Extrema Analysis ---");
    LOG_I("Min X: '%s' at %.3f", minX.name.c_str(), minX.val);
    LOG_I("Max X: '%s' at %.3f", maxX.name.c_str(), maxX.val);
    LOG_I("Min Y: '%s' at %.3f", minY.name.c_str(), minY.val);
    LOG_I("Max Y: '%s' at %.3f", maxY.name.c_str(), maxY.val);
    LOG_I("Min Z: '%s' at %.3f", minZ.name.c_str(), minZ.val);
    LOG_I("Max Z: '%s' at %.3f", maxZ.name.c_str(), maxZ.val);
    LOG_I("Total Extent: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]",
          minX.val, maxX.val, minY.val, maxY.val, minZ.val, maxZ.val);

    // Calculate distance statistics
    double maxDist = 0.0;
    double sumDist = 0.0;
    if (totalInstanceCount > 0) {
        for (auto& pair : loader->meshPool) {
            MeshInstanceInfo& info = pair.second;
            if (!info.geometry) continue;
            osg::BoundingBox geomBox = info.geometry->getBoundingBox();
            for (size_t i = 0; i < info.transforms.size(); ++i) {
                double d = (geomBox.center() * info.transforms[i] - centroid).length();
                if (d > maxDist) maxDist = d;
                sumDist += d;
            }
        }
    }
    double avgDist = (totalInstanceCount > 0) ? (sumDist / totalInstanceCount) : 0.0;

    // Threshold: Only filter if we have very far objects (> 10km) AND they are far from average
    // Adjust this logic as needed.
    double outlierThreshold = std::max(10000.0, avgDist * 20.0);
    bool hasOutliers = (maxDist > outlierThreshold);

    LOG_I("Scene Analysis: Count=%zu Centroid=(%.2f, %.2f, %.2f)", totalInstanceCount, centroid.x(), centroid.y(), centroid.z());
    LOG_I("Distance Stats: Avg=%.2f Max=%.2f Threshold=%.2f", avgDist, maxDist, outlierThreshold);

    // --- 2. Main Pass: Build Root Node & Filter ---
    osg::BoundingBox globalBounds;
    size_t skippedCount = 0;

    for (auto& pair : loader->meshPool) {
        MeshInstanceInfo& info = pair.second;
        if (!info.geometry) continue;

        osg::BoundingBox geomBox = info.geometry->getBoundingBox();
        for (size_t i = 0; i < info.transforms.size(); ++i) {
            const auto& mat = info.transforms[i];

            // Outlier Check
            if (hasOutliers) {
                osg::Vec3d instCenter = geomBox.center() * mat;
                double d = (instCenter - centroid).length();
                if (d > outlierThreshold) {
                    std::string name = (i < info.nodeNames.size()) ? info.nodeNames[i] : "unknown";
                    LOG_W("Filtering Outlier: '%s' Dist=%.2f Pos=(%.2f, %.2f, %.2f)",
                          name.c_str(), d, instCenter.x(), instCenter.y(), instCenter.z());
                    skippedCount++;
                    continue; // SKIP this instance
                }
            }

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

    if (skippedCount > 0) {
        LOG_I("Filtered %zu outlier instances.", skippedCount);
    }
    rootNode->bbox = globalBounds;

    // --- End of Filtering ---

    json rootJson;
    if (settings.splitAverageByCount) {
        LOG_I("Using average count split tiling...");
        rootJson = buildAverageTiles(globalBounds, settings.outputPath);
    } else {
        LOG_I("Building Octree...");
        buildOctree(rootNode);
        LOG_I("Processing Nodes and Generating Tiles...");
        rootJson = processNode(rootNode, settings.outputPath, -1, -1, "0");
    }

    LOG_I("--- Generated Tile Bounding Boxes (Sorted by Volume) ---");
    std::sort(tileStats.begin(), tileStats.end(), [](const TileInfo& a, const TileInfo& b){
        return a.volume > b.volume;
    });

    for(const auto& t : tileStats) {
        LOG_I("Tile: '%s' Depth=%d Vol=%.3f Dim=(%.2f, %.2f, %.2f) Center=(%.2f, %.2f, %.2f) Min=(%.2f, %.2f, %.2f) Max=(%.2f, %.2f, %.2f)",
              t.name.c_str(), t.depth, t.volume, t.dx, t.dy, t.dz,
              t.center.x(), t.center.y(), t.center.z(),
              t.minPt.x(), t.minPt.y(), t.minPt.z(),
              t.maxPt.x(), t.maxPt.y(), t.maxPt.z());
    }

    LOG_I("Writing tileset.json...");
    writeTilesetJson(settings.outputPath, globalBounds, rootJson);

    LOG_I("FBXPipeline Finished.");
    logLevelStats();
    {
        auto stats = loader->getStats();
        LOG_I("Material dedup: created=%d reused_by_hash=%d pointer_hits=%d unique_statesets=%zu",
              stats.material_created, stats.material_hash_reused, stats.material_ptr_reused, stats.unique_statesets);
        LOG_I("Mesh dedup: geometries_created=%d reused_by_hash=%d mesh_cache_hit_count=%d unique_geometries=%zu",
              stats.geometry_created, stats.geometry_hash_reused, stats.mesh_cache_hit_count, stats.unique_geometries);
    }
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

struct TileStats { size_t node_count = 0; size_t vertex_count = 0; size_t triangle_count = 0; size_t material_count = 0; };
void appendGeometryToModel(tinygltf::Model& model, const std::vector<InstanceRef>& instances, const PipelineSettings& settings, json* batchTableJson, int* batchIdCounter, const SimplificationParams& simParams, osg::BoundingBoxd* outBox = nullptr, TileStats* stats = nullptr, const char* dbgTileName = nullptr, osg::Vec3d rtcOffset = osg::Vec3d(0,0,0)) {
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
    if (stats) {
        stats->node_count = instances.size();
        stats->material_count = materialGroups.size();
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
        int triSets = 0, stripSets = 0, fanSets = 0, otherSets = 0, drawArraysSets = 0;
        int missingVertexInstances = 0;

        for (const auto& inst : pair.second) {
            osg::ref_ptr<osg::Geometry> processedGeom = inst.geom;
            if (simParams.enable_simplification) {
                processedGeom = (osg::Geometry*)inst.geom->clone(osg::CopyOp::DEEP_COPY_ALL);
                simplify_mesh_geometry(processedGeom.get(), simParams);
            }

            osg::Matrixd normalXform;
            normalXform.transpose(osg::Matrix::inverse(inst.matrix));

            uint32_t baseIndex = (uint32_t)(positions.size() / 3);
            osg::Array* va = processedGeom->getVertexArray();
            osg::Vec3Array* v = dynamic_cast<osg::Vec3Array*>(va);
            osg::Vec3dArray* v3d = dynamic_cast<osg::Vec3dArray*>(va);
            osg::Vec4Array* v4 = dynamic_cast<osg::Vec4Array*>(va);
            osg::Vec4dArray* v4d = dynamic_cast<osg::Vec4dArray*>(va);
            osg::Array* na = processedGeom->getNormalArray();
            osg::Vec3Array* n = dynamic_cast<osg::Vec3Array*>(na);
            osg::Vec3dArray* n3d = dynamic_cast<osg::Vec3dArray*>(na);
            osg::Array* ta = processedGeom->getTexCoordArray(0);
            osg::Vec2Array* t = dynamic_cast<osg::Vec2Array*>(ta);
            osg::Vec2dArray* t2d = dynamic_cast<osg::Vec2dArray*>(ta);

            bool handledVertices = false;
            if ((!v || v->empty()) && (!v3d || v3d->empty()) && (!v4 || v4->empty()) && (!v4d || v4d->empty())) {
                if (va && va->getNumElements() > 0) {
                    GLenum dt = va->getDataType();
                    unsigned int cnt = va->getNumElements();
                    unsigned int totalBytes = va->getTotalDataSize();
                    if (dt == GL_FLOAT || dt == GL_DOUBLE) {
                        unsigned int comps = dt == GL_FLOAT ? totalBytes / (cnt * (unsigned int)sizeof(float)) : totalBytes / (cnt * (unsigned int)sizeof(double));
                        if (comps >= 3) {
                            if (dt == GL_FLOAT) {
                                const float* ptr = static_cast<const float*>(va->getDataPointer());
                                for (unsigned int i = 0; i < cnt; ++i) {
                                    osg::Vec3d p((double)ptr[i*comps+0], (double)ptr[i*comps+1], (double)ptr[i*comps+2]);
                                    p = p * inst.matrix;
                                    p = p - rtcOffset;
                                    float px = (float)p.x();
                                    float py = (float)-p.z();
                                    float pz = (float)p.y();
                                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                                    if (px < minPos[0]) minPos[0] = px;
                                    if (py < minPos[1]) minPos[1] = py;
                                    if (pz < minPos[2]) minPos[2] = pz;
                                    if (px > maxPos[0]) maxPos[0] = px;
                                    if (py > maxPos[1]) maxPos[1] = py;
                                    if (pz > maxPos[2]) maxPos[2] = pz;
                                    if (outBox) outBox->expandBy(osg::Vec3d(px, py, pz));
                                    if (n && i < n->size()) {
                                        osg::Vec3 nm = (*n)[i];
                                        osg::Vec3d nmd(nm.x(), nm.y(), nm.z());
                                        nmd = osg::Matrix::transform3x3(normalXform, nmd); nmd.normalize();
                                        normals.push_back((float)nmd.x()); normals.push_back((float)-nmd.z()); normals.push_back((float)nmd.y());
                                    } else if (n3d && i < n3d->size()) {
                                        osg::Vec3d nmd = (*n3d)[i];
                                        nmd = osg::Matrix::transform3x3(normalXform, nmd); nmd.normalize();
                                        normals.push_back((float)nmd.x()); normals.push_back((float)-nmd.z()); normals.push_back((float)nmd.y());
                                    } else if (na && na->getNumElements() > i) {
                                        GLenum ndt = na->getDataType();
                                        unsigned int ncnt = na->getNumElements();
                                        unsigned int nbytes = na->getTotalDataSize();
                                        unsigned int ncomps = ndt == GL_FLOAT ? nbytes / (ncnt * (unsigned int)sizeof(float)) : ndt == GL_DOUBLE ? nbytes / (ncnt * (unsigned int)sizeof(double)) : 0;
                                        if (ncomps >= 3) {
                                            if (ndt == GL_FLOAT) {
                                                const float* nptr = static_cast<const float*>(na->getDataPointer());
                                                osg::Vec3d nm2((double)nptr[i*ncomps+0], (double)nptr[i*ncomps+1], (double)nptr[i*ncomps+2]);
                                                nm2 = osg::Matrix::transform3x3(normalXform, nm2); nm2.normalize();
                                                normals.push_back((float)nm2.x()); normals.push_back((float)-nm2.z()); normals.push_back((float)nm2.y());
                                            } else if (ndt == GL_DOUBLE) {
                                                const double* nptr = static_cast<const double*>(na->getDataPointer());
                                                osg::Vec3d nm2(nptr[i*ncomps+0], nptr[i*ncomps+1], nptr[i*ncomps+2]);
                                                nm2 = osg::Matrix::transform3x3(normalXform, nm2); nm2.normalize();
                                                normals.push_back((float)nm2.x()); normals.push_back((float)-nm2.z()); normals.push_back((float)nm2.y());
                                            } else {
                                                normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                            }
                                        } else {
                                            normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                        }
                                    } else {
                                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                    }
                                    if (t && i < t->size()) {
                                        texcoords.push_back((float)(*t)[i].x());
                                        texcoords.push_back((float)(*t)[i].y());
                                    } else if (t2d && i < t2d->size()) {
                                        texcoords.push_back((float)(*t2d)[i].x());
                                        texcoords.push_back((float)(*t2d)[i].y());
                                    } else if (ta && ta->getNumElements() > i) {
                                        GLenum tdt = ta->getDataType();
                                        unsigned int tcnt = ta->getNumElements();
                                        unsigned int tbytes = ta->getTotalDataSize();
                                        unsigned int tcomps = tdt == GL_FLOAT ? tbytes / (tcnt * (unsigned int)sizeof(float)) : tdt == GL_DOUBLE ? tbytes / (tcnt * (unsigned int)sizeof(double)) : 0;
                                        if (tcomps >= 2) {
                                            if (tdt == GL_FLOAT) {
                                                const float* tptr = static_cast<const float*>(ta->getDataPointer());
                                                texcoords.push_back((float)tptr[i*tcomps+0]);
                                                texcoords.push_back((float)tptr[i*tcomps+1]);
                                            } else if (tdt == GL_DOUBLE) {
                                                const double* tptr = static_cast<const double*>(ta->getDataPointer());
                                                texcoords.push_back((float)tptr[i*tcomps+0]);
                                                texcoords.push_back((float)tptr[i*tcomps+1]);
                                            } else {
                                                texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                            }
                                        } else {
                                            texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                        }
                                    } else {
                                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                    }
                                    batchIds.push_back((float)inst.originalBatchId);
                                }
                            } else {
                                const double* ptr = static_cast<const double*>(va->getDataPointer());
                                for (unsigned int i = 0; i < cnt; ++i) {
                                    osg::Vec3d p(ptr[i*comps+0], ptr[i*comps+1], ptr[i*comps+2]);
                                    p = p * inst.matrix;
                                    p = p - rtcOffset;
                                    float px = (float)p.x();
                                    float py = (float)-p.z();
                                    float pz = (float)p.y();
                                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                                    if (px < minPos[0]) minPos[0] = px;
                                    if (py < minPos[1]) minPos[1] = py;
                                    if (pz < minPos[2]) minPos[2] = pz;
                                    if (px > maxPos[0]) maxPos[0] = px;
                                    if (py > maxPos[1]) maxPos[1] = py;
                                    if (pz > maxPos[2]) maxPos[2] = pz;
                                    if (outBox) outBox->expandBy(osg::Vec3d(px, py, pz));
                                    if (n && i < n->size()) {
                                        osg::Vec3 nm = (*n)[i];
                                        osg::Vec3d nmd(nm.x(), nm.y(), nm.z());
                                        nmd = osg::Matrix::transform3x3(normalXform, nmd); nmd.normalize();
                                        normals.push_back((float)nmd.x()); normals.push_back((float)-nmd.z()); normals.push_back((float)nmd.y());
                                    } else if (n3d && i < n3d->size()) {
                                        osg::Vec3d nmd = (*n3d)[i];
                                        nmd = osg::Matrix::transform3x3(normalXform, nmd); nmd.normalize();
                                        normals.push_back((float)nmd.x()); normals.push_back((float)-nmd.z()); normals.push_back((float)nmd.y());
                                    } else if (na && na->getNumElements() > i) {
                                        GLenum ndt = na->getDataType();
                                        unsigned int ncnt = na->getNumElements();
                                        unsigned int nbytes = na->getTotalDataSize();
                                        unsigned int ncomps = ndt == GL_FLOAT ? nbytes / (ncnt * (unsigned int)sizeof(float)) : ndt == GL_DOUBLE ? nbytes / (ncnt * (unsigned int)sizeof(double)) : 0;
                                        if (ncomps >= 3) {
                                            if (ndt == GL_FLOAT) {
                                                const float* nptr = static_cast<const float*>(na->getDataPointer());
                                                osg::Vec3d nm2((double)nptr[i*ncomps+0], (double)nptr[i*ncomps+1], (double)nptr[i*ncomps+2]);
                                                nm2 = osg::Matrix::transform3x3(normalXform, nm2); nm2.normalize();
                                                normals.push_back((float)nm2.x()); normals.push_back((float)-nm2.z()); normals.push_back((float)nm2.y());
                                            } else if (ndt == GL_DOUBLE) {
                                                const double* nptr = static_cast<const double*>(na->getDataPointer());
                                                osg::Vec3d nm2(nptr[i*ncomps+0], nptr[i*ncomps+1], nptr[i*ncomps+2]);
                                                nm2 = osg::Matrix::transform3x3(normalXform, nm2); nm2.normalize();
                                                normals.push_back((float)nm2.x()); normals.push_back((float)-nm2.z()); normals.push_back((float)nm2.y());
                                            } else {
                                                normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                            }
                                        } else {
                                            normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                        }
                                    } else {
                                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                                    }
                                    if (t && i < t->size()) {
                                        texcoords.push_back((float)(*t)[i].x());
                                        texcoords.push_back((float)(*t)[i].y());
                                    } else if (t2d && i < t2d->size()) {
                                        texcoords.push_back((float)(*t2d)[i].x());
                                        texcoords.push_back((float)(*t2d)[i].y());
                                    } else if (ta && ta->getNumElements() > i) {
                                        GLenum tdt = ta->getDataType();
                                        unsigned int tcnt = ta->getNumElements();
                                        unsigned int tbytes = ta->getTotalDataSize();
                                        unsigned int tcomps = tdt == GL_FLOAT ? tbytes / (tcnt * (unsigned int)sizeof(float)) : tdt == GL_DOUBLE ? tbytes / (tcnt * (unsigned int)sizeof(double)) : 0;
                                        if (tcomps >= 2) {
                                            if (tdt == GL_FLOAT) {
                                                const float* tptr = static_cast<const float*>(ta->getDataPointer());
                                                texcoords.push_back((float)tptr[i*tcomps+0]);
                                                texcoords.push_back((float)tptr[i*tcomps+1]);
                                            } else if (tdt == GL_DOUBLE) {
                                                const double* tptr = static_cast<const double*>(ta->getDataPointer());
                                                texcoords.push_back((float)tptr[i*tcomps+0]);
                                                texcoords.push_back((float)tptr[i*tcomps+1]);
                                            } else {
                                                texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                            }
                                        } else {
                                            texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                        }
                                    } else {
                                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                                    }
                                    batchIds.push_back((float)inst.originalBatchId);
                                }
                            }
                            handledVertices = true;
                        }
                    }
                }
                if (!handledVertices) {
                    missingVertexInstances++;
                    if (dbgTileName) {
                        if (!va) {
                            LOG_I("Tile %s: missing vertex array (null)", dbgTileName);
                        } else if (va->getNumElements() == 0) {
                            LOG_I("Tile %s: empty vertex array (0 elements), type: %s", dbgTileName, typeid(*va).name());
                        } else {
                            LOG_I("Tile %s: unsupported vertex array type: %s", dbgTileName, typeid(*va).name());
                        }
                    }
                    continue;
                }
            }

            if (v && !v->empty()) {
                for (unsigned int i = 0; i < v->size(); ++i) {
                    osg::Vec3 vf = (*v)[i];
                    osg::Vec3d p(vf.x(), vf.y(), vf.z());
                    p = p * inst.matrix;
                    p = p - rtcOffset;
                    float px = (float)p.x();
                    float py = (float)-p.z();
                    float pz = (float)p.y();
                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                    if (px < minPos[0]) minPos[0] = px;
                    if (py < minPos[1]) minPos[1] = py;
                    if (pz < minPos[2]) minPos[2] = pz;
                    if (px > maxPos[0]) maxPos[0] = px;
                    if (py > maxPos[1]) maxPos[1] = py;
                    if (pz > maxPos[2]) maxPos[2] = pz;
                    if (outBox) outBox->expandBy(osg::Vec3d(px, py, pz));
                    if (n && i < n->size()) {
                        osg::Vec3 nmf = (*n)[i];
                        osg::Vec3d nm(nmf.x(), nmf.y(), nmf.z());
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else if (n3d && i < n3d->size()) {
                        osg::Vec3d nm = (*n3d)[i];
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else {
                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                    }
                    if (t && i < t->size()) {
                        texcoords.push_back((float)(*t)[i].x());
                        texcoords.push_back((float)(*t)[i].y());
                    } else if (t2d && i < t2d->size()) {
                        texcoords.push_back((float)(*t2d)[i].x());
                        texcoords.push_back((float)(*t2d)[i].y());
                    } else {
                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                    }
                    batchIds.push_back((float)inst.originalBatchId);
                }
            } else if (v3d && !v3d->empty()) {
                for (unsigned int i = 0; i < v3d->size(); ++i) {
                    osg::Vec3d p = (*v3d)[i];
                    p = p * inst.matrix;
                    p = p - rtcOffset;
                    float px = (float)p.x();
                    float py = (float)-p.z();
                    float pz = (float)p.y();
                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                    if (px < minPos[0]) minPos[0] = px;
                    if (py < minPos[1]) minPos[1] = py;
                    if (pz < minPos[2]) minPos[2] = pz;
                    if (px > maxPos[0]) maxPos[0] = px;
                    if (py > maxPos[1]) maxPos[1] = py;
                    if (pz > maxPos[2]) maxPos[2] = pz;
                    if (outBox) outBox->expandBy(osg::Vec3d(px, py, pz));
                    if (n3d && i < n3d->size()) {
                        osg::Vec3d nm = (*n3d)[i];
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else if (n && i < n->size()) {
                        osg::Vec3 nmf = (*n)[i];
                        osg::Vec3d nm(nmf.x(), nmf.y(), nmf.z());
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else {
                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                    }
                    if (t2d && i < t2d->size()) {
                        texcoords.push_back((float)(*t2d)[i].x());
                        texcoords.push_back((float)(*t2d)[i].y());
                    } else if (t && i < t->size()) {
                        texcoords.push_back((float)(*t)[i].x());
                        texcoords.push_back((float)(*t)[i].y());
                    } else {
                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                    }
                    batchIds.push_back((float)inst.originalBatchId);
                }
            } else if (v4 && !v4->empty()) {
                for (unsigned int i = 0; i < v4->size(); ++i) {
                    osg::Vec4 vf = (*v4)[i];
                    osg::Vec3d p(vf.x(), vf.y(), vf.z());
                    p = p * inst.matrix;
                    p = p - rtcOffset;
                    double gx = p.x();
                    double gy = -p.z();
                    double gz = p.y();
                    float px = (float)gx;
                    float py = (float)gy;
                    float pz = (float)gz;
                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                    if (px < minPos[0]) minPos[0] = px;
                    if (py < minPos[1]) minPos[1] = py;
                    if (pz < minPos[2]) minPos[2] = pz;
                    if (px > maxPos[0]) maxPos[0] = px;
                    if (py > maxPos[1]) maxPos[1] = py;
                    if (pz > maxPos[2]) maxPos[2] = pz;
                    if (outBox) outBox->expandBy(osg::Vec3d(gx, gy, gz));
                    if (n && i < n->size()) {
                        osg::Vec3 nmf = (*n)[i];
                        osg::Vec3d nm(nmf.x(), nmf.y(), nmf.z());
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else if (n3d && i < n3d->size()) {
                        osg::Vec3d nm = (*n3d)[i];
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else {
                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                    }
                    if (t && i < t->size()) {
                        texcoords.push_back((float)(*t)[i].x());
                        texcoords.push_back((float)(*t)[i].y());
                    } else if (t2d && i < t2d->size()) {
                        texcoords.push_back((float)(*t2d)[i].x());
                        texcoords.push_back((float)(*t2d)[i].y());
                    } else {
                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                    }
                    batchIds.push_back((float)inst.originalBatchId);
                }
            } else if (v4d && !v4d->empty()) {
                for (unsigned int i = 0; i < v4d->size(); ++i) {
                    osg::Vec4d vd = (*v4d)[i];
                    osg::Vec3d p(vd.x(), vd.y(), vd.z());
                    p = p * inst.matrix;
                    p = p - rtcOffset;
                    double gx = p.x();
                    double gy = -p.z();
                    double gz = p.y();
                    float px = (float)gx;
                    float py = (float)gy;
                    float pz = (float)gz;
                    positions.push_back(px); positions.push_back(py); positions.push_back(pz);
                    if (px < minPos[0]) minPos[0] = px;
                    if (py < minPos[1]) minPos[1] = py;
                    if (pz < minPos[2]) minPos[2] = pz;
                    if (px > maxPos[0]) maxPos[0] = px;
                    if (py > maxPos[1]) maxPos[1] = py;
                    if (pz > maxPos[2]) maxPos[2] = pz;
                    if (outBox) outBox->expandBy(osg::Vec3d(gx, gy, gz));
                    if (n3d && i < n3d->size()) {
                        osg::Vec3d nm = (*n3d)[i];
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else if (n && i < n->size()) {
                        osg::Vec3 nmf = (*n)[i];
                        osg::Vec3d nm(nmf.x(), nmf.y(), nmf.z());
                        nm = osg::Matrix::transform3x3(normalXform, nm); nm.normalize();
                        normals.push_back((float)nm.x()); normals.push_back((float)-nm.z()); normals.push_back((float)nm.y());
                    } else {
                        normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
                    }
                    if (t2d && i < t2d->size()) {
                        texcoords.push_back((float)(*t2d)[i].x());
                        texcoords.push_back((float)(*t2d)[i].y());
                    } else if (t && i < t->size()) {
                        texcoords.push_back((float)(*t)[i].x());
                        texcoords.push_back((float)(*t)[i].y());
                    } else {
                        texcoords.push_back(0.0f); texcoords.push_back(0.0f);
                    }
                    batchIds.push_back((float)inst.originalBatchId);
                }
            }

        // Indices
        for (unsigned int k = 0; k < processedGeom->getNumPrimitiveSets(); ++k) {
            osg::PrimitiveSet* ps = processedGeom->getPrimitiveSet(k);
            osg::PrimitiveSet::Mode mode = (osg::PrimitiveSet::Mode)ps->getMode();
            if (mode == osg::PrimitiveSet::TRIANGLES) { triSets++; }
            else if (mode == osg::PrimitiveSet::TRIANGLE_STRIP) { stripSets++; }
            else if (mode == osg::PrimitiveSet::TRIANGLE_FAN) { fanSets++; }
            else { otherSets++; continue; }

            const osg::DrawElementsUShort* deus = dynamic_cast<const osg::DrawElementsUShort*>(ps);
            const osg::DrawElementsUInt* deui = dynamic_cast<const osg::DrawElementsUInt*>(ps);
            const osg::DrawArrays* da = dynamic_cast<const osg::DrawArrays*>(ps);
            if (da) {
                drawArraysSets++;
                if (mode == osg::PrimitiveSet::TRIANGLES) {
                    unsigned int first = da->getFirst();
                    unsigned int count = da->getCount();
                    for (unsigned int idx = 0; idx + 2 < count; idx += 3) {
                        indices.push_back(baseIndex + first + idx);
                        indices.push_back(baseIndex + first + idx + 1);
                        indices.push_back(baseIndex + first + idx + 2);
                    }
                } else if (mode == osg::PrimitiveSet::TRIANGLE_STRIP) {
                    unsigned int first = da->getFirst();
                    unsigned int count = da->getCount();
                    for (unsigned int i = 0; i + 2 < count; ++i) {
                        unsigned int a = baseIndex + first + i;
                        unsigned int b = baseIndex + first + i + 1;
                        unsigned int c = baseIndex + first + i + 2;
                        if ((i & 1) == 0) { indices.push_back(a); indices.push_back(b); indices.push_back(c); }
                        else { indices.push_back(b); indices.push_back(a); indices.push_back(c); }
                    }
                } else if (mode == osg::PrimitiveSet::TRIANGLE_FAN) {
                    unsigned int first = da->getFirst();
                    unsigned int count = da->getCount();
                    unsigned int center = baseIndex + first;
                    for (unsigned int i = 1; i + 1 < count; ++i) {
                        indices.push_back(center);
                        indices.push_back(baseIndex + first + i);
                        indices.push_back(baseIndex + first + i + 1);
                    }
                }
            }

            if (deus) {
                if (mode == osg::PrimitiveSet::TRIANGLES) {
                    for (unsigned int idx = 0; idx < deus->size(); ++idx) indices.push_back(baseIndex + (*deus)[idx]);
                } else if (mode == osg::PrimitiveSet::TRIANGLE_STRIP) {
                    if (deus->size() >= 3) {
                        for (unsigned int i = 0; i + 2 < deus->size(); ++i) {
                            unsigned int a = baseIndex + (*deus)[i];
                            unsigned int b = baseIndex + (*deus)[i + 1];
                            unsigned int c = baseIndex + (*deus)[i + 2];
                            if ((i & 1) == 0) { indices.push_back(a); indices.push_back(b); indices.push_back(c); }
                            else { indices.push_back(b); indices.push_back(a); indices.push_back(c); }
                        }
                    }
                } else if (mode == osg::PrimitiveSet::TRIANGLE_FAN) {
                    if (deus->size() >= 3) {
                        unsigned int center = baseIndex + (*deus)[0];
                        for (unsigned int i = 1; i + 1 < deus->size(); ++i) {
                            indices.push_back(center);
                            indices.push_back(baseIndex + (*deus)[i]);
                            indices.push_back(baseIndex + (*deus)[i + 1]);
                        }
                    }
                }
            } else if (deui) {
                if (mode == osg::PrimitiveSet::TRIANGLES) {
                    for (unsigned int idx = 0; idx < deui->size(); ++idx) indices.push_back(baseIndex + (*deui)[idx]);
                } else if (mode == osg::PrimitiveSet::TRIANGLE_STRIP) {
                    if (deui->size() >= 3) {
                        for (unsigned int i = 0; i + 2 < deui->size(); ++i) {
                            unsigned int a = baseIndex + (*deui)[i];
                            unsigned int b = baseIndex + (*deui)[i + 1];
                            unsigned int c = baseIndex + (*deui)[i + 2];
                            if ((i & 1) == 0) { indices.push_back(a); indices.push_back(b); indices.push_back(c); }
                            else { indices.push_back(b); indices.push_back(a); indices.push_back(c); }
                        }
                    }
                } else if (mode == osg::PrimitiveSet::TRIANGLE_FAN) {
                    if (deui->size() >= 3) {
                        unsigned int center = baseIndex + (*deui)[0];
                        for (unsigned int i = 1; i + 1 < deui->size(); ++i) {
                            indices.push_back(center);
                            indices.push_back(baseIndex + (*deui)[i]);
                            indices.push_back(baseIndex + (*deui)[i + 1]);
                        }
                    }
                }
            }
        }
        }

        if (positions.empty() || indices.empty()) {
            if (dbgTileName) {
                LOG_I("Tile %s: group produced no triangles: v=%zu i=%zu tri=%d strip=%d fan=%d other=%d missVtxInst=%d drawArrays=%d",
                      dbgTileName,
                      positions.size() / 3,
                      indices.size() / 3,
                      triSets, stripSets, fanSets, otherSets,
                      missingVertexInstances, drawArraysSets);
            }
            continue;
        }
        if (stats) {
            stats->vertex_count += positions.size() / 3;
            stats->triangle_count += indices.size() / 3;
        }

        // Prepare Draco compression if enabled
        bool dracoCompressed = false;
        int dracoBufferViewIdx = -1;
        int dracoPosId = -1, dracoNormId = -1, dracoTexId = -1, dracoBatchId = -1;

        if (settings.enableDraco) {
            osg::ref_ptr<osg::Geometry> tempGeom = new osg::Geometry;
            osg::ref_ptr<osg::Vec3Array> va = new osg::Vec3Array;
            for(size_t i=0; i<positions.size(); i+=3) va->push_back(osg::Vec3(positions[i], positions[i+1], positions[i+2]));
            tempGeom->setVertexArray(va);

            if(!normals.empty()) {
                osg::ref_ptr<osg::Vec3Array> na = new osg::Vec3Array;
                for(size_t i=0; i<normals.size(); i+=3) na->push_back(osg::Vec3(normals[i], normals[i+1], normals[i+2]));
                tempGeom->setNormalArray(na);
                tempGeom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
            }

            if(!texcoords.empty()) {
                osg::ref_ptr<osg::Vec2Array> ta = new osg::Vec2Array;
                for(size_t i=0; i<texcoords.size(); i+=2) ta->push_back(osg::Vec2(texcoords[i], texcoords[i+1]));
                tempGeom->setTexCoordArray(0, ta);
            }

            osg::ref_ptr<osg::DrawElementsUInt> de = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
            for(unsigned int idx : indices) de->push_back(idx);
            tempGeom->addPrimitiveSet(de);

            DracoCompressionParams dracoParams;
            dracoParams.enable_compression = true;
            std::vector<unsigned char> compressedData;
            size_t compressedSize = 0;

            if (compress_mesh_geometry(tempGeom.get(), dracoParams, compressedData, compressedSize, &dracoPosId, &dracoNormId, &dracoTexId, &dracoBatchId, &batchIds)) {
                 size_t bufOffset = buffer.data.size();
                 size_t padding = (4 - (bufOffset % 4)) % 4;
                 if (padding > 0) {
                     buffer.data.resize(bufOffset + padding);
                     memset(buffer.data.data() + bufOffset, 0, padding);
                     bufOffset += padding;
                 }

                 buffer.data.resize(bufOffset + compressedSize);
                 memcpy(buffer.data.data() + bufOffset, compressedData.data(), compressedSize);

                 tinygltf::BufferView bv;
                 bv.buffer = 0;
                 bv.byteOffset = bufOffset;
                 bv.byteLength = compressedSize;
                 dracoBufferViewIdx = (int)model.bufferViews.size();
                 model.bufferViews.push_back(bv);

                 dracoCompressed = true;

                 // Register extension
                 if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_draco_mesh_compression") == model.extensionsUsed.end()) {
                     model.extensionsUsed.push_back("KHR_draco_mesh_compression");
                     model.extensionsRequired.push_back("KHR_draco_mesh_compression");
                 }
            }
        }

        int bvPosIdx = -1, bvNormIdx = -1, bvTexIdx = -1, bvIndIdx = -1, bvBatchIdx = -1;

        if (!dracoCompressed) {
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
            bvPosIdx = (int)model.bufferViews.size();
            model.bufferViews.push_back(bvPos);

            tinygltf::BufferView bvNorm;
            bvNorm.buffer = 0;
            bvNorm.byteOffset = normOffset;
            bvNorm.byteLength = normLen;
            bvNorm.target = TINYGLTF_TARGET_ARRAY_BUFFER;
            bvNormIdx = (int)model.bufferViews.size();
            model.bufferViews.push_back(bvNorm);

            tinygltf::BufferView bvTex;
            bvTex.buffer = 0;
            bvTex.byteOffset = texOffset;
            bvTex.byteLength = texLen;
            bvTex.target = TINYGLTF_TARGET_ARRAY_BUFFER;
            bvTexIdx = (int)model.bufferViews.size();
            model.bufferViews.push_back(bvTex);

            tinygltf::BufferView bvInd;
            bvInd.buffer = 0;
            bvInd.byteOffset = indOffset;
            bvInd.byteLength = indLen;
            bvInd.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
            bvIndIdx = (int)model.bufferViews.size();
            model.bufferViews.push_back(bvInd);

            tinygltf::BufferView bvBatch;
            bvBatch.buffer = 0;
            bvBatch.byteOffset = batchOffset;
            bvBatch.byteLength = batchLen;
            bvBatch.target = TINYGLTF_TARGET_ARRAY_BUFFER;
            bvBatchIdx = (int)model.bufferViews.size();
            model.bufferViews.push_back(bvBatch);
        }

        // Accessors
        tinygltf::Accessor accPos;
        accPos.bufferView = dracoCompressed ? -1 : bvPosIdx;
        accPos.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accPos.count = positions.size() / 3;
        accPos.type = TINYGLTF_TYPE_VEC3;
        accPos.minValues = {minPos[0], minPos[1], minPos[2]};
        accPos.maxValues = {maxPos[0], maxPos[1], maxPos[2]};
        int accPosIdx = (int)model.accessors.size();
        model.accessors.push_back(accPos);

        tinygltf::Accessor accNorm;
        accNorm.bufferView = dracoCompressed ? -1 : bvNormIdx;
        accNorm.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accNorm.count = normals.size() / 3;
        accNorm.type = TINYGLTF_TYPE_VEC3;
        int accNormIdx = (int)model.accessors.size();
        model.accessors.push_back(accNorm);

        tinygltf::Accessor accTex;
        accTex.bufferView = dracoCompressed ? -1 : bvTexIdx;
        accTex.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accTex.count = texcoords.size() / 2;
        accTex.type = TINYGLTF_TYPE_VEC2;
        int accTexIdx = (int)model.accessors.size();
        model.accessors.push_back(accTex);

        tinygltf::Accessor accInd;
        accInd.bufferView = dracoCompressed ? -1 : bvIndIdx;
        accInd.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        accInd.count = indices.size();
        accInd.type = TINYGLTF_TYPE_SCALAR;
        int accIndIdx = (int)model.accessors.size();
        model.accessors.push_back(accInd);

        tinygltf::Accessor accBatch;
        accBatch.bufferView = dracoCompressed ? -1 : bvBatchIdx;
        accBatch.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        accBatch.count = batchIds.size();
        accBatch.type = TINYGLTF_TYPE_SCALAR;
        int accBatchIdx = (int)model.accessors.size();
        model.accessors.push_back(accBatch);

        // Material
        tinygltf::Material mat;
        mat.name = "Default";
        mat.doubleSided = true; // Fix for potential backface culling issues

        if (settings.enableUnlit) {
            mat.extensions["KHR_materials_unlit"] = tinygltf::Value(tinygltf::Value::Object());
            if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_materials_unlit") == model.extensionsUsed.end()) {
                model.extensionsUsed.push_back("KHR_materials_unlit");
            }
        }

        const osg::StateSet* stateSet = pair.first;
        std::vector<double> baseColor = {1.0, 1.0, 1.0, 1.0};
        double emissiveColor[3] = {0.0, 0.0, 0.0};
        float roughnessFactor = 1.0f;
        float metallicFactor = 0.0f;
        float aoStrength = 1.0f;

        // Check for texture in StateSet
        if (stateSet) {
             // Get Material Color
             const osg::Material* osgMat = dynamic_cast<const osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
             if (osgMat) {
                 osg::Vec4 diffuse = osgMat->getDiffuse(osg::Material::FRONT);
                 baseColor = {diffuse.r(), diffuse.g(), diffuse.b(), diffuse.a()};
                 osg::Vec4 em = osgMat->getEmission(osg::Material::FRONT);
                 emissiveColor[0] = em.r();
                 emissiveColor[1] = em.g();
                 emissiveColor[2] = em.b();
             }
             const osg::Uniform* uR = stateSet->getUniform("roughnessFactor");
             if (uR) uR->get(roughnessFactor);
             const osg::Uniform* uM = stateSet->getUniform("metallicFactor");
             if (uM) uM->get(metallicFactor);
             const osg::Uniform* uAO = stateSet->getUniform("aoStrength");
             if (uAO) uAO->get(aoStrength);

            const osg::Texture* tex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
            if (tex && tex->getNumImages() > 0) {
                const osg::Image* img = tex->getImage(0);
                if (img) {
                    std::string imgPath = img->getFileName();
                    std::vector<unsigned char> imgData;
                    std::string mimeType = "image/png"; // default
                    bool hasData = false;

                    // Try KTX2 compression if enabled
                    if (settings.enableTextureCompress) {
                        std::vector<unsigned char> compressedData;
                        std::string compressedMime;
                        if (process_texture(const_cast<osg::Texture*>(tex), compressedData, compressedMime, true)) {
                            if (compressedMime == "image/ktx2") {
                                imgData = compressedData;
                                mimeType = compressedMime;
                                hasData = true;

                                // Register extension
                                if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_texture_basisu") == model.extensionsUsed.end()) {
                                    model.extensionsUsed.push_back("KHR_texture_basisu");
                                    model.extensionsRequired.push_back("KHR_texture_basisu");
                                }
                            }
                        }
                    }

                    bool hasAlphaTransparency = false;
                    {
                        GLenum pf = img->getPixelFormat();
                        GLenum dt = img->getDataType();
                        int w = img->s();
                        int h = img->t();
                        int channels = 0;
                        if (pf == GL_LUMINANCE) channels = 1;
                        else if (pf == GL_LUMINANCE_ALPHA) channels = 2;
                        else if (pf == GL_RGB) channels = 3;
                        else if (pf == GL_RGBA) channels = 4;
                        if ((channels == 2 || channels == 4) && dt == GL_UNSIGNED_BYTE && img->data() && w > 0 && h > 0) {
                            const unsigned char* p = img->data();
                            int alphaIndex = (channels == 2) ? 1 : 3;
                            int total = w * h;
                            for (int i = 0; i < total; ++i) {
                                if (p[i * channels + alphaIndex] < 255) {
                                    hasAlphaTransparency = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!hasData && !imgPath.empty() && fs::exists(imgPath)) {
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
                          if (mimeType == "image/ktx2") {
                              tinygltf::Value::Object ktxExt;
                              ktxExt["source"] = tinygltf::Value(imgIdx);
                              gltfTex.extensions["KHR_texture_basisu"] = tinygltf::Value(ktxExt);
                          } else {
                              gltfTex.source = imgIdx;
                          }
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
                         if (hasAlphaTransparency) {
                             mat.alphaMode = "BLEND";
                         }
                    }
                }
            }
            const osg::Texture* ntex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(1, osg::StateAttribute::TEXTURE));
            if (ntex && ntex->getNumImages() > 0) {
                const osg::Image* img = ntex->getImage(0);
                if (img) {
                    std::string imgPath = img->getFileName();
                    std::vector<unsigned char> imgData;
                    std::string mimeType = "image/png";
                    bool hasData = false;

                    // Try KTX2 compression if enabled
                    if (settings.enableTextureCompress) {
                        std::vector<unsigned char> compressedData;
                        std::string compressedMime;
                        if (process_texture(const_cast<osg::Texture*>(ntex), compressedData, compressedMime, true)) {
                            if (compressedMime == "image/ktx2") {
                                imgData = compressedData;
                                mimeType = compressedMime;
                                hasData = true;

                                // Register extension
                                if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_texture_basisu") == model.extensionsUsed.end()) {
                                    model.extensionsUsed.push_back("KHR_texture_basisu");
                                    model.extensionsRequired.push_back("KHR_texture_basisu");
                                }
                            }
                        }
                    }

                    if (!hasData && !imgPath.empty() && fs::exists(imgPath)) {
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
                    if (!hasData && img->data() != nullptr) {
                        std::string ext = "png";
                        if (!imgPath.empty()) {
                            std::string e = fs::path(imgPath).extension().string();
                            if (!e.empty() && e.size() > 1) {
                                ext = e.substr(1);
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            }
                        }
                        osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(ext);
                        if (rw) {
                            std::stringstream ss;
                            osgDB::ReaderWriter::WriteResult wr = rw->writeImage(*img, ss);
                            if (wr.success()) {
                                std::string s = ss.str();
                                imgData.assign(s.begin(), s.end());
                                hasData = true;
                                if (ext == "jpg" || ext == "jpeg") mimeType = "image/jpeg";
                                else mimeType = "image/png";
                            }
                        }
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
                        tinygltf::Image gltfImg;
                        gltfImg.mimeType = mimeType;
                        size_t currentSize = buffer.data.size();
                        size_t padding = (4 - (currentSize % 4)) % 4;
                        if (padding > 0) {
                            buffer.data.resize(currentSize + padding);
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
                        tinygltf::Texture gltfTex;
                        if (mimeType == "image/ktx2") {
                            tinygltf::Value::Object ktxExt;
                            ktxExt["source"] = tinygltf::Value(imgIdx);
                            gltfTex.extensions["KHR_texture_basisu"] = tinygltf::Value(ktxExt);
                        } else {
                            gltfTex.source = imgIdx;
                        }
                        int texIdx = (int)model.textures.size();
                        model.textures.push_back(gltfTex);
                        mat.normalTexture.index = texIdx;
                        size_t endSize = buffer.data.size();
                        size_t endPadding = (4 - (endSize % 4)) % 4;
                        if (endPadding > 0) {
                            buffer.data.resize(endSize + endPadding);
                            memset(buffer.data.data() + endSize, 0, endPadding);
                        }
                    }
                }
            }
            const osg::Texture* etex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(4, osg::StateAttribute::TEXTURE));
            if (etex && etex->getNumImages() > 0) {
                const osg::Image* img = etex->getImage(0);
                if (img) {
                    std::string imgPath = img->getFileName();
                    std::vector<unsigned char> imgData;
                    std::string mimeType = "image/png";
                    bool hasData = false;

                    // Try KTX2 compression if enabled
                    if (settings.enableTextureCompress) {
                        std::vector<unsigned char> compressedData;
                        std::string compressedMime;
                        if (process_texture(const_cast<osg::Texture*>(etex), compressedData, compressedMime, true)) {
                            if (compressedMime == "image/ktx2") {
                                imgData = compressedData;
                                mimeType = compressedMime;
                                hasData = true;

                                // Register extension
                                if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_texture_basisu") == model.extensionsUsed.end()) {
                                    model.extensionsUsed.push_back("KHR_texture_basisu");
                                    model.extensionsRequired.push_back("KHR_texture_basisu");
                                }
                            }
                        }
                    }

                    if (!hasData && !imgPath.empty() && fs::exists(imgPath)) {
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
                    if (!hasData && img->data() != nullptr) {
                        std::string ext = "png";
                        if (!imgPath.empty()) {
                            std::string e = fs::path(imgPath).extension().string();
                            if (!e.empty() && e.size() > 1) {
                                ext = e.substr(1);
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            }
                        }
                        osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(ext);
                        if (rw) {
                            std::stringstream ss;
                            osgDB::ReaderWriter::WriteResult wr = rw->writeImage(*img, ss);
                            if (wr.success()) {
                                std::string s = ss.str();
                                imgData.assign(s.begin(), s.end());
                                hasData = true;
                                if (ext == "jpg" || ext == "jpeg") mimeType = "image/jpeg";
                                else mimeType = "image/png";
                            }
                        }
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
                        tinygltf::Image gltfImg;
                        gltfImg.mimeType = mimeType;
                        size_t currentSize = buffer.data.size();
                        size_t padding = (4 - (currentSize % 4)) % 4;
                        if (padding > 0) {
                            buffer.data.resize(currentSize + padding);
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
                        tinygltf::Texture gltfTex;
                        if (mimeType == "image/ktx2") {
                            tinygltf::Value::Object ktxExt;
                            ktxExt["source"] = tinygltf::Value(imgIdx);
                            gltfTex.extensions["KHR_texture_basisu"] = tinygltf::Value(ktxExt);
                        } else {
                            gltfTex.source = imgIdx;
                        }
                        int texIdx = (int)model.textures.size();
                        model.textures.push_back(gltfTex);
                        mat.emissiveTexture.index = texIdx;
                        size_t endSize = buffer.data.size();
                        size_t endPadding = (4 - (endSize % 4)) % 4;
                        if (endPadding > 0) {
                            buffer.data.resize(endSize + endPadding);
                            memset(buffer.data.data() + endSize, 0, endPadding);
                        }
                    }
                }
            }
            const osg::Texture* rtex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(2, osg::StateAttribute::TEXTURE));
            const osg::Texture* mtex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(3, osg::StateAttribute::TEXTURE));
            const osg::Texture* atex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(5, osg::StateAttribute::TEXTURE));
            if ((rtex && rtex->getNumImages() > 0) || (mtex && mtex->getNumImages() > 0) || (atex && atex->getNumImages() > 0)) {
                const osg::Image* rimg = rtex ? rtex->getImage(0) : nullptr;
                const osg::Image* mimg = mtex ? mtex->getImage(0) : nullptr;
                const osg::Image* aimg = atex ? atex->getImage(0) : nullptr;
                int rw = rimg ? rimg->s() : 0, rh = rimg ? rimg->t() : 0;
                int mw = mimg ? mimg->s() : 0, mh = mimg ? mimg->t() : 0;
                int aw = aimg ? aimg->s() : 0, ah = aimg ? aimg->t() : 0;
                int tw = std::max({rw, mw, aw});
                int th = std::max({rh, mh, ah});
                if (tw == 0 || th == 0) {
                    // Fallback: use 1x1 texture from factors
                    tw = 1; th = 1;
                }
                auto extract_channel = [](const osg::Image* img, int& w, int& h) -> std::vector<unsigned char> {
                    std::vector<unsigned char> out;
                    if (!img || !img->data()) { w = 0; h = 0; return out; }
                    w = img->s(); h = img->t();
                    out.assign(w * h, 0);
                    int channels = 4;
                    GLenum pf = img->getPixelFormat();
                    if (pf == GL_LUMINANCE) channels = 1;
                    else if (pf == GL_LUMINANCE_ALPHA) channels = 2;
                    else if (pf == GL_RGB) channels = 3;
                    else if (pf == GL_RGBA) channels = 4;
                    const unsigned char* p = img->data();
                    for (int i = 0; i < w * h; ++i) out[i] = p[i * channels];
                    return out;
                };
                auto bilinear = [](const std::vector<unsigned char>& src, int sw, int sh, int tw, int th) -> std::vector<unsigned char> {
                    if (sw == tw && sh == th) return src;
                    std::vector<unsigned char> dst(tw * th, 0);
                    const float sx = sw > 1 ? (float)(sw - 1) / (float)(tw - 1) : 0.0f;
                    const float sy = sh > 1 ? (float)(sh - 1) / (float)(th - 1) : 0.0f;
                    for (int y = 0; y < th; ++y) {
                        float fy = y * sy;
                        int y0 = (int)floorf(fy);
                        int y1 = std::min(y0 + 1, sh - 1);
                        float ty = fy - y0;
                        for (int x = 0; x < tw; ++x) {
                            float fx = x * sx;
                            int x0 = (int)floorf(fx);
                            int x1 = std::min(x0 + 1, sw - 1);
                            float tx = fx - x0;
                            int i00 = y0 * sw + x0;
                            int i10 = y0 * sw + x1;
                            int i01 = y1 * sw + x0;
                            int i11 = y1 * sw + x1;
                            float v0 = src.empty() ? 0.0f : (float)src[i00] * (1.0f - tx) + (float)src[i10] * tx;
                            float v1 = src.empty() ? 0.0f : (float)src[i01] * (1.0f - tx) + (float)src[i11] * tx;
                            float v = v0 * (1.0f - ty) + v1 * ty;
                            dst[y * tw + x] = (unsigned char)std::round(std::min(std::max(v, 0.0f), 255.0f));
                        }
                    }
                    return dst;
                };
                int rw0 = 0, rh0 = 0, mw0 = 0, mh0 = 0, aw0 = 0, ah0 = 0;
                std::vector<unsigned char> rch = extract_channel(rimg, rw0, rh0);
                std::vector<unsigned char> mch = extract_channel(mimg, mw0, mh0);
                std::vector<unsigned char> aoch = extract_channel(aimg, aw0, ah0);
                if (!rch.empty()) rch = bilinear(rch, rw0, rh0, tw, th);
                if (!mch.empty()) mch = bilinear(mch, mw0, mh0, tw, th);
                if (!aoch.empty()) aoch = bilinear(aoch, aw0, ah0, tw, th);
                std::vector<unsigned char> mr(tw * th * 3, 0xff);
                for (int i = 0; i < tw * th; ++i) {
                    mr[i * 3 + 0] = atex && !aoch.empty() ? aoch[i] : 0xff;
                    mr[i * 3 + 1] = rtex && !rch.empty() ? rch[i] : (unsigned char)std::round(roughnessFactor * 255.0f);
                    mr[i * 3 + 2] = mtex && !mch.empty() ? mch[i] : (unsigned char)std::round(metallicFactor * 255.0f);
                }
                std::string finalMimeType = "image/png";
                std::vector<unsigned char> finalData;

                if (settings.enableTextureCompress) {
                     std::vector<unsigned char> mr_rgba(tw * th * 4);
                     for (int i = 0; i < tw * th; ++i) {
                         mr_rgba[i * 4 + 0] = mr[i * 3 + 0];
                         mr_rgba[i * 4 + 1] = mr[i * 3 + 1];
                         mr_rgba[i * 4 + 2] = mr[i * 3 + 2];
                         mr_rgba[i * 4 + 3] = 255;
                     }
                     if (compress_to_ktx2(mr_rgba, tw, th, finalData)) {
                         finalMimeType = "image/ktx2";

                         // Register extension if not already
                         if (std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), "KHR_texture_basisu") == model.extensionsUsed.end()) {
                             model.extensionsUsed.push_back("KHR_texture_basisu");
                             model.extensionsRequired.push_back("KHR_texture_basisu");
                         }
                     }
                }

                if (finalData.empty()) {
                    osg::ref_ptr<osg::Image> outImg = new osg::Image();
                    outImg->allocateImage(tw, th, 1, GL_RGB, GL_UNSIGNED_BYTE);
                    memcpy(outImg->data(), mr.data(), mr.size());
                    osgDB::ReaderWriter* writer = osgDB::Registry::instance()->getReaderWriterForExtension("png");
                    if (writer) {
                        std::stringstream ss;
                        osgDB::ReaderWriter::WriteResult wr = writer->writeImage(*outImg, ss);
                        if (wr.success()) {
                            std::string s = ss.str();
                            finalData.assign(s.begin(), s.end());
                        }
                    }
                }

                if (!finalData.empty()) {
                        size_t currentSize = buffer.data.size();
                        size_t padding = (4 - (currentSize % 4)) % 4;
                        if (padding > 0) {
                            buffer.data.resize(currentSize + padding);
                            memset(buffer.data.data() + currentSize, 0, padding);
                        }
                        size_t imgOffset = buffer.data.size();
                        size_t imgLen = finalData.size();
                        buffer.data.resize(imgOffset + imgLen);
                        memcpy(buffer.data.data() + imgOffset, finalData.data(), imgLen);
                        tinygltf::BufferView bvImg;
                        bvImg.buffer = 0;
                        bvImg.byteOffset = imgOffset;
                        bvImg.byteLength = imgLen;
                        int bvImgIdx = (int)model.bufferViews.size();
                        model.bufferViews.push_back(bvImg);
                        tinygltf::Image gltfImg;
                        gltfImg.mimeType = finalMimeType;
                        gltfImg.bufferView = bvImgIdx;
                        int imgIdx = (int)model.images.size();
                        model.images.push_back(gltfImg);
                        tinygltf::Texture gltfTex;

                        if (finalMimeType == "image/ktx2") {
                            tinygltf::Value::Object ktxExt;
                            ktxExt["source"] = tinygltf::Value(imgIdx);
                            gltfTex.extensions["KHR_texture_basisu"] = tinygltf::Value(ktxExt);
                        } else {
                            gltfTex.source = imgIdx;
                        }

                        int texIdx = (int)model.textures.size();
                        model.textures.push_back(gltfTex);
                        mat.pbrMetallicRoughness.metallicRoughnessTexture.index = texIdx;
                        mat.occlusionTexture.index = texIdx;
                        float aoStrengthOut = (atex && aimg && !aoch.empty()) ? aoStrength : 1.0f;
                        mat.occlusionTexture.strength = aoStrengthOut;
                        size_t endSize = buffer.data.size();
                        size_t endPadding = (4 - (endSize % 4)) % 4;
                        if (endPadding > 0) {
                            buffer.data.resize(endSize + endPadding);
                            memset(buffer.data.data() + endSize, 0, endPadding);
                        }
                }
            }
        }

        mat.pbrMetallicRoughness.baseColorFactor = baseColor;
        if (mat.alphaMode.empty()) {
            if (baseColor[3] < 0.99) mat.alphaMode = "BLEND";
            else mat.alphaMode = "OPAQUE";
        }

        mat.pbrMetallicRoughness.metallicFactor = metallicFactor;
        mat.pbrMetallicRoughness.roughnessFactor = roughnessFactor;
        mat.emissiveFactor = {emissiveColor[0], emissiveColor[1], emissiveColor[2]};
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

        if (dracoCompressed) {
            tinygltf::Value::Object dracoExt;
            dracoExt["bufferView"] = tinygltf::Value(dracoBufferViewIdx);

            tinygltf::Value::Object dracoAttribs;
            if (dracoPosId != -1) dracoAttribs["POSITION"] = tinygltf::Value(dracoPosId);
            if (dracoNormId != -1) dracoAttribs["NORMAL"] = tinygltf::Value(dracoNormId);
            if (dracoTexId != -1) dracoAttribs["TEXCOORD_0"] = tinygltf::Value(dracoTexId);
            if (dracoBatchId != -1) dracoAttribs["_BATCHID"] = tinygltf::Value(dracoBatchId);

            dracoExt["attributes"] = tinygltf::Value(dracoAttribs);

            prim.extensions["KHR_draco_mesh_compression"] = tinygltf::Value(dracoExt);
        }

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

json FBXPipeline::processNode(OctreeNode* node, const std::string& parentPath, int parentDepth, int childIndexAtParent, const std::string& treePath) {
    json nodeJson;
    nodeJson["refine"] = "REPLACE";

    osg::BoundingBoxd tightBox;
    bool hasTightBox = false;

    // 2. Content
    if (!node->content.empty()) {
        // Naming convention: tile_{treePath}
        std::string tileName = "tile_" + treePath;

        // Create content
        // For B3DM
        SimplificationParams simParams;
        simParams.enable_simplification = settings.enableSimplify;
        simParams.target_ratio = 0.5f;
        simParams.target_error = 1e-2f;
        auto result = createB3DM(node->content, parentPath, tileName, simParams);
        std::string contentUrl = result.first;
        osg::BoundingBoxd cBox = result.second;

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
        for (size_t i = 0; i < node->children.size(); ++i) {
            auto child = node->children[i];
            json childJson = processNode(child, parentPath, node->depth, (int)i, treePath + "_" + std::to_string(i));
            bool isEmptyChild = (!childJson.contains("content")) && (!childJson.contains("children") || childJson["children"].empty());
            if (!isEmptyChild) {
                nodeJson["children"].push_back(childJson);
                try {
                    auto& cBoxJson = childJson["boundingVolume"]["box"];
                    if (cBoxJson.is_array() && cBoxJson.size() == 12) {
                        double cx = cBoxJson[0];
                        double cy = cBoxJson[1];
                        double cz = cBoxJson[2];
                        double dx = cBoxJson[3];
                        double dy = cBoxJson[7];
                        double dz = cBoxJson[11];
                        tightBox.expandBy(osg::Vec3d(cx - dx, cy - dy, cz - dz));
                        tightBox.expandBy(osg::Vec3d(cx + dx, cy + dy, cz + dz));
                        hasTightBox = true;
                    }
                } catch (...) {}
            } else {
                LOG_I("Filtered empty tile: parentDepth=%d childIndex=%d nodes=%zu", node->depth, (int)i, node->children[i]->content.size());
            }
        }
    }

    // 1. Calculate Geometric Error and Bounding Volume
    double diagonal = 0.0;
    if (hasTightBox) {
        double dx = tightBox.xMax() - tightBox.xMin();
        double dy = tightBox.yMax() - tightBox.yMin();
        double dz = tightBox.zMax() - tightBox.zMin();
        double diagonalOriginal = std::sqrt(dx*dx + dy*dy + dz*dz);

        double cx = tightBox.center().x();
        double cy = tightBox.center().y();
        double cz = tightBox.center().z();
        double hx = (tightBox.xMax() - tightBox.xMin()) / 2.0;
        double hy = (tightBox.yMax() - tightBox.yMin()) / 2.0;
        double hz = (tightBox.zMax() - tightBox.zMin()) / 2.0;

        // Add small padding to avoid near-plane culling or precision issues
        hx = std::max(hx * 1.25, 1e-6);
        hy = std::max(hy * 1.25, 1e-6);
        hz = std::max(hz * 1.25, 1e-6);
        diagonal = 2.0 * std::sqrt(hx*hx + hy*hy + hz*hz);
        LOG_I("Node depth=%d tightBox center=(%.3f,%.3f,%.3f) halfAxes=(%.3f,%.3f,%.3f) diagOriginal=%.3f diagInflated=%.3f inflate=1.25", node->depth, cx, cy, cz, hx, hy, hz, diagonalOriginal, diagonal);

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

        extentX = std::max(extentX, 1e-6);
        extentY = std::max(extentY, 1e-6);
        extentZ = std::max(extentZ, 1e-6);
        diagonal = 2.0 * std::sqrt(extentX*extentX + extentY*extentY + extentZ*extentZ);
        LOG_I("Node depth=%d fallbackBox center=(%.3f,%.3f,%.3f) halfAxes=(%.3f,%.3f,%.3f) diag=%.3f", node->depth, cx, -cz, cy, extentX, extentZ, extentY, diagonal);

        diagonal = std::sqrt(extentX*extentX*4 + extentY*extentY*4 + extentZ*extentZ*4);

        // Collect Tile Stats
        double dimX = extentX * 2.0;
        double dimY = extentY * 2.0;
        double dimZ = extentZ * 2.0;
        double vol = dimX * dimY * dimZ;
        std::string tName = "Node_d" + std::to_string(node->depth) + "_i" + std::to_string(childIndexAtParent);
        if (!node->content.empty()) tName += "_Content";

        tileStats.push_back({
            tName, node->depth, vol, dimX, dimY, dimZ,
            osg::Vec3d(cx, cy, cz),
            osg::Vec3d(cx - extentX, cy - extentY, cz - extentZ),
            osg::Vec3d(cx + extentX, cy + extentY, cz + extentZ)
        });

        nodeJson["boundingVolume"] = {
            {"box", {
                cx, -cz, cy,           // Center transformed
                extentX, 0, 0,         // X axis
                0, extentZ, 0,         // Y axis (was Z)
                0, 0, extentY          // Z axis (was Y)
            }}
        };
    }

    // Geometric error = scale * diagonal (no clamp). Ensure > 0 by epsilon if degenerate.
    double geOut = std::max(1e-3, settings.geScale * diagonal);
    nodeJson["geometricError"] = geOut;
    std::string refineMode = "REPLACE";
    nodeJson["refine"] = refineMode;
    LOG_I("Node depth=%d isLeaf=%d content=%zu children=%zu geScale=%.3f geOut=%.3f refine=%s", node->depth, (int)node->isLeaf(), node->content.size(), node->children.size(), settings.geScale, geOut, refineMode.c_str());
    {
        auto& acc = levelStats[node->depth];
        acc.count += 1;
        acc.sumDiag += diagonal;
        acc.sumGe += geOut;
        if (hasTightBox) acc.tightCount += 1; else acc.fallbackCount += 1;
        if (refineMode == "ADD") acc.refineAdd += 1; else acc.refineReplace += 1;

        // Log contained nodes
        if (!node->content.empty()) {
            std::string tName = "Node_d" + std::to_string(node->depth) + "_i" + std::to_string(childIndexAtParent);
            if (!node->content.empty()) tName += "_Content";

            for (const auto& ref : node->content) {
                std::string nName = (ref.transformIndex < ref.meshInfo->nodeNames.size()) ? ref.meshInfo->nodeNames[ref.transformIndex] : "unknown";
                LOG_I("Tile: %s contains Node: %s", tName.c_str(), nName.c_str());
            }
        }
    }

    return nodeJson;
}

std::pair<std::string, osg::BoundingBoxd> FBXPipeline::createB3DM(const std::vector<InstanceRef>& instances, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams) {
    // 1. Calculate RTC Offset (Center of all instances in Target Z-Up Coordinates)
    osg::BoundingBoxd totalBox;
    size_t validBoxes = 0;
    for (const auto& inst : instances) {
        if (!inst.meshInfo || !inst.meshInfo->geometry) continue;
        const osg::BoundingBox& bbox = inst.meshInfo->geometry->getBoundingBox();
        if (!bbox.valid()) continue;

        validBoxes++;
        const osg::Matrixd& mat = inst.meshInfo->transforms[inst.transformIndex];

        // Transform the 8 corners of bbox and expand totalBox
        for(int i=0; i<8; ++i) {
            osg::Vec3d corner = osg::Vec3d(bbox.corner(i)) * mat;
            totalBox.expandBy(corner);
        }
    }

    // 2. Create GLB (TinyGLTF)
    tinygltf::Model model;
    tinygltf::Asset asset;
    asset.version = "2.0";
    asset.generator = "FBX23DTiles";
    model.asset = asset;

    json batchTableJson;
    int batchIdCounter = 0;
    osg::BoundingBoxd contentBox;

    TileStats tileStats;
    osg::Vec3d rtcCenter = totalBox.valid() ? osg::Vec3d(totalBox.center()) : osg::Vec3d(0,0,0);
    appendGeometryToModel(model, instances, settings, &batchTableJson, &batchIdCounter, simParams, &contentBox, &tileStats, tileName.c_str(), rtcCenter);
    LOG_I("Tile %s: nodes=%zu triangles=%zu vertices=%zu materials=%zu", tileName.c_str(), tileStats.node_count, tileStats.triangle_count, tileStats.vertex_count, tileStats.material_count);

    // Shift contentBox back to World Z-up space so tileset.json gets correct bounding volume
    if (contentBox.valid()) {
        osg::Vec3d rtcZUp(rtcCenter.x(), -rtcCenter.z(), rtcCenter.y());
        contentBox._min += rtcZUp;
        contentBox._max += rtcZUp;
    }

    // Populate Batch Table with node names and attributes
    std::vector<std::string> batchNames;
    std::vector<std::unordered_map<std::string, std::string>> allAttrs;
    std::set<std::string> attrKeys;

    for (const auto& ref : instances) {
        if (!ref.meshInfo || !ref.meshInfo->geometry) continue;
        std::string nName = "unknown";
        std::unordered_map<std::string, std::string> attrs;

        if (ref.transformIndex < ref.meshInfo->nodeNames.size()) {
             nName = ref.meshInfo->nodeNames[ref.transformIndex];
        }
        if (ref.transformIndex < ref.meshInfo->nodeAttrs.size()) {
             attrs = ref.meshInfo->nodeAttrs[ref.transformIndex];
             for (const auto& kv : attrs) attrKeys.insert(kv.first);
        }
        batchNames.push_back(nName);
        allAttrs.push_back(attrs);
    }

    if (!batchNames.empty()) {
        batchTableJson["name"] = batchNames;
    }

    // Add other attributes
    for (const std::string& key : attrKeys) {
        // Skip "name" as it is already handled
        if (key == "name") continue;

        std::vector<std::string> values;
        for (const auto& attrs : allAttrs) {
            auto it = attrs.find(key);
            if (it != attrs.end()) {
                values.push_back(it->second);
            } else {
                values.push_back(""); // Default empty string
            }
        }
        batchTableJson[key] = values;
    }

    // Skip writing B3DM if no mesh content was generated
    if (tileStats.triangle_count == 0 || model.meshes.empty()) {
        LOG_I("Tile %s: no content generated, skip B3DM", tileName.c_str());
        return {"", contentBox};
    }

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
    if (batchIdCounter == 0) {
        featureTable["BATCH_LENGTH"] = 0;
    } else {
        featureTable["BATCH_LENGTH"] = batchIdCounter;
    }

    // RTC_CENTER (Z-up)
    featureTable["RTC_CENTER"] = {
        rtcCenter.x(),
        -rtcCenter.z(),
        rtcCenter.y()
    };

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

    // Serialize Batch Table
    std::string batchTableString = "";
    if (!batchTableJson.empty()) {
        batchTableString = batchTableJson.dump();
        size_t batchPadding = (8 - (batchTableString.size() % 8)) % 8;
        for(size_t i=0; i<batchPadding; ++i) {
            batchTableString += " ";
        }
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
    header.batchTableJSONByteLength = (uint32_t)batchTableString.size();
    header.batchTableBinaryByteLength = 0;
    header.byteLength = (uint32_t)(sizeof(B3dmHeader) + featureTableString.size() + batchTableString.size() + glbData.size());

    outfile.write(reinterpret_cast<const char*>(&header), sizeof(B3dmHeader));
    outfile.write(featureTableString.c_str(), featureTableString.size());
    if (!batchTableString.empty()) {
        outfile.write(batchTableString.c_str(), batchTableString.size());
    }
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
        double diag = std::sqrt(dx*dx + dy*dy + dz*dz);
        geometricError = std::max(1e-3, settings.geScale * diag);
    }
    tileset["geometricError"] = geometricError;
    LOG_I("Tileset top-level geometricError=%.3f", geometricError);

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
                LOG_I("Root boundingVolume lengths x=%.3f y=%.3f z=%.3f diag=%.3f", xlen, ylen, zlen, diag);
             }
         }

         if (diag > 0.0) {
            tileset["root"]["geometricError"] = diag;
            tileset["geometricError"] = tileset["root"]["geometricError"];
            LOG_I("Forcing root geometric error to %f (calculated from root box)", diag);
            LOG_I("Tileset geometricError updated to root=%.3f", double(tileset["root"]["geometricError"]));
         } else {
             // Fallback to globalBounds if box missing (unlikely)
             // globalBounds is Y-up from FBX, but size is same
             double dx = globalBounds.xMax() - globalBounds.xMin();
             double dy = globalBounds.yMax() - globalBounds.yMin();
             double dz = globalBounds.zMax() - globalBounds.zMin();
             double fallbackDiag = std::sqrt(dx*dx + dy*dy + dz*dz);
             tileset["root"]["geometricError"] = fallbackDiag;
             tileset["geometricError"] = tileset["root"]["geometricError"];
             LOG_I("Forcing root geometric error to %f (calculated from global bounds)", fallbackDiag);
             LOG_I("Tileset geometricError updated to fallback=%.3f", double(tileset["root"]["geometricError"]));
         }
    }

    // Always add Transform to anchor local ENU coordinates to ECEF
    if (settings.longitude != 0.0 || settings.latitude != 0.0 || settings.height != 0.0) {
        glm::dmat4 enuToEcef = GeoTransform::CalcEnuToEcefMatrix(settings.longitude, settings.latitude, settings.height);

        // Calculate center of the model (in original local coordinates)
        double cx = (globalBounds.xMin() + globalBounds.xMax()) * 0.5;
        double cy = (globalBounds.yMin() + globalBounds.yMax()) * 0.5;
        double cz = (globalBounds.zMin() + globalBounds.zMax()) * 0.5;

        // Apply centering offset: Move model center to (0,0,0) before applying ENU->ECEF
        // This ensures the model is placed AT the target longitude/latitude, not offset by its original coordinates.
        // glm is column-major
        // NOTE: The geometry is converted from Y-Up (FBX) to Z-Up (B3DM) during writing (x, -z, y).
        // So we must swap the center coordinates to match:
        // Center_B3DM = (cx, -cz, cy)
        // Offset = -Center_B3DM = (-cx, cz, -cy)
        glm::dmat4 centerOffset(1.0);
        centerOffset[3][0] = -cx;
        centerOffset[3][1] = cz;
        centerOffset[3][2] = -cy;

        // Combine: Final = ENU_to_ECEF * CenterOffset
        // Vertex_Final = ENU_to_ECEF * (Vertex_Local - Center)
        enuToEcef = enuToEcef * centerOffset;

        LOG_I("Applied centering offset: (%.2f, %.2f, %.2f) to move model center to origin.", cx, cy, cz);

        const double* m = (const double*)&enuToEcef;
        tileset["root"]["transform"] = {
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]
        };
        LOG_I("Applied root transform ENU->ECEF at lon=%.6f lat=%.6f h=%.3f", settings.longitude, settings.latitude, settings.height);
    } else {
        LOG_W("No geolocation provided; root.transform not set. Tiles remain in local ENU space.");
    }

    std::string s = tileset.dump(4);
    std::ofstream out(fs::path(basePath) / "tileset.json");
    out << s;
    out.close();
}

void FBXPipeline::logLevelStats() {
    std::vector<int> levels;
    levels.reserve(levelStats.size());
    for (const auto& kv : levelStats) levels.push_back(kv.first);
    std::sort(levels.begin(), levels.end());
    LOG_I("LevelStats summary begin");
    for (int d : levels) {
        const auto& acc = levelStats[d];
        double avgDiag = acc.count ? acc.sumDiag / acc.count : 0.0;
        double avgGe = acc.count ? acc.sumGe / acc.count : 0.0;
        double tightPct = acc.count ? (double)acc.tightCount * 100.0 / (double)acc.count : 0.0;
        double fallbackPct = acc.count ? (double)acc.fallbackCount * 100.0 / (double)acc.count : 0.0;
        double addPct = acc.count ? (double)acc.refineAdd * 100.0 / (double)acc.count : 0.0;
        double replacePct = acc.count ? (double)acc.refineReplace * 100.0 / (double)acc.count : 0.0;
        LOG_I("LevelStats depth=%d tiles=%zu avgDiag=%.3f avgGe=%.3f inflate=1.25 tight=%.1f%% fallback=%.1f%% refineAdd=%.1f%% refineReplace=%.1f%%", d, acc.count, avgDiag, avgGe, tightPct, fallbackPct, addPct, replacePct);
    }
    LOG_I("LevelStats summary end");
}

nlohmann::json FBXPipeline::buildAverageTiles(const osg::BoundingBox& globalBounds, const std::string& parentPath) {
    nlohmann::json rootJson;
    rootJson["children"] = nlohmann::json::array();
    rootJson["refine"] = "REPLACE";

    // Gather all instances
    std::vector<InstanceRef> all;
    for (auto& pair : loader->meshPool) {
        MeshInstanceInfo& info = pair.second;
        if (!info.geometry) continue;
        for (size_t i = 0; i < info.transforms.size(); ++i) {
            InstanceRef ref;
            ref.meshInfo = &info;
            ref.transformIndex = (int)i;
            all.push_back(ref);
        }
    }

    // Split by average count and generate children; simultaneously accumulate ENU global bounds
    osg::BoundingBox enuGlobal;
    size_t total = all.size();
    size_t step = std::max<size_t>(1, (size_t)settings.maxItemsPerTile);
    size_t tiles = (total + step - 1) / step;
    for (size_t t = 0; t < tiles; ++t) {
        size_t start = t * step;
        size_t end = std::min(total, start + step);
        if (start >= end) break;
        std::vector<InstanceRef> chunk(all.begin() + start, all.begin() + end);
        std::string tileName = "tile_" + std::to_string(t);
        SimplificationParams simParams;
        auto b3dm = createB3DM(chunk, parentPath, tileName, simParams);
        if (b3dm.first.empty()) {
            LOG_I("AvgSplit tile=%s produced no content, skipped", tileName.c_str());
            continue;
        }
        osg::BoundingBox cb = b3dm.second; // Already ENU due to appendGeometryToModel
        enuGlobal.expandBy(cb);

        double cx = cb.center().x();
        double cy = cb.center().y();
        double cz = cb.center().z();
        double hx = std::max((cb.xMax() - cb.xMin()) / 2.0, 1e-6);
        double hy = std::max((cb.yMax() - cb.yMin()) / 2.0, 1e-6);
        double hz = std::max((cb.zMax() - cb.zMin()) / 2.0, 1e-6);

        // Collect Tile Stats
        double dimX = hx * 2.0;
        double dimY = hy * 2.0;
        double dimZ = hz * 2.0;
        double vol = dimX * dimY * dimZ;

        tileStats.push_back({
            tileName, 1, vol, dimX, dimY, dimZ,
            osg::Vec3d(cx, cy, cz),
            osg::Vec3d(cb.xMin(), cb.yMin(), cb.zMin()),
            osg::Vec3d(cb.xMax(), cb.yMax(), cb.zMax())
        });

        double diag = 2.0 * std::sqrt(hx*hx + hy*hy + hz*hz);
        double geOut = std::max(1e-3, settings.geScale * diag);

        nlohmann::json child;
        child["boundingVolume"]["box"] = { cx, cy, cz, hx, 0, 0, 0, hy, 0, 0, 0, hz };
        child["geometricError"] = geOut;
        child["refine"] = "REPLACE";
        child["content"]["uri"] = b3dm.first;
        rootJson["children"].push_back(child);

        auto& acc = levelStats[1];
        acc.count += 1;
        acc.sumDiag += diag;
        acc.sumGe += geOut;
        acc.tightCount += 1;
        acc.refineReplace += 1;
        LOG_I("AvgSplit tile=%s count=%zu diag=%.3f ge=%.3f", tileName.c_str(), chunk.size(), diag, geOut);

        // Log contained nodes
        for (const auto& ref : chunk) {
            std::string nName = (ref.transformIndex < ref.meshInfo->nodeNames.size()) ? ref.meshInfo->nodeNames[ref.transformIndex] : "unknown";
            LOG_I("Tile: %s contains Node: %s", tileName.c_str(), nName.c_str());
        }
    }

    // Compute root bounding volume from union of children (ENU space, consistent with root.transform)
    if (enuGlobal.valid()) {
        double gcx = enuGlobal.center().x();
        double gcy = enuGlobal.center().y();
        double gcz = enuGlobal.center().z();
        double halfX = std::max((enuGlobal.xMax() - enuGlobal.xMin()) / 2.0 * 1.25, 1e-6);
        double halfY = std::max((enuGlobal.yMax() - enuGlobal.yMin()) / 2.0 * 1.25, 1e-6);
        double halfZ = std::max((enuGlobal.zMax() - enuGlobal.zMin()) / 2.0 * 1.25, 1e-6);
        double gdiag = 2.0 * std::sqrt(halfX*halfX + halfY*halfY + halfZ*halfZ);
        double gge = std::max(1e-3, settings.geScale * gdiag);
        rootJson["boundingVolume"]["box"] = { gcx, gcy, gcz, halfX, 0, 0, 0, halfY, 0, 0, 0, halfZ };
        rootJson["geometricError"] = gge;
        auto& acc = levelStats[0];
        acc.count += 1;
        acc.sumDiag += gdiag;
        acc.sumGe += gge;
        acc.tightCount += 1;
        acc.refineReplace += 1;
        LOG_I("AvgSplit root diag=%.3f ge=%.3f center=(%.3f,%.3f,%.3f) halfAxes=(%.3f,%.3f,%.3f)", gdiag, gge, gcx, gcy, gcz, halfX, halfY, halfZ);
    } else {
        // Fallback to transformed original global bounds if no children created
        double halfX = std::max((globalBounds.xMax() - globalBounds.xMin()) / 2.0 * 1.25, 1e-6);
        double halfY = std::max((globalBounds.yMax() - globalBounds.yMin()) / 2.0 * 1.25, 1e-6);
        double halfZ = std::max((globalBounds.zMax() - globalBounds.zMin()) / 2.0 * 1.25, 1e-6);
        double gdiag = 2.0 * std::sqrt(halfX*halfX + halfY*halfY + halfZ*halfZ);
        double gge = settings.geScale * gdiag;
        double gcx = globalBounds.center().x();
        double gcy = -globalBounds.center().z();
        double gcz = globalBounds.center().y();
        rootJson["boundingVolume"]["box"] = { gcx, gcy, gcz, halfX, 0, 0, 0, halfZ, 0, 0, 0, halfY };
        rootJson["geometricError"] = gge;
        auto& acc = levelStats[0];
        acc.count += 1;
        acc.sumDiag += gdiag;
        acc.sumGe += gge;
        acc.fallbackCount += 1;
        acc.refineReplace += 1;
        LOG_I("AvgSplit root (fallback) diag=%.3f ge=%.3f center=(%.3f,%.3f,%.3f) halfAxes=(%.3f,%.3f,%.3f)", gdiag, gge, gcx, gcy, gcz, halfX, halfZ, halfY);
    }

    return rootJson;
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
    bool enable_unlit,
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
    settings.enableUnlit = enable_unlit;
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
