#include "fbx.h"
#include "extern.h"
#include <iostream>

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Image>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/Uniform>
#include <osg/UserDataContainer>
#include <osgDB/ReadFile>

#include <ufbx.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

#include <osgDB/ReaderWriter>
#include <osgDB/Registry>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

// Helper to create osg::Image from stb_image data
static osg::Image* createImageFromSTB(unsigned char* imgData, int width, int height, int channels, const std::string& filename) {
    if (!imgData) return nullptr;

    osg::Image* image = new osg::Image();
    GLenum pixelFormat = GL_RGB;
    if (channels == 1) pixelFormat = GL_LUMINANCE;
    else if (channels == 2) pixelFormat = GL_LUMINANCE_ALPHA;
    else if (channels == 3) pixelFormat = GL_RGB;
    else if (channels == 4) pixelFormat = GL_RGBA;

    // Use USE_MALLOC_FREE so OSG takes ownership of the malloc'd data from stbi
    image->setImage(width, height, 1,
                    (GLint)pixelFormat,
                    pixelFormat,
                    GL_UNSIGNED_BYTE,
                    imgData,
                    osg::Image::USE_MALLOC_FREE);

    if (image) {
        image->setFileName(filename.empty() ? "image.png" : filename);
        image->flipVertical();
    }
    return image;
}

// Helper functions
static std::string hash_bytes(const void* data, size_t len) {
  std::ostringstream oss;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; ++i) h = (h ^ p[i]) * 16777619u;
  oss << std::hex << h;
  return oss.str();
}

static std::string calc_part_geom_hash(
    size_t num_vertices,
    const std::vector<ufbx_vec3>& pos,
    const std::vector<ufbx_vec3>& norm,
    const std::vector<ufbx_vec2>& uv,
    const std::vector<ufbx_vec4>& color,
    const std::vector<uint32_t>& indices)
{
  std::ostringstream oss;
  uint8_t mask = 0;
  if (!pos.empty()) mask |= 1 << 0;
  if (!norm.empty()) mask |= 1 << 1;
  if (!uv.empty()) mask |= 1 << 2;
  if (!color.empty()) mask |= 1 << 3;
  oss.write(reinterpret_cast<const char*>(&mask), sizeof(mask));
  uint32_t nv = (uint32_t)num_vertices;
  oss.write(reinterpret_cast<const char*>(&nv), sizeof(nv));
  for (size_t i = 0; i < num_vertices; ++i) {
    if (!pos.empty()) {
      float x = (float)pos[i].x, y = (float)pos[i].y, z = (float)pos[i].z;
      oss.write(reinterpret_cast<const char*>(&x), sizeof(x));
      oss.write(reinterpret_cast<const char*>(&y), sizeof(y));
      oss.write(reinterpret_cast<const char*>(&z), sizeof(z));
    }
    if (!norm.empty()) {
      float x = (float)norm[i].x, y = (float)norm[i].y, z = (float)norm[i].z;
      oss.write(reinterpret_cast<const char*>(&x), sizeof(x));
      oss.write(reinterpret_cast<const char*>(&y), sizeof(y));
      oss.write(reinterpret_cast<const char*>(&z), sizeof(z));
    }
    if (!uv.empty()) {
      float x = (float)uv[i].x, y = (float)uv[i].y;
      oss.write(reinterpret_cast<const char*>(&x), sizeof(x));
      oss.write(reinterpret_cast<const char*>(&y), sizeof(y));
    }
    if (!color.empty()) {
      float x = (float)color[i].x, y = (float)color[i].y, z = (float)color[i].z, w = (float)color[i].w;
      oss.write(reinterpret_cast<const char*>(&x), sizeof(x));
      oss.write(reinterpret_cast<const char*>(&y), sizeof(y));
      oss.write(reinterpret_cast<const char*>(&z), sizeof(z));
      oss.write(reinterpret_cast<const char*>(&w), sizeof(w));
    }
  }
  uint32_t ni = (uint32_t)indices.size();
  oss.write(reinterpret_cast<const char*>(&ni), sizeof(ni));
  if (!indices.empty()) {
    oss.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));
  }
  std::string s = oss.str();
  return hash_bytes(s.data(), s.size());
}

static osg::Matrixd ufbx_matrix_to_osg(const ufbx_matrix &m) {
    return osg::Matrixd(
        m.m00, m.m10, m.m20, 0.0,
        m.m01, m.m11, m.m21, 0.0,
        m.m02, m.m12, m.m22, 0.0,
        m.m03, m.m13, m.m23, 1.0
    );
}

static std::string ufbx_string_to_std(const ufbx_string &s) {
  if (s.data == nullptr || s.length == 0) {
    return std::string();
  }
  return std::string(s.data, s.length);
}

