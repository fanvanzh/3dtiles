#pragma once

#include "fbx.h"
#include <string>
#include <vector>
#include <osg/Matrixd>
#include <osg/BoundingBox>
#include <osg/Geometry>
#include <nlohmann/json.hpp>
#include "mesh_processor.h"

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
    std::vector<float> lodRatios = {1.0f, 0.5f, 0.25f}; // Default LOD ratios (Fine to Coarse)

    // Geolocation (Origin)
    double longitude = 0.0;
    double latitude = 0.0;
    double height = 0.0;
};

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
    nlohmann::json processNode(OctreeNode* node, const std::string& parentPath);

    // Converters
    // Returns filename created
    std::string createB3DM(const std::vector<InstanceRef>& instances, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams = SimplificationParams());
    std::string createI3DM(MeshInstanceInfo* meshInfo, const std::vector<int>& transformIndices, const std::string& tilePath, const std::string& tileName, const SimplificationParams& simParams = SimplificationParams());

    // Helpers
    void writeTilesetJson(const std::string& basePath, const osg::BoundingBox& globalBounds, const nlohmann::json& rootContent);
};
