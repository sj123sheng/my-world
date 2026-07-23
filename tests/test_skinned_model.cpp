#include "native/engine/render/skinned_model.h"
#include "tests/gltf_fixture_builder.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool close(float actual, float expected, float epsilon = 0.0001f) {
  return std::fabs(actual - expected) < epsilon;
}

void expectInitializationFailure(const std::vector<uint8_t>& bytes,
                                 const std::string& assetName,
                                 const std::string& detail) {
  SkinnedModel model;
  assert(!model.tryInitialize(bytes, assetName));
  assert(!model.ready());
  assert(model.lastError().find(assetName) != std::string::npos);
  if (model.lastError().find(detail) == std::string::npos) {
    std::fprintf(stderr, "expected '%s' in '%s'\n", detail.c_str(),
                 model.lastError().c_str());
  }
  assert(model.lastError().find(detail) != std::string::npos);
}

void testInitializesMinimalGlbFromMemory() {
  SkinnedModel model;
  SkinnedAnimationState animation;
  const std::vector<uint8_t> bytes = gltf_fixture::makeMinimalGlb();

  assert(model.tryInitialize(bytes, "minimal.glb"));
  assert(model.ready());
  assert(model.vertexCount() == 3);
  assert(model.indexCount() == 3);
  assert(model.jointCount() == 2);
  assert((model.clipNames() == std::vector<std::string>{"idle", "run"}));

  const SkinPalette palette = model.update(animation, ActorRenderState{}, 0.0f);
  assert(palette.matrices.size() == 2);
  for (const glm::mat4& matrix : palette.matrices) {
    for (int column = 0; column < 4; ++column) {
      for (int row = 0; row < 4; ++row) {
        assert(std::isfinite(matrix[column][row]));
      }
    }
  }
}

void testRejectsUnsupportedRealGlbInputs() {
  const std::vector<uint8_t> valid = gltf_fixture::makeMinimalGlb();
  expectInitializationFailure({'{', '}'}, "json.gltf", "only GLB");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"mode\":4", "\"mode\":1"),
      "lines.glb", "TRIANGLES");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"POSITION\":0,", ""),
      "missing-position.glb", "POSITION");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"JOINTS_0\":3",
                                    "\"JOINTS_0\":3,\"JOINTS_1\":3"),
      "extra-joints.glb", "JOINTS_1/WEIGHTS_1");

  std::ostringstream joints;
  joints << "\"joints\":[";
  for (int i = 0; i < 65; ++i) {
    if (i != 0) joints << ',';
    joints << (i % 2 == 0 ? 1 : 2);
  }
  joints << ']';
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"joints\":[1,2]", joints.str()),
      "too-many-joints.glb", "joint count exceeds 64");

  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"interpolation\":\"LINEAR\"",
                                    "\"interpolation\":\"CUBICSPLINE\""),
      "cubic.glb", "CUBICSPLINE");

  const uint32_t jsonLength = gltf_fixture::readU32(valid, 12);
  const uint32_t binLength = gltf_fixture::readU32(valid, 20 + jsonLength);
  const std::string buffer =
      "\"buffers\":[{\"byteLength\":" + std::to_string(binLength) + "}]";
  const std::string externalBuffer =
      "\"buffers\":[{\"byteLength\":" + std::to_string(binLength) +
      ",\"uri\":\"external.bin\"}]";
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, buffer, externalBuffer),
      "external.glb", "external buffer URI");

  expectInitializationFailure(
      gltf_fixture::replaceJsonText(valid, "\"byteLength\":36",
                                    "\"byteLength\":4"),
      "bounds.glb", "out of bounds");
}

