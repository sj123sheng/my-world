#include "native/engine/render/render_animation.h"
#include "native/engine/render/render_lifecycle.h"
#include "native/engine/render/skinned_model.h"
#include "native/engine/render/surface.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

void testAnimationPriority() {
  ActorRenderState actor;
  actor.alive = false;
  actor.attacking = true;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);

  actor.alive = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Attack);

  actor.attacking = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Hit);

  actor.hit = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Run);

  actor.moving = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Idle);
}

void testClipResolutionFallsBackToIdle() {
  assert(ResolveClip({"idle"}, RenderAnimation::Attack) == "idle");
  assert(ResolveClip({"idle", "run", "attack"},
                     RenderAnimation::Attack) == "attack");
  assert(ResolveClip({"run"}, RenderAnimation::Death) == "run");
  assert(ResolveClip({}, RenderAnimation::Idle).empty());
}

void testUnavailableRuntimeModelStaysOnFallbackPath() {
  SkinnedModel model;
  assert(!model.ready());
  assert(!model.tryInitialize({0x67, 0x6c, 0x54, 0x46}, "player.glb"));
  assert(!model.ready());
  assert(model.lastError().find("runtime loader") != std::string::npos);
  model.destroy();
  assert(!model.ready());
}

void testSurfaceStoresLateModelAssetsForContextBoundInitialization() {
  Surface surface;
  surface.setModelAsset(ModelKind::Player, {1, 2, 3});
  surface.setModelAsset(ModelKind::Enemy, {4, 5});
  surface.setModelAsset(ModelKind::Boss, {6});

  assert((surface.playerModelAsset.bytes == std::vector<uint8_t>{1, 2, 3}));
  assert((surface.enemyModelAsset.bytes == std::vector<uint8_t>{4, 5}));
  assert((surface.bossModelAsset.bytes == std::vector<uint8_t>{6}));
  assert(surface.playerModelAsset.dirty);
  assert(surface.enemyModelAsset.dirty);
  assert(surface.bossModelAsset.dirty);
}

void testPendingAssetIsConsumedExactlyOnceAfterLateDirtySignal() {
  PendingModelAsset asset;
  std::vector<uint8_t> consumed;

  asset.replace({1, 2, 3});
  assert(asset.dirty);
  assert(asset.take(consumed));
  assert((consumed == std::vector<uint8_t>{1, 2, 3}));
  assert(!asset.dirty);
  assert(!asset.take(consumed));
}

void testPendingAssetReplacementAndClearRemainConsumable() {
  PendingModelAsset asset;
  std::vector<uint8_t> consumed;

  asset.replace({1});
  assert(asset.take(consumed));
  asset.replace({2, 3});
  assert(asset.take(consumed));
  assert((consumed == std::vector<uint8_t>{2, 3}));
  asset.replace({});
  assert(asset.take(consumed));
  assert(consumed.empty());
  assert(!asset.dirty);
}

void testDestroyPlanReleasesGlOnlyWithCurrentContext() {
  const SurfaceDestroyPlan currentPlan = PlanSurfaceDestroy(true);
  assert(currentPlan.releaseGlResources);
  assert(currentPlan.discardCpuGlTracking);
  assert(currentPlan.destroyEglResources);
  assert((currentPlan.glDestroyOrder == kSurfaceGlDestroyOrder));

  const SurfaceDestroyPlan failedPlan = PlanSurfaceDestroy(false);
  assert(!failedPlan.releaseGlResources);
  assert(failedPlan.discardCpuGlTracking);
  assert(failedPlan.destroyEglResources);
  assert(failedPlan.glDestroyOrder.empty());
}

}  // namespace

int main() {
  testAnimationPriority();
  testClipResolutionFallsBackToIdle();
  testUnavailableRuntimeModelStaysOnFallbackPath();
  testSurfaceStoresLateModelAssetsForContextBoundInitialization();
  testPendingAssetIsConsumedExactlyOnceAfterLateDirtySignal();
  testPendingAssetReplacementAndClearRemainConsumable();
  testDestroyPlanReleasesGlOnlyWithCurrentContext();
  return 0;
}
