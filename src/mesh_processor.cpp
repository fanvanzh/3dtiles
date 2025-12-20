#include "mesh_processor.h"
#include <osg/Texture>
#include <osg/Image>
#include <osg/Array>
#include <vector>
#include <cstdlib>

// Add Basis Universal includes for KTX2 compression
#include <basisu/encoder/basisu_comp.h>
#include <basisu/transcoder/basisu_transcoder.h>

// Include meshoptimizer for mesh simplification
#include <meshoptimizer.h>

// Include Draco compression headers
#include "draco/compression/encode.h"
#include "draco/core/encoder_buffer.h"
#include "draco/mesh/mesh.h"

#include "stb_image_write.h"

// Function to compress image data to KTX2 using Basis Universal
bool compress_to_ktx2(const std::vector<unsigned char>& rgba_data, int width, int height,
                      std::vector<unsigned char>& ktx2_data) {
    try {
        // Validate input parameters
        if (rgba_data.empty() || width <= 0 || height <= 0) {
            return false;
        }

        // Initialize Basis Universal encoder
        static bool basisu_initialized = false;
        if (!basisu_initialized) {
            basisu::basisu_encoder_init();
            basisu_initialized = true;
        }

        basisu::basis_compressor_params params;
        params.m_source_images.resize(1);
        params.m_source_images[0].init(rgba_data.data(), width, height, 4);

        // Settings
        params.m_compression_level = 2; // Balanced
        params.m_create_ktx2_file = true;
        params.m_mip_gen = true;

        basisu::basis_compressor compressor;
        if (!compressor.init(params)) return false;

        if (compressor.process() != basisu::basis_compressor::cECSuccess) return false;

        const auto& output = compressor.get_output_ktx2_file();
        ktx2_data.assign(output.begin(), output.end());

        return true;
    } catch (...) {
        return false;
    }
}

// Helper function to write buffer data (static to avoid duplicate symbol)
static void write_buf(void* context, void* data, int len) {
    std::vector<char> *buf = (std::vector<char>*)context;
    buf->insert(buf->end(), (char*)data, (char*)data + len);
}

