#include <osg/Material>
#include <osgDB/ReadFile>
#include <osgDB/ConvertUTF>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <vector>
#include <string>
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
public:
	InfoVisitor()
		:osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
	{}

	virtual void apply(osg::Node& node)
	{
		traverse(node);
	}

	virtual void apply(osg::Geode& node)
	{
		for (unsigned int n = 0; n < node.getNumDrawables(); n++)
		{
			osg::Drawable* draw = node.getDrawable(n);
			if (!draw)
				continue;
			osg::Geometry *g = draw->asGeometry();
			if (g)
				geometry_array.push_back(g);
		}
		traverse(node);
	}
public:
	std::vector<osg::Geometry*> geometry_array;
};

struct mesh_info
{
	string name;
	std::vector<double> min;
	std::vector<double> max;
};

bool osgb2glb_buf(const char* path, std::string& glb_buff, std::vector<mesh_info>& v_info) {

#ifdef WIN32
	vector<string> fileNames = { osgDB::convertStringFromUTF8toCurrentCodePage(path) };
#else
	vector<string> fileNames = { path };
#endif // WIN32

	osg::ref_ptr<osg::Node> root = osgDB::readNodeFiles(fileNames);
	if (!root.valid()) {
		return false;
	}
	InfoVisitor infoVisitor;
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
						if (-point.y() > box_max[2]) box_max[2] = -point.y();
						if (-point.y() < box_min[2]) box_min[2] = -point.y();
						if (point.z() > box_max[1]) box_max[1] = point.z();
						if (point.z() < box_min[1]) box_min[1] = point.z();
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
					osg::Array* na = g->getTexCoordArray(0);
					osg::Vec2Array* v2f = (osg::Vec2Array*)na;
					vector<double> box_max = { -1e38, -1e38  };
					vector<double> box_min = { 1e38, 1e38  };
					int texture_size = v2f->size();
					for (int vidx = 0; vidx < texture_size; vidx++)
					{
						osg::Vec2f point = v2f->at(vidx);
						put_val(buffer.data, point.x());
						put_val(buffer.data, point.y());
					}
					tinygltf::Accessor acc;
					acc.bufferView = 3;
					acc.byteOffset = acc_offset[j];
					acc_offset[j] = buffer.data.size() - buf_offset;
					acc.count = texture_size;
					acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					acc.type = TINYGLTF_TYPE_VEC2;
					acc.maxValues = {1,1};
					acc.minValues = {0,0};
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
			osg::StateSet* ss = infoVisitor.geometry_array[0]->getStateSet();
			//osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));
			osg::Texture* tex = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
			osg::Image* img = tex->getImage(0);
			int width = img->s();
			int height = img->t();
			//int image_len = img->getImageSizeInBytes();
			const char* buf = (const char*)img->getDataPointer();
			// jpg
			stbi_write_jpg_to_func(write_buf, &buffer.data, width, height, 3, buf, 80);

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
		emissiveFactor.number_array = { 0,0,0 };
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

struct tile_info
{
	std::vector<double> max;
	std::vector<double> min;
};

bool osgb23dtile_buf(const char* path, std::string& b3dm_buf, tile_info& tile_box) {
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

extern "C" bool osgb23dtile(const char* in, const char* out 
	) {
	std::string b3dm_buf;
	tile_info tile_box;
	bool ret = osgb23dtile_buf(in,b3dm_buf,tile_box);
	if (!ret) return false;
	ret = write_file(out, b3dm_buf.data(), b3dm_buf.size());
	if (!ret) return false;
	// write tileset.json
	double width_meter = tile_box.max[0] - tile_box.min[0];
	double height_meter = tile_box.max[2] - tile_box.min[2];
	printf("%f,%f\n", tile_box.max[0], tile_box.min[0]);
	printf("%f,%f\n", tile_box.max[1], tile_box.min[1]);
	printf("%f,%f\n", tile_box.max[2], tile_box.min[2]);
	double radian_x = degree2rad(120);
	double radian_y = degree2rad(30);
	double geometric_error = std::max<double>(width_meter,height_meter) / 2.0;
	std::string b3dm_fullpath = out;
	auto p0 = b3dm_fullpath.find_last_of('/');
	auto p1 = b3dm_fullpath.find_last_of('\\');
	std::string b3dm_file_name = b3dm_fullpath.substr(
		std::max<int>(p0,p1) + 1);
	std::string tileset = out;
	tileset = tileset.replace(
		//b3dm_fullpath.find_last_of('.'), 
		//tileset.length() - 1, ".json");
		std::max<int>(p0,p1) + 1,
		tileset.length() - 1, "tileset.json");
	// 米转度
	double center_mx = (tile_box.max[0] + tile_box.min[0]) / 2;
	double center_my = (tile_box.max[2] + tile_box.min[2]) / 2;
	double center_rx = meter_to_longti(center_mx,radian_y);
	//double center_rx = meter_to_lati(center_mx);
	double center_ry = meter_to_lati(center_my);
	double width_rx = meter_to_longti(width_meter,radian_y);
	//double width_rx = meter_to_lati(width_meter);
	double height_ry = meter_to_lati(height_meter);
	Transform trs = {false, radian_x, radian_y, -1160};
	Region reg = {
		radian_x + center_rx - width_rx / 2,
		radian_y - center_ry - height_ry / 2,
		radian_x + center_rx + width_rx / 2,
		radian_y - center_ry + height_ry / 2,
		0, 50
	};    
    write_tileset_region(trs, reg, 
    	geometric_error, 
    	b3dm_file_name.c_str(),
		tileset.c_str());
    return true;
}

extern "C" bool osgb2glb(const char* in, const char* out) {
	std::string b3dm_buf;
	std::vector<mesh_info> v_info;
	bool ret = osgb2glb_buf(in,b3dm_buf,v_info);
	if (!ret) return false;
	ret = write_file(out, b3dm_buf.data(), b3dm_buf.size());
	if (!ret) return false;
	return true;
}