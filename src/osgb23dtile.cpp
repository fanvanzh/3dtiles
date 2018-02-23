#include <osg/Material>
#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgUtil/SmoothingVisitor>

#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"
#include "stb_image_write.h"

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

int osgb2glb() {

	vector<string> fileNames = { R"(E:\Data\倾斜摄影\hgc\Data\Tile_+005_+004\Tile_+005_+004_L23_00033000.osgb)" };

	osg::ref_ptr<osg::Node> root = osgDB::readNodeFiles(fileNames);
	if (!root.valid()) {
		return 1;
	}
	osg::Geode* gNode = root->asGeode();
	if (!gNode) {
		return 1;
	}

	osgUtil::SmoothingVisitor sv;
	root->accept(sv);

	{
		tinygltf::TinyGLTF gltf;
		tinygltf::Model model;
		tinygltf::Buffer buffer;
		// buffer_view {index,vertex,normal(texcoord,image)}
		uint32_t buf_offset = 0;
		uint32_t acc_offset[5] = { 0,0,0,0,0 };
		for (int j = 0; j < 5; j++)
		{
			int num = gNode->getNumDrawables();
			for (size_t i = 0; i < num; i++) {
				osg::Geometry *g = gNode->getDrawable(i)->asGeometry();
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
						put_val(buffer.data, point.y());
						if (point.x() > box_max[0]) box_max[0] = point.x();
						if (point.x() < box_min[0]) box_min[0] = point.x();
						if (point.y() > box_max[2]) box_max[2] = point.y();
						if (point.y() < box_min[2]) box_min[2] = point.y();
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
						put_val(buffer.data, point.y());
						if (point.x() > box_max[0]) box_max[0] = point.x();
						if (point.x() < box_min[0]) box_min[0] = point.x();
						if (point.y() > box_max[2]) box_max[2] = point.y();
						if (point.y() < box_min[2]) box_min[2] = point.y();
						if (point.z() > box_max[1]) box_max[1] = point.z();
						if (point.z() < box_min[1]) box_min[1] = point.z();
					}
					tinygltf::Accessor acc;
					acc.bufferView = 2;
					acc.byteOffset = acc_offset[j];
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
						if (point.x() > box_max[0]) box_max[0] = point.x();
						if (point.x() < box_min[0]) box_min[0] = point.x();
						if (point.y() > box_max[1]) box_max[1] = point.y();
						if (point.y() < box_min[1]) box_min[1] = point.y();
					}
					tinygltf::Accessor acc;
					acc.bufferView = 3;
					acc.byteOffset = acc_offset[j];
					acc_offset[j] = buffer.data.size() - buf_offset;
					acc.count = texture_size;
					acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					acc.type = TINYGLTF_TYPE_VEC2;
					acc.maxValues = box_max;
					acc.minValues = box_min;
					model.accessors.push_back(acc);
				}
				else if (j == 4) {
					// image
					// 共用贴图？？
					osg::StateSet* ss = g->getStateSet();
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
					static int buf_view = 4;
					image.bufferView = buf_view++;
					model.images.push_back(image);
					tinygltf::BufferView bfv;
					bfv.buffer = 0;
					bfv.byteOffset = buf_offset;
					bfv.byteLength = buffer.data.size() - buf_offset;
					/*while (buffer.data.size() % 4 != 0) {
						buffer.data.push_back(0x00);
					}*/
					buf_offset = buffer.data.size();
					model.bufferViews.push_back(bfv);
				}
			}
			if (j != 4) {
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
				if (j == 1) { bfv.byteStride = 4 * 3; }
				model.bufferViews.push_back(bfv);
			}
		}
		model.buffers.push_back(std::move(buffer));

		int MeshNum = gNode->getNumDrawables();
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

		std::string buf = gltf.Serialize(&model);

		FILE* f = fopen("E:\\test\\osgb.glb","wb");
		fwrite(buf.data(), 1, buf.size(), f);
		fclose(f);
	}
	return 0;
}