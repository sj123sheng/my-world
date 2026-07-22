#include "native/engine/render/render_animation.h"
#include "native/engine/render/combat_animation.h"
#include "native/engine/render/render_lifecycle.h"
#include "native/engine/render/skinned_model.h"
#include "native/engine/render/surface.h"
#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>
#include <string>
#include <vector>
#include <cmath>

namespace {

void testAnimationPriority() {
  ActorRenderState actor;
  actor.alive = false;
  actor.action = RenderAnimation::Attack;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);

  actor.alive = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Attack);

  actor.action = RenderAnimation::Idle;
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

void testDedicatedActionClipNames() {
  assert(std::string(RenderAnimationName(RenderAnimation::Dodge)) ==
         "Running_Strafe_Right");
  assert(std::string(RenderAnimationName(RenderAnimation::Radiance)) ==
         "Spellcast_Raise");
  assert(std::string(RenderAnimationName(RenderAnimation::Current)) ==
         "Spellcast_Shoot");
  assert(std::string(RenderAnimationName(RenderAnimation::Corruption)) ==
         "Spellcasting");
  assert(std::string(RenderAnimationName(RenderAnimation::Ultimate)) ==
         "Spellcast_Long");
}

void testDedicatedActionClipFallbacks() {
  assert(ResolveClip({"idle", "run"}, RenderAnimation::Dodge) == "run");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Radiance) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Current) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Corruption) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Ultimate) ==
         "attack");
}

void testExplicitActionPriority() {
  ActorRenderState actor;
  actor.action = RenderAnimation::Dodge;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Dodge);
  actor.alive = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);
}

void testPlayerCombatActionMapsToDedicatedAnimation() {
  assert(PlayerRenderAnimation(ActionState::Idle, CombatAction::Attack) ==
         RenderAnimation::Idle);
  assert(PlayerRenderAnimation(ActionState::Attack1, CombatAction::Attack) ==
         RenderAnimation::Attack);
  assert(PlayerRenderAnimation(ActionState::Attack4, CombatAction::Attack) ==
         RenderAnimation::Attack);
  assert(PlayerRenderAnimation(ActionState::Dodging, CombatAction::Dodge) ==
         RenderAnimation::Dodge);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Radiance) ==
         RenderAnimation::Radiance);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Current) ==
         RenderAnimation::Current);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Corruption) ==
         RenderAnimation::Corruption);
  assert(PlayerRenderAnimation(ActionState::CastingUltimate,
                               CombatAction::Ultimate) ==
         RenderAnimation::Ultimate);
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

