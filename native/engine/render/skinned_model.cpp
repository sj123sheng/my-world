// skinned_model.cpp: GLB 骨骼模型内存装载、动画采样和 GLES 运行时。

#include "native/engine/render/skinned_model.h"
#include "native/engine/render/mesh.h"
#include "native/engine/render/shader_3d.h"

#define CGLTF_IMPLEMENTATION
#include "native/third_party/cgltf/cgltf.h"

#include "native/engine/render/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

namespace {

std::size_t leftKeyframe(const std::vector<float>& times, float time) {
  if (time <= times.front()) {
    return 0;
  }
  if (time >= times.back()) {
    return times.size() - 1;
  }
  return static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), time) -
                                  times.begin() - 1);
}

float interpolationFactor(const std::vector<float>& times, std::size_t left, float time) {
  if (left + 1 >= times.size()) {
    return 0.0f;
  }
  const float duration = times[left + 1] - times[left];
  return duration > 0.0f ? (time - times[left]) / duration : 0.0f;
}

std::string assetPrefix(const std::string& assetName) {
  return (assetName.empty() ? "unnamed asset" : assetName) + std::string(": ");
}

std::string validationFailure(const GltfValidationInput& input, const char* detail) {
  return assetPrefix(input.assetName) + detail;
}

enum class ChannelPath {
  Translation,
  Rotation,
  Scale,
};

struct OwnedNode {
  int parent = -1;
  glm::vec3 translation{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
  glm::mat4 matrix{1.0f};
  bool hasMatrix = false;
};

struct OwnedChannel {
  std::size_t targetNode = 0;
  ChannelPath path = ChannelPath::Translation;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
  std::vector<float> times;
  std::vector<glm::vec3> vec3Values;
  std::vector<glm::quat> quatValues;
};

struct OwnedClip {
  std::string name;
  float duration = 0.0f;
  std::vector<OwnedChannel> channels;
};

struct PrimitiveRange {
  uint32_t firstIndex = 0;
  uint32_t indexCount = 0;
  int textureIndex = -1;
};

struct RuntimeData {
  std::vector<SkinnedVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<PrimitiveRange> primitives;
  std::vector<OwnedNode> nodes;
  std::vector<std::size_t> jointNodes;
  std::vector<glm::mat4> inverseBindMatrices;
  std::vector<OwnedClip> clips;
  std::vector<std::string> clipNames;
  std::vector<std::vector<uint8_t>> baseColorImages;
};

struct PoseNode {
  glm::vec3 translation{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
  glm::mat4 matrix{1.0f};
  bool hasMatrix = false;
};

struct CgltfDeleter {
  void operator()(cgltf_data* data) const { cgltf_free(data); }
};

bool fail(const std::string& assetName, const std::string& detail,
          std::string& error) {
  error = assetPrefix(assetName) + "SkinnedModel runtime loader: " + detail;
  return false;
}

const char* cgltfResultName(cgltf_result result) {
  switch (result) {
    case cgltf_result_data_too_short:
      return "asset data or accessor range is out of bounds";
    case cgltf_result_unknown_format:
      return "only GLB assets are supported";
    case cgltf_result_invalid_json:
      return "GLB JSON chunk is invalid";
    case cgltf_result_invalid_gltf:
      return "glTF structure is invalid";
    case cgltf_result_out_of_memory:
      return "out of memory while parsing GLB";
    case cgltf_result_legacy_gltf:
      return "glTF 1.x assets are unsupported";
    default:
      return "could not parse GLB asset";
  }
}

bool hasGlbMagic(const std::vector<uint8_t>& bytes) {
  return bytes.size() >= 4 && bytes[0] == 0x67 && bytes[1] == 0x6c &&
         bytes[2] == 0x54 && bytes[3] == 0x46;
}

std::size_t nodeIndex(const cgltf_data& data, const cgltf_node* node) {
  return static_cast<std::size_t>(node - data.nodes);
}

glm::mat4 matrixFromFloats(const cgltf_float* values) {
  glm::mat4 result(1.0f);
  for (std::size_t column = 0; column < 4; ++column) {
    for (std::size_t row = 0; row < 4; ++row) {
      result[column][row] = values[column * 4 + row];
    }
  }
  return result;
}

bool readFloat(const cgltf_accessor* accessor, std::size_t index,
               float* values, std::size_t count) {
  return accessor != nullptr &&
         cgltf_accessor_read_float(accessor, index, values, count) != 0;
}

const cgltf_accessor* findAttribute(const cgltf_primitive& primitive,
                                    cgltf_attribute_type type, int index) {
  for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute& attribute = primitive.attributes[i];
    if (attribute.type == type && attribute.index == index) return attribute.data;
  }
  return nullptr;
}

bool validateAccessor(const cgltf_accessor* accessor, cgltf_type type,
                      const std::vector<cgltf_component_type>& components,
                      const char* semantic, const std::string& assetName,
                      std::string& error) {
  if (accessor == nullptr) {
    return fail(assetName, std::string("missing required vertex attribute ") + semantic,
                error);
  }
  if (accessor->is_sparse) {
    return fail(assetName, std::string("sparse accessor is unsupported for ") + semantic,
                error);
  }
  if (accessor->type != type ||
      std::find(components.begin(), components.end(), accessor->component_type) ==
          components.end()) {
    return fail(assetName, std::string("invalid accessor type for ") + semantic, error);
  }
  if (accessor->buffer_view == nullptr || accessor->buffer_view->buffer == nullptr ||
      accessor->buffer_view->buffer->data == nullptr) {
    return fail(assetName, std::string("accessor data is unavailable for ") + semantic,
                error);
  }
  return true;
}

bool copyTextureImage(const cgltf_primitive& primitive, RuntimeData& output,
                      PrimitiveRange& range, const std::string& assetName,
                      std::string& error) {
  if (primitive.material == nullptr ||
      !primitive.material->has_pbr_metallic_roughness) {
    return true;
  }
  const cgltf_texture_view& textureView =
      primitive.material->pbr_metallic_roughness.base_color_texture;
  if (textureView.texcoord != 0) {
    return fail(assetName, "baseColorTexture texcoord must be 0", error);
  }
  const cgltf_texture* texture = textureView.texture;
  if (texture == nullptr || texture->image == nullptr) return true;
  const cgltf_image* image = texture->image;
  if (image->uri != nullptr) {
    return fail(assetName, "external image URI is unsupported", error);
  }
  if (image->buffer_view == nullptr) {
    return fail(assetName, "baseColor image must use an embedded bufferView", error);
  }
  const uint8_t* first = cgltf_buffer_view_data(image->buffer_view);
  if (first == nullptr || image->buffer_view->size == 0) {
    return fail(assetName, "embedded baseColor image data is unavailable", error);
  }
  std::vector<uint8_t> imageBytes(first, first + image->buffer_view->size);
  const auto existing = std::find(output.baseColorImages.begin(),
                                  output.baseColorImages.end(), imageBytes);
  if (existing == output.baseColorImages.end()) {
    output.baseColorImages.push_back(std::move(imageBytes));
    range.textureIndex = static_cast<int>(output.baseColorImages.size() - 1);
  } else {
    range.textureIndex = static_cast<int>(
        std::distance(output.baseColorImages.begin(), existing));
  }
  return true;
}

bool copyPrimitive(const cgltf_primitive& primitive, std::size_t jointCount,
                   RuntimeData& output, const std::string& assetName,
                   std::string& error) {
  if (primitive.type != cgltf_primitive_type_triangles) {
    return fail(assetName, "primitive mode must be TRIANGLES", error);
  }
  if (primitive.has_draco_mesh_compression || primitive.extensions_count != 0) {
    return fail(assetName, "Draco and glTF extensions are unsupported", error);
  }
  if (primitive.targets_count != 0) {
    return fail(assetName, "morph targets are unsupported", error);
  }

  for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute& attribute = primitive.attributes[i];
    const bool supported =
        (attribute.type == cgltf_attribute_type_position && attribute.index == 0) ||
        (attribute.type == cgltf_attribute_type_normal && attribute.index == 0) ||
        (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0) ||
        (attribute.type == cgltf_attribute_type_joints && attribute.index == 0) ||
        (attribute.type == cgltf_attribute_type_weights && attribute.index == 0);
    if (!supported) {
      if ((attribute.type == cgltf_attribute_type_joints ||
           attribute.type == cgltf_attribute_type_weights) &&
          attribute.index == 1) {
        return fail(assetName, "JOINTS_1/WEIGHTS_1 are unsupported", error);
      }
      return fail(assetName, "unsupported vertex attribute in primitive", error);
    }
  }

