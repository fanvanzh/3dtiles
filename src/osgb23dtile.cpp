#include <osg/Material>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>
#include <Eigen/Eigen>

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
#include "GeoTransform.h"

using namespace std;

#ifdef max
#undef max
#endif // max
#ifdef min
#undef min
#endif // max

static bool b_pbr_texture = false;

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
        if (GeoTransform::pOgrCT)
        {
            osg::Vec3Array *vertexArr = (osg::Vec3Array *)geometry.getVertexArray();
            OGRCoordinateTransformation *poCT = GeoTransform::pOgrCT;

            /** 1. We obtain the bound of this tile */
            glm::dvec3 Min = glm::dvec3(DBL_MAX);
            glm::dvec3 Max = glm::dvec3(-DBL_MAX);
            for (int VertexIndex = 0; VertexIndex < vertexArr->size(); VertexIndex++)
            {
                osg::Vec3d Vertex = vertexArr->at(VertexIndex);
                glm::dvec3 vertex = glm::dvec3(Vertex.x(), Vertex.y(), Vertex.z());
                Min = glm::min(vertex, Min);
                Max = glm::max(vertex, Max);
            }

            /**
             * 2. We correct the eight points of the bounding box.
             * The point will be transformed from projected coordinate system
             * which is given by the original osgb tileset to geographic coordinate system, 
             * and then transformed to Cesium ECEF coordinate system, 
             * at last we transform the point from ECEF to the original projected coordinate system again. 
             * We do this to correct the coordinate offset that 
             * can occur when the tile is located far from the origin.
             */ 
            auto Correction = [&](glm::dvec3 Point) {
                glm::dvec3 cartographic = Point + glm::dvec3(GeoTransform::OriginX, GeoTransform::OriginY, GeoTransform::OriginZ);
                poCT->Transform(1, &cartographic.x, &cartographic.y, &cartographic.z);
                glm::dvec3 ecef = GeoTransform::CartographicToEcef(cartographic.x, cartographic.y, cartographic.z);
                glm::dvec3 enu = GeoTransform::EcefToEnuMatrix * glm::dvec4(ecef, 1);
                return enu;
            };
            vector<glm::dvec4> OriginalPoints(8);
            vector<glm::dvec4> CorrectedPoints(8);
            OriginalPoints[0] = glm::dvec4(Min.x, Min.y, Min.z, 1);
            OriginalPoints[1] = glm::dvec4(Max.x, Min.y, Min.z, 1);
            OriginalPoints[2] = glm::dvec4(Min.x, Max.y, Min.z, 1);
            OriginalPoints[3] = glm::dvec4(Min.x, Min.y, Max.z, 1);
            OriginalPoints[4] = glm::dvec4(Max.x, Max.y, Min.z, 1);
            OriginalPoints[5] = glm::dvec4(Min.x, Max.y, Max.z, 1);
            OriginalPoints[6] = glm::dvec4(Max.x, Min.y, Max.z, 1);
            OriginalPoints[7] = glm::dvec4(Max.x, Max.y, Max.z, 1);
            for (int i = 0; i < 8; i++)
                CorrectedPoints[i] = glm::dvec4(Correction(OriginalPoints[i]), 1);

            /**
             * 3. We use the least squares method to calculate the transformation matrix
             * that transforms the original box to the corrected box.
            */
            Eigen::MatrixXd A, B;
            A.resize(8, 4);
            B.resize(8, 4);
            for (int row = 0; row < 8; row++)
            {
                A.row(row) << OriginalPoints[row].x, OriginalPoints[row].y, OriginalPoints[row].z, 1;
            }
            for (int row = 0; row < 8; row++)
            {
                B.row(row) << CorrectedPoints[row].x, CorrectedPoints[row].y, CorrectedPoints[row].z, 1;
            }
            Eigen::BDCSVD<Eigen::MatrixXd> SVD(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
            Eigen::MatrixXd X = SVD.solve(B);

            /*
             * 4. At last we apply the matrix to all the points of the tile to correct the offset.
            */
            glm::dmat4 Transform = glm::dmat4(
                X(0, 0), X(0, 1), X(0, 2), X(0, 3),
                X(1, 0), X(1, 1), X(1, 2), X(1, 3),
                X(2, 0), X(2, 1), X(2, 2), X(2, 3),
                X(3, 0), X(3, 1), X(3, 2), X(3, 3));

            for (int VertexIndex = 0; VertexIndex < vertexArr->size(); VertexIndex++)
            {
                osg::Vec3d Vertex = vertexArr->at(VertexIndex);
                glm::dvec4 v = Transform * glm::dvec4(Vertex.x(), Vertex.y(), Vertex.z(), 1);
                Vertex = osg::Vec3d(v.x, v.y, v.z);
                vertexArr->at(VertexIndex) = Vertex;
            }
        }
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
    std::map<osg::Geometry*, osg::Texture*> texture_map;
    std::vector<std::string> sub_node_names;
};

