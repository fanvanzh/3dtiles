#include "gdal/ogrsf_frmts.h"
#include <vector>
#include <cmath>

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
    bbox _box;
    // 1 公里 ~ 0.01
    double metric = 0.04;
    node* subnode[4];
    std::vector<int> geo_items;

 public: 
    int _x = 0;
    int _y = 0;
    int _z = 0;

    void set_no(int x,int y,int z){
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

    void get_all(std::vector<void*>& items_array){
    	if(!geo_items.empty()) {
	    	items_array.push_back(this);
    	}
    	if (subnode[0] != 0) {
            for (int i = 0; i < 4; i++) {
                subnode[i]->get_all(items_array);
            }
        }
    }
};
extern "C" bool mkdirs(const char* path);
extern "C" bool write_file(const char* filename, const char* buf, unsigned long buf_len);

extern "C" bool shp2obj(const char* filename, int layer_id, const char* dest)
{
	if (!filename || layer_id < 0 || layer_id > 10000 || !dest) {
		return false;
	}
    GDALAllRegister();
    GDALDataset       *poDS;
    poDS = (GDALDataset*) GDALOpenEx( filename, GDAL_OF_VECTOR, 
        NULL, NULL, NULL );
    if( poDS == NULL )
    {
        printf( "Open failed.\n" );
        return false;
    }
    OGRLayer  *poLayer;
    poLayer = poDS->GetLayer( layer_id );
    if(!poLayer) {
    	GDALClose( poDS );
		return false;
    }
    OGRwkbGeometryType _t = poLayer->GetGeomType();
	if (_t != wkbPolygon && _t != wkbMultiPolygon && 
		_t != wkbPolygon25D && _t != wkbMultiPolygon25D)
	{
		GDALClose( poDS );
		return false;
	}

    OGREnvelope envelop;
    poLayer->GetExtent(&envelop);

    bbox bound(envelop.MinX,envelop.MaxX,envelop.MinY,envelop.MaxY);
    node root(bound);
    OGRFeature *poFeature;
    poLayer->ResetReading();
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        OGRGeometry *poGeometry;
        poGeometry = poFeature->GetGeometryRef();

        OGREnvelope envelop;
        poGeometry->getEnvelope(&envelop);
        bbox bound(envelop.MinX,envelop.MaxX,envelop.MinY,envelop.MaxY);
        unsigned long long id = poFeature->GetFID();
        root.add(id, bound);
        OGRFeature::DestroyFeature( poFeature );
    }
    // iter all node and convert to obj 
    std::vector<void*> items_array;
    root.get_all(items_array);
    //
    double build_height = 10.0;
    for ( auto item : items_array ) {
    	node* _node = (node*)item;
    	char obj_file[512];
    	sprintf(obj_file, "%s\\obj\\%d\\%d", dest, _node->_z, _node->_x);
    	mkdirs(obj_file);
    	sprintf(obj_file, "%s\\obj\\%d\\%d\\%d.obj", dest, _node->_z, _node->_x, _node->_y);
    	std::string obj_txt;
		std::string obj_face;

		std::string obj_normal_str;
		obj_normal_str += "vn 0 0 -1\n";
		obj_normal_str += "vn 0 0 1\n";
		int normal_idx = 2;

		int point_cnt = 0;
    	for (auto id : _node->get_ids()) {
			OGRFeature *poFeature = poLayer->GetFeature(id);
      	  	OGRGeometry *poGeometry;
        	poGeometry = poFeature->GetGeometryRef();	
        	if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon){	
        		// how to convert polygon to obj file 
				OGRPolygon* polyon = (OGRPolygon*)poGeometry;
				OGREnvelope geo_box;
				poGeometry->getEnvelope(&geo_box);
				OGRLinearRing* pRing = polyon->getExteriorRing();
				{
					std::vector<double> ptArray;
					int ptNum = pRing->getNumPoints();
					ptArray.resize(ptNum * 2);
					char obj_point[512];
					OGRPoint last_pt;
					for (int i = 0; i < ptNum; i++)
					{
						OGRPoint pt;
						pRing->getPoint(i, &pt);
						sprintf(obj_point,"v %f %f %f\n",
							10000.0 * ( pt.getX() - geo_box.MinX), 
							10000.0 * ( pt.getY() - geo_box.MinY), 
							0.0);
						obj_txt += obj_point;
						sprintf(obj_point,"v %f %f %f\n",
							10000.0 * (pt.getX() - geo_box.MinX),
							10000.0 * (pt.getY() - geo_box.MinY),
							build_height);
						obj_txt += obj_point;
						point_cnt++;

						if (i > 0) {
							char face_buf[2048];
							// point is 2 : 1 3 4 2 
							int a = 2 * (point_cnt - 1 ) - 1;
							int b = 2 * point_cnt - 1;
							int c = 2 * point_cnt;
							int d = 2 * ( point_cnt - 1);
							// 计算斜率
							double dx = pt.getX() - last_pt.getX();
							double dy = pt.getY() - last_pt.getY();
							double lenght = std::sqrt(dx*dx + dy*dy);
							double nx = -dy / lenght;
							double ny = dx / lenght;
							sprintf(face_buf, "vn %f %f 0\n", nx, ny);
							obj_normal_str += face_buf;
							normal_idx += 1;
							//
							sprintf(face_buf, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n",
								a,1,normal_idx,
								b,1,normal_idx,
								c,1,normal_idx,
								d,1,normal_idx
								);
							obj_face += face_buf;
						}
						last_pt = pt;
					}
					obj_face += "f ";
					for (int i = 0; i < ptNum; i++) {
						int pt_idx = point_cnt - (ptNum - i) + 1;
						char buf[256];
						int a = pt_idx * 2;
						sprintf(buf, "%d/%d/%d ", a,1,2);
						obj_face += buf;
					}
					obj_face += "\n";
					obj_face += "f ";
					for (int i = 0; i < ptNum; i++) {
						int pt_idx = point_cnt - i;
						char buf[256];
						int a = pt_idx * 2 - 1;
						sprintf(buf, "%d/%d/%d ", a,1,1);
						obj_face += buf;
					}
					obj_face += "\n";
				}
				int inner_count = polyon->getNumInteriorRings();
				for (int i = 0; i < inner_count; i++)
				{
					OGRLinearRing* pRing = polyon->getInteriorRing(i);
				}
        	}
        	else if(wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon){

        	}
			OGRFeature::DestroyFeature( poFeature );
    	}

		obj_txt += "\n";
		obj_txt += obj_normal_str;

		for (int i = 0; i < 1; i++) {
			char vt[512];
			sprintf(vt, "vt %f %f\n", 0.5, 0.5);
			obj_txt += vt;
		}

		// face
		obj_txt += obj_face;
		write_file(obj_file, obj_txt.c_str(), obj_txt.size());
    }
    //
    GDALClose( poDS );
    return true;
}