  const cgltf_accessor* positions =
      findAttribute(primitive, cgltf_attribute_type_position, 0);
  const cgltf_accessor* normals =
      findAttribute(primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* texcoords =
      findAttribute(primitive, cgltf_attribute_type_texcoord, 0);
  const cgltf_accessor* joints =
      findAttribute(primitive, cgltf_attribute_type_joints, 0);
  const cgltf_accessor* weights =
      findAttribute(primitive, cgltf_attribute_type_weights, 0);
  if (!validateAccessor(positions, cgltf_type_vec3, {cgltf_component_type_r_32f},
                        "POSITION", assetName, error) ||
      !validateAccessor(normals, cgltf_type_vec3, {cgltf_component_type_r_32f},
                        "NORMAL", assetName, error) ||
      !validateAccessor(texcoords, cgltf_type_vec2, {cgltf_component_type_r_32f},
                        "TEXCOORD_0", assetName, error) ||
      !validateAccessor(joints, cgltf_type_vec4,
                        {cgltf_component_type_r_8u, cgltf_component_type_r_16u},
                        "JOINTS_0", assetName, error) ||
      !validateAccessor(weights, cgltf_type_vec4,
                        {cgltf_component_type_r_32f, cgltf_component_type_r_8u,
                         cgltf_component_type_r_16u},
                        "WEIGHTS_0", assetName, error)) {
    return false;
  }
  if ((weights->component_type == cgltf_component_type_r_8u ||
       weights->component_type == cgltf_component_type_r_16u) &&
      !weights->normalized) {
    return fail(assetName, "integer WEIGHTS_0 accessor must be normalized", error);
  }
  const std::size_t vertexCount = positions->count;
  if (vertexCount == 0 || normals->count != vertexCount ||
      texcoords->count != vertexCount || joints->count != vertexCount ||
      weights->count != vertexCount) {
    return fail(assetName, "vertex attribute counts must match", error);
  }

  const uint32_t baseVertex = static_cast<uint32_t>(output.vertices.size());
  output.vertices.reserve(output.vertices.size() + vertexCount);
  for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
    float p[3]{};
    float n[3]{};
    float uv[2]{};
    float w[4]{};
    cgltf_uint j[4]{};
    if (!readFloat(positions, vertexIndex, p, 3) ||
        !readFloat(normals, vertexIndex, n, 3) ||
        !readFloat(texcoords, vertexIndex, uv, 2) ||
        !readFloat(weights, vertexIndex, w, 4) ||
        !cgltf_accessor_read_uint(joints, vertexIndex, j, 4)) {
      return fail(assetName, "vertex accessor read is out of bounds", error);
    }
    const float totalWeight = w[0] + w[1] + w[2] + w[3];
    if (!std::isfinite(totalWeight) || totalWeight <= 0.0f) {
      return fail(assetName, "vertex weights sum to zero", error);
    }
    for (std::size_t influence = 0; influence < 4; ++influence) {
      if (j[influence] >= jointCount) {
        return fail(assetName, "vertex joint index exceeds skin joint count", error);
      }
      w[influence] /= totalWeight;
    }
    output.vertices.push_back({
        glm::vec3(p[0], p[1], p[2]),
        glm::vec3(n[0], n[1], n[2]),
        glm::vec2(uv[0], uv[1]),
        glm::uvec4(j[0], j[1], j[2], j[3]),
        glm::vec4(w[0], w[1], w[2], w[3]),
    });
  }

