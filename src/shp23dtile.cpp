#include "tiny_gltf.h"
#include "earcut.hpp"
#include "extern.h"
#include "mesh_processor.h"
#include "attribute_storage.h"
#include "GeoTransform.h"
#include "lod_pipeline.h"
#include "shape.h"

#include "json.hpp"

/* vcpkg path */
#include <ogrsf_frmts.h>

#include <optional>
#include <fstream>
#include <osg/Material>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <osg/Geometry>
#include <osg/Geode>
#include <osgUtil/DelaunayTriangulator>
#include <osgUtil/Tessellator>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <vector>
#include <array>
#include <filesystem>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <set>
#include <limits>
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace std;

using Vextex = vector<array<float, 3>>;
using Normal = vector<array<float, 3>>;
using Index = vector<array<int, 3>>;

struct bbox
{
    bool isAdd = false;
    double minx, maxx, miny, maxy;
    bbox() {}
    bbox(double x0, double x1, double y0, double y1) {
        minx = x0, maxx = x1, miny = y0, maxy = y1;
    }

    bool contains(double x, double y) {
        return minx <= x
        && x <= maxx
        && miny <= y
        && y <= maxy;
    }

    bool contains(bbox& other) {
        return contains(other.minx, other.miny)
        && contains(other.maxx, other.maxy);
    }

    bool intersect(bbox& other) {
        return !(
            other.minx > maxx
                 || other.maxx < minx
                 || other.miny > maxy
                 || other.maxy < miny);
    }
};

class node {
public:
    bbox _box;
    // 1 km ~ 0.01
    double metric = 0.01;
    node* subnode[4];
    std::vector<int> geo_items;
public:
    int _x = 0;
    int _y = 0;
    int _z = 0;

    void set_no(int x, int y, int z) {
        _x = x;
        _y = y;
        _z = z;
    }

public:

    node(bbox& box) {
        _box = box;
        for (int i = 0; i < 4; i++) {
            subnode[i] = 0;
        }
    }

    ~node() {
        for (int i = 0; i < 4; i++) {
            if (subnode[i]) {
                delete subnode[i];
            }
        }
    }

    void split() {
        double c_x = (_box.minx + _box.maxx) / 2.0;
        double c_y = (_box.miny + _box.maxy) / 2.0;
        for (int i = 0; i < 4; i++) {
            if (!subnode[i]) {
                switch (i) {
                    case 0:
                    {
                        bbox box(_box.minx, c_x, _box.miny, c_y);
                        subnode[i] = new node(box);
                        subnode[i]->set_no(_x * 2, _y * 2, _z + 1);
                    }
                    break;
                    case 1:
                    {
                        bbox box(c_x, _box.maxx, _box.miny, c_y);
                        subnode[i] = new node(box);
                        subnode[i]->set_no(_x * 2 + 1, _y * 2, _z + 1);
                    }
                    break;
                    case 2:
                    {
                        bbox box(c_x, _box.maxx, c_y, _box.maxy);
                        subnode[i] = new node(box);
                        subnode[i]->set_no(_x * 2 + 1, _y * 2 + 1, _z + 1);
                    }
                    break;
                    case 3:
                    {
                        bbox box(_box.minx, c_x, c_y, _box.maxy);
                        subnode[i] = new node(box);
                        subnode[i]->set_no(_x * 2, _y * 2 + 1, _z + 1);
                    }
                    break;
                }
            }
        }
    }

    void add(int id, bbox& box) {
        if (!_box.intersect(box)) {
            return;
        }
        if (_box.maxx - _box.minx < metric) {
            if (!box.isAdd){
                geo_items.push_back(id);
                box.isAdd = true;
            }
            return;
        }
        if (_box.intersect(box)) {
            if (subnode[0] == 0) {
                split();
            }
            for (int i = 0; i < 4; i++) {
                subnode[i]->add(id, box);
		//when box is added to a node, stop the loop
		if (box.isAdd) {
		    break;
		}
            }
        }
    }

    std::vector<int>& get_ids() {
        return geo_items;
    }

    void get_all(std::vector<void*>& items_array) {
        if (!geo_items.empty()) {
            items_array.push_back(this);
        }
        if (subnode[0] != 0) {
            for (int i = 0; i < 4; i++) {
                subnode[i]->get_all(items_array);
            }
        }
    }
};

struct TileBBox {
    double minx = 0.0; // degrees
    double maxx = 0.0; // degrees
    double miny = 0.0; // degrees
    double maxy = 0.0; // degrees
    double minHeight = 0.0; // meters
    double maxHeight = 0.0; // meters
};

struct TileMeta {
    int z = 0;
    int x = 0;
    int y = 0;
    TileBBox bbox;
    double geometric_error = 0.0;
    std::string tileset_rel; // relative to output root
    std::string orig_tileset_rel; // original flat path (tile/z/x/y.json) used during generation
    bool is_leaf = false;
    std::vector<uint64_t> children_keys;
    double max_child_ge = 0.0; // used when aggregating
};

// Build a deterministic nested path where each child resides inside its parent's
// directory: children/<z_x_y>/tileset.json. This keeps content URIs as "./..." with
// no parent-directory traversal.
static std::string tileset_path_for_node(int z, int x, int y, int min_z) {
    if (z <= min_z) {
        return "tileset.json"; // root tileset at output root
    }
    std::vector<std::string> segments;
    int cz = z;
    int cx = x;
    int cy = y;
    while (cz > min_z) {
        std::string id = std::to_string(cz) + "_" + std::to_string(cx) + "_" + std::to_string(cy);
        segments.push_back("children/" + id);
        cz -= 1;
        cx /= 2;
        cy /= 2;
    }
    std::filesystem::path p;
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        p /= *it;
    }
    p /= "tileset.json";
    return p.generic_string();
}

static inline uint64_t encode_key(int z, int x, int y) {
    return (static_cast<uint64_t>(z) << 42) | (static_cast<uint64_t>(x) << 21) | static_cast<uint64_t>(y);
}

static TileBBox make_bbox_from_node(const bbox& b, double min_h, double max_h) {
    TileBBox r;
    r.minx = b.minx;
    r.maxx = b.maxx;
    r.miny = b.miny;
    r.maxy = b.maxy;
    r.minHeight = min_h;
    r.maxHeight = max_h;
    return r;
}

static TileBBox merge_bbox(const TileBBox& a, const TileBBox& b) {
    TileBBox r;
    r.minx = std::min(a.minx, b.minx);
    r.maxx = std::max(a.maxx, b.maxx);
    r.miny = std::min(a.miny, b.miny);
    r.maxy = std::max(a.maxy, b.maxy);
    r.minHeight = std::min(a.minHeight, b.minHeight);
    r.maxHeight = std::max(a.maxHeight, b.maxHeight);
    return r;
}

struct Polygon_Mesh
{
    std::string mesh_name;
    Vextex vertex;
    Index  index;
    Normal normal;
    // add some addition
    float height;
    // Arbitrary feature properties from the source shapefile (per-building)
    std::map<std::string, nlohmann::json> properties;
};

static std::vector<double> flatten_mat(const glm::dmat4& m) {
    std::vector<double> mat(16, 0.0);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            mat[c * 4 + r] = m[c][r];
        }
    }
    return mat;
}

static glm::dmat4 make_transform(double center_lon_deg, double center_lat_deg, double min_height) {
    // Reuse the ENU->ECEF transform used in the osgb pipeline for consistent placement
    return GeoTransform::CalcEnuToEcefMatrix(center_lon_deg, center_lat_deg, min_height);
}

static nlohmann::json box_to_json(double cx, double cy, double cz, double half_w, double half_h, double half_z) {
    double vals[12] = {
        cx, cy, cz,
        half_w, 0.0, 0.0,
        0.0, half_h, 0.0,
        0.0, 0.0, half_z
    };
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < 12; ++i) arr.push_back(vals[i]);
    return arr;
}