static std::filesystem::path resolve_texture_path(const std::string &fbxPath,
                                                  const ufbx_texture *tex) {
  if (!tex) {
    return {};
  }

  std::string path = ufbx_string_to_std(tex->filename);
  if (path.empty()) {
    path = ufbx_string_to_std(tex->relative_filename);
  }
  if (path.empty()) {
    path = ufbx_string_to_std(tex->absolute_filename);
  }
  if (path.empty()) {
    return {};
  }

  std::filesystem::path p(path);
  if (p.is_absolute() && std::filesystem::exists(p)) {
      return p;
  }

  // Try relative to FBX file
  if (!fbxPath.empty()) {
      std::filesystem::path fbxDir = std::filesystem::path(fbxPath).parent_path();
      std::filesystem::path relPath = fbxDir / p;
      if (std::filesystem::exists(relPath)) {
          return relPath;
      }
      // Try just filename in FBX dir
      std::filesystem::path filename = p.filename();
      std::filesystem::path flatPath = fbxDir / filename;
      if (std::filesystem::exists(flatPath)) {
          return flatPath;
      }
  }

  return {};
}

// Helper to create StateSet (Member function implementation)
osg::StateSet* FBXLoader::getOrCreateStateSet(const ufbx_material* mat) {
    if (!mat) return nullptr;

    auto it = materialCache.find(mat);
    if (it != materialCache.end()) {
        material_reused_ptr_count++;
        return it->second.get();
    }

    std::string matHash = calcMaterialHash(mat);
    auto hit = materialHashCache.find(matHash);
    if (hit != materialHashCache.end()) {
        materialCache[mat] = hit->second;
        material_reused_hash_count++;
        return hit->second.get();
    }

    osg::StateSet* stateSet = new osg::StateSet;
    osg::Material* material = new osg::Material;

    // Diffuse
    osg::Vec4 diffuse(1, 1, 1, 1);
    if (mat->pbr.base_color.has_value) {
        diffuse.set(mat->pbr.base_color.value_vec4.x, mat->pbr.base_color.value_vec4.y, mat->pbr.base_color.value_vec4.z, mat->pbr.base_color.value_vec4.w);
    } else if (mat->fbx.diffuse_color.has_value) {
        diffuse.set(mat->fbx.diffuse_color.value_vec3.x, mat->fbx.diffuse_color.value_vec3.y, mat->fbx.diffuse_color.value_vec3.z, 1.0f);
        if (mat->fbx.diffuse_factor.has_value) diffuse *= mat->fbx.diffuse_factor.value_real;
    }
    material->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);

    // Specular
    osg::Vec4 specular(0, 0, 0, 1);
    if (mat->fbx.specular_color.has_value) {
        specular.set(mat->fbx.specular_color.value_vec3.x, mat->fbx.specular_color.value_vec3.y, mat->fbx.specular_color.value_vec3.z, 1.0f);
        if (mat->fbx.specular_factor.has_value) specular *= mat->fbx.specular_factor.value_real;
    }
    material->setSpecular(osg::Material::FRONT_AND_BACK, specular);

    // Shininess
    if (mat->fbx.specular_exponent.has_value) {
        material->setShininess(osg::Material::FRONT_AND_BACK, mat->fbx.specular_exponent.value_real);
    }

    // Emission
    osg::Vec4 emission(0, 0, 0, 1);
    if (mat->pbr.emission_color.has_value) {
        // Ignore default white emission (1,1,1) as it's often a placeholder
        float e_r = mat->pbr.emission_color.value_vec3.x;
        float e_g = mat->pbr.emission_color.value_vec3.y;
        float e_b = mat->pbr.emission_color.value_vec3.z;
        if (fabs(e_r - 1.0f) > 1e-6 || fabs(e_g - 1.0f) > 1e-6 || fabs(e_b - 1.0f) > 1e-6) {
            emission.set(e_r, e_g, e_b, 1.0f);
        }
    } else if (mat->fbx.emission_color.has_value) {
        float e_r = mat->fbx.emission_color.value_vec3.x;
        float e_g = mat->fbx.emission_color.value_vec3.y;
        float e_b = mat->fbx.emission_color.value_vec3.z;
        if (fabs(e_r - 1.0f) > 1e-6 || fabs(e_g - 1.0f) > 1e-6 || fabs(e_b - 1.0f) > 1e-6) {
            emission.set(e_r, e_g, e_b, 1.0f);
            if (mat->fbx.emission_factor.has_value) emission *= mat->fbx.emission_factor.value_real;
        }
    }
    material->setEmission(osg::Material::FRONT_AND_BACK, emission);

    stateSet->setAttributeAndModes(material);

    // Texture
    const ufbx_texture* tex = NULL;
    if (mat->pbr.base_color.texture) tex = mat->pbr.base_color.texture;
    else if (mat->fbx.diffuse_color.texture) tex = mat->fbx.diffuse_color.texture;

    if (tex) {
        osg::ref_ptr<osg::Image> image;

        // 1. Try embedded content
        if (tex->content.data && tex->content.size > 0) {
             std::string filename = ufbx_string_to_std(tex->filename);

             int width, height, channels;
             unsigned char* imgData = stbi_load_from_memory(
                 (const unsigned char*)tex->content.data,
                 (int)tex->content.size,
                 &width, &height, &channels, 0);

             if (imgData) {
                 image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
             } else {
                 LOG_E("Failed to decode embedded image with stb_image");
             }
        }

        // 2. Try file path if not embedded or failed
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, tex);
            if (!filename.empty()) {
                // Try STB first
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);

                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    // Fallback to OSG if STB fails
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }

        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(0, texture);
        }
    }
    const ufbx_texture* ntex = NULL;
    if (mat->pbr.normal_map.texture) ntex = mat->pbr.normal_map.texture;
    else if (mat->fbx.bump.texture) ntex = mat->fbx.bump.texture;
    if (ntex) {
        osg::ref_ptr<osg::Image> image;
        if (ntex->content.data && ntex->content.size > 0) {
            std::string filename = ufbx_string_to_std(ntex->filename);
            int width, height, channels;
            unsigned char* imgData = stbi_load_from_memory(
                (const unsigned char*)ntex->content.data,
                (int)ntex->content.size,
                &width, &height, &channels, 0);
            if (imgData) {
                image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
            }
        }
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, ntex);
            if (!filename.empty()) {
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }
        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(1, texture);
        }
    }
    const ufbx_texture* etex = NULL;
    if (mat->pbr.emission_color.texture) etex = mat->pbr.emission_color.texture;
    else if (mat->fbx.emission_color.texture) etex = mat->fbx.emission_color.texture;
    if (etex) {
        osg::ref_ptr<osg::Image> image;
        if (etex->content.data && etex->content.size > 0) {
            std::string filename = ufbx_string_to_std(etex->filename);
            int width, height, channels;
            unsigned char* imgData = stbi_load_from_memory(
                (const unsigned char*)etex->content.data,
                (int)etex->content.size,
                &width, &height, &channels, 0);
            if (imgData) {
                image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
            }
        }
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, etex);
            if (!filename.empty()) {
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }
        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(4, texture);
        }
    }
    const ufbx_texture* rtex = NULL;
    if (mat->pbr.roughness.texture) rtex = mat->pbr.roughness.texture;
    if (rtex) {
        osg::ref_ptr<osg::Image> image;
        if (rtex->content.data && rtex->content.size > 0) {
            std::string filename = ufbx_string_to_std(rtex->filename);
            int width, height, channels;
            unsigned char* imgData = stbi_load_from_memory(
                (const unsigned char*)rtex->content.data,
                (int)rtex->content.size,
                &width, &height, &channels, 0);
            if (imgData) {
                image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
            }
        }
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, rtex);
            if (!filename.empty()) {
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }
        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(2, texture);
        }
    }
    const ufbx_texture* mtex = NULL;
    if (mat->pbr.metalness.texture) mtex = mat->pbr.metalness.texture;
    if (mtex) {
        osg::ref_ptr<osg::Image> image;
        if (mtex->content.data && mtex->content.size > 0) {
            std::string filename = ufbx_string_to_std(mtex->filename);
            int width, height, channels;
            unsigned char* imgData = stbi_load_from_memory(
                (const unsigned char*)mtex->content.data,
                (int)mtex->content.size,
                &width, &height, &channels, 0);
            if (imgData) {
                image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
            }
        }
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, mtex);
            if (!filename.empty()) {
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }
        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(3, texture);
        }
    }
    const ufbx_texture* aotex = NULL;
    if (mat->pbr.ambient_occlusion.texture) aotex = mat->pbr.ambient_occlusion.texture;
    if (aotex) {
        osg::ref_ptr<osg::Image> image;
        if (aotex->content.data && aotex->content.size > 0) {
            std::string filename = ufbx_string_to_std(aotex->filename);
            int width, height, channels;
            unsigned char* imgData = stbi_load_from_memory(
                (const unsigned char*)aotex->content.data,
                (int)aotex->content.size,
                &width, &height, &channels, 0);
            if (imgData) {
                image = createImageFromSTB(imgData, width, height, channels, filename.empty() ? "embedded.png" : filename);
            }
        }
        if (!image) {
            std::filesystem::path filename = resolve_texture_path(source_filename, aotex);
            if (!filename.empty()) {
                int width, height, channels;
                std::string pathStr = filename.string();
                unsigned char* imgData = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
                if (imgData) {
                    image = createImageFromSTB(imgData, width, height, channels, pathStr);
                } else {
                    image = osgDB::readImageFile(pathStr);
                }
            }
        }
        if (image) {
            osg::Texture2D* texture = new osg::Texture2D(image);
            texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
            texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            stateSet->setTextureAttributeAndModes(5, texture);
        }
    }
    float ao_strength = 1.0f;
    if (mat->pbr.ambient_occlusion.has_value) {
        ao_strength = (float)mat->pbr.ambient_occlusion.value_real;
    }
    stateSet->addUniform(new osg::Uniform("aoStrength", ao_strength));
    float roughness = 1.0f;
    if (mat->pbr.roughness.has_value) roughness = (float)mat->pbr.roughness.value_real;
    else if (mat->fbx.specular_exponent.has_value) {
        float s = (float)mat->fbx.specular_exponent.value_real;
        if (s < 0.0f) s = 0.0f;
        if (s > 128.0f) s = 128.0f;
        roughness = 1.0f - sqrtf(s / 128.0f);
    }
    float metallic = 0.0f;
    if (mat->pbr.metalness.has_value) metallic = (float)mat->pbr.metalness.value_real;
    stateSet->addUniform(new osg::Uniform("roughnessFactor", roughness));
    stateSet->addUniform(new osg::Uniform("metallicFactor", metallic));

    materialCache[mat] = stateSet;
    materialHashCache[matHash] = stateSet;
    material_created_count++;
    return stateSet;
}

