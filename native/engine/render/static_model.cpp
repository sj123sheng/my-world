#include "native/engine/render/static_model.h"

#ifdef OHOS_PLATFORM
#include "native/engine/render/shader_3d.h"
#endif

// skinned_model.cpp 已提供应用主 cgltf 实例。这里给静态解析器的内嵌实例重命名，
// 使简报中的独立宿主机编译命令无需额外链接源文件，同时避免应用链接重复符号。
#define cgltf_parse static_model_cgltf_parse
#define cgltf_parse_file static_model_cgltf_parse_file
#define cgltf_load_buffers static_model_cgltf_load_buffers
#define cgltf_load_buffer_base64 static_model_cgltf_load_buffer_base64
#define cgltf_decode_string static_model_cgltf_decode_string
#define cgltf_decode_uri static_model_cgltf_decode_uri
#define cgltf_validate static_model_cgltf_validate
#define cgltf_free static_model_cgltf_free
#define cgltf_node_transform_local static_model_cgltf_node_transform_local
#define cgltf_node_transform_world static_model_cgltf_node_transform_world
#define cgltf_buffer_view_data static_model_cgltf_buffer_view_data
#define cgltf_accessor_read_float static_model_cgltf_accessor_read_float
#define cgltf_accessor_read_uint static_model_cgltf_accessor_read_uint
#define cgltf_accessor_read_index static_model_cgltf_accessor_read_index
#define cgltf_num_components static_model_cgltf_num_components
#define cgltf_component_size static_model_cgltf_component_size
#define cgltf_calc_size static_model_cgltf_calc_size
#define cgltf_accessor_unpack_floats static_model_cgltf_accessor_unpack_floats
#define cgltf_accessor_unpack_indices static_model_cgltf_accessor_unpack_indices
#define cgltf_copy_extras_json static_model_cgltf_copy_extras_json
#define cgltf_mesh_index static_model_cgltf_mesh_index
#define cgltf_material_index static_model_cgltf_material_index
#define cgltf_accessor_index static_model_cgltf_accessor_index
#define cgltf_buffer_view_index static_model_cgltf_buffer_view_index
#define cgltf_buffer_index static_model_cgltf_buffer_index
#define cgltf_image_index static_model_cgltf_image_index
#define cgltf_texture_index static_model_cgltf_texture_index
#define cgltf_sampler_index static_model_cgltf_sampler_index
#define cgltf_skin_index static_model_cgltf_skin_index
#define cgltf_camera_index static_model_cgltf_camera_index
#define cgltf_light_index static_model_cgltf_light_index
#define cgltf_node_index static_model_cgltf_node_index
#define cgltf_scene_index static_model_cgltf_scene_index
#define cgltf_animation_index static_model_cgltf_animation_index
#define cgltf_animation_sampler_index static_model_cgltf_animation_sampler_index
#define cgltf_animation_channel_index static_model_cgltf_animation_channel_index
#define cgltf_parse_json static_model_cgltf_parse_json
#define CGLTF_IMPLEMENTATION
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "native/engine/render/stb_image.h"
#include "native/third_party/cgltf/cgltf.h"

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

namespace {

struct CgltfDeleter {
  void operator()(cgltf_data *data) const { cgltf_free(data); }
};

std::string assetPrefix(const std::string &assetName) {
  return (assetName.empty() ? "unnamed asset" : assetName) + ": ";
}

bool fail(const std::string &assetName, const std::string &detail,
          std::string &error) {
  error = assetPrefix(assetName) + detail;
  return false;
}

bool hasGlb2Header(const std::vector<uint8_t> &bytes) {
  if (bytes.size() < 12 || bytes[0] != 0x67 || bytes[1] != 0x6c ||
      bytes[2] != 0x54 || bytes[3] != 0x46) {
    return false;
  }
  uint32_t version = 0;
  std::memcpy(&version, bytes.data() + 4, sizeof(version));
  return version == 2u;
}

const cgltf_accessor *findAttribute(const cgltf_primitive &primitive,
                                    cgltf_attribute_type type, int index) {
  for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute &attribute = primitive.attributes[i];
    if (attribute.type == type && attribute.index == index) {
      return attribute.data;
    }
  }
  return nullptr;
}

