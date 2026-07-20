// skinned_model.cpp: glTF 骨骼动画纯数据实现。

#include "native/engine/render/skinned_model.h"

#include <algorithm>
#include <cmath>
#include <functional>

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

}  // namespace

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