static double compute_geometric_error_from_spans(double span_x, double span_y, double span_z) {
    double max_span = std::max({span_x, span_y, span_z});
    if (max_span <= 0.0) {
        return 0.0;
    }
    return max_span / 20.0;
}

static bool write_node_tileset(const TileMeta& node,
                               const std::unordered_map<uint64_t, TileMeta>& nodes,
                               const std::string& dest_root,
                               int min_z_root) {
    // parent bbox in degrees/meters
    double center_lon = (node.bbox.minx + node.bbox.maxx) * 0.5;
    double center_lat = (node.bbox.miny + node.bbox.maxy) * 0.5;
    double width_deg = (node.bbox.maxx - node.bbox.minx);
    double height_deg = (node.bbox.maxy - node.bbox.miny);
    double lon_rad_span = degree2rad(width_deg);
    double lat_rad_span = degree2rad(height_deg);
    // Pad bounding volumes to reduce near-plane/frustum culling when zoomed in
    const double BV_SCALE = 2.0;
    double half_w = longti_to_meter(lon_rad_span * 0.5, degree2rad(center_lat)) * 1.05 * BV_SCALE;
    double half_h = lati_to_meter(lat_rad_span * 0.5) * 1.05 * BV_SCALE;
    double half_z = (node.bbox.maxHeight - node.bbox.minHeight) * 0.5 * BV_SCALE;
    double min_h = node.bbox.minHeight;

    glm::dmat4 parent_global = make_transform(center_lon, center_lat, min_h);

    nlohmann::json root;
    root["asset"] = { {"version", "1.0"}, {"gltfUpAxis", "Z"} };
    root["geometricError"] = node.geometric_error;

    nlohmann::json root_node;
    // Apply global transform only on the root-level tileset; other nodes inherit via parent translations
    if (node.z == min_z_root) {
        root_node["transform"] = flatten_mat(parent_global);
    }
    root_node["boundingVolume"]["box"] = box_to_json(0.0, 0.0, half_z, half_w, half_h, half_z);
    root_node["refine"] = "REPLACE";
    root_node["geometricError"] = node.geometric_error;

    for (auto child_key : node.children_keys) {
        auto it = nodes.find(child_key);
        if (it == nodes.end()) {
            continue;
        }
        const TileMeta& child = it->second;
        nlohmann::json child_node;
        double child_center_lon = (child.bbox.minx + child.bbox.maxx) * 0.5;
        double child_center_lat = (child.bbox.miny + child.bbox.maxy) * 0.5;
        double child_lon_span = degree2rad(child.bbox.maxx - child.bbox.minx);
        double child_lat_span = degree2rad(child.bbox.maxy - child.bbox.miny);
        double child_half_w = longti_to_meter(child_lon_span * 0.5, degree2rad(child_center_lat)) * 1.05 * BV_SCALE;
        double child_half_h = lati_to_meter(child_lat_span * 0.5) * 1.05 * BV_SCALE;
        double child_half_z = (child.bbox.maxHeight - child.bbox.minHeight) * 0.5 * BV_SCALE;
        double child_min_h = child.bbox.minHeight;

        // offsets in parent's local ENU frame
        double east_offset = longti_to_meter(degree2rad(child_center_lon - center_lon), degree2rad(center_lat));
        double north_offset = lati_to_meter(degree2rad(child_center_lat - center_lat));
        double up_offset = child_min_h - min_h;
        double child_center_z = up_offset + child_half_z;

        child_node["boundingVolume"]["box"] = box_to_json(
            east_offset,
            north_offset,
            child_center_z,
            child_half_w,
            child_half_h,
            child_half_z);
        child_node["refine"] = "REPLACE";
        child_node["geometricError"] = child.geometric_error;

        // Child transform: relative matrix = parent_global^-1 * child_global to preserve hierarchy in ECEF
        glm::dmat4 child_global = make_transform(child_center_lon, child_center_lat, child_min_h);
        glm::dmat4 relative = glm::inverse(parent_global) * child_global;
        child_node["transform"] = flatten_mat(relative);

        std::filesystem::path parent_path = std::filesystem::path(dest_root) / node.tileset_rel;
        std::filesystem::path parent_dir = parent_path.parent_path();
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);

        // Child lives in parent_dir/children/<id>/tileset.json
        std::string child_id = std::to_string(child.z) + "_" + std::to_string(child.x) + "_" + std::to_string(child.y);
        child_node["content"]["uri"] = "./children/" + child_id + "/tileset.json";

        root_node["children"].push_back(child_node);
    }

    root["root"] = root_node;

    std::filesystem::path out_path = std::filesystem::path(dest_root) / node.tileset_rel;
    std::filesystem::create_directories(out_path.parent_path());

    std::ofstream ofs(out_path);
    if (!ofs.is_open()) {
        LOG_E("write file %s fail", out_path.string().c_str());
        return false;
    }
    ofs << root.dump(2);
    return true;
}

