#include <osg/Material>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <set>
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"
#include "stb_image_write.h"
#include "dxt_img.h"
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

struct TileBox
{
    std::vector<double> max;
    std::vector<double> min;

    void extend(double ratio) {
        ratio /= 2;
        double x = max[0] - min[0];
        double y = max[1] - min[1];
        double z = max[2] - min[2];
        max[0] += x * ratio;
        max[1] += y * ratio;
        max[2] += z * ratio;

        min[0] -= x * ratio;
        min[1] -= y * ratio;
        min[2] -= z * ratio;
    }
};

struct osg_tree {
    TileBox bbox;
    double geometricError;
    std::string file_name;
    std::vector<osg_tree> sub_nodes;
};

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
                texture_map[&geometry] = tex;
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
    // 记录 mesh 和 texture 的关系，暂时认为一个模型最多只有一个 texture
    std::map<osg::Geometry*, osg::Texture*> texture_map; 
    std::vector<std::string> sub_node_names;
};

double get_geometric_error(TileBox& bbox){
#ifdef max
#undef max
#endif // max

    double max_err = std::max((bbox.max[0] - bbox.min[0]),(bbox.max[1] - bbox.min[1]));
    max_err = std::max(max_err, (bbox.max[2] - bbox.min[2]));
    return max_err / 20.0;
//     const double pi = std::acos(-1);
//     double round = 2 * pi * 6378137.0 / 128.0;
//     return round / std::pow(2.0, lvl );
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
    else if(p0 != std::string::npos){
        int end = p0 + 2;
        while (true) {
            if (isdigit(stem[end]))
                end++;
            else
                break;
        }
        std::string substr = stem.substr(p0 + 2, end - p0 - 2);
        try { return std::stol(substr); }
        catch (...) {
            return -1;
        }
    }
    return -1;
}



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