void testSurfaceKeepsEnemyAnimationStateByStableEntityId() {
  Surface surface;
  surface.enemyAnimationStates.emplace(2001, SkinnedAnimationState{});
  surface.enemyAnimationStates.emplace(2002, SkinnedAnimationState{});

  Enemy3DRenderState remaining;
  remaining.id = 2002;
  surface.enemies3d.push_back(remaining);
  surface.pruneEnemyAnimationStates();

  assert(surface.enemyAnimationStates.size() == 1);
  assert(surface.enemyAnimationStates.find(2002) !=
         surface.enemyAnimationStates.end());
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

void testSurfaceDestroyDoesNotTouchGlOrUnbindAfterMakeCurrentFailure() {
  std::vector<std::string> calls;
  SurfaceDestroyOperations operations;
  operations.makeCurrent = [&calls] {
    calls.emplace_back("make-current");
    return false;
  };
  operations.destroyGlResource = [&calls](SurfaceGlResource resource) {
    calls.emplace_back("gl-destroy-" + std::to_string(static_cast<int>(resource)));
  };
  operations.abandonGpuResources = [&calls] { calls.emplace_back("abandon-cpu"); };
  operations.unbindCurrent = [&calls] { calls.emplace_back("unbind"); };
  operations.destroyEglSurface = [&calls] { calls.emplace_back("destroy-egl-surface"); };
  operations.destroyEglContext = [&calls] { calls.emplace_back("destroy-egl-context"); };
  operations.terminateEglDisplay = [&calls] { calls.emplace_back("terminate-egl-display"); };

  ExecuteSurfaceDestroy(operations);

  assert((calls == std::vector<std::string>{
                       "make-current", "abandon-cpu", "destroy-egl-surface",
                       "destroy-egl-context", "terminate-egl-display"}));
}

void testSurfaceDestroyDestroysGlBeforeUnbindAndEglCleanup() {
  std::vector<std::string> calls;
  SurfaceDestroyOperations operations;
  operations.makeCurrent = [&calls] {
    calls.emplace_back("make-current");
    return true;
  };
  operations.destroyGlResource = [&calls](SurfaceGlResource resource) {
    switch (resource) {
      case SurfaceGlResource::SkinnedModels:
        calls.emplace_back("destroy-skinned-models");
        break;
      case SurfaceGlResource::StaticMeshes:
        calls.emplace_back("destroy-static-meshes");
        break;
      case SurfaceGlResource::Shader3D:
        calls.emplace_back("destroy-shader-3d");
        break;
      case SurfaceGlResource::Program2D:
        calls.emplace_back("destroy-program-2d");
        break;
    }
  };
  operations.abandonGpuResources = [&calls] { calls.emplace_back("abandon-cpu"); };
  operations.unbindCurrent = [&calls] { calls.emplace_back("unbind"); };
  operations.destroyEglSurface = [&calls] { calls.emplace_back("destroy-egl-surface"); };
  operations.destroyEglContext = [&calls] { calls.emplace_back("destroy-egl-context"); };
  operations.terminateEglDisplay = [&calls] { calls.emplace_back("terminate-egl-display"); };

  ExecuteSurfaceDestroy(operations);

  assert((calls == std::vector<std::string>{
                       "make-current", "destroy-skinned-models", "destroy-static-meshes",
                       "destroy-shader-3d", "destroy-program-2d", "unbind",
                       "destroy-egl-surface", "destroy-egl-context", "terminate-egl-display"}));
}

void testEnemy3DRenderStateHasAngleField() {
  Enemy3DRenderState enemy;
  assert(enemy.angle == 0.0f);
  enemy.angle = 1.5f;
  assert(enemy.angle == 1.5f);
}

void testBoss3DRenderStateHasAngleField() {
  Boss3DRenderState boss;
  assert(boss.angle == 0.0f);
  boss.angle = 2.3f;
  assert(boss.angle == 2.3f);
}

void testEncounterEnemySnapshotHasFacingField() {
  EncounterEnemySnapshot enemy;
  assert(enemy.facing.x == 1.0f);
  assert(enemy.facing.y == 0.0f);

  enemy.facing = {0.6f, 0.8f};
  const float angle = std::atan2(enemy.facing.y, enemy.facing.x);
  assert(std::abs(angle - 0.927295218f) < 0.001f);
}

void testEncounterEnemySnapshotEqualityIncludesFacing() {
  EncounterEnemySnapshot a;
  EncounterEnemySnapshot b;
  a.facing = {1.0f, 0.0f};
  b.facing = {0.0f, 1.0f};
  assert(!(a == b));
  b.facing = {1.0f, 0.0f};
  assert(a == b);
}

}  // namespace

int main() {
  testAnimationPriority();
  testClipResolutionFallsBackToIdle();
  testDedicatedActionClipNames();
  testDedicatedActionClipFallbacks();
  testExplicitActionPriority();
  testPlayerCombatActionMapsToDedicatedAnimation();
  testUnavailableRuntimeModelStaysOnFallbackPath();
  testSurfaceStoresLateModelAssetsForContextBoundInitialization();
  testSurfaceKeepsEnemyAnimationStateByStableEntityId();
  testPendingAssetIsConsumedExactlyOnceAfterLateDirtySignal();
  testPendingAssetReplacementAndClearRemainConsumable();
  testSurfaceDestroyDoesNotTouchGlOrUnbindAfterMakeCurrentFailure();
  testSurfaceDestroyDestroysGlBeforeUnbindAndEglCleanup();
  testEnemy3DRenderStateHasAngleField();
  testBoss3DRenderStateHasAngleField();
  testEncounterEnemySnapshotHasFacingField();
  testEncounterEnemySnapshotEqualityIncludesFacing();
  return 0;
}