bool validVertexAccessor(const cgltf_accessor *accessor, cgltf_type type) {
  return accessor != nullptr && !accessor->is_sparse &&
         accessor->type == type && accessor->buffer_view != nullptr &&
         accessor->buffer_view->buffer != nullptr &&
         accessor->buffer_view->buffer->data != nullptr;
}

bool validIndexAccessor(const cgltf_accessor *accessor) {
  if (accessor == nullptr || accessor->is_sparse ||
      accessor->type != cgltf_type_scalar || accessor->buffer_view == nullptr ||
      accessor->buffer_view->buffer == nullptr ||
      accessor->buffer_view->buffer->data == nullptr) {
    return false;
  }
  return accessor->component_type == cgltf_component_type_r_8u ||
         accessor->component_type == cgltf_component_type_r_16u ||
         accessor->component_type == cgltf_component_type_r_32u;
}

glm::mat4 matrixFromFloats(const cgltf_float *values) {
  glm::mat4 result(1.0f);
  for (std::size_t column = 0; column < 4; ++column) {
    for (std::size_t row = 0; row < 4; ++row) {
      result[column][row] = values[column * 4 + row];
    }
  }
  return result;
}

const cgltf_image *baseColorImage(const cgltf_primitive &primitive) {
  if (primitive.material == nullptr ||
      !primitive.material->has_pbr_metallic_roughness) {
    return nullptr;
  }
  const cgltf_texture_view &view =
      primitive.material->pbr_metallic_roughness.base_color_texture;
  return view.texture == nullptr ? nullptr : view.texture->image;
}

bool copyImage(const cgltf_image &image, std::vector<uint8_t> &output,
               const std::string &assetName, std::string &error) {
  if (image.uri != nullptr || image.buffer_view == nullptr) {
    return fail(assetName, "external buffer/image URI is unsupported", error);
  }
  const uint8_t *first = cgltf_buffer_view_data(image.buffer_view);
  if (first == nullptr || image.buffer_view->size == 0) {
    return fail(assetName, "embedded image data is unavailable", error);
  }
  output.assign(first, first + image.buffer_view->size);
  if (output.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return fail(assetName, "embedded image is too large", error);
  }
  int width = 0;
  int height = 0;
  int components = 0;
  if (stbi_info_from_memory(output.data(), static_cast<int>(output.size()),
                            &width, &height, &components) == 0 ||
      width <= 0 || height <= 0) {
    return fail(assetName, "could not decode embedded image", error);
  }
  return true;
}