#include "tiny_gltf.h"

extern "C" bool test() {
    tinygltf::TinyGLTF gltf;
    tinygltf::Model model;

	tinygltf::Mesh mesh;
	mesh.name = "box";
	//mesh.weights = 1.0;
	tinygltf::Primitive primits;
	primits.attributes = { std::pair<std::string,int>("POSITION",1) };
	primits.indices = 0;
	mesh.primitives = {
		primits
	};
	model.meshes = { mesh };
	tinygltf::Buffer buffer;
	buffer.data = {
		 0x00 ,0x00 ,0x01 ,0x00 ,0x02 ,0x00 ,0x00 ,0x00
		,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
		,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x80 ,0x3f
		,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
		,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x80 ,0x3f
		,0x00 ,0x00 ,0x00 ,0x00
	};
	//buffer.uri = "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=";
	model.buffers = {
		buffer
	};
	tinygltf::BufferView bv0;
	bv0.buffer = 0;
	bv0.byteOffset = 0;
	bv0.byteLength = 6;
	bv0.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

	tinygltf::BufferView bv1;
	bv1.buffer = 0;
	bv1.byteOffset = 8;
	bv1.byteLength = 36;
	bv1.target = TINYGLTF_TARGET_ARRAY_BUFFER;

	model.bufferViews = {
		bv0,bv1
	};

	tinygltf::Accessor acsor0;
	acsor0.bufferView = 0;
	acsor0.byteOffset = 0;
	acsor0.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	acsor0.count = 3;
	acsor0.type = TINYGLTF_TYPE_SCALAR;
	//acsor0.max = 0;
	//acsor0.min = 0;

	tinygltf::Accessor acsor1;
	acsor1.bufferView = 1;
	acsor1.byteOffset = 0;
	acsor1.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	acsor1.count = 3;
	acsor1.type = TINYGLTF_TYPE_VEC3;
	//acsor1.max = 0;
	//acsor1.min = 0;

	model.accessors = {
		acsor0,acsor1
	};

	model.asset.version = "2.0";
	model.asset.generator = "fanfan";

	tinygltf::Scene sence;
	sence.nodes = {0};
	model.scenes = { sence };
	model.defaultScene = 0;
	tinygltf::Node node;
	node.mesh = 0;
	model.nodes = { node };

    gltf.WriteGlbSceneToFile(&model, "D:\\test.glb");
    return true;
}