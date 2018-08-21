#include "gdal/ogrsf_frmts.h"
#include "tiny_gltf.h"
#include "earcut.hpp"
#include "json.hpp"
#include "extern.h"

#include <vector>
#include <cmath>
#include <array>

struct bbox
{
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
            geo_items.push_back(id);
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
    std::vector<double> box_max; // 外包矩形
    std::vector<double> box_min; // 外包矩形
    std::vector<std::array<float, 3>> vertex; // 顶点
    std::vector<std::array<float, 3>> normal; // 面法线
    std::vector<std::array<float, 2>>  texcoord; // 贴图坐标
    std::vector<std::array<int, 3>>    index;  // 面
};

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
    OGREnvelope geo_box;
    polyon->getEnvelope(&geo_box);
    mesh.box_min = {
        longti_to_meter(degree2rad(geo_box.MinX - center_x), degree2rad( center_y )),
        bottom, 
        lati_to_meter(degree2rad(geo_box.MinY - center_y))
    };
    mesh.box_max = { 
        longti_to_meter(degree2rad(geo_box.MaxX - center_x), degree2rad(center_y)),
        height, 
        lati_to_meter(degree2rad(geo_box.MaxY - center_y))
    };
    {
        OGRLinearRing* pRing = polyon->getExteriorRing();
        int ptNum = pRing->getNumPoints();
        if(ptNum < 4) {
            return mesh;
        }
        OGRPoint last_pt;
        std::vector<std::array<float, 3>> pt_normal;
        OGRPoint pt0, pt1;
        pRing->getPoint(0, &pt0);
        pRing->getPoint(ptNum - 2, &pt1); // 闭合面用倒数第二个
        double dx = pt0.getX() - pt1.getX();
        double dy = pt0.getY() - pt1.getY();
        double lenght = std::sqrt(dx*dx + dy*dy);
        float nx = -dy / lenght;
        float ny = dx / lenght;
        pt_normal.push_back({ nx,0,ny });

        for (int i = 0; i < ptNum; i++)
        {
            OGRPoint pt;
            pRing->getPoint(i, &pt);

            mesh.vertex.push_back({
                (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y)),
                (float)bottom , 
                -(float)lati_to_meter(degree2rad(pt.getY() - center_y))
            });
            mesh.vertex.push_back({
                (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y)),
                (float)height ,
                -(float)lati_to_meter(degree2rad(pt.getY() - center_y))
            });

            int point_cnt = mesh.vertex.size();
            if (i > 0) {
                int a = point_cnt - 4;
                int b = point_cnt - 3;
                int c = point_cnt - 2;
                int d = point_cnt - 1;
                // 计算斜率
                double dx = pt.getX() - last_pt.getX();
                double dy = pt.getY() - last_pt.getY();
                double lenght = std::sqrt(dx*dx + dy*dy);
                float nx = -dy / lenght;
                float ny = dx / lenght;
                pt_normal.push_back({ nx,0,ny });
                // 添加两个面
                mesh.index.push_back({ a, c, b });
                mesh.index.push_back({ b, c, d });
            }
            last_pt = pt;
        }
        pt_normal.push_back(pt_normal[1]);
        for (int i = 0; i < ptNum; i++) {
            // 平均法线
            for (size_t j = 0; j < 2; j++)
            {
                std::array<float, 3> n0 = pt_normal[i];
                std::array<float, 3> n1 = pt_normal[i + 1];
                std::array<float, 3> n2 = { 0, -1, 0 };
                if (j == 1) n2[1] = 1;
                std::array<float, 3> normal = {
                    (n0[0] + n1[0] + n2[0]) / 3,
                    (n0[1] + n1[1] + n2[1]) / 3,
                    (n0[2] + n1[2] + n2[2]) / 3
                };
                double lenght = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2] );
                normal[0] /= lenght;
                normal[1] /= lenght;
                normal[2] /= lenght;
                mesh.normal.push_back(normal);
            }
        }
    }
    // innor ring
    {
        int inner_count = polyon->getNumInteriorRings();
        for (int j = 0; j < inner_count; j++)
        {
            OGRLinearRing* pRing = polyon->getInteriorRing(j);
            int ptNum = pRing->getNumPoints();
            if(ptNum < 4) {
                continue;
            }
            OGRPoint last_pt;
            std::vector<std::array<float, 3>> pt_normal;
            // 计算最后一个法线 ( 换 osg 计算吧)
            OGRPoint pt0, pt1;
            pRing->getPoint(0, &pt0);
            pRing->getPoint(ptNum - 2, &pt1); // 闭合面用倒数第二个
            double dx = pt0.getX() - pt1.getX();
            double dy = pt0.getY() - pt1.getY();
            double lenght = std::sqrt(dx*dx + dy*dy);
            float nx = -dy / lenght;
            float ny = dx / lenght;
            pt_normal.push_back({ nx,0,ny });
            for (int i = 0; i < ptNum; i++)
            {
                OGRPoint pt;
                pRing->getPoint(i, &pt);
                mesh.vertex.push_back({
                    (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y)),
                    (float)bottom ,
                    -(float)lati_to_meter(degree2rad(pt.getY() - center_y))
                });
                mesh.vertex.push_back({
                    (float)longti_to_meter(degree2rad(pt.getX() - center_x), degree2rad(center_y)),
                    (float)height ,
                    -(float)lati_to_meter(degree2rad(pt.getY() - center_y))
                });
                int point_cnt = mesh.vertex.size();
                if (i > 0) {
                    int a = point_cnt - 4;
                    int b = point_cnt - 3;
                    int c = point_cnt - 2;
                    int d = point_cnt - 1;
                    // 计算斜率
                    double dx = pt.getX() - last_pt.getX();
                    double dy = pt.getY() - last_pt.getY();
                    double lenght = std::sqrt(dx*dx + dy*dy);
                    float nx = -dy / lenght;
                    float ny = dx / lenght;
                    // 添加两个面
                    pt_normal.push_back({ nx,0,ny });
                    mesh.index.push_back({ a, c, b });
                    mesh.index.push_back({ b, c, d });
                }
                last_pt = pt;
            }
            pt_normal.push_back(pt_normal[1]);
            for (int i = 0; i < ptNum; i++) {
                // 平均法线
                for (size_t j = 0; j < 2; j++)
                {
                    std::array<float, 3> n0 = pt_normal[i];
                    std::array<float, 3> n1 = pt_normal[i + 1];
                    std::array<float, 3> n2 = { 0, -1, 0 };
                    if (j == 1) n2[1] = 1;
                    std::array<float, 3> normal = {
                        (n0[0] + n1[0] + n2[0]) / 3,
                        (n0[1] + n1[1] + n2[1]) / 3,
                        (n0[2] + n1[2] + n2[2]) / 3
                    };
                    double lenght = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2] );
                    normal[0] /= lenght;
                    normal[1] /= lenght;
                    normal[2] /= lenght;
                    mesh.normal.push_back(normal);
                }
            }
        }
    }
    // 封顶、底
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
                polygon[0].push_back({ pt.getX(), pt.getY() });
            }
        }
        int inner_count = polyon->getNumInteriorRings();
        for (int j = 0; j < inner_count; j++)
        {
            polygon.resize( polygon.size() + 1 );
            OGRLinearRing* pRing = polyon->getInteriorRing(j);
            int ptNum = pRing->getNumPoints();
            for (int i = 0; i < ptNum; i++)
            {
                OGRPoint pt;
                pRing->getPoint(i, &pt);
                polygon[j].push_back({ pt.getX(), pt.getY() });
            }
        }
        std::vector<int> indices = mapbox::earcut<int>(polygon);
        // 剖分三角形
        for (int idx = 0; idx < indices.size(); idx += 3) {
            mesh.index.push_back({ 2 * indices[idx] - 1, 2 * indices[idx + 2] - 1, 2 * indices[idx + 1] - 1 });
        }
        for (int idx = 0; idx < indices.size(); idx += 3) {
            mesh.index.push_back({ 2 * indices[idx], 2 * indices[idx + 1], 2 * indices[idx + 2] });
        }
    }
    return mesh;
}