FBXLoader::FBXLoader(const std::string &filename) : source_filename(filename), scene(nullptr) {}

FBXLoader::~FBXLoader() {
  if (scene != nullptr) {
    ufbx_free_scene(scene);
    scene = nullptr;
  }
}

FBXLoader::DedupStats FBXLoader::getStats() const {
  DedupStats s;
  s.material_created = material_created_count;
  s.material_hash_reused = material_reused_hash_count;
  s.material_ptr_reused = material_reused_ptr_count;
  s.geometry_created = geometry_created_count;
  s.geometry_hash_reused = geometry_reused_hash_count;
  s.mesh_cache_hit_count = mesh_cache_hit_count;
  s.unique_statesets = materialHashCache.size();
  s.unique_geometries = geometryHashCache.size();
  return s;
}

std::string FBXLoader::calcMeshHash(const ufbx_mesh *mesh) {
  if (!mesh) return "0";
  // Only hash vertices/indices/faces count
  std::ostringstream oss;
  oss.write((const char*)mesh->vertices.data, mesh->vertices.count * sizeof(ufbx_vec3));
  oss.write((const char*)mesh->vertex_indices.data, mesh->vertex_indices.count * sizeof(uint32_t));
  oss.write((const char*)mesh->faces.data, mesh->faces.count * sizeof(ufbx_face));
  std::string s = oss.str();
  return hash_bytes(s.data(), s.size());
}