bool parseStaticGlb(const std::vector<uint8_t> &bytes,
                    const std::string &assetName, std::vector<Mesh> &meshes,
                    std::vector<bool> &meshUsesTexture,
                    std::vector<Vertex> &testVertices,
                    std::vector<uint8_t> &fullTexture,
                    std::vector<uint8_t> &halfTexture, StaticModelStats &stats,
                    std::string &error) {
  if (!hasGlb2Header(bytes)) {
    return fail(assetName, "input must be GLB 2.0", error);
  }

  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  cgltf_data *parsedRaw = nullptr;
  const cgltf_result parseResult =
      cgltf_parse(&options, bytes.data(), bytes.size(), &parsedRaw);
  std::unique_ptr<cgltf_data, CgltfDeleter> parsed(parsedRaw);
  if (parseResult != cgltf_result_success || parsed == nullptr ||
      parsed->file_type != cgltf_file_type_glb) {
    return fail(assetName, "input must be GLB 2.0", error);
  }

  for (std::size_t i = 0; i < parsed->buffers_count; ++i) {
    if (parsed->buffers[i].uri != nullptr) {
      return fail(assetName, "external buffer/image URI is unsupported", error);
    }
  }
  for (std::size_t i = 0; i < parsed->images_count; ++i) {
    if (parsed->images[i].uri != nullptr ||
        parsed->images[i].buffer_view == nullptr) {
      return fail(assetName, "external buffer/image URI is unsupported", error);
    }
  }
  if (parsed->skins_count != 0) {
    return fail(assetName, "static environment must not contain skins", error);
  }
  if (parsed->animations_count != 0) {
    return fail(assetName, "static environment must not contain animations",
                error);
  }

  if (cgltf_load_buffers(&options, parsed.get(), nullptr) !=
          cgltf_result_success ||
      cgltf_validate(parsed.get()) != cgltf_result_success) {
    return fail(assetName, "invalid GLB structure or embedded buffer range",
                error);
  }

  const cgltf_image *fullImage = nullptr;
  const cgltf_image *halfImage = nullptr;
  for (std::size_t i = 0; i < parsed->images_count; ++i) {
    const cgltf_image &image = parsed->images[i];
    const std::string name = image.name == nullptr ? "" : image.name;
    if (name == "diffuse_full") {
      if (fullImage != nullptr) {
        return fail(assetName, "diffuse_full image record must be unique",
                    error);
      }
      fullImage = &image;
      if (!copyImage(image, fullTexture, assetName, error)) {
        return false;
      }
    }
    if (name == "diffuse_half") {
      if (halfImage != nullptr) {
        return fail(assetName, "diffuse_half image record must be unique",
                    error);
      }
      halfImage = &image;
      if (!copyImage(image, halfTexture, assetName, error)) {
        return false;
      }
    }
  }

  std::unordered_map<const cgltf_image *, std::size_t> mergedByImage;
  for (std::size_t nodeIndex = 0; nodeIndex < parsed->nodes_count;
       ++nodeIndex) {
    const cgltf_node &node = parsed->nodes[nodeIndex];
    if (node.mesh == nullptr) continue;

    cgltf_float worldValues[16]{};
    cgltf_node_transform_world(&node, worldValues);
    const glm::mat4 world = matrixFromFloats(worldValues);
    const glm::mat3 linear(world);
    const float determinant = glm::determinant(linear);
    if (!std::isfinite(determinant) ||
        std::fabs(determinant) <= std::numeric_limits<float>::epsilon()) {
      return fail(assetName, "node transform cannot transform normals", error);
    }
    const glm::mat3 normalTransform = glm::inverseTranspose(linear);

    for (std::size_t primitiveIndex = 0;
         primitiveIndex < node.mesh->primitives_count; ++primitiveIndex) {
      const cgltf_primitive &primitive = node.mesh->primitives[primitiveIndex];
      if (primitive.type != cgltf_primitive_type_triangles) {
        return fail(assetName, "primitive mode must be TRIANGLES", error);
      }
      const cgltf_accessor *positions =
          findAttribute(primitive, cgltf_attribute_type_position, 0);
      const cgltf_accessor *normals =
          findAttribute(primitive, cgltf_attribute_type_normal, 0);
      const cgltf_accessor *texcoords =
          findAttribute(primitive, cgltf_attribute_type_texcoord, 0);
      if (!validVertexAccessor(positions, cgltf_type_vec3)) {
        return fail(assetName, "POSITION is required and must be VEC3", error);
      }
      if (!validVertexAccessor(normals, cgltf_type_vec3)) {
        return fail(assetName, "NORMAL is required and must be VEC3", error);
      }
      if (positions->count != normals->count) {
        return fail(assetName, "POSITION and NORMAL counts must match", error);
      }
      if (texcoords != nullptr &&
          (!validVertexAccessor(texcoords, cgltf_type_vec2) ||
           texcoords->count != positions->count)) {
        return fail(assetName, "TEXCOORD_0 must be VEC2 with matching count",
                    error);
      }
      if (!validIndexAccessor(primitive.indices) ||
          primitive.indices->count % 3u != 0u) {
        return fail(assetName,
                    "indices must be unsigned byte/short/int triangles", error);
      }

      const cgltf_image *image = baseColorImage(primitive);
      if (image != nullptr &&
          (image != fullImage || halfImage == nullptr ||
           fullTexture.empty() || halfTexture.empty())) {
        return fail(
            assetName,
            "base-color texture requires diffuse_full and diffuse_half images",
            error);
      }
      auto merged = mergedByImage.find(image);
      if (merged == mergedByImage.end()) {
        merged = mergedByImage.emplace(image, meshes.size()).first;
        meshes.emplace_back();
        meshUsesTexture.push_back(image != nullptr);
      }
      Mesh &mesh = meshes[merged->second];
      const uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());
      if (positions->count >
          static_cast<std::size_t>(std::numeric_limits<uint32_t>::max() -
                                   baseVertex)) {
        return fail(assetName, "merged vertex count exceeds uint32 range",
                    error);
      }
      mesh.vertices.reserve(mesh.vertices.size() + positions->count);
      for (std::size_t vertexIndex = 0; vertexIndex < positions->count;
           ++vertexIndex) {
        float position[3]{};
        float normal[3]{};
        float uv[2]{};
        if (cgltf_accessor_read_float(positions, vertexIndex, position, 3) ==
                0 ||
            cgltf_accessor_read_float(normals, vertexIndex, normal, 3) == 0 ||
            (texcoords != nullptr &&
             cgltf_accessor_read_float(texcoords, vertexIndex, uv, 2) == 0)) {
          return fail(assetName, "vertex accessor data is out of bounds",
                      error);
        }
        const glm::vec4 transformed =
            world * glm::vec4(position[0], position[1], position[2], 1.0f);
        const glm::vec3 transformedNormal = glm::normalize(
            normalTransform * glm::vec3(normal[0], normal[1], normal[2]));
        mesh.vertices.push_back({
            glm::vec3(transformed),
            transformedNormal,
            glm::vec2(uv[0], uv[1]),
        });
      }
      mesh.indices.reserve(mesh.indices.size() + primitive.indices->count);
      for (std::size_t index = 0; index < primitive.indices->count; ++index) {
        const std::size_t value =
            cgltf_accessor_read_index(primitive.indices, index);
        if (value >= positions->count) {
          return fail(assetName, "primitive index is out of bounds", error);
        }
        mesh.indices.push_back(baseVertex + static_cast<uint32_t>(value));
      }
      stats.triangleCount += primitive.indices->count / 3u;
    }
  }

  if (meshes.empty()) {
    return fail(assetName, "static environment contains no mesh primitives",
                error);
  }
  stats.primitiveCount = meshes.size();
  for (const Mesh &mesh : meshes) {
    testVertices.insert(testVertices.end(), mesh.vertices.begin(),
                        mesh.vertices.end());
  }
  return true;
}

}  // namespace

