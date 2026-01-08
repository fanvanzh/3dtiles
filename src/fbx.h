#pragma once

#include <osg/Node>
#include <osg/ref_ptr>
#include <ufbx.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Mesh合并与属性挂载辅助结构
struct MeshKey {
    std::string geomHash; // mesh内容hash
    std::string matHash;  // 材质hash
    bool operator==(const MeshKey &o) const { return geomHash == o.geomHash && matHash == o.matHash; }
};
namespace std {
    template<>
    struct hash<MeshKey> {
        size_t operator()(const MeshKey &k) const {
            return hash<std::string>()(k.geomHash) ^ (hash<std::string>()(k.matHash) << 1);
        }
    };
}

struct MeshInstanceInfo {
    MeshKey key;
    osg::ref_ptr<osg::Geometry> geometry;
    std::vector<osg::Matrixd> transforms; // 所有引用该mesh的节点变换
    std::vector<std::string> nodeNames;   // 所有引用该mesh的节点名
    std::vector<std::unordered_map<std::string, std::string>> nodeAttrs; // 节点属性
    int featureId = -1; // 合并后featureId
};

class FBXLoader {
public:
    FBXLoader(const std::string &filename);
    ~FBXLoader();

    void load();

    osg::ref_ptr<osg::Node> getRoot() const { return _root; }

    // 全局mesh池，key为MeshKey，value为合并信息
    std::unordered_map<MeshKey, MeshInstanceInfo> meshPool;

    // 节点名到featureId映射
    std::unordered_map<std::string, int> nodeFeatureIdMap;

    // 递归构建 OSG 层级并收集 mesh/属性
    osg::ref_ptr<osg::Node> loadNode(ufbx_node *node, const osg::Matrixd &parentXform = osg::Matrixd::identity());

    // 缓存已处理的 Mesh，避免重复计算 (ufbx_mesh* -> list of geometries)
    struct CachedPart {
        osg::ref_ptr<osg::Geometry> geometry;
        std::string geomHash;
        std::string matHash;
    };
    std::unordered_map<const ufbx_mesh*, std::vector<CachedPart>> meshCache;

    // 缓存已处理的材质 (ufbx_material* -> osg::StateSet*)
    std::unordered_map<const ufbx_material*, osg::ref_ptr<osg::StateSet>> materialCache;
    // 基于材质内容哈希的去重缓存 (hash -> osg::StateSet*)
    std::unordered_map<std::string, osg::ref_ptr<osg::StateSet>> materialHashCache;
    // 基于几何内容哈希的去重缓存 (hash -> osg::Geometry*)
    std::unordered_map<std::string, osg::ref_ptr<osg::Geometry>> geometryHashCache;

    // 处理 Mesh 并返回 Geode (如果需要挂载到场景)
    osg::ref_ptr<osg::Geode> processMesh(ufbx_node *node, ufbx_mesh *mesh, const osg::Matrixd &globalXform);

    // 创建或获取缓存的 StateSet
    osg::StateSet* getOrCreateStateSet(const ufbx_material* mat);

    // 工具：计算mesh内容hash、材质hash、收集FBX属性
    static std::string calcMeshHash(const ufbx_mesh *mesh);
    static std::string calcMaterialHash(const ufbx_material *mat);
    static std::unordered_map<std::string, std::string> collectNodeAttrs(const ufbx_node *node);

    struct DedupStats {
        int material_created;
        int material_hash_reused;
        int material_ptr_reused;
        int geometry_created;
        int geometry_hash_reused;
        int mesh_cache_hit_count;
        size_t unique_statesets;
        size_t unique_geometries;
    };
    DedupStats getStats() const;

private:
    ufbx_scene *scene = nullptr;
    std::string source_filename;
    osg::ref_ptr<osg::Node> _root;
    int material_created_count = 0;
    int material_reused_hash_count = 0;
    int material_reused_ptr_count = 0;
    int geometry_created_count = 0;
    int geometry_reused_hash_count = 0;
    int mesh_cache_hit_count = 0;
    std::unordered_set<const ufbx_node*> displayLayerHiddenNodes;
};