// Function to process textures (KTX2 compression)
bool process_texture(osg::Texture* tex, std::vector<unsigned char>& image_data, std::string& mime_type, bool enable_texture_compress) {
    // Check if KTX2 compression is enabled
    if (enable_texture_compress) {
        // Handle KTX2 compression using Basis Universal
        std::vector<unsigned char> ktx2_buf;
        int width, height;

        if (tex) {
            if (tex->getNumImages() > 0) {
                osg::Image* img = tex->getImage(0);
                if (img) {
                    width = img->s();
                    height = img->t();

                    // Extract raw RGBA data for compression
                    std::vector<unsigned char> rgba_data;
                    const GLenum format = img->getPixelFormat();
                    const unsigned char* source_data = img->data();
                    size_t data_size = img->getTotalSizeInBytes();

                    // Check if there's row padding that needs to be handled
                    unsigned int rowStep = img->getRowStepInBytes();
                    unsigned int rowSize = img->getRowSizeInBytes();
                    bool hasRowPadding = (rowStep != rowSize);

                    // Convert to RGBA if needed
                    if (format == GL_RGBA) {
                        if (hasRowPadding) {
                            // Handle row padding
                            rgba_data.resize(width * height * 4);
                            for (int row = 0; row < height; row++) {
                                memcpy(&rgba_data[row * width * 4],
                                       &source_data[row * rowStep],
                                       width * 4);
                            }
                        } else {
                            rgba_data.assign(source_data, source_data + data_size);
                        }
                    } else if (format == GL_RGB) {
                        // Convert RGB to RGBA
                        rgba_data.resize(width * height * 4);
                        if (hasRowPadding) {
                            // Handle row padding for RGB
                            for (int row = 0; row < height; row++) {
                                for (int col = 0; col < width; col++) {
                                    int src_idx = row * rowStep + col * 3;
                                    int dst_idx = (row * width + col) * 4;
                                    rgba_data[dst_idx + 0] = source_data[src_idx + 0];
                                    rgba_data[dst_idx + 1] = source_data[src_idx + 1];
                                    rgba_data[dst_idx + 2] = source_data[src_idx + 2];
                                    rgba_data[dst_idx + 3] = 255;
                                }
                            }
                        } else {
                            for (int i = 0; i < width * height; i++) {
                                rgba_data[i * 4 + 0] = source_data[i * 3 + 0];
                                rgba_data[i * 4 + 1] = source_data[i * 3 + 1];
                                rgba_data[i * 4 + 2] = source_data[i * 3 + 2];
                                rgba_data[i * 4 + 3] = 255;
                            }
                        }
                    } else if (format == GL_BGRA) {
                        // Convert BGRA to RGBA
                        rgba_data.resize(width * height * 4);
                        if (hasRowPadding) {
                            for (int row = 0; row < height; row++) {
                                for (int col = 0; col < width; col++) {
                                    int src_idx = row * rowStep + col * 4;
                                    int dst_idx = (row * width + col) * 4;
                                    rgba_data[dst_idx + 0] = source_data[src_idx + 2]; // R
                                    rgba_data[dst_idx + 1] = source_data[src_idx + 1]; // G
                                    rgba_data[dst_idx + 2] = source_data[src_idx + 0]; // B
                                    rgba_data[dst_idx + 3] = source_data[src_idx + 3]; // A
                                }
                            }
                        } else {
                            for (int i = 0; i < width * height; i++) {
                                rgba_data[i * 4 + 0] = source_data[i * 4 + 2]; // R
                                rgba_data[i * 4 + 1] = source_data[i * 4 + 1]; // G
                                rgba_data[i * 4 + 2] = source_data[i * 4 + 0]; // B
                                rgba_data[i * 4 + 3] = source_data[i * 4 + 3]; // A
                            }
                        }
                    }

                    // Compress to KTX2 using Basis Universal
                    if (!rgba_data.empty()) {
                        if (compress_to_ktx2(rgba_data, width, height, ktx2_buf)) {
                            // Successfully compressed to KTX2
                            image_data = ktx2_buf;
                            mime_type = "image/ktx2";
                            return true;
                        }
                    }
                }
            }
        }

        // If KTX2 compression failed, fall back to JPEG
    }

    // Fallback to JPEG compression
    std::vector<unsigned char> jpeg_buf;
    int width, height;
    if (tex) {
        if (tex->getNumImages() > 0) {
            osg::Image* img = tex->getImage(0);
            if (img) {
                width = img->s();
                height = img->t();

                const GLenum format = img->getPixelFormat();
                const char* rgb = (const char*)(img->data());
                uint32_t rowStep = img->getRowStepInBytes();
                uint32_t rowSize = img->getRowSizeInBytes();
                switch (format)
                {
                case GL_RGBA:
                    jpeg_buf.resize(width * height * 3);
                    for (int i = 0; i < height; i++)
                    {
                        for (int j = 0; j < width; j++)
                        {
                            jpeg_buf[i * width * 3 + j * 3] = rgb[i * width * 4 + j * 4];
                            jpeg_buf[i * width * 3 + j * 3 + 1] = rgb[i * width * 4 + j * 4 + 1];
                            jpeg_buf[i * width * 3 + j * 3 + 2] = rgb[i * width * 4 + j * 4 + 2];
                        }
                    }
                    break;
                case GL_BGRA:
                    jpeg_buf.resize(width * height * 3);
                    for (int i = 0; i < height; i++)
                    {
                        for (int j = 0; j < width; j++)
                        {
                            jpeg_buf[i * width * 3 + j * 3] = rgb[i * width * 4 + j * 4 + 2];
                            jpeg_buf[i * width * 3 + j * 3 + 1] = rgb[i * width * 4 + j * 4 + 1];
                            jpeg_buf[i * width * 3 + j * 3 + 2] = rgb[i * width * 4 + j * 4];
                        }
                    }
                    break;
                case GL_RGB:
                    for (int i = 0; i < height; i++)
                    {
                        for (int j = 0; j < rowSize; j++)
                        {
                            jpeg_buf.push_back(rgb[rowStep * i + j]);
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
    if (!jpeg_buf.empty()) {
        std::vector<char> buffer_data;
        stbi_write_jpg_to_func(write_buf, &buffer_data, width, height, 3, jpeg_buf.data(), 80);
        image_data.assign(buffer_data.begin(), buffer_data.end());
        mime_type = "image/jpeg";
        return true;
    }
    else {
        std::vector<char> v_data(256 * 256 * 3, 255);
        width = height = 256;
        std::vector<char> buffer_data;
        stbi_write_jpg_to_func(write_buf, &buffer_data, width, height, 3, v_data.data(), 80);
        image_data.assign(buffer_data.begin(), buffer_data.end());
        mime_type = "image/jpeg";
        return true;
    }

    return false;
}

// Function to optimize and simplify mesh data using meshoptimizer
bool optimize_and_simplify_mesh(
    std::vector<VertexData>& vertices,
    size_t& vertex_count,
    std::vector<unsigned int>& indices,
    size_t original_index_count,
    std::vector<unsigned int>& simplified_indices,
    size_t& simplified_index_count,
    const SimplificationParams& params) {

    // Calculate target index count based on ratio
    size_t target_index_count = static_cast<size_t>(original_index_count * params.target_ratio);

    // Auto-detect if normals are present by checking if any vertex has non-zero normals
    bool hasNormals = false;
    if (params.preserve_normals && vertex_count > 0) {
        for (size_t i = 0; i < vertex_count; ++i) {
            if (vertices[i].nx != 0.0f || vertices[i].ny != 0.0f || vertices[i].nz != 0.0f) {
                hasNormals = true;
                break;
            }
        }
    }

    // ============================================================================
    // Step 1: Generate vertex remap to remove duplicate vertices
    // ============================================================================
    std::vector<unsigned int> remap(vertex_count);
    size_t unique_vertex_count = meshopt_generateVertexRemap(
        remap.data(),
        indices.data(),
        original_index_count,
        vertices.data(),
        vertex_count,
        sizeof(VertexData)
    );

    // Remap index buffer
    meshopt_remapIndexBuffer(
        indices.data(),
        indices.data(),
        original_index_count,
        remap.data()
    );

    // Remap vertex buffer (positions, normals, UVs all together)
    std::vector<VertexData> remapped_vertices(unique_vertex_count);
    meshopt_remapVertexBuffer(
        remapped_vertices.data(),
        vertices.data(),
        vertex_count,
        sizeof(VertexData),
        remap.data()
    );

    // Update vertices to use remapped version
    vertices = std::move(remapped_vertices);
    vertex_count = unique_vertex_count;

    // ============================================================================
    // Step 2: Optimize vertex cache
    // ============================================================================
    meshopt_optimizeVertexCache(
        indices.data(),
        indices.data(),
        original_index_count,
        vertex_count
    );

    // ============================================================================
    // Step 3: Optimize overdraw
    // ============================================================================
    meshopt_optimizeOverdraw(
        indices.data(),
        indices.data(),
        original_index_count,
        &vertices[0].x,
        vertex_count,
        sizeof(VertexData),
        1.05f  // Threshold
    );

    // ============================================================================
    // Step 4: Optimize vertex fetch
    // ============================================================================
    meshopt_optimizeVertexFetch(
        vertices.data(),
        indices.data(),
        original_index_count,
        vertices.data(),
        vertex_count,
        sizeof(VertexData)
    );

    // ============================================================================
    // Step 5: Mesh simplification
    // ============================================================================
    // Allocate memory for simplified indices (worst case scenario)
    simplified_indices.resize(original_index_count);

    // Use meshopt_simplifyWithAttributes to preserve normals during simplification
    float result_error = 0;

    if (hasNormals) {
        // Normal weights for each component (nx, ny, nz)
        float attribute_weights[3] = {0.5f, 0.5f, 0.5f};

        simplified_index_count = meshopt_simplifyWithAttributes(
            simplified_indices.data(),
            indices.data(),
            original_index_count,
            &vertices[0].x,          // Position data
            vertex_count,
            sizeof(VertexData),       // Stride between positions
            &vertices[0].nx,          // Normal data
            sizeof(VertexData),       // Stride between normals
            attribute_weights,
            3,                        // 3 normal components
            nullptr,
            target_index_count,
            params.target_error,
            0,
            &result_error
        );
    } else {
        // No normals - use standard simplification
        simplified_index_count = meshopt_simplify(
            simplified_indices.data(),
            indices.data(),
            original_index_count,
            &vertices[0].x,
            vertex_count,
            sizeof(VertexData),
            target_index_count,
            params.target_error,
            0,
            &result_error
        );
    }

    // Resize to actual simplified size
    simplified_indices.resize(simplified_index_count);

    return true;
}

// Function to simplify mesh geometry using meshoptimizer
bool simplify_mesh_geometry(osg::Geometry* geometry, const SimplificationParams& params) {
    if (!params.enable_simplification || !geometry) {
        return false;
    }

    // Get vertex array
    osg::Vec3Array* vertexArray = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
    if (!vertexArray || vertexArray->empty()) {
        return false;
    }

    // Get index array (we need to handle different primitive set types)
    if (geometry->getNumPrimitiveSets() == 0) {
        return false;
    }

    // For now, we'll only handle the first primitive set
    osg::PrimitiveSet* primitiveSet = geometry->getPrimitiveSet(0);
    if (!primitiveSet) {
        return false;
    }

    // Get vertex attributes
    size_t vertex_count = vertexArray->size();

    // Get normals if available and should be preserved
    osg::Vec3Array* normalArray = dynamic_cast<osg::Vec3Array*>(geometry->getNormalArray());
    bool hasNormals = params.preserve_normals && normalArray && normalArray->size() == vertex_count;

    // Get texture coordinates if available and should be preserved
    osg::Vec2Array* texCoordArray = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
    bool hasTexCoords = params.preserve_texture_coords && texCoordArray && texCoordArray->size() == vertex_count;

    // Convert OSG vertex data to VertexData structure
    std::vector<VertexData> vertices(vertex_count);

    for (size_t i = 0; i < vertex_count; ++i) {
        // Position
        const osg::Vec3& vertex = vertexArray->at(i);
        vertices[i].x = vertex.x();
        vertices[i].y = vertex.y();
        vertices[i].z = vertex.z();

        // Normals
        if (hasNormals) {
            const osg::Vec3& normal = normalArray->at(i);
            vertices[i].nx = normal.x();
            vertices[i].ny = normal.y();
            vertices[i].nz = normal.z();
        } else {
            vertices[i].nx = 0.0f;
            vertices[i].ny = 0.0f;
            vertices[i].nz = 0.0f;
        }

        // Texture coordinates
        if (hasTexCoords) {
            const osg::Vec2& texcoord = texCoordArray->at(i);
            vertices[i].u = texcoord.x();
            vertices[i].v = texcoord.y();
        } else {
            vertices[i].u = 0.0f;
            vertices[i].v = 0.0f;
        }
    }

    // Handle different primitive set types
    std::vector<unsigned int> indices;
    size_t original_index_count = 0;

    switch (primitiveSet->getType()) {
        case osg::PrimitiveSet::DrawElementsUBytePrimitiveType: {
            const osg::DrawElementsUByte* drawElements = static_cast<const osg::DrawElementsUByte*>(primitiveSet);
            original_index_count = drawElements->size();
            indices.resize(original_index_count);
            for (size_t i = 0; i < original_index_count; ++i) {
                indices[i] = static_cast<unsigned int>(drawElements->at(i));
            }
            break;
        }
        case osg::PrimitiveSet::DrawElementsUShortPrimitiveType: {
            const osg::DrawElementsUShort* drawElements = static_cast<const osg::DrawElementsUShort*>(primitiveSet);
            original_index_count = drawElements->size();
            indices.resize(original_index_count);
            for (size_t i = 0; i < original_index_count; ++i) {
                indices[i] = static_cast<unsigned int>(drawElements->at(i));
            }
            break;
        }
        case osg::PrimitiveSet::DrawElementsUIntPrimitiveType: {
            const osg::DrawElementsUInt* drawElements = static_cast<const osg::DrawElementsUInt*>(primitiveSet);
            original_index_count = drawElements->size();
            indices.resize(original_index_count);
            for (size_t i = 0; i < original_index_count; ++i) {
                indices[i] = drawElements->at(i);
            }
            break;
        }
        case osg::PrimitiveSet::DrawArraysPrimitiveType: {
            // For DrawArrays, we need to create indices
            osg::DrawArrays* drawArrays = static_cast<osg::DrawArrays*>(primitiveSet);
            unsigned int first = drawArrays->getFirst();
            unsigned int count = drawArrays->getCount();
            original_index_count = count;
            indices.resize(count);
            for (unsigned int i = 0; i < count; ++i) {
                indices[i] = first + i;
            }
            break;
        }
        default:
            // Unsupported primitive type
            return false;
    }

    // Calculate target index count based on ratio
    size_t target_index_count = static_cast<size_t>(original_index_count * params.target_ratio);

    // Use the extracted optimization and simplification function
    std::vector<unsigned int> simplified_indices;
    size_t simplified_index_count = 0;

    if (!optimize_and_simplify_mesh(
            vertices, vertex_count,
            indices, original_index_count,
            simplified_indices, simplified_index_count,
            params)) {
        return false;
    }

    osg::ref_ptr<osg::Vec3Array> newVertexArray = new osg::Vec3Array();
    newVertexArray->reserve(vertex_count);

    for (size_t i = 0; i < vertex_count; ++i) {
        newVertexArray->push_back(osg::Vec3(vertices[i].x, vertices[i].y, vertices[i].z));
    }
    geometry->setVertexArray(newVertexArray);

    // Update normals if they exist
    if (hasNormals) {
        osg::ref_ptr<osg::Vec3Array> newNormalArray = new osg::Vec3Array();
        newNormalArray->reserve(vertex_count);

        for (size_t i = 0; i < vertex_count; ++i) {
            newNormalArray->push_back(osg::Vec3(vertices[i].nx, vertices[i].ny, vertices[i].nz));
        }
        geometry->setNormalArray(newNormalArray);
        geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    // Update texture coordinates if they exist
    if (hasTexCoords) {
        osg::ref_ptr<osg::Vec2Array> newTexCoordArray = new osg::Vec2Array();
        newTexCoordArray->reserve(vertex_count);

        for (size_t i = 0; i < vertex_count; ++i) {
            newTexCoordArray->push_back(osg::Vec2(vertices[i].u, vertices[i].v));
        }
        geometry->setTexCoordArray(0, newTexCoordArray);
    }

    // Create new primitive set with simplified indices
    switch (primitiveSet->getType()) {
        case osg::PrimitiveSet::DrawElementsUBytePrimitiveType: {
            osg::DrawElementsUByte* newDrawElements = new osg::DrawElementsUByte(primitiveSet->getMode());
            for (size_t i = 0; i < simplified_index_count; ++i) {
                newDrawElements->push_back(static_cast<osg::DrawElementsUByte::value_type>(simplified_indices[i]));
            }
            geometry->setPrimitiveSet(0, newDrawElements);
            break;
        }
        case osg::PrimitiveSet::DrawElementsUShortPrimitiveType: {
            osg::DrawElementsUShort* newDrawElements = new osg::DrawElementsUShort(primitiveSet->getMode());
            for (size_t i = 0; i < simplified_index_count; ++i) {
                newDrawElements->push_back(static_cast<osg::DrawElementsUShort::value_type>(simplified_indices[i]));
            }
            geometry->setPrimitiveSet(0, newDrawElements);
            break;
        }
        case osg::PrimitiveSet::DrawElementsUIntPrimitiveType: {
            osg::DrawElementsUInt* newDrawElements = new osg::DrawElementsUInt(primitiveSet->getMode());
            for (size_t i = 0; i < simplified_index_count; ++i) {
                newDrawElements->push_back(simplified_indices[i]);
            }
            geometry->setPrimitiveSet(0, newDrawElements);
            break;
        }
        case osg::PrimitiveSet::DrawArraysPrimitiveType: {
            // For DrawArrays, we need to create a new DrawElements
            osg::DrawElementsUInt* newDrawElements = new osg::DrawElementsUInt(primitiveSet->getMode());
            for (size_t i = 0; i < simplified_index_count; ++i) {
                newDrawElements->push_back(simplified_indices[i]);
            }
            geometry->setPrimitiveSet(0, newDrawElements);
            break;
        }
    }

    return true;
}

// Function to compress mesh geometry using Draco
bool compress_mesh_geometry(osg::Geometry* geometry, const DracoCompressionParams& params,
                           std::vector<unsigned char>& compressed_data, size_t& compressed_size,
                           int* out_position_att_id, int* out_normal_att_id,
                           int* out_texcoord_att_id, int* out_batchid_att_id,
                           const std::vector<float>* batchIds) {
    if (!params.enable_compression || !geometry) {
        return false;
    }

    // Get vertex array
    osg::Vec3Array* vertexArray = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
    if (!vertexArray || vertexArray->empty()) {
        return false;
    }

    // Create Draco mesh
    std::unique_ptr<draco::Mesh> dracoMesh(new draco::Mesh());

    // Set up vertices
    const size_t vertexCount = vertexArray->size();
    dracoMesh->set_num_points(vertexCount);

    // Add position attribute
    draco::GeometryAttribute posAttr;
    posAttr.Init(draco::GeometryAttribute::POSITION, nullptr, 3, draco::DT_FLOAT32, false, sizeof(float) * 3, 0);
    int posAttId = dracoMesh->AddAttribute(posAttr, true, vertexCount);
    if (out_position_att_id) *out_position_att_id = posAttId;

    // Copy vertex positions
    for (size_t i = 0; i < vertexCount; ++i) {
        const osg::Vec3& vertex = vertexArray->at(i);
        const float pos[3] = { static_cast<float>(vertex.x()), static_cast<float>(vertex.y()), static_cast<float>(vertex.z()) };
        dracoMesh->attribute(posAttId)->SetAttributeValue(draco::AttributeValueIndex(i), &pos[0]);
    }

    // Handle normals if present
    osg::Vec3Array* normalArray = dynamic_cast<osg::Vec3Array*>(geometry->getNormalArray());
    if (normalArray && normalArray->size() == vertexCount) {
        draco::GeometryAttribute normalAttr;
        normalAttr.Init(draco::GeometryAttribute::NORMAL, nullptr, 3, draco::DT_FLOAT32, false, sizeof(float) * 3, 0);
        int normalAttId = dracoMesh->AddAttribute(normalAttr, true, vertexCount);
        if (out_normal_att_id) *out_normal_att_id = normalAttId;

        // Copy normals
        for (size_t i = 0; i < vertexCount; ++i) {
            const osg::Vec3& normal = normalArray->at(i);
            const float norm[3] = { static_cast<float>(normal.x()), static_cast<float>(normal.y()), static_cast<float>(normal.z()) };
            dracoMesh->attribute(normalAttId)->SetAttributeValue(draco::AttributeValueIndex(i), &norm[0]);
        }
    }

    // Handle texture coordinates if present
    osg::Vec2Array* texCoordArray = dynamic_cast<osg::Vec2Array*>(geometry->getTexCoordArray(0));
    if (texCoordArray && texCoordArray->size() == vertexCount) {
        draco::GeometryAttribute uvAttr;
        uvAttr.Init(draco::GeometryAttribute::TEX_COORD, nullptr, 2, draco::DT_FLOAT32, false, sizeof(float) * 2, 0);
        int uvAttId = dracoMesh->AddAttribute(uvAttr, true, vertexCount);
        if (out_texcoord_att_id) *out_texcoord_att_id = uvAttId;

        for (size_t i = 0; i < vertexCount; ++i) {
            const osg::Vec2& uv = texCoordArray->at(i);
            const float tex[2] = { static_cast<float>(uv.x()), static_cast<float>(uv.y()) };
            dracoMesh->attribute(uvAttId)->SetAttributeValue(draco::AttributeValueIndex(i), &tex[0]);
        }
    }

    // Handle Batch IDs if present
    if (batchIds && batchIds->size() == vertexCount) {
        draco::GeometryAttribute batchIdAttr;
        batchIdAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1, draco::DT_FLOAT32, false, sizeof(float), 0);
        int batchIdAttId = dracoMesh->AddAttribute(batchIdAttr, true, vertexCount);
        if (out_batchid_att_id) *out_batchid_att_id = batchIdAttId;

        for (size_t i = 0; i < vertexCount; ++i) {
            float val = (*batchIds)[i];
            dracoMesh->attribute(batchIdAttId)->SetAttributeValue(draco::AttributeValueIndex(i), &val);
        }
    }

    // Handle primitive sets (indices)
    if (geometry->getNumPrimitiveSets() > 0) {
        osg::PrimitiveSet* primitiveSet = geometry->getPrimitiveSet(0);
        unsigned int numIndices = primitiveSet->getNumIndices();

        if (numIndices > 0) {
            // Create faces for the mesh
            std::vector<uint32_t> indices(numIndices);
            for (unsigned int i = 0; i < numIndices; ++i) {
                indices[i] = primitiveSet->index(i);
            }

            // Convert triangle list to faces
            const size_t faceCount = numIndices / 3;
            dracoMesh->SetNumFaces(faceCount);

            for (size_t i = 0; i < faceCount; ++i) {
                draco::Mesh::Face face;
                face[0] = indices[i * 3];
                face[1] = indices[i * 3 + 1];
                face[2] = indices[i * 3 + 2];
                dracoMesh->SetFace(draco::FaceIndex(i), face);
            }
        }
    }

    // Encode the mesh
    draco::Encoder encoder;

    // Set encoding options
    encoder.SetSpeedOptions(5, 5); // Default speed options
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, params.position_quantization_bits);

    if (normalArray) {
        encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, params.normal_quantization_bits);
    }
    if (texCoordArray) {
        encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, params.tex_coord_quantization_bits);
    }

    // Encode the mesh
    draco::EncoderBuffer buffer;
    draco::Status status = encoder.EncodeMeshToBuffer(*dracoMesh, &buffer);

    if (!status.ok()) {
        return false;
    }

    // Copy compressed data
    compressed_size = buffer.size();
    compressed_data.resize(compressed_size);
    std::memcpy(compressed_data.data(), buffer.data(), compressed_size);

    return true;
}