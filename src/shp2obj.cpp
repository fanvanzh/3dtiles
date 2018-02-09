#include "gdal/ogrsf_frmts.h"
#include "tiny_gltf.h"
#include "earcut.hpp"

#include <vector>
#include <cmath>
#include <array>

/////////////////////////
// extern function impl by rust
extern "C" bool mkdirs(const char* path);
extern "C" bool write_file(const char* filename, const char* buf, unsigned long buf_len);


////////////////////////

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
		return contains(other.minx, other.miny)
			|| contains(other.maxx, other.maxy)
			|| contains(other.minx, other.maxy)
			|| contains(other.maxx, other.miny)
			|| other.contains(*this);
	}
};

class node {
public:
	bbox _box;
	// 1 公里 ~ 0.01
	double metric = 0.04;
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
Polygon_Mesh convert_polygon(OGRPolygon* polyon, double center_x, double center_y) {
	double scale = 111 * 1000;
	double bottom = 0.0;
	double height = 50.0;
	Polygon_Mesh mesh;

	OGREnvelope geo_box;
	polyon->getEnvelope(&geo_box);
	{
		OGRLinearRing* pRing = polyon->getExteriorRing();
		int ptNum = pRing->getNumPoints();
        if(ptNum < 4) {
            return mesh;
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
				(float)(scale * (pt.getX() - center_x)) , 
				(float)bottom , 
				(float)(scale * (pt.getY() - center_y))
			});
			mesh.vertex.push_back({
				(float)(scale * (pt.getX() - center_x)) , 
				(float)height , 
				(float)(scale * (pt.getY() - center_y))
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
			for (size_t i = 0; i < 2; i++)
			{
				std::array<float, 3> n0 = pt_normal[i];
				std::array<float, 3> n1 = pt_normal[i + 1];
				std::array<float, 3> n2 = { 0, -1, 0 };
				if (i == 1) n2[1] = 1;
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
					(float)(scale * (pt.getX() - center_x)) , 
					(float)bottom , 
					(float)(scale * (pt.getY() - center_y))
				});
				mesh.vertex.push_back({
					(float)(scale * (pt.getX() - center_x)) , 
					(float)height , 
					(float)(scale * (pt.getY() - center_y))
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
				for (size_t i = 0; i < 2; i++)
				{
					std::array<float, 3> n0 = pt_normal[i];
					std::array<float, 3> n1 = pt_normal[i + 1];
					std::array<float, 3> n2 = { 0, -1, 0 };
					if (i == 1) n2[1] = 1;
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
			mesh.index.push_back({ 2 * indices[idx] - 1, 2 * indices[idx + 1] - 1, 2 * indices[idx + 2] - 1 });
		}
		for (int idx = 0; idx < indices.size(); idx += 3) {
			mesh.index.push_back({ 2 * indices[idx], 2 * indices[idx + 1], 2 * indices[idx + 2] });
		}
	}
	return mesh;
}


void make_polymesh(std::vector<Polygon_Mesh>& meshes, const char* mesh_name, const char* filename);
//
extern "C" bool shp2obj(const char* filename, int layer_id, const char* dest)
{
	if (!filename || layer_id < 0 || layer_id > 10000 || !dest) {
		return false;
	}
	GDALAllRegister();
	GDALDataset       *poDS;
	poDS = (GDALDataset*)GDALOpenEx(filename, GDAL_OF_VECTOR,
		NULL, NULL, NULL);
	if (poDS == NULL)
	{
		printf("Open failed.\n");
		return false;
	}
	OGRLayer  *poLayer;
	poLayer = poDS->GetLayer(layer_id);
	if (!poLayer) {
		GDALClose(poDS);
		return false;
	}
	OGRwkbGeometryType _t = poLayer->GetGeomType();
	if (_t != wkbPolygon && _t != wkbMultiPolygon &&
		_t != wkbPolygon25D && _t != wkbMultiPolygon25D)
	{
		GDALClose(poDS);
		return false;
	}

	OGREnvelope envelop;
	poLayer->GetExtent(&envelop);

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
	double build_height = 10.0;
	for (auto item : items_array) {
		node* _node = (node*)item;
		char obj_file[512];
		sprintf(obj_file, "%s\\obj\\%d\\%d", dest, _node->_z, _node->_x);
		mkdirs(obj_file);
		double center_x = _node->_box.minx;
		double center_y = _node->_box.miny;
        std::vector<Polygon_Mesh> v_meshes;
		for (auto id : _node->get_ids()) {
			OGRFeature *poFeature = poLayer->GetFeature(id);
			OGRGeometry *poGeometry;
			poGeometry = poFeature->GetGeometryRef();
			if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon) {
				OGRPolygon* polyon = (OGRPolygon*)poGeometry;
				Polygon_Mesh mesh = convert_polygon(polyon, center_x, center_y);
                v_meshes.push_back(mesh);
			}
			else if (wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon) {
				OGRMultiPolygon* _multi = (OGRMultiPolygon*)poGeometry;
				int sub_count = _multi->getNumGeometries();
				for (int j = 0; j < sub_count; j++) {
					OGRPolygon * polyon = (OGRPolygon*)_multi->getGeometryRef(j);
					Polygon_Mesh mesh = convert_polygon(polyon, center_x, center_y);
                    v_meshes.push_back(mesh);
				}
			}
			OGRFeature::DestroyFeature(poFeature);
		}
        sprintf(obj_file, "%s\\obj\\%d\\%d\\%d.glb", dest, _node->_z, _node->_x, _node->_y);
        make_polymesh(v_meshes,"",obj_file);
	}
	//
	GDALClose(poDS);
	return true;
}

template<class T> 
void put_val(std::vector<unsigned char>& buf, T val) {
	buf.insert(buf.end(), (unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

void make_polymesh(std::vector<Polygon_Mesh>& meshes, const char* mesh_name, const char* filename) {
	
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
			}
			else if( j == 1){
				// vec3
				float max_v3[3] = { meshes[i].vertex[0][0],meshes[i].vertex[0][1],meshes[i].vertex[0][2] };
				float min_v3[3] = { meshes[i].vertex[0][0],meshes[i].vertex[0][1],meshes[i].vertex[0][2] };
				int vec_size = meshes[i].vertex.size();
				for (auto& vertex : meshes[i].vertex) {
					put_val(buffer.data, vertex[0]);
					put_val(buffer.data, vertex[1]);
					put_val(buffer.data, vertex[2]);
					for (int i = 0; i < 3; i++) {
						if (max_v3[i] < vertex[i]) max_v3[i] = vertex[i];
						if (min_v3[i] > vertex[i]) min_v3[i] = vertex[i];
					}
				}
				tinygltf::Accessor acc;
				acc.bufferView = 1;
				acc.byteOffset = acc_offset[j];
				acc_offset[j] = buffer.data.size() - buf_offset;
				acc.count = vec_size;
				acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
				acc.type = TINYGLTF_TYPE_VEC3;
				acc.maxValues = { max_v3[0],max_v3[1],max_v3[2] };
				acc.minValues = { min_v3[0],min_v3[1],min_v3[2] };
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
		if (mesh_name) {
			mesh.name = mesh_name;
		}
		tinygltf::Primitive primits;
		primits.attributes = { 
			std::pair<std::string,int>("POSITION", meshes.size() + i),
			std::pair<std::string,int>("NORMAL",2 * meshes.size() + i),
		};
		primits.indices = i ;
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
	
	model.asset.version = "2.0";
	model.asset.generator = "fanfan";
	
	std::string buf = gltf.Serialize(&model);
	write_file(filename, buf.data(), buf.size());
}