static void build_hierarchical_tilesets(const std::vector<TileMeta>& leaves,
                                        const std::string& dest_root) {
    constexpr int MAX_LEVELS = 4; // root + 3 levels of depth to keep hierarchy shallow
    if (leaves.empty()) return;

    if (leaves.size() == 1) {
        // trivial case: wrap single leaf into a root tileset that references it
        std::unordered_map<uint64_t, TileMeta> nodes;
        const auto& leaf = leaves.front();
        uint64_t leaf_key = encode_key(leaf.z, leaf.x, leaf.y);
        nodes[leaf_key] = leaf;

        TileMeta root;
        root.z = std::max(leaf.z - 1, 0); // virtual parent level
        root.x = leaf.x / 2;
        root.y = leaf.y / 2;
        root.bbox = leaf.bbox;
        root.geometric_error = leaf.geometric_error * 2.0;
        root.tileset_rel = "tileset.json";
        root.is_leaf = false;
        root.children_keys.push_back(leaf_key);
        nodes[encode_key(root.z, root.x, root.y)] = root;

        write_node_tileset(root, nodes, dest_root, root.z);
        return;
    }

    std::unordered_map<uint64_t, TileMeta> nodes;
    std::vector<uint64_t> current_keys;
    int max_z = 0;
    int min_z = std::numeric_limits<int>::max();

    for (const auto& leaf : leaves) {
        uint64_t key = encode_key(leaf.z, leaf.x, leaf.y);
        nodes[key] = leaf;
        current_keys.push_back(key);
        max_z = std::max(max_z, leaf.z);
        min_z = std::min(min_z, leaf.z);
    }

    std::vector<std::vector<uint64_t>> levels;
    levels.push_back(current_keys);

    while (current_keys.size() > 1) {
        if (levels.size() >= MAX_LEVELS) {
            break; // stop merging to avoid too deep hierarchy
        }
        std::unordered_map<uint64_t, TileMeta> parent_level;
        std::set<uint64_t> parent_keys;
        for (auto key : current_keys) {
            const TileMeta& child = nodes[key];
            int pz = child.z - 1;
            if (pz < 0) continue;
            int px = child.x / 2;
            int py = child.y / 2;
            uint64_t pkey = encode_key(pz, px, py);
            auto it = parent_level.find(pkey);
            if (it == parent_level.end()) {
                TileMeta parent;
                parent.z = pz;
                parent.x = px;
                parent.y = py;
                parent.is_leaf = false;
                parent.bbox = child.bbox;
                parent.max_child_ge = child.geometric_error;
                parent.children_keys.push_back(key);
                parent.tileset_rel = (std::filesystem::path("tile") / std::to_string(pz) / std::to_string(px) / std::to_string(py) / "tileset.json").generic_string();
                parent_level[pkey] = parent;
            } else {
                it->second.bbox = merge_bbox(it->second.bbox, child.bbox);
                it->second.max_child_ge = std::max(it->second.max_child_ge, child.geometric_error);
                it->second.children_keys.push_back(key);
            }
            parent_keys.insert(pkey);
        }

        for (auto& kv : parent_level) {
            kv.second.geometric_error = kv.second.max_child_ge * 2.0;
            nodes[kv.first] = kv.second;
        }

        current_keys.assign(parent_keys.begin(), parent_keys.end());
        levels.push_back(current_keys);
    }

    // If we stopped early (more than one root candidate), create a synthetic root to bind them
    if (current_keys.size() > 1) {
        TileMeta root;
        root.is_leaf = false;
        root.z = nodes[current_keys.front()].z - 1;
        root.x = 0;
        root.y = 0;
        root.bbox = nodes[current_keys.front()].bbox;
        root.max_child_ge = 0.0;
        for (auto key : current_keys) {
            const auto& child = nodes[key];
            root.bbox = merge_bbox(root.bbox, child.bbox);
            root.max_child_ge = std::max(root.max_child_ge, child.geometric_error);
            root.children_keys.push_back(key);
        }
        root.geometric_error = root.max_child_ge * 2.0;
        uint64_t root_key = encode_key(root.z, root.x, root.y);
        nodes[root_key] = root;
        current_keys = {root_key};
        levels.push_back(current_keys);
    }

    // Determine actual root level (minimum z across all nodes) and assign nested paths
    int min_z_all = std::numeric_limits<int>::max();
    if (!current_keys.empty()) {
        for (const auto& kv : nodes) {
            min_z_all = std::min(min_z_all, kv.second.z);
        }
        std::unordered_map<uint64_t, TileMeta> updated;
        for (auto& kv : nodes) {
            TileMeta meta = kv.second;
            meta.tileset_rel = tileset_path_for_node(meta.z, meta.x, meta.y, min_z_all);
            updated[kv.first] = meta;
        }
        nodes = std::move(updated);
    }

    // Relocate leaves into nested structure while preserving per-LOD content names
    std::vector<uint64_t> leaf_keys;
    for (const auto& kv : nodes) {
        if (kv.second.is_leaf) leaf_keys.push_back(kv.first);
    }

    for (auto key : leaf_keys) {
        auto it = nodes.find(key);
        if (it == nodes.end()) continue;
        TileMeta meta = it->second;
        std::filesystem::path src_json = std::filesystem::path(dest_root) / meta.orig_tileset_rel;
        std::filesystem::path src_dir = src_json.parent_path();
        std::filesystem::path dst_json = std::filesystem::path(dest_root) / meta.tileset_rel;
        std::filesystem::path dst_dir = dst_json.parent_path();
        std::filesystem::create_directories(dst_dir);
        // Copy/move all b3dm under src_dir (covers content_lod*.b3dm)
        std::error_code ec;
        for (auto const& entry : std::filesystem::directory_iterator(src_dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".b3dm") continue;
            std::filesystem::path dst_b3dm = dst_dir / entry.path().filename();
            std::filesystem::rename(entry.path(), dst_b3dm, ec);
            if (ec) {
                std::filesystem::copy_file(entry.path(), dst_b3dm, std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(entry.path());
            }
        }

        // Copy/move json as-is (content URIs already relative to its directory)
        std::filesystem::rename(src_json, dst_json, ec);
        if (ec) {
            std::ifstream ifs(src_json);
            if (!ifs.is_open()) {
                LOG_E("open leaf tileset %s fail", src_json.string().c_str());
            } else {
                nlohmann::json leaf;
                ifs >> leaf;
                ifs.close();
                std::ofstream ofs(dst_json);
                if (ofs.is_open()) {
                    ofs << leaf.dump(2);
                } else {
                    LOG_E("write leaf tileset %s fail", dst_json.string().c_str());
                }
                std::filesystem::remove(src_json);
            }
        }

        // update map
        nodes[key] = meta;
    }

    // Drop flat intermediate tile/ directory to avoid confusion and stale files
    {
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(dest_root) / "tile", ec);
        if (ec) {
            LOG_E("remove flat tile dir failed: %s", ec.message().c_str());
        }
    }

    // write parents from bottom (high z) to top
    std::vector<TileMeta> parents;
    for (const auto& kv : nodes) {
        if (!kv.second.is_leaf) {
            parents.push_back(kv.second);
        }
    }
    std::sort(parents.begin(), parents.end(), [](const TileMeta& a, const TileMeta& b) {
        return a.z > b.z; // write deeper levels first
    });

    for (const auto& parent : parents) {
        write_node_tileset(parent, nodes, dest_root, min_z_all);
    }
}

osg::ref_ptr<osg::Geometry> make_triangle_mesh_auto(Polygon_Mesh& mesh) {
    osg::ref_ptr<osg::Vec3Array> va = new osg::Vec3Array(mesh.vertex.size());
    for (int i = 0; i < mesh.vertex.size(); i++) {
        (*va)[i].set(mesh.vertex[i][0], mesh.vertex[i][1], mesh.vertex[i][2]);
    }
    osg::ref_ptr<osgUtil::DelaunayTriangulator> trig = new osgUtil::DelaunayTriangulator();
    trig->setInputPointArray(va);
    osg::Vec3Array *norms = new osg::Vec3Array;
    trig->setOutputNormalArray(norms);
    trig->triangulate();
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setVertexArray(va);
    geometry->setNormalArray(norms);
    auto* uIntId = trig->getTriangles();
    osg::DrawElementsUShort* _set = new osg::DrawElementsUShort(osg::DrawArrays::TRIANGLES);
    for (unsigned int i = 0; i < uIntId->getNumPrimitives(); i++) {
        _set->addElement(uIntId->getElement(i));
    }
    geometry->addPrimitiveSet(_set);
    return geometry;
}

osg::ref_ptr<osg::Geometry> make_triangle_mesh(Polygon_Mesh& mesh) {
    osg::ref_ptr<osg::Vec3Array> va = new osg::Vec3Array(mesh.vertex.size());
    for (int i = 0; i < mesh.vertex.size(); i++) {
        (*va)[i].set(mesh.vertex[i][0], mesh.vertex[i][1], mesh.vertex[i][2]);
    }
    osg::ref_ptr<osg::Vec3Array> vn = new osg::Vec3Array(mesh.normal.size());
    for (int i = 0; i < mesh.normal.size(); i++) {
        (*vn)[i].set(mesh.normal[i][0], mesh.normal[i][1], mesh.normal[i][2]);
    }
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setVertexArray(va);
    geometry->setNormalArray(vn);
    osg::DrawElementsUShort* _set = new osg::DrawElementsUShort(osg::DrawArrays::TRIANGLES);
    for (int i = 0; i < mesh.index.size(); i++) {
        _set->addElement(mesh.index[i][0]);
        _set->addElement(mesh.index[i][1]);
        _set->addElement(mesh.index[i][2]);
    }
    geometry->addPrimitiveSet(_set);
    //osgUtil::SmoothingVisitor::smooth(*geometry);
    return geometry;
}

void calc_normal(int baseCnt, int ptNum, Polygon_Mesh &mesh)
{
    // normal stand for one triangle
    for (int i = 0; i < ptNum; i+=2) {
        osg::Vec2 *nor1 = 0;
        nor1 = new osg::Vec2(mesh.vertex[baseCnt + 2 * (i + 1)][0], mesh.vertex[baseCnt + 2 * (i + 1)][1]);
        *nor1 = *nor1 - osg::Vec2(mesh.vertex[baseCnt + 2 * i][0], mesh.vertex[baseCnt + 2 * i][1]);
        osg::Vec3 nor3 = osg::Vec3(-nor1->y(), nor1->x(), 0);
        nor3.normalize();
        delete nor1;
        mesh.normal.push_back({ nor3.x(), nor3.y(), nor3.z() });
        mesh.normal.push_back({ nor3.x(), nor3.y(), nor3.z() });
        mesh.normal.push_back({ nor3.x(), nor3.y(), nor3.z() });
        mesh.normal.push_back({ nor3.x(), nor3.y(), nor3.z() });
    }
}

