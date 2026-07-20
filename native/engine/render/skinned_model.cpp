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
  return assetName.empty() ? std::string() : assetName + ": ";
}

}  // namespace

bool ValidateGltf(const GltfValidationInput& input, std::string& reason) {
  if (input.jointCount > kMaxSkinJoints) {
    reason = "joint count exceeds 64";
    return false;
  }
  if (!input.hasPosition || !input.hasNormal || !input.hasUv || !input.hasJoints ||
      !input.hasWeights) {
    reason = "missing required vertex attribute";
    return false;
  }
  if (input.hasCubicSpline) {
    reason = assetPrefix(input.assetName) +
             "animation interpolation CUBICSPLINE is unsupported";
    return false;
  }
  if (!input.singleSkin || !input.trianglesOnly) {
    reason = "unsupported glTF feature";
    return false;
  }
  reason.clear();
  return true;
}

float WrapAnimationTime(float seconds, float duration) {
  return duration > 0.0f ? std::fmod(std::max(seconds, 0.0f), duration) : 0.0f;
}

glm::vec3 SampleVec3(const AnimationChannel<glm::vec3>& channel, float time) {
  if (channel.times.empty() || channel.values.empty()) {
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
  if (channel.times.empty() || channel.values.empty()) {
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
  std::vector<bool> resolved(parents.size(), false);
  std::function<void(std::size_t)> resolveGlobal = [&](std::size_t joint) {
    if (resolved[joint]) {
      return;
    }
    const int parent = parents[joint];
    if (parent >= 0 && static_cast<std::size_t>(parent) < parents.size()) {
      resolveGlobal(static_cast<std::size_t>(parent));
      globals[joint] = globals[static_cast<std::size_t>(parent)] * localTransforms[joint];
    } else {
      globals[joint] = localTransforms[joint];
    }
    resolved[joint] = true;
  };

  palette.matrices.resize(parents.size());
  for (std::size_t joint = 0; joint < parents.size(); ++joint) {
    resolveGlobal(joint);
    palette.matrices[joint] = globals[joint] * inverseBindMatrices[joint];
  }
  return palette;
}
