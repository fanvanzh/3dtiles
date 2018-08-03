#include <osg/Material>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <set>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"
#include "stb_image_write.h"
#include "extern.h"

using namespace std;

template<class T>
void put_val(std::vector<unsigned char>& buf, T val) {
    buf.insert(buf.end(), (unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

template<class T>
void put_val(std::string& buf, T val) {
    buf.append((unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

void write_buf(void* context, void* data, int len) {
    std::vector<char> *buf = (std::vector<char>*)context;
    buf->insert(buf->end(), (char*)data, (char*)data + len);
}

class InfoVisitor : public osg::NodeVisitor
{
    std::string path;
public:
    InfoVisitor(std::string _path)
    :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
    ,path(_path)
    {}

    ~InfoVisitor() {
    }

    void apply(osg::Geometry& geometry){
        geometry_array.push_back(&geometry);
        if (auto ss = geometry.getStateSet() ) {
            osg::Texture* tex = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
            if (tex) {
                texture_array.insert(tex);
            }
        }
    }
    
    void apply(osg::PagedLOD& node) {
        //std::string path = node.getDatabasePath();
        int n = node.getNumFileNames();
        for (size_t i = 1; i < n; i++)
        {
            std::string file_name = path + "/" + node.getFileName(i);
            sub_node_names.push_back(file_name);
        }
        traverse(node);
    }

public:
    std::vector<osg::Geometry*> geometry_array;
    std::set<osg::Texture*> texture_array;
    std::vector<std::string> sub_node_names;
};

double get_geometric_error(int lvl ){
    const double pi = std::acos(-1);
    double round = 2 * pi * 6378137.0 / 128.0;
    return round / std::pow(2.0, lvl );
}

std::string get_file_name(std::string path) {
    auto p0 = path.find_last_of("/\\");
    return path.substr(p0 + 1);
}

std::string replace(std::string str, std::string s0, std::string s1) {
    auto p0 = str.find(s0);
    return str.replace(p0, p0 + s0.length() - 1, s1);
}

std::string get_parent(std::string str) {
    auto p0 = str.find_last_of("/\\");
    if (p0 != std::string::npos)
        return str.substr(0, p0);
    else
        return "";
}

std::string osg_string ( const char* path ) {
    #ifdef WIN32
        std::string root_path =
        osgDB::convertStringFromUTF8toCurrentCodePage(path);
    #else
        std::string root_path = (path);
    #endif // WIN32
    return root_path;
}

std::string utf8_string (const char* path) {
    #ifdef WIN32
        std::string utf8 =
        osgDB::convertStringFromCurrentCodePageToUTF8(path);
    #else
        std::string utf8 = (path);
    #endif // WIN32
    return utf8;   
}

int get_lvl_num(std::string file_name){
    std::string stem = get_file_name(file_name);
    auto p0 = stem.find("_L");
    auto p1 = stem.find("_", p0 + 2);
    if (p0 != std::string::npos && p1 != std::string::npos) {
        std::string substr = stem.substr(p0 + 2, p1 - p0 - 2);
        try { return std::stol(substr); }
        catch (...) {
            return -1;
        }
    }
    return -1;
}

struct tile_box
{
    std::vector<double> max;
    std::vector<double> min;
};

struct osg_tree {
    tile_box bbox;
    std::string file_name;
    std::vector<osg_tree> sub_nodes;
};

osg_tree get_all_tree(std::string& file_name) {
    osg_tree root_tile;
    vector<string> fileNames = { file_name };
    
    InfoVisitor infoVisitor(get_parent(file_name));
    {   // add block to release Node
        osg::ref_ptr<osg::Node> root = osgDB::readNodeFiles(fileNames);
        if (!root) {
            std::string name = utf8_string(file_name.c_str());
            LOG_E("read node files [%s] fail!", name.c_str());
            return root_tile;
        }
        root_tile.file_name = file_name;
        root->accept(infoVisitor);    
    }
    
    for (auto& i : infoVisitor.sub_node_names) {
        osg_tree tree = get_all_tree(i);
        if (!tree.file_name.empty()) {
            root_tile.sub_nodes.push_back(tree);
        }
    }
    return root_tile;
}

struct mesh_info
{
    string name;
    std::vector<double> min;
    std::vector<double> max;
};

bool osgb2glb_buf(std::string path, std::string& glb_buff, std::vector<mesh_info>& v_info) {
    vector<string> fileNames = { path };
    std::string parent_path = get_parent(path);
    osg::ref_ptr<osg::Node> root = osgDB::readNodeFiles(fileNames);
    if (!root.valid()) {
        return false;
    }
    InfoVisitor infoVisitor(parent_path);
    root->accept(infoVisitor);
    if (infoVisitor.geometry_array.empty())
        return false;

    osgUtil::SmoothingVisitor sv;
    root->accept(sv);

    {
        tinygltf::TinyGLTF gltf;
        tinygltf::Model model;
        tinygltf::Buffer buffer;
        // buffer_view {index,vertex,normal(texcoord,image)}
        uint32_t buf_offset = 0;
        uint32_t acc_offset[4] = { 0,0,0,0 };
        for (int j = 0; j < 4; j++)
        {
            for (auto g : infoVisitor.geometry_array) {
                if (g->getNumPrimitiveSets() == 0) {
                    continue;
                }
                osg::Array* va = g->getVertexArray();
                if (j == 0) {
                    // indc
                    //for (unsigned int i = 0; i < g->getNumPrimitiveSets(); ++i)
                    {
                        osg::PrimitiveSet* ps = g->getPrimitiveSet(0);
                        osg::PrimitiveSet::Type t = ps->getType();
                        int idx_size = ps->getNumIndices();
                        switch (t)
                        {
                            case(osg::PrimitiveSet::DrawElementsUBytePrimitiveType):
                            {
                                const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(ps);
                                int IndNum = drawElements->getNumIndices();
                                for (size_t m = 0; m < IndNum; m++)
                                {
                                    put_val(buffer.data, drawElements->at(m));
                                }
                                break;
                            }
                            case(osg::PrimitiveSet::DrawElementsUShortPrimitiveType):
                            {
                                const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(ps);
                                int IndNum = drawElements->getNumIndices();
                                for (size_t m = 0; m < IndNum; m++)
                                {
                                    put_val(buffer.data, drawElements->at(m));
                                }
                                break;
                            }
                            case(osg::PrimitiveSet::DrawElementsUIntPrimitiveType):
                            {
                                const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(ps);
                                int IndNum = drawElements->getNumIndices();
                                for (size_t m = 0; m < IndNum; m++)
                                {
                                    put_val(buffer.data, drawElements->at(m));
                                }
                                break;
                            }
                            default:
                            break;
                        }
                        tinygltf::Accessor acc;
                        acc.bufferView = 0;
                        acc.byteOffset = acc_offset[j];
                        acc_offset[j] = buffer.data.size();
                        acc.componentType = TINYGLTF_COMPONENT_TYPE_INT;
                        switch (t)
                        {
                            case osg::PrimitiveSet::DrawElementsUBytePrimitiveType:
                            acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                            break;
                            case osg::PrimitiveSet::DrawElementsUShortPrimitiveType:
                            acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                            break;
                            case osg::PrimitiveSet::DrawElementsUIntPrimitiveType:
                            acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                            break;
                            default:
                            break;
                        }
                        acc.count = idx_size;
                        acc.type = TINYGLTF_TYPE_SCALAR;
                        osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                        int vec_size = v3f->size();
                        acc.maxValues = { (double)vec_size - 1 };
                        acc.minValues = { 0 };
                        model.accessors.push_back(acc);
                    }
                }
                else if (j == 1) {
                    osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                    int vec_size = v3f->size();
                    vector<double> box_max = {-1e38, -1e38 ,-1e38 };
                    vector<double> box_min = { 1e38, 1e38 ,1e38 };
                    for (int vidx = 0; vidx < vec_size; vidx++)
                    {
                        osg::Vec3f point = v3f->at(vidx);
                        put_val(buffer.data, point.x());
                        put_val(buffer.data, point.z());
                        put_val(buffer.data, -point.y());
                        if (point.x() > box_max[0]) box_max[0] = point.x();
                        if (point.x() < box_min[0]) box_min[0] = point.x();
                        
                        if (point.y() > box_max[1]) box_max[1] = point.y();
                        if (point.y() < box_min[1]) box_min[1] = point.y();

                        if (point.z() > box_max[2]) box_max[2] = point.z();
                        if (point.z() < box_min[2]) box_min[2] = point.z();
                    }
                    tinygltf::Accessor acc;
                    acc.bufferView = 1;
                    acc.byteOffset = acc_offset[j];
                    acc_offset[j] = buffer.data.size() - buf_offset;
                    acc.count = vec_size;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.type = TINYGLTF_TYPE_VEC3;
                    acc.maxValues = box_max;
                    acc.minValues = box_min;
                    model.accessors.push_back(acc);

                    // calc the box
                    mesh_info osgb_info;
                    osgb_info.name = g->getName();
                    osgb_info.min = box_min;
                    osgb_info.max = box_max;
                    v_info.push_back(osgb_info);
                }
                else if (j == 2) {
                    // normal
                    osg::Array* na = g->getNormalArray();
                    osg::Vec3Array* v3f = (osg::Vec3Array*)na;
                    vector<double> box_max = { -1e38, -1e38 ,-1e38 };
                    vector<double> box_min = { 1e38, 1e38 ,1e38 };
                    int normal_size = v3f->size();
                    for (int vidx = 0; vidx < normal_size; vidx++)
                    {
                        osg::Vec3f point = v3f->at(vidx);
                        put_val(buffer.data, point.x());
                        put_val(buffer.data, point.z());
                        put_val(buffer.data, -point.y());
                    }
                    tinygltf::Accessor acc;
                    acc.bufferView = 2;
                    acc.byteOffset = acc_offset[j];
                    acc_offset[j] = buffer.data.size() - buf_offset;
                    acc.count = normal_size;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.type = TINYGLTF_TYPE_VEC3;
                    acc.maxValues = {1,1,1};
                    acc.minValues = {-1,-1,-1};
                    model.accessors.push_back(acc);
                }
                else if (j == 3) {
                    // text
                    int texture_size = 0;
                    osg::Array* na = g->getTexCoordArray(0);
                    if (na) {
                        osg::Vec2Array* v2f = (osg::Vec2Array*)na;
                        vector<double> box_max = { -1e38, -1e38 };
                        vector<double> box_min = { 1e38, 1e38 };
                        texture_size = v2f->size();
                        for (int vidx = 0; vidx < texture_size; vidx++)
                        {
                            osg::Vec2f point = v2f->at(vidx);
                            put_val(buffer.data, point.x());
                            put_val(buffer.data, point.y());
                        }
                    }
                    else { // mesh 没有纹理坐标
                        osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                        int vec_size = v3f->size();
                        texture_size = vec_size;
                        for (int vidx = 0; vidx < vec_size; vidx++)
                        {
                            float x = 0;
                            put_val(buffer.data, x);
                            put_val(buffer.data, x);
                        }
                    }
                    tinygltf::Accessor acc;
                    acc.bufferView = 3;
                    acc.byteOffset = acc_offset[j];
                    acc_offset[j] = buffer.data.size() - buf_offset;
                    acc.count = texture_size;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.type = TINYGLTF_TYPE_VEC2;
                    acc.maxValues = { 1,1 };
                    acc.minValues = { 0,0 };
                    model.accessors.push_back(acc);
                }
            }
            tinygltf::BufferView bfv;
            bfv.buffer = 0;
            if (j == 0) {
                bfv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
            }
            else {
                bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
            }
            bfv.byteOffset = buf_offset;
            while (buffer.data.size() % 4 != 0) {
                buffer.data.push_back(0x00);
            }
            bfv.byteLength = buffer.data.size() - buf_offset;
            buf_offset = buffer.data.size();
            if (infoVisitor.geometry_array.size() > 1) {
                if (j == 1) { bfv.byteStride = 4 * 3; } // ¶¥µã
                if (j == 2) { bfv.byteStride = 4 * 3; } // ·¨Ïß
                if (j == 3) { bfv.byteStride = 4 * 2; } // ÌùÍ¼
            }
            model.bufferViews.push_back(bfv);
        }
        // image
        // 共用贴图？
        {
            //char* buf = 0;
            std::vector<unsigned char> jpeg_buf;
            jpeg_buf.reserve(512*512*3);
            int width, height;
            {
                osg::Texture* tex = *infoVisitor.texture_array.begin();
                if (tex) {
                    if (tex->getNumImages() > 0) {
                        osg::Image* img = tex->getImage(0);
                        if (img) {
                            width = img->s();
                            height = img->t(); 
                            for (int i = 0; i < height; i++) {
                                for (int j = 0; j < width; j++) {
                                    osg::Vec4 color = img->getColor(j,i);
                                    jpeg_buf.push_back((unsigned char)255*color.r());
                                    jpeg_buf.push_back((unsigned char)255*color.g());
                                    jpeg_buf.push_back((unsigned char)255*color.b());
                                }
                            }
                        }
                    }
                }
            }
            if (!jpeg_buf.empty()) {
                stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, jpeg_buf.data(), 80);
            }
            else {
                std::vector<char> v_data;
                width = height = 256;
                v_data.resize(width * height * 3);
                stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, v_data.data(), 80);
            }
            tinygltf::Image image;
            image.mimeType = "image/jpeg";
            int buf_view = 4;
            image.bufferView = buf_view;
            model.images.push_back(image);
            tinygltf::BufferView bfv;
            bfv.buffer = 0;
            bfv.byteOffset = buf_offset;
            bfv.byteLength = buffer.data.size() - buf_offset;
            while (buffer.data.size() % 4 != 0) {
                buffer.data.push_back(0x00);
            }
            buf_offset = buffer.data.size();
            model.bufferViews.push_back(bfv);
        }
        model.buffers.push_back(std::move(buffer));

        int MeshNum = infoVisitor.geometry_array.size();
        for (int i = 0; i < MeshNum; i++) {
            tinygltf::Mesh mesh;
            //mesh.name = meshes[i].mesh_name;
            tinygltf::Primitive primits;
            primits.attributes = {
                //std::pair<std::string,int>("_BATCHID", 2 * i + 1),
                std::pair<std::string,int>("POSITION", 1 * MeshNum + i),
                std::pair<std::string,int>("NORMAL",   2 * MeshNum + i),
                std::pair<std::string,int>("TEXCOORD_0",   3 * MeshNum + i),
            };
            primits.indices = i;
            primits.material = 0;
            primits.mode = TINYGLTF_MODE_TRIANGLES;
            mesh.primitives = {
                primits
            };
            model.meshes.push_back(mesh);
        }

        // 加载所有的模型
        for (int i = 0; i < MeshNum; i++) {
            tinygltf::Node node;
            node.mesh = i;
            model.nodes.push_back(node);
        }
        // 一个场景
        tinygltf::Scene sence;
        for (int i = 0; i < MeshNum; i++) {
            sence.nodes.push_back(i);
        }
        // 所有场景
        model.scenes = { sence };
        model.defaultScene = 0;

        tinygltf::Sampler sample;
        sample.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        sample.minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
        sample.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
        sample.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
        model.samplers = { sample };
        /// --------------
        tinygltf::Material material;
        material.name = "default";
        tinygltf::Parameter baseColorFactor;
        baseColorFactor.number_array = { 1.0,1.0,1.0,1.0 };
        material.values["baseColorFactor"] = baseColorFactor;
        // 可能会出现多材质的情况
        tinygltf::Parameter baseColorTexture;
        baseColorTexture.json_int_value = {std::pair<string,int>("index",0)};
        material.values["baseColorTexture"] = baseColorTexture;

        tinygltf::Parameter metallicFactor;
        metallicFactor.number_value = 0;
        material.values["metallicFactor"] = metallicFactor;
        tinygltf::Parameter roughnessFactor;
        roughnessFactor.number_value = 1;
        material.values["roughnessFactor"] = roughnessFactor;
        /// ---------
        tinygltf::Parameter emissiveFactor;
        emissiveFactor.number_array = { 0.0,0.0,0.0 };
        material.additionalValues["emissiveFactor"] = emissiveFactor;
        tinygltf::Parameter alphaMode;
        alphaMode.string_value = "OPAQUE";
        material.additionalValues["alphaMode"] = alphaMode;
        tinygltf::Parameter doubleSided;
        doubleSided.bool_value = false;
        material.additionalValues["doubleSided"] = doubleSided;
        model.materials = { material };
        /// ----------------------
        tinygltf::Texture texture;
        texture.source = 0;
        texture.sampler = 0;
        model.textures = {texture};

        model.asset.version = "2.0";
        model.asset.generator = "fanfan";

        glb_buff = gltf.Serialize(&model);
    }
    return true;
}

bool osgb23dtile_buf(std::string path, std::string& b3dm_buf, tile_box& tile_box) {
    using nlohmann::json;
    
    std::string glb_buf;
    std::vector<mesh_info> v_info;
    bool ret = osgb2glb_buf(path, glb_buf,v_info);
    if (!ret) return false;

    tile_box.max = {-1e38,-1e38,-1e38};
    tile_box.min = {1e38,1e38,1e38};
    for ( auto &mesh: v_info) {
        for(int i = 0; i < 3; i++) {
            if (mesh.min[i] < tile_box.min[i])
                tile_box.min[i] = mesh.min[i];
            if (mesh.max[i] > tile_box.max[i])
                tile_box.max[i] = mesh.max[i];
        }
    }

    int mesh_count = v_info.size();
    std::string feature_json_string;
    feature_json_string += "{\"BATCH_LENGTH\":";
    feature_json_string += std::to_string(mesh_count);
    feature_json_string += "}";
    while (feature_json_string.size() % 4 != 0 ) {
        feature_json_string.push_back(' ');
    }
    
    json batch_json;
    std::vector<int> ids;
    for (int i = 0; i < mesh_count; ++i) {
        ids.push_back(i);
    }
    std::vector<std::string> names;
    for (int i = 0; i < mesh_count; ++i) {
        std::string mesh_name = "mesh_";
        mesh_name += std::to_string(i);
        names.push_back(mesh_name);
    }
    batch_json["batchId"] = ids;
    batch_json["name"] = names;
    std::string batch_json_string = batch_json.dump();
    while (batch_json_string.size() % 4 != 0 ) {
        batch_json_string.push_back(' ');
    }


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
    return true;
}


std::vector<double> convert_bbox(tile_box tile) {
    double center_mx = (tile.max[0] + tile.min[0]) / 2;
    double center_my = (tile.max[1] + tile.min[1]) / 2;
    double center_mz = (tile.max[2] + tile.min[2]) / 2;
    double x_meter = (tile.max[0] - tile.min[0]) * 1.01;
    double y_meter = (tile.max[1] - tile.min[1]) * 1.01;
    double z_meter = (tile.max[2] - tile.min[2]) * 1.01;
    if (x_meter < 0.01) { x_meter = 0.01; }
    if (y_meter < 0.01) { y_meter = 0.01; }
    if (z_meter < 0.01) { z_meter = 0.01; }
    std::vector<double> v = {
        center_mx,center_my,center_mz,
        x_meter/2, 0, 0,
        0, y_meter/2, 0,
        0, 0, z_meter/2
    };
    return v;
}

// 生成 b3dm ， 再统一外扩模型的 bbox
void do_tile_job(osg_tree& tree, std::string out_path, int max_lvl) {
    // 转瓦片、写json
    std::string json_str;
    if (tree.file_name.empty()) return;
    int lvl = get_lvl_num(tree.file_name);
    if (lvl > max_lvl) return;
    // 转 tile 
    std::string b3dm_buf;
    osgb23dtile_buf(tree.file_name, b3dm_buf, tree.bbox);
    // false 可能当前为空, 但存在子节点
    std::string out_file = out_path;
    out_file += "/";
    out_file += replace(get_file_name(tree.file_name),".osgb",".b3dm");
    if (!b3dm_buf.empty()) {
        write_file(out_file.c_str(), b3dm_buf.data(), b3dm_buf.size());
    }
    for (auto& i : tree.sub_nodes) {
        do_tile_job(i,out_path,max_lvl);
    }
}

void expend_box(tile_box& box, tile_box& box_new) {
    if (box_new.max.empty() || box_new.min.empty()) {
        return;
    }
    if (box.max.empty()) {
        box.max = box_new.max;
    }
    if (box.min.empty()) {
        box.min = box_new.min;
    }
    for (int i = 0; i < 3; i++) {
        if (box.min[i] > box_new.min[i])
            box.min[i] = box_new.min[i];
        if (box.max[i] < box_new.max[i])
            box.max[i] = box_new.max[i];
    }
}

tile_box extend_tile_box(osg_tree& tree) {
    tile_box box = tree.bbox;
    for (auto& i : tree.sub_nodes) {
        tile_box sub_tile = extend_tile_box(i);
        expend_box(box, sub_tile);
    }
    tree.bbox = box;
    return box;
}

std::string encode_tile_json(osg_tree& tree) {
    if (tree.bbox.max.empty() || tree.bbox.min.empty()) {
        return "";
    }
    // Todo:: 获取 Geometric Error
    int lvl = get_lvl_num(tree.file_name);
    if (lvl == -1) lvl = 10;
    char buf[512];
    sprintf(buf, "{ \"geometricError\":%.2f,", 
        tree.sub_nodes.empty()? 0 : get_geometric_error(lvl)
        );
    std::string tile = buf;
    string box_str = "\"boundingVolume\":{";
    box_str += "\"box\":[";
    std::vector<double> v_box = convert_bbox(tree.bbox);
    for (auto v: v_box) {
        box_str += std::to_string(v);
        box_str += ",";
    }
    box_str.pop_back();
    box_str += "]}";
    tile += box_str;
    tile += ",";
    tile += "\"content\":{ \"url\":";
    // Data/Tile_0/Tile_0.b3dm
    std::string url_path = "Data/";
    std::string file_name = get_file_name(tree.file_name);
    std::string parent_str = get_parent(tree.file_name);
    std::string file_path = get_file_name(parent_str);
    url_path += file_path;
    url_path += "/";
    url_path += file_name;
    std::string url = replace(url_path,".osgb",".b3dm");
    tile += "\"";
    tile += url;
    tile += "\",";
    tile += box_str;
    tile += "},\"children\":[";
    for ( auto& i : tree.sub_nodes ){
        std::string node_json = encode_tile_json(i);
        if (!node_json.empty()) {
            tile += node_json;
            tile += ",";
        }
    }
    if (tile.back() == ',')
        tile.pop_back();
    tile += "]}";
    return tile;
}

/**
外部创建好目录
外面分配好 box[6][double]
外面分配好 string [1024*1024]
*/
extern "C" void* osgb23dtile_path(
    const char* in_path, const char* out_path, 
    double *box, int* len, int max_lvl) {
    
    std::string path = osg_string(in_path);
    osg_tree root = get_all_tree(path);
    if (root.file_name.empty()) {
        LOG_E( "open file [%s] fail!", in_path);
        return NULL;
    }
    do_tile_job(root, out_path, max_lvl);
    // 返回 json 和 最大bbox
    extend_tile_box(root);
    if (root.bbox.max.empty() || root.bbox.min.empty()) {
        LOG_E( "[%s] bbox is empty!", in_path);
        return NULL;
    }
    std::string json = encode_tile_json(root);
    memcpy(box, root.bbox.max.data(), 3 * sizeof(double));
    memcpy(box + 3, root.bbox.min.data(), 3 * sizeof(double));
    void* str = malloc(json.length());
    memcpy(str, json.c_str(), json.length());
    *len = json.length();
    return str;
}

extern "C" bool osgb23dtile(
    const char* in, const char* out ) {
    std::string b3dm_buf;
    tile_box tile_box;
    std::string path = osg_string(in);
    bool ret = osgb23dtile_buf(path.c_str(),b3dm_buf,tile_box);
    if (!ret) return false;
    ret = write_file(out, b3dm_buf.data(), b3dm_buf.size());
    if (!ret) return false;
    // write tileset.json
    std::string b3dm_fullpath = out;
    auto p0 = b3dm_fullpath.find_last_of('/');
    auto p1 = b3dm_fullpath.find_last_of('\\');
    std::string b3dm_file_name = b3dm_fullpath.substr(
        std::max<int>(p0,p1) + 1);
    std::string tileset = out;
    tileset = tileset.replace(
        b3dm_fullpath.find_last_of('.'), 
        tileset.length() - 1, ".json");
    double center_mx = (tile_box.max[0] + tile_box.min[0]) / 2;
    double center_my = (tile_box.max[2] + tile_box.min[2]) / 2;
    double center_mz = (tile_box.max[1] + tile_box.min[1]) / 2;

    double width_meter = tile_box.max[0] - tile_box.min[0];
    double height_meter = tile_box.max[2] - tile_box.min[2];
    double z_meter = tile_box.max[1] - tile_box.min[1];
    if (width_meter < 0.01) { width_meter = 0.01; }
    if (height_meter < 0.01) { height_meter = 0.01; }
    if (z_meter < 0.01) { z_meter = 0.01; }
    Box box;
    std::vector<double> v = {
        center_mx,center_my,center_mz,
        width_meter / 2, 0, 0,
        0, height_meter / 2, 0,
        0, 0, z_meter / 2
    };
    std::memcpy(box.matrix, v.data(), 12 * sizeof(double));
    write_tileset_box(
        NULL, box, 100, 
        b3dm_file_name.c_str(),
        tileset.c_str());
    return true;
}

// 所有接口都是 utf8 字符串
extern "C" bool osgb2glb(const char* in, const char* out) {
    std::string b3dm_buf;
    std::vector<mesh_info> v_info;
    std::string path = osg_string(in);
    bool ret = osgb2glb_buf(path,b3dm_buf,v_info);
    if (!ret) return false;
    ret = write_file(out, b3dm_buf.data(), b3dm_buf.size());
    if (!ret) return false;
    return true;
}