std::string FBXLoader::calcMaterialHash(const ufbx_material *mat) {
  if (!mat) return "0";

  std::ostringstream oss;

  // Diffuse
  float diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (mat->pbr.base_color.has_value) {
    diffuse[0] = mat->pbr.base_color.value_vec4.x;
    diffuse[1] = mat->pbr.base_color.value_vec4.y;
    diffuse[2] = mat->pbr.base_color.value_vec4.z;
    diffuse[3] = mat->pbr.base_color.value_vec4.w;
  } else if (mat->fbx.diffuse_color.has_value) {
    float factor = mat->fbx.diffuse_factor.has_value ? (float)mat->fbx.diffuse_factor.value_real : 1.0f;
    diffuse[0] = (float)mat->fbx.diffuse_color.value_vec3.x * factor;
    diffuse[1] = (float)mat->fbx.diffuse_color.value_vec3.y * factor;
    diffuse[2] = (float)mat->fbx.diffuse_color.value_vec3.z * factor;
    diffuse[3] = 1.0f;
  }
  oss.write(reinterpret_cast<const char*>(diffuse), sizeof(diffuse));

  // Specular
  float specular[3] = {0.0f, 0.0f, 0.0f};
  if (mat->fbx.specular_color.has_value) {
    float sf = mat->fbx.specular_factor.has_value ? (float)mat->fbx.specular_factor.value_real : 1.0f;
    specular[0] = (float)mat->fbx.specular_color.value_vec3.x * sf;
    specular[1] = (float)mat->fbx.specular_color.value_vec3.y * sf;
    specular[2] = (float)mat->fbx.specular_color.value_vec3.z * sf;
  }
  oss.write(reinterpret_cast<const char*>(specular), sizeof(specular));

  float shininess = mat->fbx.specular_exponent.has_value ? (float)mat->fbx.specular_exponent.value_real : 0.0f;
  oss.write(reinterpret_cast<const char*>(&shininess), sizeof(shininess));

  float emission[3] = {0.0f, 0.0f, 0.0f};
  if (mat->pbr.emission_color.has_value) {
    float e_r = (float)mat->pbr.emission_color.value_vec3.x;
    float e_g = (float)mat->pbr.emission_color.value_vec3.y;
    float e_b = (float)mat->pbr.emission_color.value_vec3.z;
    if (fabs(e_r - 1.0f) > 1e-6 && fabs(e_g - 1.0f) > 1e-6 && fabs(e_b - 1.0f) > 1e-6) {
        emission[0] = (float)mat->pbr.emission_color.value_vec3.x;
        emission[1] = (float)mat->pbr.emission_color.value_vec3.y;
        emission[2] = (float)mat->pbr.emission_color.value_vec3.z;
    }
  } else if (mat->fbx.emission_color.has_value) {
    float e_r = (float)mat->fbx.emission_color.value_vec3.x;
    float e_g = (float)mat->fbx.emission_color.value_vec3.y;
    float e_b = (float)mat->fbx.emission_color.value_vec3.z;
    float ef = mat->fbx.emission_factor.has_value ? (float)mat->fbx.emission_factor.value_real : 1.0f;
    if (fabs(e_r - 1.0f) > 1e-6 && fabs(e_g - 1.0f) > 1e-6 && fabs(e_b - 1.0f) > 1e-6) {
        emission[0] = (float)mat->fbx.emission_color.value_vec3.x * ef;
        emission[1] = (float)mat->fbx.emission_color.value_vec3.y * ef;
        emission[2] = (float)mat->fbx.emission_color.value_vec3.z * ef;
    }
  }
  oss.write(reinterpret_cast<const char*>(emission), sizeof(emission));

  const ufbx_texture* tex = nullptr;
  if (mat->pbr.base_color.texture) tex = mat->pbr.base_color.texture;
  else if (mat->fbx.diffuse_color.texture) tex = mat->fbx.diffuse_color.texture;

  if (tex) {
    if (tex->content.data && tex->content.size > 0) {
      std::string contentHash = hash_bytes(tex->content.data, tex->content.size);
      oss.write(contentHash.data(), contentHash.size());
    } else {
      std::string path;
      if (tex->absolute_filename.data && tex->absolute_filename.length) {
        path.assign(tex->absolute_filename.data, tex->absolute_filename.length);
      } else if (tex->filename.data && tex->filename.length) {
        path.assign(tex->filename.data, tex->filename.length);
      } else if (tex->relative_filename.data && tex->relative_filename.length) {
        path.assign(tex->relative_filename.data, tex->relative_filename.length);
      }
      if (!path.empty()) {
        for (char &c : path) {
          if (c == '\\') c = '/';
          c = (char)std::tolower((unsigned char)c);
        }
        oss.write(path.data(), path.size());
      }
    }
  }

  std::string s = oss.str();
  return hash_bytes(s.data(), s.size());
}

