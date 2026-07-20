#include "native/engine/render/render_animation.h"
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

  assert((surface.playerModelAsset == std::vector<uint8_t>{1, 2, 3}));
  assert((surface.enemyModelAsset == std::vector<uint8_t>{4, 5}));
  assert((surface.bossModelAsset == std::vector<uint8_t>{6}));
  assert(surface.playerModelAssetDirty);
  assert(surface.enemyModelAssetDirty);
  assert(surface.bossModelAssetDirty);
}

}  // namespace

int main() {
  testAnimationPriority();
  testClipResolutionFallsBackToIdle();
  testUnavailableRuntimeModelStaysOnFallbackPath();
  testSurfaceStoresLateModelAssetsForContextBoundInitialization();
  return 0;
}