  PrimitiveRange range;
  range.firstIndex = static_cast<uint32_t>(output.indices.size());
  if (primitive.indices != nullptr) {
    const cgltf_accessor* accessor = primitive.indices;
    if (accessor->is_sparse || accessor->type != cgltf_type_scalar ||
        (accessor->component_type != cgltf_component_type_r_8u &&
         accessor->component_type != cgltf_component_type_r_16u &&
         accessor->component_type != cgltf_component_type_r_32u)) {
      return fail(assetName, "invalid primitive index accessor", error);
    }
    for (std::size_t i = 0; i < accessor->count; ++i) {
      const std::size_t index = cgltf_accessor_read_index(accessor, i);
      if (index >= vertexCount) {
        return fail(assetName, "primitive index is out of bounds", error);
      }
      output.indices.push_back(baseVertex + static_cast<uint32_t>(index));
    }
    range.indexCount = static_cast<uint32_t>(accessor->count);
  } else {
    for (std::size_t i = 0; i < vertexCount; ++i) {
      output.indices.push_back(baseVertex + static_cast<uint32_t>(i));
    }
    range.indexCount = static_cast<uint32_t>(vertexCount);
  }
  if (range.indexCount == 0 || range.indexCount % 3 != 0) {
    return fail(assetName, "TRIANGLES index count must be a non-zero multiple of 3", error);
  }
  if (!copyTextureImage(primitive, output, range, assetName, error)) return false;
  output.primitives.push_back(range);
  return true;
}

bool copyNodes(const cgltf_data& data, RuntimeData& output,
               const std::string& assetName, std::string& error) {
  output.nodes.resize(data.nodes_count);
  for (std::size_t i = 0; i < data.nodes_count; ++i) {
    const cgltf_node& source = data.nodes[i];
    OwnedNode& node = output.nodes[i];
    if (source.parent != nullptr) {
      const std::size_t parent = nodeIndex(data, source.parent);
      if (parent >= data.nodes_count) {
        return fail(assetName, "node parent is out of bounds", error);
      }
      node.parent = static_cast<int>(parent);
    }
    if (source.has_translation) {
      node.translation = glm::vec3(source.translation[0], source.translation[1],
                                   source.translation[2]);
    }
    if (source.has_rotation) {
      node.rotation = glm::normalize(glm::quat(source.rotation[3], source.rotation[0],
                                                source.rotation[1], source.rotation[2]));
    }
    if (source.has_scale) {
      node.scale = glm::vec3(source.scale[0], source.scale[1], source.scale[2]);
    }
    if (source.has_matrix) {
      node.matrix = matrixFromFloats(source.matrix);
      node.hasMatrix = true;
    }
  }
  return true;
}

bool copySkin(const cgltf_data& data, RuntimeData& output,
              const std::string& assetName, std::string& error) {
  if (data.skins_count != 1) {
    return fail(assetName, "exactly one skin is required", error);
  }
  const cgltf_skin& skin = data.skins[0];
  if (skin.joints_count == 0) {
    return fail(assetName, "skin must contain at least one joint", error);
  }
  if (skin.joints_count > kMaxSkinJoints) {
    return fail(assetName, "joint count exceeds 64", error);
  }
  output.jointNodes.reserve(skin.joints_count);
  for (std::size_t i = 0; i < skin.joints_count; ++i) {
    const std::size_t index = nodeIndex(data, skin.joints[i]);
    if (index >= data.nodes_count) {
      return fail(assetName, "skin joint node is out of bounds", error);
    }
    output.jointNodes.push_back(index);
  }

  output.inverseBindMatrices.assign(skin.joints_count, glm::mat4(1.0f));
  if (skin.inverse_bind_matrices == nullptr) return true;
  const cgltf_accessor* accessor = skin.inverse_bind_matrices;
  if (accessor->is_sparse || accessor->type != cgltf_type_mat4 ||
      accessor->component_type != cgltf_component_type_r_32f ||
      accessor->count != skin.joints_count) {
    return fail(assetName, "invalid inverse bind matrix accessor", error);
  }
  for (std::size_t i = 0; i < skin.joints_count; ++i) {
    float values[16]{};
    if (!readFloat(accessor, i, values, 16)) {
      return fail(assetName, "inverse bind matrix accessor is out of bounds", error);
    }
    output.inverseBindMatrices[i] = matrixFromFloats(values);
  }
  return true;
}