std::unordered_map<std::string, std::string> FBXLoader::collectNodeAttrs(const ufbx_node *node) {
  std::unordered_map<std::string, std::string> attrs;
  if (!node) return attrs;
  attrs["name"] = node->name.data ? std::string(node->name.data, node->name.length) : "";
  for (size_t i = 0; i < node->props.props.count; ++i) {
    const ufbx_prop &p = node->props.props.data[i];
    if (p.name.data && p.value_str.data) {
      attrs[std::string(p.name.data, p.name.length)] = std::string(p.value_str.data, p.value_str.length);
    }
  }
  return attrs;
}

void FBXLoader::load() {
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up; // Convert to glTF/OpenGL standard (Y-up)
    opts.target_unit_meters = 1.0f; // Force output unit to be Meters (Cesium standard)
    opts.clean_skin_weights = true;
    opts.allow_missing_vertex_position = false; // Handle models with missing positions
    opts.generate_missing_normals = true; // Automatically generate normals if missing

    // Ensure we handle triangulation if needed (though ufbx does this well by default)
    // opts.generate_indices is NOT a field in ufbx_load_opts. We must call ufbx_generate_indices manually per mesh.

    ufbx_error error;
    scene = ufbx_load_file(source_filename.c_str(), &opts, &error);
    if (!scene) {
        LOG_E("Failed to load FBX: %s", error.description.data);
        return;
    }

    // Log settings for debugging (metadata often contains the original file info)
    if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_X) LOG_I("FBX File Up-Axis: +X");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_X) LOG_I("FBX File Up-Axis: -X");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_Y) LOG_I("FBX File Up-Axis: +Y");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_Y) LOG_I("FBX File Up-Axis: -Y");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_Z) LOG_I("FBX File Up-Axis: +Z");
    else if (scene->settings.axes.up == UFBX_COORDINATE_AXIS_NEGATIVE_Z) LOG_I("FBX File Up-Axis: -Z");
    else LOG_I("FBX File Up-Axis: Unknown");

    if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_X) LOG_I("FBX File Front-Axis: +X");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_X) LOG_I("FBX File Front-Axis: -X");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_Y) LOG_I("FBX File Front-Axis: +Y");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_Y) LOG_I("FBX File Front-Axis: -Y");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_Z) LOG_I("FBX File Front-Axis: +Z");
    else if (scene->settings.axes.front == UFBX_COORDINATE_AXIS_NEGATIVE_Z) LOG_I("FBX File Front-Axis: -Z");

    LOG_I("FBX File Original Unit Meters: %f", scene->settings.unit_meters);

    displayLayerHiddenNodes.clear();
    for (size_t i = 0; i < scene->display_layers.count; i++) {
        ufbx_display_layer *layer = scene->display_layers.data[i];
        if (!layer->visible || layer->frozen) {
            for (size_t j = 0; j < layer->nodes.count; j++) {
                displayLayerHiddenNodes.insert(layer->nodes.data[j]);
            }
        }
    }

    // Start loading from the root node
    if (scene->root_node) {
        _root = loadNode(scene->root_node, osg::Matrixd::identity());
    }

    LOG_I("Material dedup: created=%d reused_by_hash=%d pointer_hits=%d unique_statesets=%zu",
          material_created_count, material_reused_hash_count, material_reused_ptr_count, materialHashCache.size());
    LOG_I("Mesh dedup: geometries_created=%d reused_by_hash=%d mesh_cache_hits=%d unique_geometries=%zu",
          geometry_created_count, geometry_reused_hash_count, mesh_cache_hit_count, geometryHashCache.size());
}

