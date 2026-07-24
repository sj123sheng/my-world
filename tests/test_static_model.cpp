#include <cassert>

#include "native/engine/render/static_model.h"
#include "tests/gltf_fixture_builder.h"

void testParsesStaticSceneAndBakesNodeTransform() {
  StaticModel model;
  assert(model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "outer.glb"));
  assert(model.ready());
  assert(model.stats().primitiveCount == 1);
  assert(model.stats().triangleCount == 1);
  assert(model.cpuVerticesForTest().front().position.x == 2.0f);
}

void testRejectsSkinAndExternalUris() {
  StaticModel model;
  assert(!model.tryInitialize(gltf_fixture::makeMinimalGlb(), "skinned.glb"));
  assert(model.lastError() ==
         "skinned.glb: static environment must not contain skins");
  assert(!model.tryInitialize(gltf_fixture::makeStaticSceneWithExternalUri(),
                              "external.glb"));
  assert(model.lastError() ==
         "external.glb: external buffer/image URI is unsupported");
}

void testTextureTierTransitionIsStable() {
  StaticModel model;
  assert(model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "outer.glb"));
  assert(model.textureTier() == StaticTextureTier::Full);
  const std::size_t fullBytes = model.stats().textureBytes;
  model.setTextureTier(StaticTextureTier::Half);
  assert(model.textureTier() == StaticTextureTier::Half);
  assert(model.stats().textureBytes == fullBytes + 1);
  model.setTextureTier(StaticTextureTier::Half);
  assert(model.textureTier() == StaticTextureTier::Half);
  assert(model.stats().textureBytes == fullBytes + 1);
}

void testMergesSharedTextureAndKeepsUntexturedBatchSeparate() {
  StaticModel shared;
  assert(shared.tryInitialize(
      gltf_fixture::makeTwoStaticPrimitiveGlb(), "shared.glb"));
  assert(shared.stats().primitiveCount == 1);
  assert(shared.stats().triangleCount == 2);

  StaticModel mixed;
  assert(mixed.tryInitialize(
      gltf_fixture::makeTwoStaticPrimitiveGlb(false), "mixed.glb"));
  assert(mixed.stats().primitiveCount == 2);
  assert(mixed.stats().triangleCount == 2);
}

void testDestroyClearsOwnedStateAndAbandonPreservesCpuState() {
  StaticModel model;
  assert(
      model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "destroy.glb"));
  model.destroy();
  assert(!model.ready());
  assert(model.cpuVerticesForTest().empty());
  assert(model.stats().primitiveCount == 0);

  assert(
      model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "abandon.glb"));
  model.abandonGpuResources();
  assert(model.ready());
  assert(!model.cpuVerticesForTest().empty());
  assert(model.stats().primitiveCount == 1);
}

int main() {
  testParsesStaticSceneAndBakesNodeTransform();
  testRejectsSkinAndExternalUris();
  testTextureTierTransitionIsStable();
  testMergesSharedTextureAndKeepsUntexturedBatchSeparate();
  testDestroyClearsOwnedStateAndAbandonPreservesCpuState();
}