double get_geometric_error(TileBox& bbox){
    if (bbox.max.empty() || bbox.min.empty())
    {
        LOG_E("bbox is empty!");
        return 0;
    }

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

struct MeshInfo
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

std::string vs_str() {
    return
R"(
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
}

std::string fs_str() {
    return
R"(
precision highp float;
varying vec2 v_texcoord0;
uniform sampler2D u_diffuse;
void main(void)
{
  gl_FragColor = texture2D(u_diffuse, v_texcoord0);
}
)";
}

std::string program(int vs, int fs) {
    char buf[512];
std::string fmt = R"(
{
"attributes": [
"a_position",
"a_texcoord0"
],
"vertexShader": %d,
"fragmentShader": %d
}
)";
    sprintf(buf, fmt.data(), vs, fs);
    return buf;
}

std::string tech_string() {
return
R"(
{
  "attributes": {
    "a_batchid": {
      "semantic": "_BATCHID",
      "type": 5123
    },
    "a_position": {
      "semantic": "POSITION",
      "type": 35665
    },
    "a_texcoord0": {
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
    "u_diffuse": {
      "type": 35678
    },
    "u_modelViewMatrix": {
      "semantic": "MODELVIEW",
      "type": 35676
    },
    "u_projectionMatrix": {
      "semantic": "PROJECTION",
      "type": 35676
    }
  }
})";
}

void make_gltf2_shader(tinygltf::Model& model, int mat_size, tinygltf::Buffer& buffer) {
    model.extensionsRequired = { "KHR_techniques_webgl" };
    model.extensionsUsed = { "KHR_techniques_webgl" };
    // add vs shader
    {
        tinygltf::BufferView bfv_vs;
        bfv_vs.buffer = 0;
        bfv_vs.byteOffset = buffer.data.size();
        bfv_vs.target = TINYGLTF_TARGET_ARRAY_BUFFER;

        std::string vs_shader = vs_str();

        buffer.data.insert(buffer.data.end(), vs_shader.begin(), vs_shader.end());
        bfv_vs.byteLength = vs_shader.size();
        alignment_buffer(buffer.data);
        model.bufferViews.push_back(bfv_vs);

        tinygltf::Shader shader;
        shader.bufferView = model.bufferViews.size() - 1;
        shader.type = TINYGLTF_SHADER_TYPE_VERTEX_SHADER;
        model.extensions.KHR_techniques_webgl.shaders.push_back(shader);
    }
    // add fs shader
    {
        tinygltf::BufferView bfv_fs;
        bfv_fs.buffer = 0;
        bfv_fs.byteOffset = buffer.data.size();
        bfv_fs.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        std::string fs_shader = fs_str();
        buffer.data.insert(buffer.data.end(), fs_shader.begin(), fs_shader.end());
        bfv_fs.byteLength = fs_shader.size();
        alignment_buffer(buffer.data);
        model.bufferViews.push_back(bfv_fs);

        tinygltf::Shader shader;
        shader.bufferView = model.bufferViews.size() - 1;
        shader.type = TINYGLTF_SHADER_TYPE_FRAGMENT_SHADER;
        model.extensions.KHR_techniques_webgl.shaders.push_back(shader);
    }
    // tech
    {
        tinygltf::Technique tech;
        tech.tech_string = tech_string();
        model.extensions.KHR_techniques_webgl.techniques = { tech };
    }
    // program
    {
        tinygltf::Program prog;
        prog.prog_string = program(0, 1);
        model.extensions.KHR_techniques_webgl.programs = { prog };
    }

    for (int i = 0; i < mat_size; i++)
    {
        tinygltf::Material material;
        material.name = "osgb";
        char shaderBuffer[512];
        sprintf(shaderBuffer, R"(
{
"extensions": {
"KHR_techniques_webgl": {
"technique": 0,
"values": {
"u_diffuse": {
"index": %d,
"texCoord": 0
}
}
}
}
}
)", i);
        material.shaderMaterial = shaderBuffer;
        model.materials.push_back(material);
    }
}