Polygon_Mesh
convert_polygon(OGRPolygon* polyon, double center_x, double center_y, double height)
{
    //double bottom = 0.0;
    Polygon_Mesh mesh;
    OGRLinearRing* pRing = polyon->getExteriorRing();
    int ptNum = pRing->getNumPoints();
    if (ptNum < 4) {
        return mesh;
    }
    int pt_count = 0;
    for (int i = 0; i < ptNum; i++) {
        OGRPoint pt;
        pRing->getPoint(i, &pt);
		double bottom = pt.getZ();
        float point_x = (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y));
        float point_y = (float)lati_to_meter(degree2rad(pt.getY() - center_y));
        mesh.vertex.push_back({ point_x , point_y, (float)bottom });
        mesh.vertex.push_back({ point_x , point_y, (float)height });
        // double vertex
        if (i != 0 && i != ptNum - 1) {
            mesh.vertex.push_back({ point_x , point_y, (float)bottom });
            mesh.vertex.push_back({ point_x , point_y, (float)height });
        }
    }
    int vertex_num = mesh.vertex.size() / 2;
    for (int i = 0; i < vertex_num; i += 2) {
        if (i != vertex_num - 1) {
            mesh.index.push_back({ 2 * i,2 * i + 1,2 * (i + 1) + 1 });
            mesh.index.push_back({ 2 * (i + 1),2 * i,2 * (i + 1) + 1 });
        }
    }
    calc_normal(0, vertex_num, mesh);
    pt_count += 2 * vertex_num;

    int inner_count = polyon->getNumInteriorRings();
    for (int j = 0; j < inner_count; j++) {
        OGRLinearRing* pRing = polyon->getInteriorRing(j);
        int ptNum = pRing->getNumPoints();
        if (ptNum < 4) {
            continue;
        }
        for (int i = 0; i < ptNum; i++) {
            OGRPoint pt;
            pRing->getPoint(i, &pt);
			double bottom = pt.getZ();
            float point_x = (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y));
            float point_y = (float)lati_to_meter(degree2rad(pt.getY() - center_y));
            mesh.vertex.push_back({ point_x , point_y, (float)bottom });
            mesh.vertex.push_back({ point_x , point_y, (float)height });
            // double vertex
            if (i != 0 && i != ptNum - 1) {
                mesh.vertex.push_back({ point_x , point_y, (float)bottom });
                mesh.vertex.push_back({ point_x , point_y, (float)height });
            }
        }
        vertex_num = mesh.vertex.size() / 2 - pt_count;
        for (int i = 0; i < vertex_num; i += 2) {
            if (i != vertex_num - 1) {
                mesh.index.push_back({ pt_count + 2 * i, pt_count + 2 * i + 1, pt_count + 2 * (i + 1) });
                mesh.index.push_back({ pt_count + 2 * (i + 1), pt_count + 2 * i, pt_count + 2 * (i + 1) });
            }
        }
        calc_normal(pt_count, ptNum, mesh);
        pt_count = mesh.vertex.size();
    }
    // top and bottom
    {
        using Point = std::array<double, 2>;
        std::vector<std::vector<Point>> polygon(1);
        {
            OGRLinearRing* pRing = polyon->getExteriorRing();
            int ptNum = pRing->getNumPoints();
            for (int i = 0; i < ptNum; i++)
            {
                OGRPoint pt;
                pRing->getPoint(i, &pt);
				double bottom = pt.getZ();
                float point_x = (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y));
                float point_y = (float)lati_to_meter(degree2rad(pt.getY() - center_y));
                polygon[0].push_back({ point_x, point_y });
                mesh.vertex.push_back({ point_x , point_y, (float)bottom });
                mesh.vertex.push_back({ point_x , point_y, (float)height });
                mesh.normal.push_back({ 0,0,-1 });
                mesh.normal.push_back({ 0,0,1 });
            }
        }
        int inner_count = polyon->getNumInteriorRings();
        for (int j = 0; j < inner_count; j++)
        {
            polygon.resize(polygon.size() + 1);
            OGRLinearRing* pRing = polyon->getInteriorRing(j);
            int ptNum = pRing->getNumPoints();
            for (int i = 0; i < ptNum; i++)
            {
                OGRPoint pt;
                pRing->getPoint(i, &pt);
				double bottom = pt.getZ();
                float point_x = (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y));
                float point_y = (float)lati_to_meter(degree2rad(pt.getY() - center_y));
                polygon[j].push_back({ point_x, point_y });
                mesh.vertex.push_back({ point_x , point_y, (float)bottom });
                mesh.vertex.push_back({ point_x , point_y, (float)height });
                mesh.normal.push_back({ 0,0,-1 });
                mesh.normal.push_back({ 0,0,1 });
            }
        }
        std::vector<int> indices = mapbox::earcut<int>(polygon);
        for (int idx = 0; idx < indices.size(); idx += 3) {
            mesh.index.push_back({
                pt_count + 2 * indices[idx],
                pt_count + 2 * indices[idx + 2],
                pt_count + 2 * indices[idx + 1] });
        }
        for (int idx = 0; idx < indices.size(); idx += 3) {
            mesh.index.push_back({
                pt_count + 2 * indices[idx] + 1,
                pt_count + 2 * indices[idx + 1] + 1,
                pt_count + 2 * indices[idx + 2] + 1});
        }
    }
    return mesh;
}

std::string make_polymesh(std::vector<Polygon_Mesh>& meshes,
    bool enable_simplify = false,
    std::optional<SimplificationParams> simplification_params = std::nullopt,
    bool enable_draco = false,
    std::optional<DracoCompressionParams> draco_params = std::nullopt);

std::string make_b3dm(std::vector<Polygon_Mesh>& meshes,
    bool with_height = false,
    bool enable_simplify = false,
    std::optional<SimplificationParams> simplification_params = std::nullopt,
    bool enable_draco = false,
    std::optional<DracoCompressionParams> draco_params = std::nullopt);
