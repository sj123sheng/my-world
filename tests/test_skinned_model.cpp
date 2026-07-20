#include "native/engine/render/skinned_model.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {

bool close(float actual, float expected, float epsilon = 0.0001f) {
  return std::fabs(actual - expected) < epsilon;
}

GltfValidationInput validValidationInput() {
  GltfValidationInput input;
  input.assetName = "player.glb";
  input.jointCount = 2;
  input.hasPosition = true;
  input.hasNormal = true;
  input.hasUv = true;
  input.hasJoints = true;
  input.hasWeights = true;
  input.singleSkin = true;
  input.trianglesOnly = true;
  return input;
}

void testWrapAndStepSampling() {
  assert(close(WrapAnimationTime(2.25f, 2.0f), 0.25f));
  assert(close(WrapAnimationTime(-0.5f, 2.0f), 0.0f));
  assert(close(WrapAnimationTime(1.0f, 0.0f), 0.0f));

  AnimationChannel<glm::vec3> channel{{0.0f, 1.0f}, {{0, 0, 0}, {2, 0, 0}},
                                      AnimationInterpolation::Step};
  assert(SampleVec3(channel, 0.75f).x == 0.0f);
}

void testLinearSampling() {
  AnimationChannel<glm::vec3> position{{0.0f, 1.0f}, {{0, 0, 0}, {2, 4, 6}},
                                       AnimationInterpolation::Linear};
  const glm::vec3 sampledPosition = SampleVec3(position, 0.25f);
  assert(close(sampledPosition.x, 0.5f));
  assert(close(sampledPosition.y, 1.0f));
  assert(close(sampledPosition.z, 1.5f));

  AnimationChannel<glm::quat> rotation{{0.0f, 1.0f},
                                       {glm::quat(1, 0, 0, 0), glm::quat(0, 0, 1, 0)},
                                       AnimationInterpolation::Linear};
  const glm::quat sampledRotation = SampleQuat(rotation, 0.5f);
  assert(close(std::fabs(sampledRotation.w), 0.7071067f));
  assert(close(std::fabs(sampledRotation.y), 0.7071067f));
}

void testBuildsParentChildSkinPalette() {
  const std::vector<int> parents{-1, 0};
  const std::vector<glm::mat4> locals{
      glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)),
  };
  const std::vector<glm::mat4> inverseBinds{glm::mat4(1.0f), glm::mat4(1.0f)};

  const SkinPalette palette = BuildSkinPalette(parents, locals, inverseBinds);
  assert(palette.matrices.size() == 2);
  assert(close(palette.matrices[0][3].x, 1.0f));
  assert(close(palette.matrices[1][3].x, 1.0f));
  assert(close(palette.matrices[1][3].y, 2.0f));
}

void testRejectsMissingAttributesAndUnsupportedFeatures() {
  GltfValidationInput input = validValidationInput();
  input.hasUv = false;
  std::string reason;
  assert(!ValidateGltf(input, reason));
  assert(reason == "missing required vertex attribute");

  input = validValidationInput();
  input.hasCubicSpline = true;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: animation interpolation CUBICSPLINE is unsupported");
}

void testRejectsOver64Joints() {
  GltfValidationInput input = validValidationInput();
  input.jointCount = kMaxSkinJoints + 1;
  std::string reason;
  assert(!ValidateGltf(input, reason));
  assert(reason == "joint count exceeds 64");
}

}  // namespace

int main() {
  testWrapAndStepSampling();
  testLinearSampling();
  testBuildsParentChildSkinPalette();
  testRejectsMissingAttributesAndUnsupportedFeatures();
  testRejectsOver64Joints();
  return 0;
}