bool copyAnimations(const cgltf_data& data, RuntimeData& output,
                    const std::string& assetName, std::string& error) {
  output.clips.reserve(data.animations_count);
  output.clipNames.reserve(data.animations_count);
  for (std::size_t animationIndex = 0; animationIndex < data.animations_count;
       ++animationIndex) {
    const cgltf_animation& animation = data.animations[animationIndex];
    OwnedClip clip;
    clip.name = animation.name == nullptr ? std::string{} : animation.name;
    clip.channels.reserve(animation.channels_count);
    for (std::size_t channelIndex = 0; channelIndex < animation.channels_count;
         ++channelIndex) {
      const cgltf_animation_channel& source = animation.channels[channelIndex];
      if (source.sampler == nullptr || source.target_node == nullptr) {
        return fail(assetName, "animation channel is missing sampler or target", error);
      }
      if (source.sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
        return fail(assetName,
                    "animation interpolation CUBICSPLINE is unsupported", error);
      }
      if (source.sampler->interpolation != cgltf_interpolation_type_linear &&
          source.sampler->interpolation != cgltf_interpolation_type_step) {
        return fail(assetName, "animation interpolation is unsupported", error);
      }
      const cgltf_accessor* input = source.sampler->input;
      const cgltf_accessor* values = source.sampler->output;
      if (input == nullptr || values == nullptr || input->is_sparse ||
          values->is_sparse || input->component_type != cgltf_component_type_r_32f ||
          input->type != cgltf_type_scalar || input->count == 0 ||
          values->count != input->count) {
        return fail(assetName, "invalid animation sampler accessors", error);
      }
      OwnedChannel channel;
      channel.targetNode = nodeIndex(data, source.target_node);
      if (channel.targetNode >= output.nodes.size()) {
        return fail(assetName, "animation target node is out of bounds", error);
      }
      if (output.nodes[channel.targetNode].hasMatrix) {
        return fail(assetName, "animation cannot target a matrix node", error);
      }
      channel.interpolation = source.sampler->interpolation == cgltf_interpolation_type_step
                                  ? AnimationInterpolation::Step
                                  : AnimationInterpolation::Linear;
      channel.times.resize(input->count);
      for (std::size_t key = 0; key < input->count; ++key) {
        if (!readFloat(input, key, &channel.times[key], 1) ||
            !std::isfinite(channel.times[key]) ||
            (key != 0 && channel.times[key] <= channel.times[key - 1])) {
          return fail(assetName, "animation keyframe times must be finite and increasing",
                      error);
        }
      }
      clip.duration = std::max(clip.duration, channel.times.back());

      if (source.target_path == cgltf_animation_path_type_translation ||
          source.target_path == cgltf_animation_path_type_scale) {
        if (values->type != cgltf_type_vec3 ||
            values->component_type != cgltf_component_type_r_32f) {
          return fail(assetName, "animation TRS output accessor has invalid type", error);
        }
        channel.path = source.target_path == cgltf_animation_path_type_translation
                           ? ChannelPath::Translation
                           : ChannelPath::Scale;
        channel.vec3Values.resize(values->count);
        for (std::size_t key = 0; key < values->count; ++key) {
          float value[3]{};
          if (!readFloat(values, key, value, 3)) {
            return fail(assetName, "animation output accessor is out of bounds", error);
          }
          channel.vec3Values[key] = glm::vec3(value[0], value[1], value[2]);
        }
      } else if (source.target_path == cgltf_animation_path_type_rotation) {
        if (values->type != cgltf_type_vec4 ||
            values->component_type != cgltf_component_type_r_32f) {
          return fail(assetName, "animation rotation output accessor has invalid type", error);
        }
        channel.path = ChannelPath::Rotation;
        channel.quatValues.resize(values->count);
        for (std::size_t key = 0; key < values->count; ++key) {
          float value[4]{};
          if (!readFloat(values, key, value, 4)) {
            return fail(assetName, "animation output accessor is out of bounds", error);
          }
          const glm::quat rotation(value[3], value[0], value[1], value[2]);
          if (glm::length(rotation) <= std::numeric_limits<float>::epsilon()) {
            return fail(assetName, "animation rotation quaternion is invalid", error);
          }
          channel.quatValues[key] = glm::normalize(rotation);
        }
      } else {
        return fail(assetName, "only translation/rotation/scale animation channels are supported",
                    error);
      }
      clip.channels.push_back(std::move(channel));
    }
    output.clipNames.push_back(clip.name);
    output.clips.push_back(std::move(clip));
  }
  return true;
}

bool copyMeshes(const cgltf_data& data, RuntimeData& output,
                const std::string& assetName, std::string& error) {
  const cgltf_skin* skin = &data.skins[0];
  bool foundSkinnedMesh = false;
  for (std::size_t node = 0; node < data.nodes_count; ++node) {
    if (data.nodes[node].mesh == nullptr || data.nodes[node].skin != skin) continue;
    foundSkinnedMesh = true;
    const cgltf_mesh& mesh = *data.nodes[node].mesh;
    for (std::size_t primitive = 0; primitive < mesh.primitives_count; ++primitive) {
      if (!copyPrimitive(mesh.primitives[primitive], output.jointNodes.size(), output,
                         assetName, error)) {
        return false;
      }
    }
  }
  if (!foundSkinnedMesh || output.primitives.empty()) {
    return fail(assetName, "no mesh node references the skin", error);
  }
  return true;
}

