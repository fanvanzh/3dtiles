#pragma once

#include "fbx.h"
#include <string>
#include <vector>
#include <osg/Matrixd>
#include <osg/BoundingBox>
#include <osg/Geometry>
#include <nlohmann/json.hpp>
#include "mesh_processor.h"
#include <unordered_map>

// Forward declarations
namespace tinygltf {
    class Model;
}

struct PipelineSettings {
    std::string inputPath;
    std::string outputPath;
    int maxDepth = 5;
    int maxItemsPerTile = 1000;

    // Optimization flags
    bool enableSimplify = false;
    bool enableDraco = false;
    bool enableTextureCompress = false; // KTX2
    bool enableLOD = false; // Enable Hierarchical LOD generation
    bool enableUnlit = false; // Enable KHR_materials_unlit
    std::vector<float> lodRatios = {1.0f, 0.5f, 0.25f}; // Default LOD ratios (Fine to Coarse)

    // Geolocation (Origin)
    double longitude = 0.0;
    double latitude = 0.0;
    double height = 0.0;

    // Geometric error scale (multiplier applied to boundingVolume diagonal)
    double geScale = 0.5; // Adjusted for better LOD switching with SSE=16

    // Split strategy: when true, split by average count using maxItemsPerTile; when false, use octree
    bool splitAverageByCount = false;
} ;

struct InstanceRef {
    MeshInstanceInfo* meshInfo;
    int transformIndex;
};

class FBXPipeline {
public:
    FBXPipeline(const PipelineSettings& settings);
    ~FBXPipeline();

    void run();

private:
    PipelineSettings settings;
    FBXLoader* loader = nullptr;
    struct LevelAccum { size_t count = 0; double sumDiag = 0.0; double sumGe = 0.0; size_t tightCount = 0; size_t fallbackCount = 0; size_t refineAdd = 0; size_t refineReplace = 0; };
    std::unordered_map<int, LevelAccum> levelStats;

    struct TileInfo {
        std::string name;
        int depth;
        double volume;
        double dx, dy, dz;
        osg::Vec3d center;
        osg::Vec3d minPt, maxPt;
    };
    std::vector<TileInfo> tileStats;

    void logLevelStats();
    nlohmann::json buildAverageTiles(const osg::BoundingBox& globalBounds, const std::string& parentPath);

    // Octree Node Definition
    struct OctreeNode {
        osg::BoundingBox bbox;
        std::vector<InstanceRef> content;
        std::vector<OctreeNode*> children;
        int depth = 0;

        bool isLeaf() const { return children.empty(); }
        ~OctreeNode() { for (auto c : children) delete c; }
    };

    OctreeNode* rootNode = nullptr;

    // Build Octree
    void buildOctree(OctreeNode* node);

    // Process Octree to generate Tiles
    // Returns the JSON object representing this node and its children (if any)
    // treePath: A string representing the path in the tree (e.g., "0_1_4") for naming
    nlohmann::json processNode(OctreeNode* node, const std::string& parentPath, int parentDepth, int childIndexAtParent, const std::string& treePath);

    // Converters
    // Returns filename created and the tight bounding box of the content (in ENU)
    std::pair<std::string, osg::BoundingBoxd> createB3DM(const std::vector<InstanceRef>& instances, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams = SimplificationParams());
    std::string createI3DM(MeshInstanceInfo* meshInfo, const std::vector<int>& transformIndices, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams = SimplificationParams());

    // Helpers
    void writeTilesetJson(const std::string& basePath, const osg::BoundingBox& globalBounds, const nlohmann::json& rootContent);
};