void testRejectsAdditionalUnsupportedGlbFeatures() {
  const std::vector<uint8_t> valid = gltf_fixture::makeMinimalGlb();
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(
          valid, "\"asset\":{\"version\":\"2.0\"},",
          "\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_draco_mesh_compression\"],"),
      "extension.glb", "extensions");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(
          valid,
          "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}",
          "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
          "\"sparse\":{\"count\":1,\"indices\":{\"bufferView\":5,\"componentType\":5123},"
          "\"values\":{\"bufferView\":0}}}"),
      "sparse.glb", "sparse");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(
          valid, "\"meshes\":[",
          "\"images\":[{\"uri\":\"external.png\"}],\"meshes\":["),
      "external-image.glb", "external image URI");
  expectInitializationFailure(
      gltf_fixture::replaceJsonText(
          valid,
          "{\"inverseBindMatrices\":6,\"joints\":[1,2],\"skeleton\":1}",
          "{\"inverseBindMatrices\":6,\"joints\":[1,2],\"skeleton\":1},"
          "{\"inverseBindMatrices\":6,\"joints\":[1,2],\"skeleton\":1}"),
      "multiple-skins.glb", "exactly one skin");
  expectInitializationFailure(gltf_fixture::makeZeroWeightsGlb(),
                              "zero-weights.glb", "weights sum to zero");
}

void testCombinesMultiplePrimitivesAndOwnsEmbeddedImage() {
  SkinnedModel multiple;
  const bool multipleReady = multiple.tryInitialize(
      gltf_fixture::makeTwoPrimitiveGlb(), "multiple.glb");
  if (!multipleReady) std::fprintf(stderr, "%s\n", multiple.lastError().c_str());
  assert(multipleReady);
  assert(multiple.vertexCount() == 6);
  assert(multiple.indexCount() == 6);

  SkinnedModel textured;
  const bool texturedReady = textured.tryInitialize(
      gltf_fixture::makeEmbeddedTextureGlb(), "textured.glb");
  if (!texturedReady) std::fprintf(stderr, "%s\n", textured.lastError().c_str());
  assert(texturedReady);
  assert(textured.hasTexture());
  assert(textured.vertexCount() == 3);
  assert(textured.indexCount() == 3);

  expectInitializationFailure(gltf_fixture::makeInvalidEmbeddedTextureGlb(),
                              "broken-texture.glb", "decode");
}

void testKeepsTextureStatePerPrimitive() {
  SkinnedModel model;
  const bool ready = model.tryInitialize(
      gltf_fixture::makeMixedPrimitiveTextureGlb(), "mixed-material.glb");
  if (!ready) std::fprintf(stderr, "%s\n", model.lastError().c_str());
  assert(ready);
  assert(model.primitiveCount() == 2);
  assert(model.primitiveHasTexture(0));
  assert(!model.primitiveHasTexture(1));
  assert(model.embeddedTextureCount() == 1);

  SkinnedModel twoTextures;
  const bool twoTexturesReady = twoTextures.tryInitialize(
      gltf_fixture::makeTwoTexturePrimitiveGlb(), "two-textures.glb");
  if (!twoTexturesReady) {
    std::fprintf(stderr, "%s\n", twoTextures.lastError().c_str());
  }
  assert(twoTexturesReady);
  assert(twoTextures.primitiveCount() == 2);
  assert(twoTextures.embeddedTextureCount() == 2);
  assert(twoTextures.primitiveTextureIndex(0) == 0);
  assert(twoTextures.primitiveTextureIndex(1) == 1);
}

void testRejectsUnsupportedBaseColorUvSet() {
  expectInitializationFailure(gltf_fixture::makeBaseColorTexcoord1Glb(),
                              "uv-one.glb", "baseColorTexture texcoord must be 0");
}