bool parseGlb(const std::vector<uint8_t>& bytes, const std::string& assetName,
              RuntimeData& output, std::string& error) {
  if (!hasGlbMagic(bytes)) {
    return fail(assetName, "only GLB assets are supported", error);
  }
  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  cgltf_data* parsedRaw = nullptr;
  const cgltf_result parseResult =
      cgltf_parse(&options, bytes.data(), bytes.size(), &parsedRaw);
  std::unique_ptr<cgltf_data, CgltfDeleter> parsed(parsedRaw);
  if (parseResult != cgltf_result_success) {
    return fail(assetName, cgltfResultName(parseResult), error);
  }
  if (parsed->file_type != cgltf_file_type_glb) {
    return fail(assetName, "only GLB assets are supported", error);
  }
  if (parsed->extensions_used_count != 0 || parsed->extensions_required_count != 0 ||
      parsed->data_extensions_count != 0) {
    return fail(assetName, "Draco and glTF extensions are unsupported", error);
  }
  for (std::size_t i = 0; i < parsed->buffers_count; ++i) {
    if (parsed->buffers[i].uri != nullptr) {
      return fail(assetName, "external buffer URI is unsupported", error);
    }
  }
  for (std::size_t i = 0; i < parsed->images_count; ++i) {
    if (parsed->images[i].uri != nullptr) {
      return fail(assetName, "external image URI is unsupported", error);
    }
  }
  for (std::size_t i = 0; i < parsed->accessors_count; ++i) {
    if (parsed->accessors[i].is_sparse) {
      return fail(assetName, "sparse accessors are unsupported", error);
    }
  }
  const cgltf_result loadResult = cgltf_load_buffers(&options, parsed.get(), nullptr);
  if (loadResult != cgltf_result_success) {
    return fail(assetName, cgltfResultName(loadResult), error);
  }
  // cgltf_validate 会先按 CUBICSPLINE 的 3 倍输出规则拒绝数据。先识别项目明确
  // 不支持的插值，确保错误能指出资产名和真正的不兼容原因。
  for (std::size_t animation = 0; animation < parsed->animations_count; ++animation) {
    for (std::size_t sampler = 0;
         sampler < parsed->animations[animation].samplers_count; ++sampler) {
      if (parsed->animations[animation].samplers[sampler].interpolation ==
          cgltf_interpolation_type_cubic_spline) {
        return fail(assetName,
                    "animation interpolation CUBICSPLINE is unsupported", error);
      }
    }
  }
  const cgltf_result validateResult = cgltf_validate(parsed.get());
  if (validateResult != cgltf_result_success) {
    return fail(assetName, cgltfResultName(validateResult), error);
  }
  if (!copyNodes(*parsed, output, assetName, error) ||
      !copySkin(*parsed, output, assetName, error) ||
      !copyMeshes(*parsed, output, assetName, error) ||
      !copyAnimations(*parsed, output, assetName, error)) {
    return false;
  }
  return true;
}

glm::mat4 composeNode(const PoseNode& node) {
  if (node.hasMatrix) return node.matrix;
  return glm::translate(glm::mat4(1.0f), node.translation) *
         glm::mat4_cast(node.rotation) * glm::scale(glm::mat4(1.0f), node.scale);
}

float wrappedClipTime(float seconds, float duration) {
  return duration > 0.0f ? WrapAnimationTime(seconds, duration) : 0.0f;
}

std::vector<PoseNode> defaultPose(const RuntimeData& data) {
  std::vector<PoseNode> pose(data.nodes.size());
  for (std::size_t i = 0; i < data.nodes.size(); ++i) {
    pose[i].translation = data.nodes[i].translation;
    pose[i].rotation = data.nodes[i].rotation;
    pose[i].scale = data.nodes[i].scale;
    pose[i].matrix = data.nodes[i].matrix;
    pose[i].hasMatrix = data.nodes[i].hasMatrix;
  }
  return pose;
}

std::vector<PoseNode> samplePose(const RuntimeData& data, int clipIndex,
                                 float time) {
  std::vector<PoseNode> pose = defaultPose(data);
  if (clipIndex < 0 || static_cast<std::size_t>(clipIndex) >= data.clips.size()) {
    return pose;
  }
  const OwnedClip& clip = data.clips[static_cast<std::size_t>(clipIndex)];
  const float sampleTime = wrappedClipTime(time, clip.duration);
  for (const OwnedChannel& channel : clip.channels) {
    if (channel.path == ChannelPath::Rotation) {
      AnimationChannel<glm::quat> source{channel.times, channel.quatValues,
                                         channel.interpolation};
      pose[channel.targetNode].rotation = SampleQuat(source, sampleTime);
    } else {
      AnimationChannel<glm::vec3> source{channel.times, channel.vec3Values,
                                         channel.interpolation};
      const glm::vec3 value = SampleVec3(source, sampleTime);
      if (channel.path == ChannelPath::Translation) {
        pose[channel.targetNode].translation = value;
      } else {
        pose[channel.targetNode].scale = value;
      }
    }
  }
  return pose;
}

void blendPoses(std::vector<PoseNode>& current,
                const std::vector<PoseNode>& previous, float factor) {
  const float t = std::clamp(factor, 0.0f, 1.0f);
  for (std::size_t i = 0; i < current.size() && i < previous.size(); ++i) {
    if (current[i].hasMatrix || previous[i].hasMatrix) continue;
    current[i].translation = glm::mix(previous[i].translation,
                                      current[i].translation, t);
    current[i].rotation = glm::slerp(previous[i].rotation, current[i].rotation, t);
    current[i].scale = glm::mix(previous[i].scale, current[i].scale, t);
  }
}