bool StaticModel::tryInitialize(const std::vector<uint8_t> &bytes,
                                const std::string &assetName) {
  destroy();

  std::vector<Mesh> meshes;
  std::vector<bool> meshUsesTexture;
  std::vector<Vertex> testVertices;
  std::vector<uint8_t> fullTexture;
  std::vector<uint8_t> halfTexture;
  StaticModelStats stats;
  std::string error;
  if (!parseStaticGlb(bytes, assetName, meshes, meshUsesTexture, testVertices,
                      fullTexture, halfTexture, stats, error)) {
    lastError_ = std::move(error);
    return false;
  }

  primitives_ = std::move(meshes);
  primitiveUsesTexture_ = std::move(meshUsesTexture);
  testVertices_ = std::move(testVertices);
  fullTextureBytes_ = std::move(fullTexture);
  halfTextureBytes_ = std::move(halfTexture);
  stats_ = stats;
  textureTier_ = StaticTextureTier::Full;
  stats_.textureBytes = fullTextureBytes_.size();
  textureDirty_ = !fullTextureBytes_.empty();
  ready_ = true;
  lastError_.clear();
  return true;
}

bool StaticModel::ready() const { return ready_; }

const std::string &StaticModel::lastError() const { return lastError_; }

const StaticModelStats &StaticModel::stats() const { return stats_; }