void testUsesNonJointAncestorsAndAnimationTransitions() {
  SkinnedModel model;
  SkinnedAnimationState animation;
  assert(model.tryInitialize(gltf_fixture::makeMinimalGlb(), "animated.glb"));
  const SkinPalette idle = model.update(animation, ActorRenderState{}, 0.0f);
  assert(close(idle.matrices[0][3].x, 3.0f));

  ActorRenderState running;
  running.moving = true;
  const SkinPalette halfBlend = model.update(animation, running, 0.075f);
  assert(halfBlend.matrices[0][3].x > 3.0f);
  assert(halfBlend.matrices[0][3].x < 3.15f);
  const SkinPalette completeBlend = model.update(animation, running, 0.075f);
  assert(close(completeBlend.matrices[0][3].x, 3.3f));

  const auto expectImmediateFallback = [&model, &running](
                                           const ActorRenderState& intent) {
    SkinnedAnimationState fallbackAnimation;
    model.update(fallbackAnimation, running, 0.15f);
    const SkinPalette fallback =
        model.update(fallbackAnimation, intent, 0.01f);
    assert(close(fallback.matrices[0][3].x, 3.0f));
  };
  ActorRenderState attackWithoutClip;
  attackWithoutClip.action = RenderAnimation::Attack;
  expectImmediateFallback(attackWithoutClip);
  ActorRenderState hitWithoutClip;
  hitWithoutClip.hit = true;
  expectImmediateFallback(hitWithoutClip);
  ActorRenderState deathWithoutClip;
  deathWithoutClip.alive = false;
  expectImmediateFallback(deathWithoutClip);

  SkinnedModel attackModel;
  SkinnedAnimationState attackAnimation;
  assert(attackModel.tryInitialize(gltf_fixture::makeMinimalGlb(true),
                                   "attack.glb"));
  attackModel.update(attackAnimation, running, 0.15f);
  const SkinPalette runPose = attackModel.update(attackAnimation, running, 0.35f);
  assert(close(runPose.matrices[0][3].x, 4.0f));
  ActorRenderState attacking;
  attacking.action = RenderAnimation::Attack;
  const SkinPalette immediateAttack =
      attackModel.update(attackAnimation, attacking, 0.0f);
  assert(close(immediateAttack.matrices[0][3].x, 3.0f));
}

void testKeepsAnimationPlaybackStatePerInstance() {
  SkinnedModel sharedModel;
  assert(sharedModel.tryInitialize(gltf_fixture::makeMinimalGlb(),
                                   "shared-enemy.glb"));

  SkinnedAnimationState firstEnemy;
  SkinnedAnimationState secondEnemy;
  ActorRenderState running;
  running.moving = true;

  sharedModel.update(firstEnemy, ActorRenderState{}, 0.0f);
  sharedModel.update(secondEnemy, ActorRenderState{}, 0.0f);
  sharedModel.update(firstEnemy, running, 0.15f);
  const SkinPalette firstAtHalfSecond =
      sharedModel.update(firstEnemy, running, 0.35f);
  assert(close(firstAtHalfSecond.matrices[0][3].x, 4.0f));

  const SkinPalette secondStillIdle =
      sharedModel.update(secondEnemy, ActorRenderState{}, 0.1f);
  assert(close(secondStillIdle.matrices[0][3].x, 3.0f));

  const SkinPalette firstContinuesIndependently =
      sharedModel.update(firstEnemy, running, 0.1f);
  assert(close(firstContinuesIndependently.matrices[0][3].x, 4.2f));
}

void testDestroyAndAbandonClearAllTracking() {
  SkinnedModel model;
  assert(model.tryInitialize(gltf_fixture::makeMinimalGlb(), "cleanup.glb"));
  assert(model.gpuResourceCount() == 0);
  model.draw();
  model.destroy();
  assert(!model.ready());
  assert(model.vertexCount() == 0);
  assert(model.indexCount() == 0);
  assert(model.jointCount() == 0);
  assert(model.clipNames().empty());
  assert(model.gpuResourceCount() == 0);

  assert(model.tryInitialize(gltf_fixture::makeMinimalGlb(), "abandon.glb"));
  model.abandonGpuResources();
  assert(!model.ready());
  assert(model.vertexCount() == 0);
  assert(model.indexCount() == 0);
  assert(model.jointCount() == 0);
  assert(model.clipNames().empty());
  assert(model.gpuResourceCount() == 0);
}