SkinPalette buildRuntimePalette(const RuntimeData& data,
                                const std::vector<PoseNode>& pose) {
  SkinPalette palette;
  if (pose.size() != data.nodes.size() ||
      data.jointNodes.size() != data.inverseBindMatrices.size()) {
    return palette;
  }
  std::vector<glm::mat4> globals(data.nodes.size(), glm::mat4(1.0f));
  std::vector<uint8_t> states(data.nodes.size(), 0);
  std::function<bool(std::size_t)> resolve = [&](std::size_t nodeIndexValue) {
    if (states[nodeIndexValue] == 2) return true;
    if (states[nodeIndexValue] == 1) return false;
    states[nodeIndexValue] = 1;
    const int parent = data.nodes[nodeIndexValue].parent;
    const glm::mat4 local = composeNode(pose[nodeIndexValue]);
    if (parent == -1) {
      globals[nodeIndexValue] = local;
    } else if (parent >= 0 && static_cast<std::size_t>(parent) < data.nodes.size() &&
               resolve(static_cast<std::size_t>(parent))) {
      globals[nodeIndexValue] = globals[static_cast<std::size_t>(parent)] * local;
    } else {
      return false;
    }
    states[nodeIndexValue] = 2;
    return true;
  };

  palette.matrices.resize(data.jointNodes.size());
  for (std::size_t joint = 0; joint < data.jointNodes.size(); ++joint) {
    const std::size_t node = data.jointNodes[joint];
    if (node >= data.nodes.size() || !resolve(node)) {
      palette.matrices.clear();
      return palette;
    }
    palette.matrices[joint] = globals[node] * data.inverseBindMatrices[joint];
  }
  return palette;
}