std::string make_polymesh(std::vector<Polygon_Mesh>& meshes);
std::string make_b3dm(std::vector<Polygon_Mesh>& meshes);
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

    bbox bound(envelop.MinX, envelop.MaxX, envelop.MinY, envelop.MaxY);
    node root(bound);
    OGRFeature *poFeature;
    poLayer->ResetReading();
    while ((poFeature = poLayer->GetNextFeature()) != NULL)
    {
        OGRGeometry *poGeometry;
        poGeometry = poFeature->GetGeometryRef();

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
                v_meshes.push_back(mesh);
            }
            else if (wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon) {
                OGRMultiPolygon* _multi = (OGRMultiPolygon*)poGeometry;
                int sub_count = _multi->getNumGeometries();
                for (int j = 0; j < sub_count; j++) {
                    OGRPolygon * polyon = (OGRPolygon*)_multi->getGeometryRef(j);
                    Polygon_Mesh mesh = convert_polygon(polyon, center_x, center_y, height);
                    mesh.mesh_name = "mesh_" + std::to_string(id);
                    v_meshes.push_back(mesh);
                }
            }
            OGRFeature::DestroyFeature(poFeature);
        }

        sprintf(b3dm_file, "%s\\tile\\%d\\%d\\%d.b3dm", dest, _node->_z, _node->_x, _node->_y);
        std::string b3dm_buf = make_b3dm(v_meshes);
        write_file(b3dm_file, b3dm_buf.data(), b3dm_buf.size());

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