template<class T>
void alignment_buffer(std::vector<T>& buf) {
    while (buf.size() % 4 != 0) {
        buf.push_back(0x00);
    }
}

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
                    {
                        osg::PrimitiveSet* ps = g->getPrimitiveSet(0);
                        osg::PrimitiveSet::Type t = ps->getType();
                        int idx_size = ps->getNumIndices();
                        int max_index = 0, min_index = 1 << 30;
                        switch (t)
                        {
                            case(osg::PrimitiveSet::DrawElementsUBytePrimitiveType):
                            {
                                const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(ps);
                                int IndNum = drawElements->getNumIndices();
                                for (size_t m = 0; m < IndNum; m++)
                                {
                                    put_val(buffer.data, drawElements->at(m));
                                    if (drawElements->at(m) > max_index) max_index = drawElements->at(m);
                                    if (drawElements->at(m) < min_index) min_index = drawElements->at(m);
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
                                    if (drawElements->at(m) > max_index) max_index = drawElements->at(m);
                                    if (drawElements->at(m) < min_index) min_index = drawElements->at(m);
                                }
                                break;
                            }
                            case(osg::PrimitiveSet::DrawElementsUIntPrimitiveType):
                            {
                                const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(ps);
                                unsigned int IndNum = drawElements->getNumIndices();
                                for (size_t m = 0; m < IndNum; m++)
                                {
                                    put_val(buffer.data, drawElements->at(m));
                                    if (drawElements->at(m) > (unsigned)max_index) max_index = drawElements->at(m);
                                    if (drawElements->at(m) < (unsigned)min_index) min_index = drawElements->at(m);
                                }
                                break;
                            }
                            case osg::PrimitiveSet::DrawArraysPrimitiveType: {
                                osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(ps);
                                auto mode = da->getMode();
                                if (mode != GL_TRIANGLES) {
                                    LOG_E("GLenum is not GL_TRIANGLES in osgb");
                                }
                                int first = da->getFirst();
                                int count = da->getCount();
                                int max_num = first + count;
                                if (max_num >= 65535) {
                                    max_num = 65535; idx_size = 65535;
                                }
                                min_index = first;
                                max_index = max_num - 1;
                                for (int i = first; i < max_num; i++) {
                                    if (max_num < 256)
                                        put_val(buffer.data, (unsigned char)i);
                                    else if (max_num < 65536)
                                        put_val(buffer.data, (unsigned short)i);
                                    else 
                                        put_val(buffer.data, i);
                                }
                                break;
                            }
                            default:
                            {
                                LOG_E("missing osg::PrimitiveSet::Type [%d]", t);
                                break;
                            }
                        }
                        tinygltf::Accessor acc;
                        acc.bufferView = 0;
                        acc.byteOffset = acc_offset[j];
                        alignment_buffer(buffer.data);
                        acc_offset[j] = buffer.data.size();
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
                            case osg::PrimitiveSet::DrawArraysPrimitiveType: {
                                osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(ps);
                                int first = da->getFirst();
                                int count = da->getCount();
                                int max_num = first + count;
                                if (max_num >= 65535) max_num = 65535;
                                if (max_num < 256) {
                                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                                } else if(max_num < 65536) {
                                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                                }else {
                                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                                }
                                break;
                            }
                            default:
                            //LOG_E("missing osg::PrimitiveSet::Type [%d]", t);
                            break;
                        }
                        acc.count = idx_size;
                        acc.type = TINYGLTF_TYPE_SCALAR;
                        osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                        int vec_size = v3f->size();
                        acc.maxValues = { (double)max_index };
                        acc.minValues = { (double)min_index };
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
                        put_val(buffer.data, point.y());
                        put_val(buffer.data, point.z());
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
                    alignment_buffer(buffer.data);
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
                        put_val(buffer.data, point.y());
                        put_val(buffer.data, point.z());

                        if (point.x() > box_max[0]) box_max[0] = point.x();
                        if (point.x() < box_min[0]) box_min[0] = point.x();
                        if (point.y() > box_max[1]) box_max[1] = point.y();
                        if (point.y() < box_min[1]) box_min[1] = point.y();
                        if (point.z() > box_max[2]) box_max[2] = point.z();
                        if (point.z() < box_min[2]) box_min[2] = point.z();
                    }
                    tinygltf::Accessor acc;
                    acc.bufferView = 2;
                    acc.byteOffset = acc_offset[j];
                    alignment_buffer(buffer.data);
                    acc_offset[j] = buffer.data.size() - buf_offset;
                    acc.count = normal_size;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.type = TINYGLTF_TYPE_VEC3;
                    acc.maxValues = box_max;
                    acc.minValues = box_min;
                    model.accessors.push_back(acc);
                }
                else if (j == 3) {
                    // text
                    vector<double> box_max = { -1e38, -1e38 };
                    vector<double> box_min = { 1e38, 1e38 };
                    int texture_size = 0;
                    osg::Array* na = g->getTexCoordArray(0);
                    if (na) {
                        osg::Vec2Array* v2f = (osg::Vec2Array*)na;
                        texture_size = v2f->size();
                        for (int vidx = 0; vidx < texture_size; vidx++)
                        {
                            osg::Vec2f point = v2f->at(vidx);
                            put_val(buffer.data, point.x());
                            put_val(buffer.data, point.y());
                            if (point.x() > box_max[0]) box_max[0] = point.x();
                            if (point.x() < box_min[0]) box_min[0] = point.x();
                            if (point.y() > box_max[1]) box_max[1] = point.y();
                            if (point.y() < box_min[1]) box_min[1] = point.y();
                        }
                    }
                    else { // mesh 没有纹理坐标
                        osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                        int vec_size = v3f->size();
                        texture_size = vec_size;
                        box_max = { 0,0 };
                        box_min = { 0,0 };
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
                    alignment_buffer(buffer.data);
                    acc_offset[j] = buffer.data.size() - buf_offset;
                    acc.count = texture_size;
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    acc.type = TINYGLTF_TYPE_VEC2;
                    acc.maxValues = box_max;
                    acc.minValues = box_min;
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
            alignment_buffer(buffer.data);
            bfv.byteLength = buffer.data.size() - buf_offset;
            buf_offset = buffer.data.size();
            if (infoVisitor.geometry_array.size() > 1) {
                if (j == 1) { bfv.byteStride = 4 * 3; } 
                if (j == 2) { bfv.byteStride = 4 * 3; }
                if (j == 3) { bfv.byteStride = 4 * 2; } 
            }
            model.bufferViews.push_back(bfv);
        }
        // image
        {
            int buf_view = 4;
            for (auto tex : infoVisitor.texture_array) {
                std::vector<unsigned char> jpeg_buf;
                jpeg_buf.reserve(512 * 512 * 3);
                int width, height, comp;
                {
                    if (tex) {
                        if (tex->getNumImages() > 0) {
                            osg::Image* img = tex->getImage(0);
                            if (img) {
                                width = img->s();
                                height = img->t();
                                comp = img->getPixelSizeInBits();
                                if (comp == 8) comp = 1;
                                if (comp == 24) comp = 3;
                                if (comp == 4) {
                                    comp = 3;
                                    fill_4BitImage(jpeg_buf, img, width, height);
                                }
                                else
                                {
                                    unsigned row_step = img->getRowStepInBytes();
                                    unsigned row_size = img->getRowSizeInBytes();
                                    for (size_t i = 0; i < height; i++)
                                    {
                                        jpeg_buf.insert(jpeg_buf.end(),
                                            img->data() + row_step * i,
                                            img->data() + row_step * i + row_size);
                                    }
                                }
                            }
                        }
                    }
                }
                if (!jpeg_buf.empty()) {
                    int buf_size = buffer.data.size();
                    buffer.data.reserve(buffer.data.size() + width * height * comp);
                    stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, comp, jpeg_buf.data(), 80);
                }
                else {
                    std::vector<char> v_data;
                    width = height = 256;
                    v_data.resize(width * height * 3);
                    stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, v_data.data(), 80);
                }
                tinygltf::Image image;
                image.mimeType = "image/jpeg";

                image.bufferView = buf_view++;
                model.images.push_back(image);
                tinygltf::BufferView bfv;
                bfv.buffer = 0;
                bfv.byteOffset = buf_offset;
                bfv.byteLength = buffer.data.size() - buf_offset;
                alignment_buffer(buffer.data);
                buf_offset = buffer.data.size();
                model.bufferViews.push_back(bfv);
            }
        }
        // mesh 
        {
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
                if (infoVisitor.texture_array.size() > 1) {
                    auto geomtry = infoVisitor.geometry_array[i];
                    auto tex = infoVisitor.texture_map[geomtry];
                    for (auto texture : infoVisitor.texture_array) {
                        if (tex != texture) {
                            primits.material++;
                        }
                        else {
                            break;
                        }
                    }
                }
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
        }
        // scene
        {
            // 一个场景
            tinygltf::Scene sence;
            for (int i = 0; i < infoVisitor.geometry_array.size(); i++) {
                sence.nodes.push_back(i);
            }
            // 所有场景
            model.scenes = { sence };
            model.defaultScene = 0;
        }
        // sample
        {
            tinygltf::Sampler sample;
            sample.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
            sample.minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
            sample.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
            sample.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
            model.samplers = { sample };
        }
        /// --------------
        if(0)
        {
            for (int i = 0 ; i < infoVisitor.texture_array.size(); i++)
            {
                tinygltf::Material material;
                material.name = "default";
                tinygltf::Parameter baseColorFactor;
                baseColorFactor.number_array = { 1.0,1.0,1.0,1.0 };
                material.values["baseColorFactor"] = baseColorFactor;
                // 可能会出现多材质的情况
                tinygltf::Parameter baseColorTexture;
                baseColorTexture.json_int_value = { std::pair<string,int>("index",i) };
                material.values["baseColorTexture"] = baseColorTexture;

                tinygltf::Parameter metallicFactor;
                metallicFactor.number_value = new double(0);
                material.values["metallicFactor"] = metallicFactor;
                tinygltf::Parameter roughnessFactor;
                roughnessFactor.number_value = new double(1);
                material.values["roughnessFactor"] = roughnessFactor;
                /// ---------
//                 tinygltf::Parameter emissiveFactor;
//                 emissiveFactor.number_array = { 0.0,0.0,0.0 };
//                 material.additionalValues["emissiveFactor"] = emissiveFactor;
//                 tinygltf::Parameter alphaMode;
//                 alphaMode.string_value = "OPAQUE";
//                 material.additionalValues["alphaMode"] = alphaMode;
//                 tinygltf::Parameter doubleSided;
//                 doubleSided.bool_value = false;
//                 material.additionalValues["doubleSided"] = doubleSided;
                //
                model.materials.push_back(material);
            }
        }
        // use shader material
        else {
            model.extensionsRequired = { "KHR_technique_webgl" };
            model.extensionsUsed = { "KHR_technique_webgl" };
            // add shader buffer view
            {
                tinygltf::BufferView bfv_vs;
                bfv_vs.buffer = 0;
                bfv_vs.byteOffset = buf_offset;
                bfv_vs.target = TINYGLTF_TARGET_ARRAY_BUFFER;
                std::string vs_shader = R"(
precision highp float;
uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;
attribute vec3 a_position;
attribute vec2 a_texcoord0;
attribute float a_batchid;
varying vec2 v_texcoord0;
void main(void)
{   
    v_texcoord0 = a_texcoord0;
    gl_Position = u_projectionMatrix * u_modelViewMatrix * vec4(a_position, 1.0);
}
)";

                buffer.data.insert(buffer.data.end(), vs_shader.begin(), vs_shader.end());
                bfv_vs.byteLength = buffer.data.size() - buf_offset;
                alignment_buffer(buffer.data);
                buf_offset = buffer.data.size();
                model.bufferViews.push_back(bfv_vs);

                tinygltf::BufferView bfv_fs;
                bfv_fs.buffer = 0;
                bfv_fs.byteOffset = buf_offset;
                bfv_fs.target = TINYGLTF_TARGET_ARRAY_BUFFER;
                std::string fs_shader = R"(
precision highp float;
varying vec2 v_texcoord0;
uniform sampler2D u_diffuse;
void main(void)
{
  gl_FragColor = texture2D(u_diffuse, v_texcoord0);
}
)";
                buffer.data.insert(buffer.data.end(), fs_shader.begin(), fs_shader.end());
                bfv_fs.byteLength = buffer.data.size() - buf_offset;
                alignment_buffer(buffer.data);
                buf_offset = buffer.data.size();
                model.bufferViews.push_back(bfv_fs);
            }
            // shader
            {
                int buf_view = 4 + infoVisitor.texture_array.size();
                {
                    tinygltf::Shader shader;
                    shader.bufferView = buf_view++;
                    shader.type = TINYGLTF_SHADER_TYPE_VERTEX_SHADER;
                    model.shaders.push_back(shader);
                }
                {
                    tinygltf::Shader shader;
                    shader.bufferView = buf_view++;
                    shader.type = TINYGLTF_SHADER_TYPE_FRAGMENT_SHADER;
                    model.shaders.push_back(shader);
                }
            }
            // tech
            {
                tinygltf::Technique tech;
                tech.tech_string = R"(
{
      "attributes": {
        "a_batchid": "batchid",
        "a_position": "position",
        "a_texcoord0": "texcoord0"
      },
      "parameters": {
        "batchid": {
          "semantic": "_BATCHID",
          "type": 5123
        },
        "diffuse": {
          "type": 35678
        },
        "modelViewMatrix": {
          "semantic": "MODELVIEW",
          "type": 35676
        },
        "position": {
          "semantic": "POSITION",
          "type": 35665
        },
        "projectionMatrix": {
          "semantic": "PROJECTION",
          "type": 35676
        },
        "texcoord0": {
          "semantic": "TEXCOORD_0",
          "type": 35664
        }
      },
      "program": 0,
      "states": {
        "enable": [
          2884,
          2929
        ]
      },
      "uniforms": {
        "u_diffuse": "diffuse",
        "u_modelViewMatrix": "modelViewMatrix",
        "u_projectionMatrix": "projectionMatrix"
      }
    })";
                model.techniques = { tech };
            }
            // program
            {
                tinygltf::Program prog;
                prog.prog_string = R"(
    {
      "attributes": [
        "a_position",
        "a_texcoord0"
      ],
      "vertexShader": 0,
      "fragmentShader": 1
    }
)";
                model.programs = { prog };
            }

            for (int i = 0; i < infoVisitor.texture_array.size(); i++)
            {
                tinygltf::Material material;
                material.name = "osgb";
                //material.values[""]
                char shaderBuffer[512];
                sprintf(shaderBuffer, R"(
            {
      "extensions": {
        "KHR_technique_webgl": {
          "technique": 0,
          "values": {
            "diffuse": 0
          }
        }
      },
      "technique": 0,
      "values": {
        "diffuse": {
          "index": %d,
          "texCoord": 0
        }
      }
            })", i);
                material.shaderMaterial = shaderBuffer;
                model.materials.push_back(material);
            }
        }
        // finish buffer
        model.buffers.push_back(std::move(buffer));

        /// ----------------------
        // texture
        {
            int texture_index = 0;
            for (auto tex : infoVisitor.texture_array)
            {
                tinygltf::Texture texture;
                texture.source = texture_index++;
                texture.sampler = 0;
                model.textures.push_back(texture);
            }
        }
        model.asset.version = "2.0";
        model.asset.generator = "fanfan";

        glb_buff = gltf.Serialize(&model);
    }
    return true;
}