int findClip(const RuntimeData& data, const std::string& name) {
  for (std::size_t i = 0; i < data.clips.size(); ++i) {
    if (data.clips[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

bool isLocomotionClip(const std::string& name) {
  return name == "idle" || name == "run";
}

}  // namespace

struct SkinnedModel::Impl {
  RuntimeData data;
  bool ready = false;
  std::string lastError;
  uint64_t assetRevision = 0;
  unsigned int vbo = 0;
  unsigned int ibo = 0;
  std::vector<unsigned int> textures;
};

void SkinnedAnimationState::reset() {
  owner = nullptr;
  assetRevision = 0;
  currentClip = -1;
  previousClip = -1;
  currentTime = 0.0f;
  previousTime = 0.0f;
  blendElapsed = 0.0f;
}

bool ValidateGltf(const GltfValidationInput& input, std::string& reason) {
  if (input.assetFormat != GltfAssetFormat::Glb) {
    reason = validationFailure(input, "only GLB assets are supported");
    return false;
  }
  if (input.primitiveMode != GltfPrimitiveMode::Triangles) {
    reason = validationFailure(input, "primitive mode must be TRIANGLES");
    return false;
  }
  if (input.jointCount > kMaxSkinJoints) {
    reason = validationFailure(input, "joint count exceeds 64");
    return false;
  }
  if (!input.hasPosition) {
    reason = validationFailure(input, "missing required vertex attribute POSITION");
    return false;
  }
  if (!input.hasNormal) {
    reason = validationFailure(input, "missing required vertex attribute NORMAL");
    return false;
  }
  if (!input.hasTexcoord0) {
    reason = validationFailure(input, "missing required vertex attribute TEXCOORD_0");
    return false;
  }
  if (!input.hasJoints0) {
    reason = validationFailure(input, "missing required vertex attribute JOINTS_0");
    return false;
  }
  if (!input.hasWeights0) {
    reason = validationFailure(input, "missing required vertex attribute WEIGHTS_0");
    return false;
  }
  if (input.hasJoints1 || input.hasWeights1) {
    reason = validationFailure(input, "JOINTS_1/WEIGHTS_1 are unsupported");
    return false;
  }
  if (input.maxVertexInfluences > 4) {
    reason = validationFailure(input, "vertex influence count exceeds 4");
    return false;
  }
  if (input.hasCubicSpline) {
    reason = validationFailure(input, "animation interpolation CUBICSPLINE is unsupported");
    return false;
  }
  if (!input.singleSkin) {
    reason = validationFailure(input, "exactly one skin is required");
    return false;
  }
  reason.clear();
  return true;
}

float WrapAnimationTime(float seconds, float duration) {
  return duration > 0.0f ? std::fmod(std::max(seconds, 0.0f), duration) : 0.0f;
}

glm::vec3 SampleVec3(const AnimationChannel<glm::vec3>& channel, float time) {
  if (channel.times.empty() || channel.times.size() != channel.values.size()) {
    return glm::vec3(0.0f);
  }
  const std::size_t left = leftKeyframe(channel.times, time);
  if (channel.interpolation == AnimationInterpolation::Step ||
      left + 1 >= channel.times.size() || left + 1 >= channel.values.size()) {
    return channel.values[left];
  }
  return glm::mix(channel.values[left], channel.values[left + 1],
                  interpolationFactor(channel.times, left, time));
}

glm::quat SampleQuat(const AnimationChannel<glm::quat>& channel, float time) {
  if (channel.times.empty() || channel.times.size() != channel.values.size()) {
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }
  const std::size_t left = leftKeyframe(channel.times, time);
  if (channel.interpolation == AnimationInterpolation::Step ||
      left + 1 >= channel.times.size() || left + 1 >= channel.values.size()) {
    return channel.values[left];
  }
  return glm::slerp(channel.values[left], channel.values[left + 1],
                    interpolationFactor(channel.times, left, time));
}

SkinPalette BuildSkinPalette(const std::vector<int>& parents,
                             const std::vector<glm::mat4>& localTransforms,
                             const std::vector<glm::mat4>& inverseBindMatrices) {
  SkinPalette palette;
  if (parents.size() != localTransforms.size() || parents.size() != inverseBindMatrices.size() ||
      parents.size() > kMaxSkinJoints) {
    return palette;
  }

  std::vector<glm::mat4> globals(parents.size(), glm::mat4(1.0f));
  std::vector<unsigned char> states(parents.size(), 0);
  std::function<bool(std::size_t)> resolveGlobal = [&](std::size_t joint) {
    if (states[joint] == 2) {
      return true;
    }
    if (states[joint] == 1) {
      return false;
    }
    states[joint] = 1;
    const int parent = parents[joint];
    if (parent == -1) {
      globals[joint] = localTransforms[joint];
    } else if (parent >= 0 && static_cast<std::size_t>(parent) < parents.size()) {
      if (!resolveGlobal(static_cast<std::size_t>(parent))) {
        return false;
      }
      globals[joint] = globals[static_cast<std::size_t>(parent)] * localTransforms[joint];
    } else {
      return false;
    }
    states[joint] = 2;
    return true;
  };

  palette.matrices.resize(parents.size());
  for (std::size_t joint = 0; joint < parents.size(); ++joint) {
    if (!resolveGlobal(joint)) {
      palette.matrices.clear();
      return palette;
    }
    palette.matrices[joint] = globals[joint] * inverseBindMatrices[joint];
  }
  return palette;
}

SkinnedModel::SkinnedModel() : impl_(std::make_unique<Impl>()) {}

SkinnedModel::~SkinnedModel() = default;

bool SkinnedModel::tryInitialize(const std::vector<uint8_t>& bytes,
                                 const std::string& assetName) {
  destroy();

  RuntimeData parsed;
  std::string error;
  if (!parseGlb(bytes, assetName, parsed, error)) {
    impl_->lastError = std::move(error);
    return false;
  }

  unsigned int vbo = 0;
  unsigned int ibo = 0;
  struct DecodedImage {
    stbi_uc* pixels = nullptr;
    int width = 0;
    int height = 0;
  };
  std::vector<DecodedImage> decodedImages;
  decodedImages.reserve(parsed.baseColorImages.size());
  const auto freeDecodedImages = [&decodedImages] {
    for (DecodedImage& image : decodedImages) {
      if (image.pixels != nullptr) stbi_image_free(image.pixels);
      image.pixels = nullptr;
    }
  };
  for (const std::vector<uint8_t>& imageBytes : parsed.baseColorImages) {
    if (imageBytes.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      freeDecodedImages();
      impl_->lastError = assetPrefix(assetName) +
                         "embedded baseColor image is too large";
      return false;
    }
    DecodedImage image;
    int imageChannels = 0;
    image.pixels = stbi_load_from_memory(
        imageBytes.data(), static_cast<int>(imageBytes.size()),
        &image.width, &image.height, &imageChannels, STBI_rgb_alpha);
    if (image.pixels == nullptr || image.width <= 0 || image.height <= 0) {
      if (image.pixels != nullptr) stbi_image_free(image.pixels);
      freeDecodedImages();
      impl_->lastError = assetPrefix(assetName) +
                         "could not decode embedded baseColor image";
      return false;
    }
    decodedImages.push_back(image);
  }
  std::vector<unsigned int> textures;
#ifdef OHOS_PLATFORM
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ibo);
  if (vbo == 0u || ibo == 0u) {
    freeDecodedImages();
    if (vbo != 0u) glDeleteBuffers(1, &vbo);
    if (ibo != 0u) glDeleteBuffers(1, &ibo);
    impl_->lastError = assetPrefix(assetName) + "could not create GLES mesh buffers";
    return false;
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(parsed.vertices.size() * sizeof(SkinnedVertex)),
               parsed.vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(parsed.indices.size() * sizeof(uint32_t)),
               parsed.indices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  textures.reserve(decodedImages.size());
  for (const DecodedImage& image : decodedImages) {
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0u) {
      freeDecodedImages();
      if (!textures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
      }
      glDeleteBuffers(1, &vbo);
      glDeleteBuffers(1, &ibo);
      impl_->lastError = assetPrefix(assetName) + "could not create GLES texture";
      return false;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image.pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    textures.push_back(texture);
  }
#endif
  freeDecodedImages();

  impl_->data = std::move(parsed);
  impl_->vbo = vbo;
  impl_->ibo = ibo;
  impl_->textures = std::move(textures);
  impl_->ready = true;
  impl_->lastError.clear();
  return true;
}

bool SkinnedModel::ready() const { return impl_->ready; }

SkinPalette SkinnedModel::update(SkinnedAnimationState& animation,
                                 const ActorRenderState& actor,
                                 float dtSeconds) const {
  if (!impl_->ready) return {};
  if (animation.owner != this ||
      animation.assetRevision != impl_->assetRevision) {
    animation.reset();
    animation.owner = this;
    animation.assetRevision = impl_->assetRevision;
    animation.currentClip = findClip(
        impl_->data, ResolveClip(impl_->data.clipNames, RenderAnimation::Idle));
  }
  const float dt = std::max(dtSeconds, 0.0f);
  const RenderAnimation requestedAnimation = ChooseAnimation(actor);
  const std::string desiredName =
      ResolveClip(impl_->data.clipNames, requestedAnimation);
  const int desiredClip = findClip(impl_->data, desiredName);
  if (desiredClip != animation.currentClip) {
    const std::string currentName =
        animation.currentClip >= 0
            ? impl_->data.clips[static_cast<std::size_t>(animation.currentClip)].name
            : std::string{};
    const bool requestedLocomotion =
        requestedAnimation == RenderAnimation::Idle ||
        requestedAnimation == RenderAnimation::Run;
    if (requestedLocomotion && animation.currentClip >= 0 && desiredClip >= 0 &&
        isLocomotionClip(currentName) && isLocomotionClip(desiredName)) {
      animation.previousClip = animation.currentClip;
      animation.previousTime = animation.currentTime;
      animation.blendElapsed = 0.0f;
    } else {
      animation.previousClip = -1;
      animation.previousTime = 0.0f;
      animation.blendElapsed = 0.0f;
    }
    animation.currentClip = desiredClip;
    animation.currentTime = 0.0f;
  }

  animation.currentTime += dt;
  if (animation.previousClip >= 0) {
    animation.previousTime += dt;
    animation.blendElapsed += dt;
  }

  std::vector<PoseNode> pose =
      samplePose(impl_->data, animation.currentClip, animation.currentTime);
  if (animation.previousClip >= 0) {
    const std::vector<PoseNode> previous =
        samplePose(impl_->data, animation.previousClip, animation.previousTime);
    constexpr float kLocomotionBlendSeconds = 0.15f;
    blendPoses(pose, previous, animation.blendElapsed / kLocomotionBlendSeconds);
    if (animation.blendElapsed >= kLocomotionBlendSeconds) {
      animation.previousClip = -1;
      animation.previousTime = 0.0f;
      animation.blendElapsed = 0.0f;
    }
  }
  return buildRuntimePalette(impl_->data, pose);
}

SkinPalette SkinnedModel::update(const ActorRenderState& actor,
                                 float dtSeconds) const {
  SkinnedAnimationState animation;
  return update(animation, actor, dtSeconds);
}

void SkinnedModel::draw() const { drawInternal(nullptr); }

void SkinnedModel::draw(Shader3D& shader) const { drawInternal(&shader); }

void SkinnedModel::drawInternal(Shader3D* shader) const {
#ifdef OHOS_PLATFORM
  if (!impl_->ready || impl_->vbo == 0u || impl_->ibo == 0u) return;
  glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ibo);
  const GLsizei stride = static_cast<GLsizei>(sizeof(SkinnedVertex));

  glEnableVertexAttribArray(kPositionAttribute);
  glVertexAttribPointer(kPositionAttribute, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));
  glEnableVertexAttribArray(kNormalAttribute);
  glVertexAttribPointer(kNormalAttribute, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
  glEnableVertexAttribArray(kUvAttribute);
  glVertexAttribPointer(kUvAttribute, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SkinnedVertex, uv)));
  glEnableVertexAttribArray(kJointsAttribute);
  glVertexAttribIPointer(kJointsAttribute, 4, GL_UNSIGNED_INT, stride,
                         reinterpret_cast<void*>(offsetof(SkinnedVertex, joints)));
  glEnableVertexAttribArray(kWeightsAttribute);
  glVertexAttribPointer(kWeightsAttribute, 4, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(SkinnedVertex, weights)));

  for (const PrimitiveRange& primitive : impl_->data.primitives) {
    const bool hasTexture =
        primitive.textureIndex >= 0 &&
        static_cast<std::size_t>(primitive.textureIndex) < impl_->textures.size();
    if (shader != nullptr) shader->setHasTexture(hasTexture);
    glBindTexture(GL_TEXTURE_2D,
                  hasTexture
                      ? impl_->textures[static_cast<std::size_t>(primitive.textureIndex)]
                      : 0u);
    const std::uintptr_t offset =
        static_cast<std::uintptr_t>(primitive.firstIndex) * sizeof(uint32_t);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(primitive.indexCount),
                   GL_UNSIGNED_INT, reinterpret_cast<void*>(offset));
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
#ifndef OHOS_PLATFORM
  (void)shader;