StaticTextureTier StaticModel::textureTier() const { return textureTier_; }

void StaticModel::setTextureTier(StaticTextureTier tier) {
  if (textureTier_ == tier) return;
  textureTier_ = tier;
  stats_.textureBytes = selectedTextureBytes().size();
  textureDirty_ = true;
}

void StaticModel::draw(Shader3D &shader) {
  if (!ready_) return;
#ifdef OHOS_PLATFORM
  for (Mesh &mesh : primitives_) {
    mesh.upload();
  }
  if (textureDirty_) {
    const std::vector<uint8_t> &bytes = selectedTextureBytes();
    if (bytes.empty()) {
      if (texture_ != 0u) {
        glDeleteTextures(1, &texture_);
        texture_ = 0u;
      }
      textureDirty_ = false;
    } else if (bytes.size() <=
               static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      int width = 0;
      int height = 0;
      int components = 0;
      stbi_uc *pixels =
          stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                &width, &height, &components, STBI_rgb_alpha);
      if (pixels != nullptr && width > 0 && height > 0) {
        unsigned int replacement = 0u;
        glGenTextures(1, &replacement);
        if (replacement != 0u) {
          glBindTexture(GL_TEXTURE_2D, replacement);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, pixels);
          glGenerateMipmap(GL_TEXTURE_2D);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glBindTexture(GL_TEXTURE_2D, 0);
          if (texture_ != 0u) {
            glDeleteTextures(1, &texture_);
          }
          texture_ = replacement;
          textureDirty_ = false;
        }
      }
      if (pixels != nullptr) {
        stbi_image_free(pixels);
      }
    }
  }
  shader.setSkinned(false);
  for (std::size_t i = 0; i < primitives_.size(); ++i) {
    Mesh &mesh = primitives_[i];
    const bool hasTexture = primitiveUsesTexture_[i] && texture_ != 0u;
    shader.setHasTexture(hasTexture);
    mesh.texture = hasTexture ? texture_ : 0u;
    mesh.draw();
  }
#else
  (void)shader;
#endif
}

void StaticModel::destroy() {
#ifdef OHOS_PLATFORM
  if (texture_ != 0u) {
    glDeleteTextures(1, &texture_);
  }
#endif
  texture_ = 0u;
  for (Mesh &mesh : primitives_) {
    mesh.destroy();
  }
  clearOwnedState();
}

void StaticModel::abandonGpuResources() {
  texture_ = 0u;
  for (Mesh &mesh : primitives_) {
    mesh.abandonGpuResources();
  }
  textureDirty_ = !selectedTextureBytes().empty();
}

const std::vector<Vertex> &StaticModel::cpuVerticesForTest() const {
  return testVertices_;
}

void StaticModel::clearOwnedState() {
  primitives_.clear();
  primitiveUsesTexture_.clear();
  testVertices_.clear();
  fullTextureBytes_.clear();
  halfTextureBytes_.clear();
  ready_ = false;
  lastError_.clear();
  stats_ = StaticModelStats{};
  textureTier_ = StaticTextureTier::Full;
  textureDirty_ = false;
}

const std::vector<uint8_t> &StaticModel::selectedTextureBytes() const {
  return textureTier_ == StaticTextureTier::Full ? fullTextureBytes_
                                                 : halfTextureBytes_;
}
