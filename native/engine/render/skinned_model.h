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
#include <memory>
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

class SkinnedModel;
class Shader3D;

// 每个渲染实体独立持有的动画播放状态。网格、纹理和 clip 数据仍由
// SkinnedModel 共享，避免为每个实体复制 GPU 资产。
struct SkinnedAnimationState {
  void reset();
  bool shouldReport(RenderAnimation animation, const std::string& clip) {
    return logState.shouldReport(animation, clip);
  }

 private:
  friend class SkinnedModel;
  const SkinnedModel* owner = nullptr;
  uint64_t assetRevision = 0;
  int currentClip = -1;
  int previousClip = -1;
  float currentTime = 0.0f;
  float previousTime = 0.0f;
  float blendElapsed = 0.0f;
  AnimationLogState logState;
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

class SkinnedModel {
 public:
  SkinnedModel();
  ~SkinnedModel();
  SkinnedModel(const SkinnedModel&) = delete;
  SkinnedModel& operator=(const SkinnedModel&) = delete;

  bool tryInitialize(const std::vector<uint8_t>& bytes,
                     const std::string& assetName);
  bool ready() const;
  SkinPalette update(SkinnedAnimationState& animation,
                     const ActorRenderState& actor, float dtSeconds) const;
  // 保留旧调用签名用于一次性采样；连续播放应显式传入实例状态。
  SkinPalette update(const ActorRenderState& actor, float dtSeconds) const;
  void draw() const;
  void draw(Shader3D& shader) const;
  void destroy();

  // context 已不可 current 时仅丢弃 CPU/GPU 侧跟踪，绝不发出 GL 调用。
  void abandonGpuResources();

  const std::string& lastError() const;
  std::size_t vertexCount() const;
  std::size_t indexCount() const;
  std::size_t jointCount() const;
  const std::vector<std::string>& clipNames() const;
  bool hasTexture() const;
  std::size_t primitiveCount() const;
  bool primitiveHasTexture(std::size_t primitiveIndex) const;
  int primitiveTextureIndex(std::size_t primitiveIndex) const;
  std::size_t embeddedTextureCount() const;
  std::size_t gpuResourceCount() const;

 private:
  struct Impl;
  void drawInternal(Shader3D* shader) const;
  std::unique_ptr<Impl> impl_;
};
