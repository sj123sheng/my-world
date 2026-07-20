// skinned_model.h: glTF 骨骼动画的纯数据核心。
//
// 这里不依赖 EGL/GLES，便于在宿主机验证 glTF 输入、动画采样和蒙皮矩阵计算。

#pragma once

#include "native/engine/render/render_animation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t kMaxSkinJoints = 64;

enum class AnimationInterpolation {
  Linear,
  Step,
};

template <typename T>
struct AnimationChannel {
  std::vector<float> times;
  std::vector<T> values;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
};

struct SkinPalette {
  std::vector<glm::mat4> matrices;
};

enum class GltfAssetFormat {
  Glb,
  Gltf,
};

enum class GltfPrimitiveMode {
  Triangles,
  Other,
};

struct GltfValidationInput {
  std::string assetName;
  uint32_t jointCount = 0;
  GltfAssetFormat assetFormat = GltfAssetFormat::Gltf;
  GltfPrimitiveMode primitiveMode = GltfPrimitiveMode::Other;
  bool hasPosition = false;
  bool hasNormal = false;
  bool hasTexcoord0 = false;
  bool hasJoints0 = false;
  bool hasWeights0 = false;
  bool hasJoints1 = false;
  bool hasWeights1 = false;
  uint32_t maxVertexInfluences = 0;
  bool singleSkin = false;
  bool hasCubicSpline = false;
};

bool ValidateGltf(const GltfValidationInput& input, std::string& reason);

float WrapAnimationTime(float seconds, float duration);

glm::vec3 SampleVec3(const AnimationChannel<glm::vec3>& channel, float time);
glm::quat SampleQuat(const AnimationChannel<glm::quat>& channel, float time);

SkinPalette BuildSkinPalette(const std::vector<int>& parents,
                             const std::vector<glm::mat4>& localTransforms,
                             const std::vector<glm::mat4>& inverseBindMatrices);

// Task 1 当前只提供纯数据校验/采样核心，尚未实现 cgltf 到 CPU/GPU 网格的运行时
// 装载器。这个最小接口让 Surface 能严格管理尝试、降级和 GL 生命周期；在真正的
// 运行时装载器接入前，它会明确失败并保持 ready()==false，绝不把 GLB 误报为已加载。
class SkinnedModel {
 public:
  bool tryInitialize(const std::vector<uint8_t>& bytes,
                     const std::string& assetName) {
    (void)bytes;
    ready_ = false;
    lastError_ = (assetName.empty() ? "unnamed asset" : assetName) +
                 std::string(": SkinnedModel runtime loader is unavailable");
    return false;
  }

  bool ready() const { return ready_; }

  SkinPalette update(const ActorRenderState& actor, float dtSeconds) {
    (void)actor;
    (void)dtSeconds;
    return {};
  }

  void draw() const {}

  void destroy() {
    ready_ = false;
    lastError_.clear();
  }

  const std::string& lastError() const { return lastError_; }

 private:
  bool ready_ = false;
  std::string lastError_;
};