osg::ref_ptr<osg::Node> FBXLoader::loadNode(ufbx_node *node, const osg::Matrixd &parentXform) {
    if (!node) return nullptr;

    // Check visibility
    if (!node->visible) {
        return nullptr;
    }
    if (displayLayerHiddenNodes.find(node) != displayLayerHiddenNodes.end()) {
        return nullptr;
    }

    osg::Matrixd globalMatrix = ufbx_matrix_to_osg(node->node_to_world);

    osg::Matrixd parentInv;
    if (!parentInv.invert(parentXform)) {
        parentInv.makeIdentity();
    }

    osg::Matrixd localMatrix = globalMatrix * parentInv;

    osg::ref_ptr<osg::Group> group;
    osg::ref_ptr<osg::MatrixTransform> transform;

    bool hasTransform = !localMatrix.isIdentity();
    if (hasTransform) {
        transform = new osg::MatrixTransform(localMatrix);
        group = transform;
    } else {
        group = new osg::Group;
    }

    std::string name = ufbx_string_to_std(node->name);
    group->setName(name);

    // Process Meshes
    if (node->mesh) {
        ufbx_mesh *mesh = node->mesh;

        // Apply Geometry Transform (Pivot/Offset specific to the mesh attachment)
        // ufbx provides geometry_to_node which transforms mesh to node local space.
        // Or geometry_to_world directly.
        // We use geometry_to_world to ensure the mesh is placed exactly where ufbx calculates it to be,
        // accounting for all pivot offsets, scaling, and inheritance modes.
        osg::Matrixd geomToWorld = ufbx_matrix_to_osg(node->geometry_to_world);

        // Calculate the effective local transform for the mesh relative to the node
        // node_to_world * geom_to_node = geometry_to_world
        // geom_to_node = geometry_to_world * inverse(node_to_world)
        osg::Matrixd globalInv;
        globalInv.invert(globalMatrix);
        osg::Matrixd geomToNode = geomToWorld * globalInv;

        // Use geomToWorld for mesh processing (MeshPool needs global positions)
        osg::Matrixd meshGlobalMatrix = geomToWorld;

        osg::ref_ptr<osg::Geode> geode = processMesh(node, mesh, meshGlobalMatrix);
        if (geode) {
            // Apply Geometry Transform (important for pivot offsets)
            if (!geomToNode.isIdentity()) {
                osg::MatrixTransform* geomTransform = new osg::MatrixTransform(geomToNode);
                geomTransform->addChild(geode);
                group->addChild(geomTransform);
            } else {
                group->addChild(geode);
            }
        }
    }

    // Process Children
    // We pass globalMatrix just in case, but actually if we switch to using node->node_to_world for everyone,
    // we don't strictly need to pass accumulated parentXform down for calculation,
    // BUT loadNode signature expects it.
    // The recursive call logic here is fine for scene graph building.

    for (size_t i = 0; i < node->children.count; ++i) {
        // For the child, the parent transform is THIS node's global transform?
        // Wait, 'parentXform' passed to loadNode is the parent's world transform.
        // 'globalMatrix' computed here IS this node's world transform.
        // So passing 'globalMatrix' to children is correct for accumulation if we were doing it manually.
        // But since we use ufbx's node_to_world, the child will just use its own node_to_world.

        osg::ref_ptr<osg::Node> childNode = loadNode(node->children.data[i], globalMatrix);
        if (childNode) {
            group->addChild(childNode);
        }
    }

    return group;
}