tinygltf::Material make_color_material_osgb(double r, double g, double b) {
    tinygltf::Material material;
    material.name = "default";
    tinygltf::Parameter baseColorFactor;
    baseColorFactor.number_array = { r, g, b, 1.0 };
    material.values["baseColorFactor"] = baseColorFactor;

    tinygltf::Parameter metallicFactor;
    metallicFactor.number_value = new double(0);
    material.values["metallicFactor"] = metallicFactor;
    tinygltf::Parameter roughnessFactor;
    roughnessFactor.number_value = new double(1);
    material.values["roughnessFactor"] = roughnessFactor;
    //
    return material;
}

struct OsgBuildState
{
    tinygltf::Buffer* buffer;
    tinygltf::Model* model;
    osg::Vec3f point_max;
    osg::Vec3f point_min;
    int draw_array_first;
    int draw_array_count;
};

void expand_bbox3d(osg::Vec3f& point_max, osg::Vec3f& point_min, osg::Vec3f point)
{
    point_max.x() = std::max(point.x(), point_max.x());
    point_min.x() = std::min(point.x(), point_min.x());
    point_max.y() = std::max(point.y(), point_max.y());
    point_min.y() = std::min(point.y(), point_min.y());
    point_max.z() = std::max(point.z(), point_max.z());
    point_min.z() = std::min(point.z(), point_min.z());
}

void expand_bbox2d(osg::Vec2f& point_max, osg::Vec2f& point_min, osg::Vec2f point)
{
    point_max.x() = std::max(point.x(), point_max.x());
    point_min.x() = std::min(point.x(), point_min.x());
    point_max.y() = std::max(point.y(), point_max.y());
    point_min.y() = std::min(point.y(), point_min.y());
}