// convert poly-mesh to glb buffer
std::string make_polymesh(std::vector<Polygon_Mesh>& meshes) {
    
    tinygltf::TinyGLTF gltf;
    tinygltf::Model model;
    // model.name = model_name;
    // only one buffer
    tinygltf::Buffer buffer;
    // buffer_view {index,vertex,normal(texcoord,image)}
    uint32_t buf_offset = 0;
    uint32_t acc_offset[3] = {0,0,0};
    for (int j = 0; j < 3; j++)
    {
        for (int i = 0; i < meshes.size(); i++) {
            if (j == 0) {
                // indc
                int vec_size = meshes[i].vertex.size();
                int idx_size = meshes[i].index.size();
                if (vec_size <= 65535) {
                    for (size_t m = 0; m < meshes[i].index.size(); m++)
                    {
                        for (auto idx : meshes[i].index[m]) {
                            put_val(buffer.data, (unsigned short)idx);
                        }
                    }
                }
                else {
                    for (size_t m = 0; m < meshes[i].index.size(); m++)
                    {
                        for (auto idx : meshes[i].index[m]) {
                            put_val(buffer.data, idx);
                        }
                    }
                }
                tinygltf::Accessor acc;
                acc.bufferView = 0;
                acc.byteOffset = acc_offset[j];
                acc_offset[j] = buffer.data.size();
                if (vec_size <= 65535)
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                else
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_INT;
                acc.count = idx_size * 3;
                acc.type = TINYGLTF_TYPE_SCALAR;
                model.accessors.push_back(acc);
                // as convert to b3dm , we add a BATCH , same as vertex`s count
                {
                    unsigned short batch_id = i;
                    for (auto& vertex : meshes[i].vertex) {
                        put_val(buffer.data, batch_id);
                    }
                    tinygltf::Accessor acc;
                    acc.bufferView = 0;
                    acc.byteOffset = acc_offset[j];
                    acc_offset[j] = buffer.data.size();
                    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                    acc.count = vec_size;
                    acc.type = TINYGLTF_TYPE_SCALAR;
                    model.accessors.push_back(acc);
                }
            }
            else if( j == 1){
                int vec_size = meshes[i].vertex.size();
                for (auto& vertex : meshes[i].vertex) {
                    put_val(buffer.data, vertex[0]);
                    put_val(buffer.data, vertex[1]);
                    put_val(buffer.data, vertex[2]);
                }
                tinygltf::Accessor acc;
                acc.bufferView = 1;
                acc.byteOffset = acc_offset[j];
                acc_offset[j] = buffer.data.size() - buf_offset;
                acc.count = vec_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
                acc.maxValues = meshes[i].box_max;
                acc.minValues = meshes[i].box_min;
                model.accessors.push_back(acc);
            }
            else if (j == 2) {
                // normal
                int normal_size = meshes[i].normal.size();
                for (auto& normal : meshes[i].normal) {
                    put_val(buffer.data, normal[0]);
                    put_val(buffer.data, normal[1]);
                    put_val(buffer.data, normal[2]);
                }
                tinygltf::Accessor acc;
                acc.bufferView = 2;
                acc.byteOffset = acc_offset[j];
                acc_offset[j] = buffer.data.size() - buf_offset;
                acc.count = normal_size;
                acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                acc.type = TINYGLTF_TYPE_VEC3;
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
        while(buffer.data.size() % 4 != 0) {
            buffer.data.push_back(0x00);
        }
        bfv.byteLength = buffer.data.size() - buf_offset;
        buf_offset = buffer.data.size();
        model.bufferViews.push_back(bfv);
    }
    model.buffers.push_back(std::move(buffer));

    for (int i = 0; i < meshes.size(); i++) {
        tinygltf::Mesh mesh;
        mesh.name = meshes[i].mesh_name;
        tinygltf::Primitive primits;
        primits.attributes = { 
            std::pair<std::string,int>("_BATCHID", 2 * i + 1),
            std::pair<std::string,int>("POSITION", 2 * meshes.size() + i),
            std::pair<std::string,int>("NORMAL",   3 * meshes.size() + i),
        };
        primits.indices = i * 2 ;
        primits.material = 0;
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

    tinygltf::Sampler sample;
    sample.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    sample.minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
    sample.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
    sample.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
    model.samplers = {sample};
    /// --------------
    tinygltf::Material material;
    material.name = "default";
    tinygltf::Parameter baseColorFactor;
    baseColorFactor.number_array = {0.5,0.5,0.5,1.0};
    material.values["baseColorFactor"] = baseColorFactor;
    tinygltf::Parameter metallicFactor;
    metallicFactor.number_value = 0;
    material.values["metallicFactor"] = metallicFactor;
    tinygltf::Parameter roughnessFactor;
    roughnessFactor.number_value = 1;
    material.values["roughnessFactor"] = roughnessFactor;
    /// ---------
    tinygltf::Parameter emissiveFactor;
    emissiveFactor.number_array = { 0.5,0.5,0.5 };
    material.additionalValues["emissiveFactor"] = emissiveFactor;
    tinygltf::Parameter alphaMode;
    alphaMode.string_value = "OPAQUE";
    material.additionalValues["alphaMode"] = alphaMode;
    tinygltf::Parameter doubleSided;
    doubleSided.bool_value = false;
    material.additionalValues["doubleSided"] = doubleSided;
    model.materials = { material };

    model.asset.version = "2.0";
    model.asset.generator = "fanfan";
    
    std::string buf = gltf.Serialize(&model);
    return buf;
}

std::string make_b3dm(std::vector<Polygon_Mesh>& meshes) {
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
