// skinned_model.h: glTF 骨骼动画的纯数据核心。
//
// 这里不依赖 EGL/GLES，便于在宿主机验证 glTF 输入、动画采样和蒙皮矩阵计算。

#pragma once

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

struct GltfValidationInput {
  std::string assetName;
  uint32_t jointCount = 0;
  bool hasPosition = false;
  bool hasNormal = false;
  bool hasUv = false;
  bool hasJoints = false;
  bool hasWeights = false;
  bool singleSkin = false;
  bool hasCubicSpline = false;
  bool trianglesOnly = false;
};

bool ValidateGltf(const GltfValidationInput& input, std::string& reason);

float WrapAnimationTime(float seconds, float duration);

glm::vec3 SampleVec3(const AnimationChannel<glm::vec3>& channel, float time);
glm::quat SampleQuat(const AnimationChannel<glm::quat>& channel, float time);

SkinPalette BuildSkinPalette(const std::vector<int>& parents,
                             const std::vector<glm::mat4>& localTransforms,
                             const std::vector<glm::mat4>& inverseBindMatrices);