bool osgb23dtile_buf(std::string path, std::string& b3dm_buf, TileBox& tile_box) {
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


std::vector<double> convert_bbox(TileBox tile) {
    double center_mx = (tile.max[0] + tile.min[0]) / 2;
    double center_my = (tile.max[1] + tile.min[1]) / 2;
    double center_mz = (tile.max[2] + tile.min[2]) / 2;
    double x_meter = (tile.max[0] - tile.min[0]) * 1;
    double y_meter = (tile.max[1] - tile.min[1]) * 1;
    double z_meter = (tile.max[2] - tile.min[2]) * 1;
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
    // test
    //std::string glb_buf;
    //std::vector<mesh_info> v_info;
    //osgb2glb_buf(tree.file_name, glb_buf, v_info);
    //out_file = replace(out_file, ".b3dm", ".glb");
    //write_file(out_file.c_str(), glb_buf.data(), glb_buf.size());
    // end test
    for (auto& i : tree.sub_nodes) {
        do_tile_job(i,out_path,max_lvl);
    }
}

void expend_box(TileBox& box, TileBox& box_new) {
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

TileBox extend_tile_box(osg_tree& tree) {
    TileBox box = tree.bbox;
    for (auto& i : tree.sub_nodes) {
        TileBox sub_tile = extend_tile_box(i);
        expend_box(box, sub_tile);
    }
    tree.bbox = box;
    return box;
}

std::string get_boundingBox(TileBox bbox) {
    std::string box_str = "\"boundingVolume\":{";
    box_str += "\"box\":[";
    std::vector<double> v_box = convert_bbox(bbox);
    for (auto v: v_box) {
        box_str += std::to_string(v);
        box_str += ",";
    }
    box_str.pop_back();
    box_str += "]}";
    return box_str;
}

std::string get_boundingRegion(TileBox bbox, double x, double y) {
    std::string box_str = "\"boundingVolume\":{";
    box_str += "\"region\":[";
    std::vector<double> v_box(6);
    v_box[0] = meter_to_longti(bbox.min[0],y) + x;
    v_box[1] = meter_to_lati(bbox.min[1]) + y;
    v_box[2] = meter_to_longti(bbox.max[0], y) + x;
    v_box[3] = meter_to_lati(bbox.max[1]) + y;
    v_box[4] = bbox.min[2];
    v_box[5] = bbox.max[2];

    for (auto v : v_box) {
        box_str += std::to_string(v);
        box_str += ",";
    }
    box_str.pop_back();
    box_str += "]}";
    return box_str;
}

void calc_geometric_error(osg_tree& tree) {
    // depth first
    for ( auto& i : tree.sub_nodes ){
        calc_geometric_error(i);
    }
    if (tree.sub_nodes.empty()) {
        tree.geometricError = 0.0;
    }
    else {
        if(tree.sub_nodes[0].geometricError == 0) 
            tree.geometricError = get_geometric_error(tree.bbox);
        else
            tree.geometricError = tree.sub_nodes[0].geometricError * 2.0;
    }
}

std::string encode_tile_json(osg_tree& tree, double x, double y) {
    if (tree.bbox.max.empty() || tree.bbox.min.empty()) {
        return "";
    }
    
    std::string file_name = get_file_name(tree.file_name);
    std::string parent_str = get_parent(tree.file_name);
    std::string file_path = get_file_name(parent_str);

    char buf[512];
    sprintf(buf, "{ \"geometricError\":%.2f,", tree.geometricError);
    std::string tile = buf;
    TileBox cBox = tree.bbox;
    //cBox.extend(0.1);
    std::string content_box = get_boundingBox(cBox);
    TileBox bbox = tree.bbox;
    //bbox.extend(0.1);
    std::string tile_box = get_boundingBox(bbox);

    tile += tile_box;
    tile += ",";
    tile += "\"content\":{ \"url\":";
    // Data/Tile_0/Tile_0.b3dm
    std::string url_path = "./";
    url_path += file_name;
    std::string url = replace(url_path,".osgb",".b3dm");
    tile += "\"";
    tile += url;
    tile += "\",";
    tile += content_box;
    tile += "},\"children\":[";
    for ( auto& i : tree.sub_nodes ){
        std::string node_json = encode_tile_json(i,x,y);
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
    double *box, int* len, double x, double y,int max_lvl) {
    
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
    // prevent for root node disappear
    calc_geometric_error(root);
    root.geometricError = 1000.0;
    std::string json = encode_tile_json(root,x,y);
    root.bbox.extend(0.2);
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
    TileBox tile_box;
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