#endif
}

void SkinnedModel::destroy() {
#ifdef OHOS_PLATFORM
  if (!impl_->textures.empty()) {
    glDeleteTextures(static_cast<GLsizei>(impl_->textures.size()),
                     impl_->textures.data());
  }
  if (impl_->vbo != 0u) glDeleteBuffers(1, &impl_->vbo);
  if (impl_->ibo != 0u) glDeleteBuffers(1, &impl_->ibo);
#endif
  impl_->vbo = 0;
  impl_->ibo = 0;
  impl_->textures.clear();
  impl_->data = RuntimeData{};
  impl_->ready = false;
  impl_->lastError.clear();
  ++impl_->assetRevision;
}

void SkinnedModel::abandonGpuResources() {
  impl_->vbo = 0;
  impl_->ibo = 0;
  impl_->textures.clear();
  impl_->data = RuntimeData{};
  impl_->ready = false;
  impl_->lastError.clear();
  ++impl_->assetRevision;
}

const std::string& SkinnedModel::lastError() const { return impl_->lastError; }

std::size_t SkinnedModel::vertexCount() const { return impl_->data.vertices.size(); }

std::size_t SkinnedModel::indexCount() const { return impl_->data.indices.size(); }

std::size_t SkinnedModel::jointCount() const { return impl_->data.jointNodes.size(); }

const std::vector<std::string>& SkinnedModel::clipNames() const {
  return impl_->data.clipNames;
}

bool SkinnedModel::hasTexture() const {
  return !impl_->data.baseColorImages.empty();
}

std::size_t SkinnedModel::primitiveCount() const {
  return impl_->data.primitives.size();
}

bool SkinnedModel::primitiveHasTexture(std::size_t primitiveIndex) const {
  return primitiveIndex < impl_->data.primitives.size() &&
         impl_->data.primitives[primitiveIndex].textureIndex >= 0;
}

int SkinnedModel::primitiveTextureIndex(std::size_t primitiveIndex) const {
  return primitiveIndex < impl_->data.primitives.size()
             ? impl_->data.primitives[primitiveIndex].textureIndex
             : -1;
}

std::size_t SkinnedModel::embeddedTextureCount() const {
  return impl_->data.baseColorImages.size();
}

std::size_t SkinnedModel::gpuResourceCount() const {
  return static_cast<std::size_t>(impl_->vbo != 0u) +
         static_cast<std::size_t>(impl_->ibo != 0u) +
         static_cast<std::size_t>(std::count_if(
             impl_->textures.begin(), impl_->textures.end(),
             [](unsigned int texture) { return texture != 0u; }));
}
