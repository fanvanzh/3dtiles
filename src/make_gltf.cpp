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

#include "tiny_gltf.h"
#include "stb_image_write.h"
#include "dxt_img.h"
#include "extern.h"

using namespace std;

namespace gltf_convert {

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

std::string osg_string(const char* path) {
#ifdef WIN32
    std::string root_path =
        osgDB::convertStringFromUTF8toCurrentCodePage(path);
#else
    std::string root_path = (path);
#endif // WIN32
    return root_path;
}

template<class T>
void alignment_buffer(std::vector<T>& buf) {
    while (buf.size() % 4 != 0) {
        buf.push_back(0x00);
    }
}

bool make_glb_buf(std::string path, std::string& glb_buff) {
    path = osg_string(path.c_str());
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
//                                 if (max_num >= 65535) {
//                                     max_num = 65535; 
//                                     idx_size = 65535;
//                                 }
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
                                //if (max_num >= 65535) max_num = 65535;
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
                if (j == 1) { bfv.byteStride = 4 * 3; } // 
                if (j == 2) { bfv.byteStride = 4 * 3; } // 
                if (j == 3) { bfv.byteStride = 4 * 2; } // 
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
                metallicFactor.number_value = new double(0.3);
                material.values["metallicFactor"] = metallicFactor;
                tinygltf::Parameter roughnessFactor;
                roughnessFactor.number_value = new double(0.7);
                material.values["roughnessFactor"] = roughnessFactor;
                /// ---------
                tinygltf::Parameter emissiveFactor;
                emissiveFactor.number_array = { 0.0,0.0,0.0 };
                material.additionalValues["emissiveFactor"] = emissiveFactor;
                tinygltf::Parameter alphaMode;
                alphaMode.string_value = "OPAQUE";
                material.additionalValues["alphaMode"] = alphaMode;
                tinygltf::Parameter doubleSided;
                doubleSided.bool_value = new bool(false);
                material.additionalValues["doubleSided"] = doubleSided;
                //
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
}

// extern "C" 
extern "C" bool make_gltf(const char* in_path, const char* out_path) {
    std::string glb_buf;
    bool ret = gltf_convert::make_glb_buf(in_path, glb_buf);
    if (!ret) return false;
    ret = write_file(out_path, glb_buf.data(), glb_buf.size());
    if (!ret) return false;
    return true;
}