template<class T> void
write_osg_indecis(T* drawElements, OsgBuildState* osgState, int componentType)
{
    unsigned max_index = 0;
    unsigned min_index = 1 << 30;
    unsigned buffer_start = osgState->buffer->data.size();

    unsigned IndNum = drawElements->getNumIndices();
    for (unsigned m = 0; m < IndNum; m++)
    {
        auto idx = drawElements->at(m);
        put_val(osgState->buffer->data, idx);
        if (idx > max_index) max_index = idx;
        if (idx < min_index) min_index = idx;
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.type = TINYGLTF_TYPE_SCALAR;
    acc.componentType = componentType;
    acc.count = IndNum;
    acc.maxValues = { (double)max_index };
    acc.minValues = { (double)min_index };
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}

void
write_vec3_array(osg::Vec3Array* v3f, OsgBuildState* osgState, osg::Vec3f& point_max, osg::Vec3f& point_min)
{
    int vec_start = 0;
    int vec_end   = v3f->size();
    if (osgState->draw_array_first >= 0)
    {
        vec_start = osgState->draw_array_first;
        vec_end   = osgState->draw_array_count + vec_start;
    }
    unsigned buffer_start = osgState->buffer->data.size();
    for (int vidx = vec_start; vidx < vec_end; vidx++)
    {
        osg::Vec3f point = v3f->at(vidx);
        put_val(osgState->buffer->data, point.x());
        put_val(osgState->buffer->data, point.y());
        put_val(osgState->buffer->data, point.z());
        expand_bbox3d(point_max, point_min, point);
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.count = vec_end - vec_start;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type = TINYGLTF_TYPE_VEC3;
    acc.maxValues = {point_max.x(), point_max.y(), point_max.z()};
    acc.minValues = {point_min.x(), point_min.y(), point_min.z()};
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}

void
write_vec2_array(osg::Vec2Array* v2f, OsgBuildState* osgState)
{
    int vec_start = 0;
    int vec_end   = v2f->size();
    if (osgState->draw_array_first >= 0)
    {
        vec_start = osgState->draw_array_first;
        vec_end   = osgState->draw_array_count + vec_start;
    }
    osg::Vec2f point_max(-1e38, -1e38);
    osg::Vec2f point_min(1e38, 1e38);
    unsigned buffer_start = osgState->buffer->data.size();
    for (int vidx = vec_start; vidx < vec_end; vidx++)
    {
        osg::Vec2f point = v2f->at(vidx);
        put_val(osgState->buffer->data, point.x());
        put_val(osgState->buffer->data, point.y());
        expand_bbox2d(point_max, point_min, point);
    }
    alignment_buffer(osgState->buffer->data);

    tinygltf::Accessor acc;
    acc.bufferView = osgState->model->bufferViews.size();
    acc.count = vec_end - vec_start;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type = TINYGLTF_TYPE_VEC2;
    acc.maxValues = {point_max.x(), point_max.y()};
    acc.minValues = {point_min.x(), point_min.y()};
    osgState->model->accessors.push_back(acc);

    tinygltf::BufferView bfv;
    bfv.buffer = 0;
    bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    bfv.byteOffset = buffer_start;
    bfv.byteLength = osgState->buffer->data.size() - buffer_start;
    osgState->model->bufferViews.push_back(bfv);
}

struct PrimitiveState
{
    int vertexAccessor;
    int normalAccessor;
    int textcdAccessor;
};

void
write_element_array_primitive(osg::Geometry* g, osg::PrimitiveSet* ps, OsgBuildState* osgState, PrimitiveState* pmtState)
{
    tinygltf::Primitive primits;
    // indecis
    primits.indices = osgState->model->accessors.size();
    // reset draw_array state
    osgState->draw_array_first = -1;
    osg::PrimitiveSet::Type t = ps->getType();
    switch (t)
    {
        case(osg::PrimitiveSet::DrawElementsUBytePrimitiveType):
        {
            const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
            break;
        }
        case(osg::PrimitiveSet::DrawElementsUShortPrimitiveType):
        {
            const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            break;
        }
        case(osg::PrimitiveSet::DrawElementsUIntPrimitiveType):
        {
            const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(ps);
            write_osg_indecis(drawElements, osgState, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
            break;
        }
        case osg::PrimitiveSet::DrawArraysPrimitiveType:
        {
            primits.indices = -1;
            osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(ps);
            osgState->draw_array_first = da->getFirst();
            osgState->draw_array_count = da->getCount();
            break;
        }
        default:
        {
            LOG_E("unsupport osg::PrimitiveSet::Type [%d]", t);
            exit(1);
            break;
        }
    }
    // vertex: full vertex and part indecis
    if (pmtState->vertexAccessor > -1 && osgState->draw_array_first == -1)
    {
        primits.attributes["POSITION"] = pmtState->vertexAccessor;
    }
    else
    {
        osg::Vec3f point_max(-1e38, -1e38, -1e38);
        osg::Vec3f point_min(1e38, 1e38, 1e38);
        osg::Vec3Array* vertexArr = (osg::Vec3Array*)g->getVertexArray();
        primits.attributes["POSITION"] = osgState->model->accessors.size();
        // reuse vertex accessor if multi indecis
        if (pmtState->vertexAccessor == -1 && osgState->draw_array_first == -1)
        {
            pmtState->vertexAccessor = osgState->model->accessors.size();
        }
        write_vec3_array(vertexArr, osgState, point_max, point_min);
        // merge mesh bbox
        expand_bbox3d(osgState->point_max, osgState->point_min, point_max);
        expand_bbox3d(osgState->point_max, osgState->point_min, point_min);
    }
    // normal
    osg::Vec3Array* normalArr = (osg::Vec3Array*)g->getNormalArray();
    if (normalArr)
    {
        if (pmtState->normalAccessor > -1 && osgState->draw_array_first == -1)
        {
            primits.attributes["NORMAL"] = pmtState->normalAccessor;
        }
        else
        {
            osg::Vec3f point_max(-1e38, -1e38, -1e38);
            osg::Vec3f point_min(1e38, 1e38, 1e38);
            primits.attributes["NORMAL"] = osgState->model->accessors.size();
            // reuse vertex accessor if multi indecis
            if (pmtState->normalAccessor == -1 && osgState->draw_array_first == -1)
            {
                pmtState->normalAccessor = osgState->model->accessors.size();
            }
            write_vec3_array(normalArr, osgState, point_max, point_min);
        }
    }
    // textcoord
    osg::Vec2Array* texArr = (osg::Vec2Array*)g->getTexCoordArray(0);
    if (texArr)
    {
        if (pmtState->textcdAccessor > -1 && osgState->draw_array_first == -1)
        {
            primits.attributes["TEXCOORD_0"] = pmtState->textcdAccessor;
        }
        else
        {
            primits.attributes["TEXCOORD_0"] = osgState->model->accessors.size();
            // reuse textcoord accessor if multi indecis
            if (pmtState->textcdAccessor == -1 && osgState->draw_array_first == -1)
            {
                pmtState->textcdAccessor = osgState->model->accessors.size();
            }
            write_vec2_array(texArr, osgState);
        }
    }
    // material
    primits.material = -1;

    switch (ps->getMode())
    {
    case GL_TRIANGLES:
        primits.mode = TINYGLTF_MODE_TRIANGLES;
        break;
    case GL_TRIANGLE_STRIP:
        primits.mode = TINYGLTF_MODE_TRIANGLE_STRIP;
        break;
    case GL_TRIANGLE_FAN:
        primits.mode = TINYGLTF_MODE_TRIANGLE_FAN;
        break;
    default:
        LOG_E("Unsupport Primitive Mode: %d", (int)ps->getMode());
        exit(1);
        break;
    }
    osgState->model->meshes.back().primitives.push_back(primits);
}

void write_osgGeometry(osg::Geometry* g, OsgBuildState* osgState)
{
    osg::PrimitiveSet::Type t = g->getPrimitiveSet(0)->getType();
    PrimitiveState pmtState = {-1, -1, -1};
    for (unsigned int k = 0; k < g->getNumPrimitiveSets(); k++)
    {
        osg::PrimitiveSet* ps = g->getPrimitiveSet(k);
        if (t != ps->getType())
        {
            LOG_E("PrimitiveSets type are NOT same in osgb");
            exit(1);
        }
        write_element_array_primitive(g, ps, osgState, &pmtState);
    }
}

bool osgb2glb_buf(std::string path, std::string& glb_buff, MeshInfo& mesh_info) {
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

    tinygltf::TinyGLTF gltf;
    tinygltf::Model model;
    tinygltf::Buffer buffer;

    osg::Vec3f point_max, point_min;
    OsgBuildState osgState = {
        &buffer, &model, osg::Vec3f(-1e38,-1e38,-1e38), osg::Vec3f(1e38,1e38,1e38), -1, -1
    };
    // mesh
    model.meshes.resize(1);
    int primitive_idx = 0;
    for (auto g : infoVisitor.geometry_array)
    {
        if (!g->getVertexArray() || g->getVertexArray()->getDataSize() == 0)
            continue;

        write_osgGeometry(g, &osgState);
        // update primitive material index
        if (infoVisitor.texture_array.size())
        {
            for (unsigned int k = 0; k < g->getNumPrimitiveSets(); k++)
            {
                auto tex = infoVisitor.texture_map[g];
                // if hava texture
                if (tex)
                {
                    for (auto texture : infoVisitor.texture_array)
                    {
                        model.meshes[0].primitives[primitive_idx].material++;
                        if (tex == texture)
                            break;
                    }
                }
                primitive_idx++;
            }
        }
    }
    // empty geometry or empty vertex-array
    if (model.meshes[0].primitives.empty())
        return false;

    mesh_info.min = {
        osgState.point_min.x(),
        osgState.point_min.y(),
        osgState.point_min.z()
    };
    mesh_info.max = {
        osgState.point_max.x(),
        osgState.point_max.y(),
        osgState.point_max.z()
    };
    // image
    {
        for (auto tex : infoVisitor.texture_array)
        {
            unsigned buffer_start = buffer.data.size();
            std::vector<unsigned char> jpeg_buf;
            jpeg_buf.reserve(512 * 512 * 3);
            int width, height, comp;
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
            image.bufferView = model.bufferViews.size();
            model.images.push_back(image);
            tinygltf::BufferView bfv;
            bfv.buffer = 0;
            bfv.byteOffset = buffer_start;
            alignment_buffer(buffer.data);
            bfv.byteLength = buffer.data.size() - buffer_start;
            model.bufferViews.push_back(bfv);
        }
    }
    // node
    {
        tinygltf::Node node;
        node.mesh = 0;
        model.nodes.push_back(node);
    }
    // scene
    {
        tinygltf::Scene sence;
        sence.nodes.push_back(0);
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
    // use KHR_materials_unlit
    model.extensionsRequired = { "KHR_materials_unlit" };
    model.extensionsUsed = { "KHR_materials_unlit" };
    for (int i = 0 ; i < infoVisitor.texture_array.size(); i++)
    {
        tinygltf::Material mat = make_color_material_osgb(1.0, 1.0, 1.0);
        mat.b_unlit = true; // use KHR_materials_unlit
        tinygltf::Parameter baseColorTexture;
        baseColorTexture.json_int_value = { std::pair<string,int>("index",i) };
        mat.values["baseColorTexture"] = baseColorTexture;
        model.materials.push_back(mat);
    }

    // finish buffer
    model.buffers.push_back(std::move(buffer));
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
    model.asset.generator = "fanvanzh";

    glb_buff = gltf.Serialize(&model);
    return true;
}

bool osgb2b3dm_buf(std::string path, std::string& b3dm_buf, TileBox& tile_box)
{
    using nlohmann::json;

    std::string glb_buf;
    MeshInfo minfo;
    bool ret = osgb2glb_buf(path, glb_buf, minfo);
    if (!ret)
        return false;

    tile_box.max = minfo.max;
    tile_box.min = minfo.min;

    int mesh_count = 1;
    std::string feature_json_string;
    feature_json_string += "{\"BATCH_LENGTH\":";
    feature_json_string += std::to_string(mesh_count);
    feature_json_string += "}";
    while ((feature_json_string.size()+28) % 8 != 0 ) {
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
    while (batch_json_string.size() % 8 != 0 ) {
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

void do_tile_job(osg_tree& tree, std::string out_path, int max_lvl) {
    std::string json_str;
    if (tree.file_name.empty()) return;
    int lvl = get_lvl_num(tree.file_name);
    if (lvl > max_lvl) return;
    std::string b3dm_buf;
    osgb2b3dm_buf(tree.file_name, b3dm_buf, tree.bbox);
    std::string out_file = out_path;
    out_file += "/";
    out_file += replace(get_file_name(tree.file_name),".osgb",".b3dm");
    if (!b3dm_buf.empty()) {
        write_file(out_file.c_str(), b3dm_buf.data(), b3dm_buf.size());
    }
    // test
    // std::string glb_buf;
    // std::vector<mesh_info> v_info;
    // osgb2glb_buf(tree.file_name, glb_buf, v_info);
    // out_file = replace(out_file, ".b3dm", ".glb");
    // write_file(out_file.c_str(), glb_buf.data(), glb_buf.size());
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
    const double EPS = 1e-12;
    // depth first
    for (auto& i : tree.sub_nodes) {
        calc_geometric_error(i);
    }
    if (tree.sub_nodes.empty()) {
        tree.geometricError = 0.0;
    }
    else {
        bool has = false;
        osg_tree leaf;
        for (auto& i : tree.sub_nodes) {
            if (abs(i.geometricError) > EPS)
            {
                has = true;
                leaf = i;
            }
        }

        if (has == false)
            tree.geometricError = get_geometric_error(tree.bbox);
        else
            tree.geometricError = leaf.geometricError * 2.0;
    }
}

std::string
encode_tile_json(osg_tree& tree, double x, double y)
{
    if (tree.bbox.max.empty() || tree.bbox.min.empty())
        return "";

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
    tile += "\"content\":{ \"uri\":";
    // Data/Tile_0/Tile_0.b3dm
    std::string uri_path = "./";
    uri_path += file_name;
    std::string uri = replace(uri_path,".osgb",".b3dm");
    tile += "\"";
    tile += uri;
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

/***/
extern "C" void* 
osgb23dtile_path(const char* in_path, const char* out_path,
                    double *box, int* len, double x, double y,
                    int max_lvl, bool pbr_texture)
{
    std::string path = osg_string(in_path);
    osg_tree root = get_all_tree(path);
    if (root.file_name.empty())
    {
        LOG_E( "open file [%s] fail!", in_path);
        return NULL;
    }
    b_pbr_texture = pbr_texture;
    do_tile_job(root, out_path, max_lvl);
    extend_tile_box(root);
    if (root.bbox.max.empty() || root.bbox.min.empty())
    {
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

extern "C" bool
osgb2glb(const char* in, const char* out)
{
    b_pbr_texture = true;
    MeshInfo minfo;
    std::string glb_buf;
    std::string path = osg_string(in);
    bool ret = osgb2glb_buf(path, glb_buf, minfo);
    if (!ret)
    {
        LOG_E("convert to glb failed");
        return false;
    }

    ret = write_file(out, glb_buf.data(), (unsigned long)glb_buf.size());
    if (!ret)
    {
        LOG_E("write glb file failed");
        return false;
    }
    return true;
}
