#include "gdal/ogrsf_frmts.h"
#include "tiny_gltf.h"
#include "earcut.hpp"
#include "json.hpp"
#include "extern.h"

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
#include <cmath>
#include <array>

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
    // 1 公里 ~ 0.01
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

struct Polygon_Mesh
{
    std::string mesh_name; // 模型名称
    Vextex vertex;
    Index  index;
    Normal normal;
    // add some addition 
    float height;
};

osg::ref_ptr<osg::Geometry> make_triangle_mesh_auto(Polygon_Mesh& mesh) {
    osg::ref_ptr<osg::Vec3Array> va = new osg::Vec3Array(mesh.vertex.size());
    for (int i = 0; i < mesh.vertex.size(); i++) {
        (*va)[i].set(mesh.vertex[i][0], mesh.vertex[i][1], mesh.vertex[i][2]);
    }
    osg::ref_ptr<osgUtil::DelaunayTriangulator> trig = new osgUtil::DelaunayTriangulator();
    trig->setInputPointArray(va);
    osg::Vec3Array *norms = new osg::Vec3Array;
    trig->setOutputNormalArray(norms);
    //三角化处理
    trig->triangulate();
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setVertexArray(va);
    geometry->setNormalArray(norms);
    auto* uIntId = trig->getTriangles();
    osg::DrawElementsUShort* _set = new osg::DrawElementsUShort(osg::DrawArrays::TRIANGLES);
    for (int i = 0; i < uIntId->getNumPrimitives(); i++) {
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

/**
@brief: convet polygon to mesh
@param: polygon , 多边形
@param: center_x, 投影中心点
@param: center_y, 投影中心点
*/
Polygon_Mesh convert_polygon(
    OGRPolygon* polyon, 
    double center_x, 
    double center_y,
    double height
    ) {
    double bottom = 0.0;
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
        // 剖分三角形
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

std::string make_polymesh(std::vector<Polygon_Mesh>& meshes);
std::string make_b3dm(std::vector<Polygon_Mesh>& meshes, bool);
//
extern "C" bool shp23dtile(
    const char* filename, int layer_id, 
    const char* dest, const char* height)
{
    if (!filename || layer_id < 0 || layer_id > 10000 || !dest) {
        LOG_E("make shp23dtile [%s] failed", filename);
        return false;
    }
    std::string height_field = "";
    if( height ) {
        height_field = height;
    }
    GDALAllRegister();
    GDALDataset       *poDS;
    poDS = (GDALDataset*)GDALOpenEx(
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
    
    if (!height_field.empty()) {
        field_index = poLayer->GetLayerDefn()->GetFieldIndex(height_field.c_str());
        if (field_index == -1) {
            LOG_E("can`t found field [%s] in [%s]", height_field.c_str(), filename);
        }
    }
    for (auto item : items_array) {
        node* _node = (node*)item;
        char b3dm_file[512];
        sprintf(b3dm_file, "%s\\tile\\%d\\%d", dest, _node->_z, _node->_x);
        mkdirs(b3dm_file);
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
                    v_meshes.push_back(mesh);
                }
            }
            OGRFeature::DestroyFeature(poFeature);
        }

        sprintf(b3dm_file, "%s\\tile\\%d\\%d\\%d.b3dm", dest, _node->_z, _node->_x, _node->_y);
        std::string b3dm_buf = make_b3dm(v_meshes, true);
        write_file(b3dm_file, b3dm_buf.data(), b3dm_buf.size());
        // test
        //sprintf(b3dm_file, "%s\\tile\\%d\\%d\\%d.glb", dest, _node->_z, _node->_x, _node->_y);
        //std::string glb_buf = make_polymesh(v_meshes);
        //write_file(b3dm_file, glb_buf.data(), glb_buf.size());
        //

        char b3dm_name[512], tile_json_path[512];
        sprintf(b3dm_name,"./tile/%d/%d/%d.b3dm",_node->_z,_node->_x,_node->_y);
        sprintf(tile_json_path, "%s\\tile\\%d\\%d\\%d.json", dest, _node->_z, _node->_x, _node->_y);
        double box_width = ( _node->_box.maxx - _node->_box.minx )  ;
        double box_height = ( _node->_box.maxy - _node->_box.miny ) ;
        const double pi = std::acos(-1);
        double radian_x = degree2rad(center_x);
        double radian_y = degree2rad(center_y);
        write_tileset(radian_x, radian_y, 
            longti_to_meter(degree2rad(box_width) * 1.05, radian_y),
            lati_to_meter(degree2rad(box_height)  * 1.05),
            0 , max_height, 100,
            b3dm_name,tile_json_path);
    }
    //
    GDALClose(poDS);
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
    tinygltf::Parameter baseColorFactor;
    baseColorFactor.number_array = { r,g,b,1 };
    material.values["baseColorFactor"] = baseColorFactor;
    tinygltf::Parameter metallicFactor;
    metallicFactor.number_value = new double(0.3);
    material.values["metallicFactor"] = metallicFactor;
    tinygltf::Parameter roughnessFactor;
    roughnessFactor.number_value = new double(0.7);
    material.values["roughnessFactor"] = roughnessFactor;
    return material;
}

// convert poly-mesh to glb buffer
std::string make_polymesh(std::vector<Polygon_Mesh>& meshes) {
    vector<osg::ref_ptr<osg::Geometry>> osg_Geoms;
    for (auto& mesh : meshes) {
        osg_Geoms.push_back(make_triangle_mesh(mesh));
    }
    tinygltf::TinyGLTF gltf;
    tinygltf::Model model;
    // model.name = model_name;
    // only one buffer
    tinygltf::Buffer buffer;
    // buffer_view {index,vertex,normal(texcoord,image)}
    uint32_t buf_offset = 0;
    auto calc_offset = [&]() -> int{
        return buffer.data.size() - buf_offset;
    };
    uint32_t acc_offset[4] = {0,0,0,0};
    int buf_times = 4;
    for (int j = 0; j < buf_times; j++)
    {
        for (int i = 0; i < meshes.size(); i++) {
            if (osg_Geoms[i]->getNumPrimitiveSets() == 0) continue;
            if (j == 0) {
                // indc
                osg::PrimitiveSet* ps = osg_Geoms[i]->getPrimitiveSet(0);
                int idx_size = ps->getNumIndices();
                int max_idx = 0;

                const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(ps);
                int IndNum = drawElements->getNumIndices();
                for (int m = 0; m < IndNum; m++)
                {
                    unsigned short idx = drawElements->at(m);
                    put_val(buffer.data, idx);
                    SET_MAX(max_idx, idx);
                }
                    
                tinygltf::Accessor acc;
                acc.bufferView = 0;
                acc.byteOffset = acc_offset[j];
                alignment_buffer(buffer.data);
                acc_offset[j] = calc_offset();
                acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                acc.count = idx_size;
                acc.type = TINYGLTF_TYPE_SCALAR;
                acc.maxValues = { (double)max_idx };
                acc.minValues = { 0.0 };
                model.accessors.push_back(acc);
            }else if ( j == 1) {
                osg::Array* va = osg_Geoms[i]->getVertexArray();
                osg::Vec3Array* v3f = (osg::Vec3Array*)va;
                int vec_size = v3f->size();
                std::vector<double> box_max = { -1e38, -1e38 ,-1e38 };
                std::vector<double> box_min = { 1e38, 1e38 ,1e38 };
                for (int vidx = 0; vidx < vec_size; vidx++) {
                    osg::Vec3f point = v3f->at(vidx);
                    vector<float> vertex = { point.x(), point.y(), point.z()};
                    for (int i = 0; i < 3; i++)
                    {
                        put_val(buffer.data, vertex[i]);
                        SET_MAX(box_max[i], vertex[i]);
                        SET_MIN(box_min[i], vertex[i]);
                    }
                }
                tinygltf::Accessor acc;
                acc.bufferView = 1;
                acc.byteOffset = acc_offset[j];
                alignment_buffer(buffer.data);
                acc_offset[j] = calc_offset();
                acc.count = vec_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
                acc.maxValues = box_max;
                acc.minValues = box_min;
                model.accessors.push_back(acc);
            }else if ( j == 2) {
                // normal
                osg::Array* na = osg_Geoms[i]->getNormalArray();
                if (!na) continue;
                osg::Vec3Array* v3f = (osg::Vec3Array*)na;
                std::vector<double> box_max = { -1e38, -1e38 ,-1e38 };
                std::vector<double> box_min = { 1e38, 1e38 ,1e38 };
                int normal_size = v3f->size();
                for (int vidx = 0; vidx < normal_size; vidx++)
                {
                    osg::Vec3f point = v3f->at(vidx);
                    vector<float> normal = { point.x(), point.y(), point.z() };
                    
                    for (int i = 0; i < 3; i++)
                    {
                        put_val(buffer.data, normal[i]);
                        SET_MAX(box_max[i], normal[i]);
                        SET_MIN(box_min[i], normal[i]);
                    }
                }
                tinygltf::Accessor acc;
                acc.bufferView = 2;
                acc.byteOffset = acc_offset[j];
                alignment_buffer(buffer.data);
                acc_offset[j] = calc_offset();
                acc.count = normal_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
                acc.minValues = box_min;
                acc.maxValues = box_max;
                model.accessors.push_back(acc);
            }else if ( j == 3) {
                // batch id
                unsigned short batch_id = i;
                for (auto& vertex : meshes[i].vertex) {
                    put_val(buffer.data, batch_id);
                }
                tinygltf::Accessor acc;
                acc.bufferView = 3;
                acc.byteOffset = acc_offset[j];
                alignment_buffer(buffer.data);
                acc_offset[j] = calc_offset();
                acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                acc.count = meshes[i].vertex.size();
                acc.type = TINYGLTF_TYPE_SCALAR;
                acc.maxValues = { (double)i };
                acc.minValues = { (double)batch_id };
                model.accessors.push_back(acc);
            }
        }
        tinygltf::BufferView bfv;
        bfv.buffer = 0;
        if (j == 0 || j == 3) {
            bfv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
        }
        else {
            bfv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
            bfv.byteStride = 4 * 3;
        }
        bfv.byteOffset = buf_offset;
        alignment_buffer(buffer.data);
        bfv.byteLength = calc_offset();
        buf_offset = buffer.data.size();
        model.bufferViews.push_back(bfv);
    }

    bool use_multi_material = false;
    for (int i = 0; i < meshes.size(); i++) {
        tinygltf::Mesh mesh;
        mesh.name = meshes[i].mesh_name;
        tinygltf::Primitive primits;
        primits.attributes = { 
            std::pair<std::string,int>("POSITION", 1 * meshes.size() + i),
            std::pair<std::string,int>("NORMAL",   2 * meshes.size() + i),
            std::pair<std::string,int>("_BATCHID", 3 * meshes.size() + i),
        };
        primits.indices = i;
        if(use_multi_material) {
            //TODO: turn height to rgb(r,g,b)
            tinygltf::Material material =  make_color_material(1.0, 0.0, 0.0);
            model.materials.push_back(material);
            primits.material = i;
        } else {
            primits.material = 0;
        }
        primits.mode = TINYGLTF_MODE_TRIANGLES;
        mesh.primitives = {
            primits
        };
        model.meshes.push_back(mesh);
    }

    // 加载所有的模型
    for (int i = 0; i < meshes.size(); i++) {
        tinygltf::Node node;
        node.mesh = i;
        model.nodes.push_back(node);
    }
    // 一个场景
    tinygltf::Scene sence;
    for (int i = 0; i < meshes.size(); i++) {
        sence.nodes.push_back(i);
    }
    // 所有场景
    model.scenes = { sence };
    model.defaultScene = 0;
    /// --------------
    if (use_multi_material) {
        // code has realized about
    } else {
        tinygltf::Material material;
        material.name = "default";
//      tinygltf::Parameter baseColorFactor;
//      baseColorFactor.number_array = { 1,1,1,1 };
//      material.values["baseColorFactor"] = baseColorFactor;
        tinygltf::Parameter metallicFactor;
        metallicFactor.number_value = new double(0.3);
        material.values["metallicFactor"] = metallicFactor;
        tinygltf::Parameter roughnessFactor;
        roughnessFactor.number_value = new double(0.7);
        material.values["roughnessFactor"] = roughnessFactor;
        /// ---------
//      tinygltf::Parameter emissiveFactor;
//      emissiveFactor.number_array = { 0,0,0 };
//      material.additionalValues["emissiveFactor"] = emissiveFactor;
//      tinygltf::Parameter alphaMode;
//      alphaMode.string_value = "OPAQUE";
//      material.additionalValues["alphaMode"] = alphaMode;
//      tinygltf::Parameter doubleSided;
//      doubleSided.bool_value = false;
//      material.additionalValues["doubleSided"] = doubleSided;
        model.materials = { material };
    }

    model.buffers.push_back(std::move(buffer));
    model.asset.version = "2.0";
    model.asset.generator = "fanfan";
    
    std::string buf = gltf.Serialize(&model);
    return buf;
}

std::string make_b3dm(std::vector<Polygon_Mesh>& meshes, bool with_height = false) {
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

    std::string glb_buf = make_polymesh(meshes);
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