GltfValidationInput validValidationInput() {
  GltfValidationInput input;
  input.assetName = "player.glb";
  input.jointCount = 2;
  input.assetFormat = GltfAssetFormat::Glb;
  input.primitiveMode = GltfPrimitiveMode::Triangles;
  input.hasPosition = true;
  input.hasNormal = true;
  input.hasTexcoord0 = true;
  input.hasJoints0 = true;
  input.hasWeights0 = true;
  input.maxVertexInfluences = 4;
  input.singleSkin = true;
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

void testRejectsUnequalAnimationChannelLengths() {
  AnimationChannel<glm::vec3> position{{0.0f}, {{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}},
                                       AnimationInterpolation::Step};
  assert(SampleVec3(position, 0.0f) == glm::vec3(0.0f));

  AnimationChannel<glm::quat> rotation{{0.0f},
                                       {glm::quat(0.0f, 1.0f, 0.0f, 0.0f),
                                        glm::quat(1.0f, 0.0f, 0.0f, 0.0f)},
                                       AnimationInterpolation::Linear};
  assert(SampleQuat(rotation, 0.0f) == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
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

void testRejectsInvalidSkinHierarchy() {
  const std::vector<glm::mat4> transforms{glm::mat4(1.0f), glm::mat4(1.0f)};
  assert(BuildSkinPalette({0}, {glm::mat4(1.0f)}, {glm::mat4(1.0f)}).matrices.empty());
  assert(BuildSkinPalette({1, 0}, transforms, transforms).matrices.empty());
  assert(BuildSkinPalette({-2}, {glm::mat4(1.0f)}, {glm::mat4(1.0f)}).matrices.empty());
  assert(BuildSkinPalette({2}, {glm::mat4(1.0f)}, {glm::mat4(1.0f)}).matrices.empty());
}

void testRejectsUnsupportedGltfInputsWithAssetNames() {
  GltfValidationInput input = validValidationInput();
  std::string reason;
  input.assetFormat = GltfAssetFormat::Gltf;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: only GLB assets are supported");

  input = validValidationInput();
  input.primitiveMode = GltfPrimitiveMode::Other;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: primitive mode must be TRIANGLES");

  input = validValidationInput();
  input.hasTexcoord0 = false;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: missing required vertex attribute TEXCOORD_0");

  input = validValidationInput();
  input.hasJoints1 = true;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: JOINTS_1/WEIGHTS_1 are unsupported");

  input = validValidationInput();
  input.hasWeights1 = true;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: JOINTS_1/WEIGHTS_1 are unsupported");

  input = validValidationInput();
  input.maxVertexInfluences = 5;
  assert(!ValidateGltf(input, reason));
  assert(reason == "player.glb: vertex influence count exceeds 4");

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
  assert(reason == "player.glb: joint count exceeds 64");

  input = validValidationInput();
  input.assetName.clear();
  input.singleSkin = false;
  assert(!ValidateGltf(input, reason));
  assert(reason == "unnamed asset: exactly one skin is required");
}

}  // namespace

int main() {
  testInitializesMinimalGlbFromMemory();
  testRejectsUnsupportedRealGlbInputs();
  testRejectsAdditionalUnsupportedGlbFeatures();
  testCombinesMultiplePrimitivesAndOwnsEmbeddedImage();
  testKeepsTextureStatePerPrimitive();
  testRejectsUnsupportedBaseColorUvSet();
  testUsesNonJointAncestorsAndAnimationTransitions();
  testKeepsAnimationPlaybackStatePerInstance();
  testDestroyAndAbandonClearAllTracking();
  testWrapAndStepSampling();
  testLinearSampling();
  testRejectsUnequalAnimationChannelLengths();
  testBuildsParentChildSkinPalette();
  testRejectsInvalidSkinHierarchy();
  testRejectsUnsupportedGltfInputsWithAssetNames();
  testRejectsOver64Joints();
  return 0;
}