//
extern "C" bool
shp23dtile(const ShapeConversionParams* params)
{
    if (!params || !params->input_path || !params->output_path) {
        LOG_E("make shp23dtile failed: invalid parameters");
        return false;
    }

    const char* filename = params->input_path;
    const char* dest = params->output_path;
    std::string height_field = "";
    if (params->height_field) {
        height_field = params->height_field;
    }

    // Build LOD configuration from params
    LODPipelineSettings lod_cfg;
    if (params->enable_lod) {
        // Use default LOD configuration: [1.0, 0.5, 0.25]
        std::vector<float> default_ratios = {1.0f, 0.5f, 0.25f};
        float default_base_error = 0.01f;
        bool default_draco_for_lod0 = false;  // Don't apply Draco to highest detail LOD

        lod_cfg.enable_lod = true;
        lod_cfg.levels = build_lod_levels(
            default_ratios,
            default_base_error,
            params->simplify_params,
            params->draco_compression_params,
            default_draco_for_lod0
        );
    } else {
        lod_cfg.enable_lod = false;
    }

    // Use configuration from params
    const SimplificationParams& simplify_params = params->simplify_params;
    const DracoCompressionParams& draco_params = params->draco_compression_params;

    int layer_id = params->layer_id;
    GDALAllRegister();

    // Ensure destination directory exists before creating any auxiliary files (e.g., attributes.db)
    std::error_code mkdir_ec;
    std::filesystem::create_directories(std::filesystem::path(dest), mkdir_ec);

    GDALDataset* poDS = (GDALDataset*)GDALOpenEx(
        filename, GDAL_OF_VECTOR,
        NULL, NULL, NULL);
    if (poDS == NULL)
    {
        LOG_E("open shapefile [%s] failed", filename);
        return false;
    }
    OGRLayer  *poLayer;
    poLayer = poDS->GetLayer(layer_id);
    if (!poLayer) {
        GDALClose(poDS);
        LOG_E("open layer [%s]:[%d] failed", filename, layer_id);
        return false;
    }

    // Store feature attributes to SQLite database using RAII wrapper
    char sqlite_path[512];
#ifdef _WIN32
    sprintf(sqlite_path, "%s\\attributes.db", dest);
#else
    sprintf(sqlite_path, "%s/attributes.db", dest);
#endif

    {
        // RAII: AttributeStorage will auto-commit and close on scope exit
        AttributeStorage attr_storage(sqlite_path);

        if (!attr_storage.isOpen()) {
            LOG_E("Failed to open attribute database: %s", attr_storage.getLastError().c_str());
        } else {
            // Create table schema
            if (!attr_storage.createTable(poLayer->GetLayerDefn())) {
                LOG_E("Failed to create table: %s", attr_storage.getLastError().c_str());
            } else {
                // Insert all features in batches (1000 features per transaction)
                // This prevents data loss in case of errors during bulk insert
                attr_storage.insertFeaturesInBatches(poLayer, 1000);
            }
        }
        // Database automatically closed and committed here (RAII)
    }
    OGRwkbGeometryType _t = poLayer->GetGeomType();
    if (_t != wkbPolygon && _t != wkbMultiPolygon &&
        _t != wkbPolygon25D && _t != wkbMultiPolygon25D)
    {
        GDALClose(poDS);
        LOG_E("only support polyon now");
        return false;
    }

    OGREnvelope envelop;
    OGRErr err = poLayer->GetExtent(&envelop);
    if (err != OGRERR_NONE) {
        LOG_E("no extent found in shapefile");
        return false;
    }
    if (envelop.MaxX > 180 || envelop.MinX < -180 || envelop.MaxY > 90 || envelop.MinY < -90) {
        LOG_E("only support WGS-84 now");
        return false;
    }

    bbox bound(envelop.MinX, envelop.MaxX, envelop.MinY, envelop.MaxY);
    node root(bound);
    OGRFeature *poFeature;
    poLayer->ResetReading();
    while ((poFeature = poLayer->GetNextFeature()) != NULL)
    {
        OGRGeometry *poGeometry;
        poGeometry = poFeature->GetGeometryRef();
        if (poGeometry == NULL) {
            OGRFeature::DestroyFeature(poFeature);
            continue;
        }
        OGREnvelope envelop;
        poGeometry->getEnvelope(&envelop);
        bbox bound(envelop.MinX, envelop.MaxX, envelop.MinY, envelop.MaxY);
        unsigned long long id = poFeature->GetFID();
        root.add(id, bound);
        OGRFeature::DestroyFeature(poFeature);
    }
    // iter all node and convert to obj
    std::vector<void*> items_array;
    root.get_all(items_array);
    //
    int field_index = -1;
    std::vector<TileMeta> leaf_tiles;

    if (!height_field.empty()) {
        field_index = poLayer->GetLayerDefn()->GetFieldIndex(height_field.c_str());
        if (field_index == -1) {
            LOG_E("can`t found field [%s] in [%s]", height_field.c_str(), filename);
        }
    }
    OGRFeatureDefn* layer_defn = poLayer->GetLayerDefn();

    for (auto item : items_array) {
        node* _node = (node*)item;
        std::filesystem::path tile_dir = std::filesystem::path(dest) / "tile" / std::to_string(_node->_z) / std::to_string(_node->_x);
        std::error_code ec;
        std::filesystem::create_directories(tile_dir, ec);
        if (ec) {
            LOG_E("create directory %s fail", tile_dir.string().c_str());
        }
        // fix the box
        {
            OGREnvelope node_box;
            for (auto id : _node->get_ids()) {
                OGRFeature *poFeature = poLayer->GetFeature(id);
                OGRGeometry* poGeometry = poFeature->GetGeometryRef();
                OGREnvelope geo_box;
                poGeometry->getEnvelope(&geo_box);
                if ( !node_box.IsInit() ) {
                    node_box = geo_box;
                }
                else {
                    node_box.Merge(geo_box);
                }
            }
            _node->_box.minx = node_box.MinX;
            _node->_box.maxx = node_box.MaxX;
            _node->_box.miny = node_box.MinY;
            _node->_box.maxy = node_box.MaxY;
        }
        double center_x = ( _node->_box.minx + _node->_box.maxx ) / 2;
        double center_y = ( _node->_box.miny + _node->_box.maxy ) / 2;
        double max_height = 0;
        std::vector<Polygon_Mesh> v_meshes;
        for (auto id : _node->get_ids()) {
            OGRFeature *poFeature = poLayer->GetFeature(id);
            OGRGeometry *poGeometry;
            poGeometry = poFeature->GetGeometryRef();
            double height = 50.0;
            if( field_index >= 0 ) {
                height = poFeature->GetFieldAsDouble(field_index);
            }
            if (height > max_height) {
                max_height = height;
            }
            if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon) {
                OGRPolygon* polyon = (OGRPolygon*)poGeometry;
                Polygon_Mesh mesh = convert_polygon(polyon, center_x, center_y, height);
                mesh.mesh_name = "mesh_" + std::to_string(id);
                mesh.height = height;
                if (layer_defn) {
                    int field_count = layer_defn->GetFieldCount();
                    for (int f = 0; f < field_count; ++f) {
                        OGRFieldDefn* fld = layer_defn->GetFieldDefn(f);
                        std::string fname = fld->GetNameRef();
                        if (!poFeature->IsFieldSetAndNotNull(f)) {
                            mesh.properties[fname] = nullptr;
                            continue;
                        }
                        switch (fld->GetType()) {
                            case OFTInteger:
                                mesh.properties[fname] = poFeature->GetFieldAsInteger(f);
                                break;
                            case OFTInteger64:
                                mesh.properties[fname] = poFeature->GetFieldAsInteger64(f);
                                break;
                            case OFTReal:
                                mesh.properties[fname] = poFeature->GetFieldAsDouble(f);
                                break;
                            case OFTString:
                                mesh.properties[fname] = std::string(poFeature->GetFieldAsString(f));
                                break;
                            default:
                                mesh.properties[fname] = std::string(poFeature->GetFieldAsString(f));
                                break;
                        }
                    }
                }
                v_meshes.push_back(mesh);
            }
            else if (wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon) {
                OGRMultiPolygon* _multi = (OGRMultiPolygon*)poGeometry;
                int sub_count = _multi->getNumGeometries();
                for (int j = 0; j < sub_count; j++) {
                    OGRPolygon * polyon = (OGRPolygon*)_multi->getGeometryRef(j);
                    Polygon_Mesh mesh = convert_polygon(polyon, center_x, center_y, height);
                    mesh.mesh_name = "mesh_" + std::to_string(id);
                    mesh.height = height;
                    if (layer_defn) {
                        int field_count = layer_defn->GetFieldCount();
                        for (int f = 0; f < field_count; ++f) {
                            OGRFieldDefn* fld = layer_defn->GetFieldDefn(f);
                            std::string fname = fld->GetNameRef();
                            if (!poFeature->IsFieldSetAndNotNull(f)) {
                                mesh.properties[fname] = nullptr;
                                continue;
                            }
                            switch (fld->GetType()) {
                                case OFTInteger:
                                    mesh.properties[fname] = poFeature->GetFieldAsInteger(f);
                                    break;
                                case OFTInteger64:
                                    mesh.properties[fname] = poFeature->GetFieldAsInteger64(f);
                                    break;
                                case OFTReal:
                                    mesh.properties[fname] = poFeature->GetFieldAsDouble(f);
                                    break;
                                case OFTString:
                                    mesh.properties[fname] = std::string(poFeature->GetFieldAsString(f));
                                    break;
                                default:
                                    mesh.properties[fname] = std::string(poFeature->GetFieldAsString(f));
                                    break;
                            }
                        }
                    }
                    v_meshes.push_back(mesh);
                }
            }
            OGRFeature::DestroyFeature(poFeature);
        }

        // Store one or more b3dm under flat tile/z/x/y/ first; relocation happens later
        std::filesystem::path leaf_dir = std::filesystem::path("tile") / std::to_string(_node->_z) / std::to_string(_node->_x) / std::to_string(_node->_y);
        std::filesystem::create_directories(std::filesystem::path(dest) / leaf_dir, ec);
        std::filesystem::path tile_json_rel = leaf_dir / "tileset.json";
        std::filesystem::path tile_json_full = std::filesystem::path(dest) / tile_json_rel;
        std::string tile_json_path = tile_json_full.string();

        double box_width = (_node->_box.maxx - _node->_box.minx);
        double box_height = (_node->_box.maxy - _node->_box.miny);
        const double pi = std::acos(-1);
        double radian_x = degree2rad(center_x);
        double radian_y = degree2rad(center_y);

        // Convert angular span to meters and inflate slightly for safety
        double tile_w_m = longti_to_meter(degree2rad(box_width) * 1.05, radian_y);
        double tile_h_m = lati_to_meter(degree2rad(box_height) * 1.05);
        double tile_z_m = std::max(max_height, 5.0); // height range already in meters (extrusion height)

        // Geometric error per commit fc40399...: max span divided by 20
        double ge = compute_geometric_error_from_spans(tile_w_m, tile_h_m, tile_z_m);

        // Use LOD configuration from params (already extracted at function start)
        const bool lod_enabled = lod_cfg.enable_lod && !lod_cfg.levels.empty();

        std::vector<double> identity_transform = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        double half_w = tile_w_m * 0.5;
        double half_h = tile_h_m * 0.5;
        double half_z = tile_z_m * 0.5;

        auto build_lod_tree_for_meshes = [&](std::vector<Polygon_Mesh>& meshes,
                             const std::string& name_prefix) -> std::pair<nlohmann::json, double> {
            if (meshes.empty()) {
                return {nlohmann::json(), -1.0};
            }

            std::vector<std::string> lod_names;
            std::vector<double> lod_errors;

            auto make_filename = [&](size_t idx) {
                std::string prefix = name_prefix.empty() ? "" : name_prefix + "_";
                return std::string("content_") + prefix + "lod" + std::to_string(idx) + ".b3dm";
            };

            auto push_lod_output = [&](size_t idx,
                                       bool lvl_enable_simplify,
                                       std::optional<SimplificationParams> lvl_simplify,
                                       bool lvl_enable_draco,
                                       std::optional<DracoCompressionParams> lvl_draco,
                                       double lvl_ratio) {
                std::string filename = make_filename(idx);
                std::filesystem::path b3dm_rel = leaf_dir / filename;
                std::filesystem::path b3dm_full = std::filesystem::path(dest) / b3dm_rel;
                std::string b3dm_buf = make_b3dm(meshes, true, lvl_enable_simplify, lvl_simplify, lvl_enable_draco, lvl_draco);
                write_file(b3dm_full.string().c_str(), b3dm_buf.data(), b3dm_buf.size());

                lod_names.push_back(filename);
                double span_z = std::max(tile_z_m, 5.0); // avoid near-zero vertical span
                double base_ge = compute_geometric_error_from_spans(tile_w_m, tile_h_m, span_z);
                double ratio = std::clamp(static_cast<double>(lvl_ratio), 0.01, 1.0);
                // coarser LOD (smaller ratio) gets larger geometric error
                double ge_level = base_ge * std::max(1.0, 1.0 / std::sqrt(ratio));
                lod_errors.push_back(ge_level);
            };

            if (lod_enabled) {
                for (size_t i = 0; i < lod_cfg.levels.size(); ++i) {
                    const auto& lvl = lod_cfg.levels[i];
                    std::optional<SimplificationParams> level_simplify = std::nullopt;
                    if (lvl.enable_simplification) {
                        level_simplify = lvl.simplify;
                        level_simplify->target_ratio = lvl.target_ratio;
                        level_simplify->target_error = lvl.target_error;
                    }
                    std::optional<DracoCompressionParams> level_draco = std::nullopt;
                    if (lvl.enable_draco) {
                        level_draco = lvl.draco;
                        level_draco->enable_compression = true;
                    }
                    push_lod_output(i, lvl.enable_simplification, level_simplify, lvl.enable_draco, level_draco, lvl.target_ratio);
                }
            } else {
                // Use simplification params from function params
                std::optional<SimplificationParams> simplification_params_opt = std::nullopt;
                if (simplify_params.enable_simplification) {
                    simplification_params_opt = simplify_params;
                }
                push_lod_output(0, simplify_params.enable_simplification, simplification_params_opt,
                               draco_params.enable_compression,
                               draco_params.enable_compression ? std::make_optional(draco_params) : std::nullopt,
                               1.0);
            }

            double span_z = std::max(tile_z_m, 0.001);
            double bucket_half_z = span_z * 0.5;
            double bucket_center_z = bucket_half_z;

            auto make_lod_node = [&](size_t idx) {
                nlohmann::json node_json;
                node_json["refine"] = "REPLACE";
                node_json["geometricError"] = lod_errors[idx];
                node_json["boundingVolume"]["box"] = box_to_json(0.0, 0.0, bucket_center_z, half_w, half_h, bucket_half_z);
                node_json["transform"] = identity_transform;
                node_json["content"]["uri"] = std::string("./") + lod_names[idx];
                return node_json;
            };

            std::vector<size_t> order(lod_names.size());
            std::iota(order.begin(), order.end(), 0);
            if (lod_enabled) {
                std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                    return lod_cfg.levels[a].target_ratio < lod_cfg.levels[b].target_ratio;
                });
            }

            nlohmann::json lod_tree = make_lod_node(order.back());
            for (int idx = static_cast<int>(order.size()) - 2; idx >= 0; --idx) {
                size_t level_idx = order[idx];
                nlohmann::json parent = make_lod_node(level_idx);
                parent["children"].push_back(lod_tree);
                lod_tree = parent;
            }

            double root_ge = lod_tree.value("geometricError", 0.0);
            if (!lod_errors.empty()) {
                // coarsest (smallest ratio) should sit at root with largest geometric error
                root_ge = lod_errors[order.front()];
            }
            return {lod_tree, root_ge};
        };

        double leaf_root_ge = ge;
        nlohmann::json leaf_root_node;

            auto res = build_lod_tree_for_meshes(v_meshes, "");
            leaf_root_node = res.first;
            leaf_root_ge = res.second > 0 ? res.second : ge;

        nlohmann::json leaf;
        leaf["asset"] = { {"version", "1.0"}, {"gltfUpAxis", "Z"} };
        leaf["geometricError"] = leaf_root_ge;
        leaf["root"] = leaf_root_node;

        std::ofstream ofs(tile_json_path);
        if (!ofs.is_open()) {
            LOG_E("write leaf tileset %s fail", tile_json_path.c_str());
        } else {
            ofs << leaf.dump(2);
        }

        TileMeta meta;
        meta.z = _node->_z;
        meta.x = _node->_x;
        meta.y = _node->_y;
        meta.bbox = make_bbox_from_node(_node->_box, 0.0, max_height);
        meta.geometric_error = leaf_root_ge;
        meta.orig_tileset_rel = tile_json_rel.generic_string();
        meta.is_leaf = true;
        leaf_tiles.push_back(meta);
    }
    //
    GDALClose(poDS);
    build_hierarchical_tilesets(leaf_tiles, dest);
    return true;
}