osg::ref_ptr<osg::Geode> FBXLoader::processMesh(ufbx_node *node, ufbx_mesh *mesh, const osg::Matrixd &globalXform) {
    if (!mesh) return nullptr;

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    // 1. Check Cache
    auto it = meshCache.find(mesh);
    if (it != meshCache.end()) {
        mesh_cache_hit_count += (int)it->second.size();
        for (const auto& part : it->second) {
            geode->addDrawable(part.geometry);

            // Update MeshPool
            MeshKey key;
            key.geomHash = part.geomHash;
            key.matHash = part.matHash;

            if (meshPool.find(key) != meshPool.end()) {
                 meshPool[key].transforms.push_back(globalXform);
                 meshPool[key].nodeNames.push_back(ufbx_string_to_std(node->name));
                 meshPool[key].nodeAttrs.push_back(collectNodeAttrs(node));
            } else {
                 MeshInstanceInfo info;
                 info.key = key;
                 info.geometry = part.geometry;
                 info.transforms.push_back(globalXform);
                 info.nodeNames.push_back(ufbx_string_to_std(node->name));
                 info.nodeAttrs.push_back(collectNodeAttrs(node));
                 meshPool[key] = info;
            }
        }
        return geode;
    }

    // 2. Not in cache, process mesh
    // Use ufbx_generate_indices to handle vertex deduplication

    // Calculate Mesh Hash (Base geometry hash)
    std::string meshHash = calcMeshHash(mesh);

    // We need to flatten the indexed ufbx data into "wedge" arrays first,
    // because ufbx_generate_indices expects flat arrays of size num_indices.
    // It will then reorder/compact these arrays in-place.

    size_t num_indices = mesh->num_indices;
    if (num_indices == 0) return nullptr;

    std::vector<ufbx_vec3> tempPos;
    std::vector<ufbx_vec3> tempNorm;
    std::vector<ufbx_vec2> tempUV;
    std::vector<ufbx_vec4> tempColor;

    // Helper to resize if exists
    if (mesh->vertex_position.exists) tempPos.resize(num_indices);
    if (mesh->vertex_normal.exists) tempNorm.resize(num_indices);
    if (mesh->vertex_uv.exists) tempUV.resize(num_indices);
    if (mesh->vertex_color.exists) tempColor.resize(num_indices);

    // Flatten data
    for (size_t i = 0; i < num_indices; ++i) {
        if (mesh->vertex_position.exists) {
            tempPos[i] = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
        }
        if (mesh->vertex_normal.exists) {
            tempNorm[i] = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
        }
        if (mesh->vertex_uv.exists) {
            tempUV[i] = ufbx_get_vertex_vec2(&mesh->vertex_uv, i);
        }
        if (mesh->vertex_color.exists) {
            tempColor[i] = ufbx_get_vertex_vec4(&mesh->vertex_color, i);
        }
    }

    const osg::Matrixd finalXform = globalXform;

    // Check for missing or bad normals
    if (mesh->vertex_normal.exists) {
        if (mesh->generated_normals) {
             std::string nodeName = ufbx_string_to_std(node->name);
             LOG_W("Mesh node '%s' had no normals, they were automatically generated.", nodeName.c_str());
        }
    } else {
        std::string nodeName = ufbx_string_to_std(node->name);
        LOG_W("Mesh node '%s' has no normals. Lighting may be incorrect.", nodeName.c_str());
    }

    // Check for zero-length normals
    if (mesh->vertex_normal.exists) {
        size_t zero_normal_count = 0;
        for (const auto& n : tempNorm) {
            if (n.x*n.x + n.y*n.y + n.z*n.z < 1e-6) {
                zero_normal_count++;
            }
        }
        if (zero_normal_count > 0) {
            std::string nodeName = ufbx_string_to_std(node->name);
            LOG_W("Mesh node '%s' has %zu zero-length normals (out of %zu). Lighting may be incorrect.", nodeName.c_str(), zero_normal_count, tempNorm.size());
        }
    }

    // Prepare streams for ufbx_generate_indices
    std::vector<ufbx_vertex_stream> streams;
    ufbx_vertex_stream s;

    if (!tempPos.empty()) {
        s.data = tempPos.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec3);
        streams.push_back(s);
    }
    if (!tempNorm.empty()) {
        s.data = tempNorm.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec3);
        streams.push_back(s);
    }
    if (!tempUV.empty()) {
        s.data = tempUV.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec2);
        streams.push_back(s);
    }
    if (!tempColor.empty()) {
        s.data = tempColor.data();
        s.vertex_count = num_indices;
        s.vertex_size = sizeof(ufbx_vec4);
        streams.push_back(s);
    }

    // Generate indices
    std::vector<uint32_t> generated_indices(num_indices);
    ufbx_error error;
    size_t num_vertices = ufbx_generate_indices(streams.data(), streams.size(), generated_indices.data(), num_indices, nullptr, &error);

    if (num_vertices == 0 && num_indices > 0) {
        return nullptr;
    }

    // Copy to OSG arrays (only the unique vertices)
    osg::ref_ptr<osg::Vec3dArray> osgPos = new osg::Vec3dArray;
    osg::ref_ptr<osg::Vec3Array> osgNorm = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec2Array> osgUV = new osg::Vec2Array;
    osg::ref_ptr<osg::Vec4Array> osgColor = new osg::Vec4Array;

    osgPos->reserve(num_vertices);
    if (!tempNorm.empty()) osgNorm->reserve(num_vertices);
    if (!tempUV.empty()) osgUV->reserve(num_vertices);
    if (!tempColor.empty()) osgColor->reserve(num_vertices);

    for(size_t i=0; i<num_vertices; ++i) {
        if (!tempPos.empty()) osgPos->push_back(osg::Vec3d(tempPos[i].x, tempPos[i].y, tempPos[i].z));
        if (!tempNorm.empty()) osgNorm->push_back(osg::Vec3(tempNorm[i].x, tempNorm[i].y, tempNorm[i].z));
        if (!tempUV.empty()) osgUV->push_back(osg::Vec2(tempUV[i].x, tempUV[i].y));
        if (!tempColor.empty()) osgColor->push_back(osg::Vec4(tempColor[i].x, tempColor[i].y, tempColor[i].z, tempColor[i].w));
    }

    // Scratch buffer for triangulation
    std::vector<uint32_t> triIndices;
    triIndices.resize(mesh->max_face_triangles * 3);

    // Split by material parts
    std::vector<CachedPart> cachedParts;

    size_t num_parts = mesh->material_parts.count > 0 ? mesh->material_parts.count : 1;
    size_t faceOffset = 0;

    for (size_t partIndex = 0; partIndex < num_parts; ++partIndex) {
        // Identify faces for this part
        std::vector<uint32_t> partFaces;
        size_t matIndex = 0;

        if (mesh->material_parts.count > 0) {
            ufbx_mesh_part *part = &mesh->material_parts.data[partIndex];
            matIndex = part->index;
            if (part->num_faces == 0) continue;

            if (part->face_indices.count > 0) {
                partFaces.resize(part->face_indices.count);
                for(size_t i=0; i<part->face_indices.count; ++i) {
                    partFaces[i] = part->face_indices.data[i];
                }
            } else {
                // Contiguous range
                partFaces.resize(part->num_faces);
                for(size_t i=0; i<part->num_faces; ++i) partFaces[i] = (uint32_t)(faceOffset + i);
                faceOffset += part->num_faces;
            }
        } else {
            // No parts, process all faces
            partFaces.resize(mesh->num_faces);
            for(size_t i=0; i<mesh->num_faces; ++i) partFaces[i] = (uint32_t)i;
        }

        if (partFaces.empty()) continue;

        if (mesh->face_hole.data && mesh->face_hole.count == mesh->num_faces) {
            std::vector<uint32_t> filteredFaces;
            filteredFaces.reserve(partFaces.size());
            for (uint32_t fi : partFaces) {
                if (fi < mesh->num_faces && !mesh->face_hole.data[fi]) {
                    filteredFaces.push_back(fi);
                }
            }
            partFaces.swap(filteredFaces);
            if (partFaces.empty()) continue;
        }

        std::vector<uint32_t> partIndices;
        for (uint32_t faceIdx : partFaces) {
            ufbx_face face = mesh->faces.data[faceIdx];

            // Triangulate using ufbx
            size_t numTris = ufbx_triangulate_face(triIndices.data(), triIndices.size(), mesh, face);

            for (size_t t = 0; t < numTris; ++t) {
                for (size_t v = 0; v < 3; ++v) {
                    uint32_t wedgeIdx = triIndices[t * 3 + v];
                    // Map wedge index to unique index
                    uint32_t uniqueIdx = generated_indices[wedgeIdx];
                    partIndices.push_back(uniqueIdx);
                }
            }
        }

        if (partIndices.empty()) {
            continue;
        }

        // Generate Normals if missing
        if (!mesh->vertex_normal.exists && osgPos->size() > 0) {
             for(size_t i=0; i<partIndices.size(); i+=3) {
                 uint32_t i0 = partIndices[i];
                 uint32_t i1 = partIndices[i+1];
                 uint32_t i2 = partIndices[i+2];

                 osg::Vec3d p0 = (*osgPos)[i0];
                 osg::Vec3d p1 = (*osgPos)[i1];
                 osg::Vec3d p2 = (*osgPos)[i2];

                 osg::Vec3d edge1 = p1 - p0;
                 osg::Vec3d edge2 = p2 - p0;
                 osg::Vec3d normal = edge1 ^ edge2; // Cross product
                 normal.normalize();

                 osg::Vec3 n(normal.x(), normal.y(), normal.z());
                 (*osgNorm)[i0] += n;
                 (*osgNorm)[i1] += n;
                 (*osgNorm)[i2] += n;
             }
        }

        std::string geomHash = calc_part_geom_hash(num_vertices, tempPos, tempNorm, tempUV, tempColor, partIndices);
        osg::ref_ptr<osg::Geometry> geometry;
        auto ghit = geometryHashCache.find(geomHash);
        if (ghit != geometryHashCache.end()) {
            geometry = ghit->second;
            geometry_reused_hash_count++;
        } else {
            // Finalize generated normals
            if (!mesh->vertex_normal.exists && osgNorm->size() > 0) {
                for(size_t i=0; i<osgNorm->size(); ++i) {
                    (*osgNorm)[i].normalize();
                }
            }

            geometry = new osg::Geometry;
            geometry->setVertexArray(osgPos);
            if (osgNorm->size() > 0) geometry->setNormalArray(osgNorm, osg::Array::BIND_PER_VERTEX);
            if (osgUV->size() > 0) geometry->setTexCoordArray(0, osgUV, osg::Array::BIND_PER_VERTEX);
            if (osgColor->size() > 0) geometry->setColorArray(osgColor, osg::Array::BIND_PER_VERTEX);
            osg::ref_ptr<osg::DrawElementsUInt> drawElements = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
            drawElements->reserve(partIndices.size());
            for (uint32_t idx : partIndices) drawElements->push_back(idx);
            geometry->addPrimitiveSet(drawElements);
            geometryHashCache[geomHash] = geometry;
            geometry_created_count++;
        }

        // Material
        if (mesh->materials.count > matIndex) {
             ufbx_material *mat = mesh->materials.data[matIndex];
             geometry->setStateSet(getOrCreateStateSet(mat));
        }

        geode->addDrawable(geometry);

        // Cache Info
        CachedPart cpart;
        cpart.geometry = geometry;
        cpart.geomHash = geomHash;
        cpart.matHash = calcMaterialHash(mesh->materials.count > matIndex ? mesh->materials.data[matIndex] : nullptr);
        cachedParts.push_back(cpart);

        // MeshPool Update
        MeshInstanceInfo info;
        info.key.geomHash = cpart.geomHash;
        info.key.matHash = cpart.matHash;
        info.geometry = geometry;
        info.transforms.push_back(finalXform);
        info.nodeNames.push_back(ufbx_string_to_std(node->name));
        info.nodeAttrs.push_back(collectNodeAttrs(node));

        if (meshPool.find(info.key) != meshPool.end()) {
             meshPool[info.key].transforms.push_back(finalXform);
             meshPool[info.key].nodeNames.push_back(ufbx_string_to_std(node->name));
             meshPool[info.key].nodeAttrs.push_back(collectNodeAttrs(node));
        } else {
             meshPool[info.key] = info;
        }
    }

    // Store in Cache
    meshCache[mesh] = cachedParts;

    return geode;
}