template<class T>
void put_val(std::vector<unsigned char>& buf, T val) {
    buf.insert(buf.end(), (unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

template<class T>
void put_val(std::string& buf, T val) {
    buf.append((unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

template<class T>
void alignment_buffer(std::vector<T>& buf) {
    while (buf.size() % 4 != 0) {
        buf.push_back(0x00);
    }
}

#define SET_MIN(x,v) do{ if (x > v) x = v; }while (0);
#define SET_MAX(x,v) do{ if (x < v) x = v; }while (0);

tinygltf::Material make_color_material(double r, double g, double b) {
    tinygltf::Material material;
    char buf[512];
    sprintf(buf,"default_%.1f_%.1f_%.1f",r,g,b);
    material.name = buf;
    material.pbrMetallicRoughness.baseColorFactor = { r,g,b,1 };
    material.pbrMetallicRoughness.roughnessFactor = 0.7;
    material.pbrMetallicRoughness.metallicFactor = 0.3;
    return material;
}

tinygltf::BufferView create_buffer_view(int target, int byteOffset, int byteLength) {
  tinygltf::BufferView bfv;
  bfv.buffer = 0;
  bfv.target = target;
  bfv.byteOffset = byteOffset;
  bfv.byteLength = byteLength;
  return bfv;
}


// convert poly-mesh to glb buffer
std::string make_polymesh(std::vector<Polygon_Mesh> &meshes,
    bool enable_simplify,
    std::optional<SimplificationParams> simplification_params,
    bool enable_draco,
    std::optional<DracoCompressionParams> draco_params) {
        vector<osg::ref_ptr<osg::Geometry>> osg_Geoms;
        osg_Geoms.reserve(meshes.size());
        for (auto& mesh : meshes) {
                osg_Geoms.push_back(make_triangle_mesh(mesh));
        }

        if (osg_Geoms.empty()) {
                return {};
        }

        tinygltf::TinyGLTF gltf;
        tinygltf::Model model;
        tinygltf::Buffer buffer;
        bool use_multi_material = false;
        tinygltf::Scene sence;

        const bool draco_requested = enable_draco && draco_params.has_value() && draco_params->enable_compression;

        // Simplify each geometry before merging so batch id mapping stays consistent
        if (enable_simplify && simplification_params.has_value()) {
                for (auto& geom : osg_Geoms) {
                        if (geom.valid() && geom->getNumPrimitiveSets() > 0) {
                                simplify_mesh_geometry(geom.get(), simplification_params.value());
                        }
                }
        }

        // Merge all buildings into one geometry while tracking per-building batch ids
        osg::ref_ptr<osg::Geometry> merged_geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> merged_vertices = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec3Array> merged_normals = new osg::Vec3Array();
        osg::ref_ptr<osg::DrawElementsUInt> merged_indices = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        std::vector<uint32_t> merged_batch_ids;

        for (size_t i = 0; i < osg_Geoms.size(); ++i) {
                if (!osg_Geoms[i].valid()) continue;
                osg::Vec3Array* vArr = dynamic_cast<osg::Vec3Array*>(osg_Geoms[i]->getVertexArray());
                if (!vArr || vArr->empty()) continue;
                osg::Vec3Array* nArr = dynamic_cast<osg::Vec3Array*>(osg_Geoms[i]->getNormalArray());

                const size_t base = merged_vertices->size();
                merged_vertices->insert(merged_vertices->end(), vArr->begin(), vArr->end());

                if (nArr && nArr->size() == vArr->size()) {
                        merged_normals->insert(merged_normals->end(), nArr->begin(), nArr->end());
                } else {
                        // Fallback normals keep alignment if input is missing
                        merged_normals->insert(merged_normals->end(), vArr->size(), osg::Vec3(0.0f, 0.0f, 1.0f));
                }

                merged_batch_ids.insert(merged_batch_ids.end(), vArr->size(), static_cast<uint32_t>(i));

                if (osg_Geoms[i]->getNumPrimitiveSets() > 0) {
                        osg::PrimitiveSet* ps = osg_Geoms[i]->getPrimitiveSet(0);
                        const auto idx_cnt = ps->getNumIndices();
                        for (unsigned int k = 0; k < idx_cnt; ++k) {
                                merged_indices->push_back(static_cast<unsigned int>(base + ps->index(k)));
                        }
                }
        }

        if (merged_vertices->empty() || merged_indices->empty()) {
                return {};
        }

        merged_geom->setVertexArray(merged_vertices.get());
        merged_geom->setNormalArray(merged_normals.get());
        merged_geom->addPrimitiveSet(merged_indices.get());

        // Optionally Draco-compress the merged geometry; fallback data is still present
        std::vector<unsigned char> draco_data;
        size_t draco_size = 0;
        int draco_pos_att = -1;
        int draco_norm_att = -1;
        bool wrote_draco_ext = false;
        if (draco_requested) {
                DracoCompressionParams params = draco_params.value();
                params.enable_compression = true;
                compress_mesh_geometry(merged_geom.get(), params, draco_data, draco_size, &draco_pos_att, &draco_norm_att);
        }

        // Build GLB buffers from the merged geometry
        int index_accessor_index = -1;
        int vertex_accessor_index = -1;
        int normal_accessor_index = -1;
        int batchid_accessor_index = -1;

        {
                osg::PrimitiveSet* ps = merged_geom->getPrimitiveSet(0);
                int idx_size = ps->getNumIndices();
                uint32_t max_idx = 0;

                int byteOffset = buffer.data.size();
                for (int m = 0; m < idx_size; m++) {
                        uint32_t idx = static_cast<uint32_t>(ps->index(m));
                        put_val(buffer.data, idx);
                        SET_MAX(max_idx, idx);
                }

                index_accessor_index = model.accessors.size();

                tinygltf::Accessor acc;
                acc.bufferView = model.bufferViews.size();
                acc.byteOffset = 0;
                alignment_buffer(buffer.data);
                acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                acc.count = idx_size;
                acc.type = TINYGLTF_TYPE_SCALAR;
                acc.maxValues = {(double)max_idx};
                acc.minValues = {0.0};
                model.accessors.push_back(acc);

                tinygltf::BufferView bfv = create_buffer_view(TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER, byteOffset,
                                                             buffer.data.size() - byteOffset);
                model.bufferViews.push_back(bfv);
        }
        {
                osg::Vec3Array* v3f = merged_vertices.get();
                int vec_size = v3f->size();
                std::vector<double> box_max = {-1e38, -1e38, -1e38};
                std::vector<double> box_min = {1e38, 1e38, 1e38};
                int byteOffset = buffer.data.size();
                for (int vidx = 0; vidx < vec_size; vidx++) {
                    osg::Vec3f point = v3f->at(vidx);
                    vector<float> vertex = {point.x(), point.y(), point.z()};
                    for (int i = 0; i < 3; i++) {
                        put_val(buffer.data, vertex[i]);
                        SET_MAX(box_max[i], vertex[i]);
                        SET_MIN(box_min[i], vertex[i]);
                    }
                }

                vertex_accessor_index = model.accessors.size();
                tinygltf::Accessor acc;
                acc.bufferView = model.bufferViews.size();
                acc.byteOffset = 0;
                alignment_buffer(buffer.data);
                acc.count = vec_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
                acc.maxValues = box_max;
                acc.minValues = box_min;
                model.accessors.push_back(acc);

                tinygltf::BufferView bfv = create_buffer_view(TINYGLTF_TARGET_ARRAY_BUFFER, byteOffset,
                                                             buffer.data.size() - byteOffset);
                model.bufferViews.push_back(bfv);
        }
        {
                osg::Vec3Array* v3f = merged_normals.get();
                std::vector<double> box_max = {-1e38, -1e38, -1e38};
                std::vector<double> box_min = {1e38, 1e38, 1e38};
                int normal_size = v3f->size();
                int byteOffset = buffer.data.size();
                for (int vidx = 0; vidx < normal_size; vidx++) {
                    osg::Vec3f point = v3f->at(vidx);
                    vector<float> normal = {point.x(), point.y(), point.z()};

                    for (int i = 0; i < 3; i++) {
                        put_val(buffer.data, normal[i]);
                        SET_MAX(box_max[i], normal[i]);
                        SET_MIN(box_min[i], normal[i]);
                    }
                }
                normal_accessor_index = model.accessors.size();
                tinygltf::Accessor acc;
                acc.bufferView = model.bufferViews.size();
                acc.byteOffset = 0;
                alignment_buffer(buffer.data);
                acc.count = normal_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
                acc.minValues = box_min;
                acc.maxValues = box_max;
                model.accessors.push_back(acc);

                tinygltf::BufferView bfv = create_buffer_view(TINYGLTF_TARGET_ARRAY_BUFFER, byteOffset,
                                                             buffer.data.size() - byteOffset);
                model.bufferViews.push_back(bfv);
        }
        {
                int byteOffset = buffer.data.size();
                uint32_t max_batch = 0;
                for (auto batch_id : merged_batch_ids) {
                        put_val(buffer.data, batch_id);
                        SET_MAX(max_batch, batch_id);
                }
                batchid_accessor_index = model.accessors.size();
                tinygltf::Accessor acc;
                acc.bufferView = model.bufferViews.size();
                acc.byteOffset = 0;
                alignment_buffer(buffer.data);
                acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                acc.count = merged_batch_ids.size();
                acc.type = TINYGLTF_TYPE_SCALAR;
                acc.maxValues = {(double)max_batch};
                acc.minValues = {0.0};
                model.accessors.push_back(acc);

                tinygltf::BufferView bfv = create_buffer_view(TINYGLTF_TARGET_ARRAY_BUFFER, byteOffset,
                                                             buffer.data.size() - byteOffset);
                model.bufferViews.push_back(bfv);
        }

        tinygltf::Mesh mesh;
        mesh.name = meshes.size() == 1 ? meshes.front().mesh_name : "merged_mesh";
        tinygltf::Primitive primits;
        primits.attributes = {
                std::pair<std::string, int>("POSITION", vertex_accessor_index),
                std::pair<std::string, int>("NORMAL", normal_accessor_index),
                std::pair<std::string, int>("_BATCHID", batchid_accessor_index),
        };
        primits.indices = index_accessor_index;
        primits.material = 0;
        primits.mode = TINYGLTF_MODE_TRIANGLES;
        mesh.primitives = {primits};
        model.meshes.push_back(mesh);

        tinygltf::Node node;
        node.mesh = model.meshes.size() - 1;
        model.nodes.push_back(node);
        sence.nodes.push_back(model.nodes.size() - 1);

        // Append Draco payload at the end of buffer and wire extension for merged mesh
        if (!draco_data.empty()) {
                int draco_view_index = model.bufferViews.size();
                int byteOffset = buffer.data.size();
                buffer.data.insert(buffer.data.end(), draco_data.begin(), draco_data.end());

                tinygltf::BufferView draco_view;
                draco_view.buffer = 0;
                draco_view.byteOffset = byteOffset;
                draco_view.byteLength = draco_data.size();
                model.bufferViews.push_back(draco_view);

                tinygltf::Value::Object attrs;
                attrs["POSITION"] = tinygltf::Value(draco_pos_att);
                if (draco_norm_att >= 0) {
                        attrs["NORMAL"] = tinygltf::Value(draco_norm_att);
                }

                tinygltf::Value::Object draco_ext;
                draco_ext["bufferView"] = tinygltf::Value(draco_view_index);
                draco_ext["attributes"] = tinygltf::Value(attrs);
                model.meshes.back().primitives.back().extensions["KHR_draco_mesh_compression"] = tinygltf::Value(draco_ext);
                wrote_draco_ext = true;
        }
    model.scenes = { sence };
    model.defaultScene = 0;
    /// --------------
    if (use_multi_material) {
        // code has realized about
    } else {
        model.materials = { make_color_material(1.0, 1.0, 1.0) };
    }

    model.buffers.push_back(std::move(buffer));
    model.asset.version = "2.0";
    model.asset.generator = "fanfan";

    if (wrote_draco_ext) {
        auto ensure_ext = [](std::vector<std::string>& list, const std::string& ext) {
            if (std::find(list.begin(), list.end(), ext) == list.end()) {
                list.push_back(ext);
            }
        };
        ensure_ext(model.extensionsRequired, "KHR_draco_mesh_compression");
        ensure_ext(model.extensionsUsed, "KHR_draco_mesh_compression");
    }

    std::ostringstream ss;
    bool res = gltf.WriteGltfSceneToStream(&model, ss, false, true);
    std::string buf = ss.str();
    return buf;
}

std::string make_b3dm(std::vector<Polygon_Mesh>& meshes, bool with_height, bool enable_simplify, std::optional<SimplificationParams> simplification_params, bool enable_draco, std::optional<DracoCompressionParams> draco_params) {
    using nlohmann::json;

    std::string feature_json_string;
    feature_json_string += "{\"BATCH_LENGTH\":";
    feature_json_string += std::to_string(meshes.size());
    feature_json_string += "}";
    while (feature_json_string.size() % 4 != 0 ) {
        feature_json_string.push_back(' ');
    }

    json batch_json;
    std::vector<int> ids;
    for (int i = 0; i < meshes.size(); ++i) {
        ids.push_back(i);
    }
    std::vector<std::string> names;
    for (int i = 0; i < meshes.size(); ++i) {
        names.push_back(meshes[i].mesh_name);
    }
    batch_json["batchId"] = ids;
    batch_json["name"] = names;

    // Collect all attribute keys across meshes
    std::set<std::string> attribute_keys;
    for (const auto& m : meshes) {
        for (const auto& kv : m.properties) {
            attribute_keys.insert(kv.first);
        }
    }

    // Build per-attribute arrays aligned with batch ids
    std::map<std::string, std::vector<nlohmann::json>> attribute_columns;
    for (const auto& key : attribute_keys) {
        attribute_columns[key] = std::vector<nlohmann::json>(meshes.size(), nullptr);
    }
    for (int i = 0; i < meshes.size(); ++i) {
        for (const auto& kv : meshes[i].properties) {
            auto it = attribute_columns.find(kv.first);
            if (it != attribute_columns.end()) {
                it->second[i] = kv.second;
            }
        }
    }
    for (const auto& kv : attribute_columns) {
        batch_json[kv.first] = kv.second;
    }

    if (with_height) {
        std::vector<float> heights;
        for (int i = 0; i < meshes.size(); ++i) {
            heights.push_back(meshes[i].height);
        }
        batch_json["height"] = heights;
    }

    std::string batch_json_string = batch_json.dump();
    while (batch_json_string.size() % 4 != 0 ) {
        batch_json_string.push_back(' ');
    }

    std::string glb_buf = make_polymesh(meshes, enable_simplify, simplification_params, enable_draco, draco_params);
    // how length total ?

    //test
    //feature_json_string.clear();
    //batch_json_string.clear();
    //end-test

    int feature_json_len = feature_json_string.size();
    int feature_bin_len = 0;
    int batch_json_len = batch_json_string.size();
    int batch_bin_len = 0;
    int total_len = 28 /*header size*/ + feature_json_len + batch_json_len + glb_buf.size();

    std::string b3dm_buf;
    b3dm_buf += "b3dm";
    int version = 1;
    put_val(b3dm_buf, version);
    put_val(b3dm_buf, total_len);
    put_val(b3dm_buf, feature_json_len);
    put_val(b3dm_buf, feature_bin_len);
    put_val(b3dm_buf, batch_json_len);
    put_val(b3dm_buf, batch_bin_len);
    //put_val(b3dm_buf, total_len);
    b3dm_buf.append(feature_json_string.begin(),feature_json_string.end());
    b3dm_buf.append(batch_json_string.begin(),batch_json_string.end());
    b3dm_buf.append(glb_buf);
    return b3dm_buf;
}
