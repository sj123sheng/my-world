#include "native/engine/core/loop.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

// 毫秒级时序/软锁定场景只验证 Loop 与训练脉冲行为：排除野外刷怪对
// 时序场景的干扰，清空野外刷怪区隔离之。
void isolateWildSpawns(Loop& loop) { loop.wildSpawn.resetZones({}); }

}  // namespace

int main() {
  static_assert(std::is_same_v<decltype(&Loop::tickOnce), void (Loop::*)(int64_t)>);
  static_assert(std::is_same_v<decltype(&Loop::updateFixed), void (Loop::*)(Tick, int64_t)>);

  // 生产变更破坏点：Loop 若仍把 Surface::player 当世界真值，跨块后局部
  // 坐标会逃出 [0,1)，快照也无法发布完整 ChunkCoord 与缓存/待生成统计。
  Loop infiniteWorldLoop;
  isolateWildSpawns(infiniteWorldLoop);
  infiniteWorldLoop.surface.ready = true;
  infiniteWorldLoop.streamScheduler.setSyncMode(true);
  const float originHeightBeforeTravel =
      infiniteWorldLoop.terrain.heightAt({0, 0}, 0.25f, 0.5f);
  for (int quality = 0; quality < 3; ++quality) {
    infiniteWorldLoop.qualityPreset = quality;
    for (int64_t chunkX = 0; chunkX <= 50; ++chunkX) {
      infiniteWorldLoop.playerWorldPosition =
          NormalizeWorldPosition({chunkX, 0}, 0.25, 0.5);
      infiniteWorldLoop.streamScheduler.beginBurst(1, 1024);
      infiniteWorldLoop.updateFixed(
          static_cast<Tick>(quality * 1000 + chunkX + 1), 16);
      infiniteWorldLoop.tickOnce(0);
      const GameSnapshot streamed = infiniteWorldLoop.snapshot();
      assert(streamed.playerChunkX == chunkX);
      assert(streamed.playerChunkY == 0);
      assert(streamed.playerLocalX >= 0.0 && streamed.playerLocalX < 1.0);
      assert(streamed.playerLocalY >= 0.0 && streamed.playerLocalY < 1.0);
      const int radius = WorldGrid::ActiveRadiusForQuality(quality);
      assert(streamed.activeChunkCount == (radius * 2 + 1) * (radius * 2 + 1));
      const int cacheRadius = radius + 2;
      assert(streamed.cachedChunkCount <=
             (cacheRadius * 2 + 1) * (cacheRadius * 2 + 1));
      assert(streamed.streamingPendingCount >= 0);
    }
  }
  infiniteWorldLoop.playerWorldPosition =
      NormalizeWorldPosition({0, 0}, 0.25, 0.5);
  infiniteWorldLoop.streamScheduler.beginBurst(1, 1024);
  infiniteWorldLoop.updateFixed(4000, 16);
  infiniteWorldLoop.tickOnce(0);
  assert(infiniteWorldLoop.snapshot().playerChunkX == 0);
  assert(infiniteWorldLoop.snapshot().playerLocalX == 0.25);
  assert(infiniteWorldLoop.terrain.heightAt({0, 0}, 0.25f, 0.5f) ==
         originHeightBeforeTravel);

  // V10 必须由 Loop 接线保存/恢复世界种子与规范化后的 WorldPosition，
  // 不能只让 Save 单元测试覆盖序列化器而遗漏真实调用链。
  Loop saveLoop;
  isolateWildSpawns(saveLoop);
  saveLoop.worldSeed = 0x123456789abcdef0ULL;
  saveLoop.playerWorldPosition =
      NormalizeWorldPosition({-37, 22}, 1.75, -0.25);
  const WorldPosition expectedRestoredPosition = saveLoop.playerWorldPosition;
  const std::string savePath =
      "/tmp/my_world_task8_loop_v10_" +
      std::to_string(reinterpret_cast<std::uintptr_t>(&saveLoop)) + ".save";
  std::remove(savePath.c_str());
  assert(saveLoop.saveProgress(savePath));
  Loop restoredLoop;
  isolateWildSpawns(restoredLoop);
  assert(restoredLoop.loadProgress(savePath));
  assert(restoredLoop.worldSeed == saveLoop.worldSeed);
  assert(restoredLoop.playerWorldPosition.chunk ==
         expectedRestoredPosition.chunk);
  assert(restoredLoop.playerWorldPosition.local.x ==
         expectedRestoredPosition.local.x);
  assert(restoredLoop.playerWorldPosition.local.y ==
         expectedRestoredPosition.local.y);
  std::remove(savePath.c_str());

  // 活动 procedural chunk 的植被必须投影到玩家局部坐标；缓存块卸载后
  // Surface 不得继续持有旧块实例。
  Loop foliageRuntimeLoop;
  isolateWildSpawns(foliageRuntimeLoop);
  foliageRuntimeLoop.streamScheduler.setSyncMode(true);
  foliageRuntimeLoop.qualityPreset = 2;
  foliageRuntimeLoop.playerWorldPosition =
      NormalizeWorldPosition({2, 0}, 0.5, 0.5);
  foliageRuntimeLoop.streamScheduler.beginBurst(1, 1024);
  foliageRuntimeLoop.updateFixed(1, 16);
  std::size_t expectedActiveFoliage = 0;
  for (const auto& entry : foliageRuntimeLoop.proceduralChunks) {
    if (entry.second.active) {
      expectedActiveFoliage += entry.second.content.foliage.size();
    }
  }
  assert(expectedActiveFoliage > 0);
  assert(foliageRuntimeLoop.surface.foliageInstances.size() ==
         expectedActiveFoliage);
  std::size_t expectedActiveCollectibles = 0;
  for (const auto& entry : foliageRuntimeLoop.proceduralChunks) {
    if (entry.second.active) {
      expectedActiveCollectibles += entry.second.content.collectibles.size();
    }
  }
  assert(expectedActiveCollectibles > 0);
  assert(foliageRuntimeLoop.proceduralCollectibleCount() ==
         expectedActiveCollectibles);

  // 传送遮罩必须覆盖完整 3x3 安全圈；九块全部 Active 后才复位相机并
  // 恢复画面，不能只等待中心块。
  Loop teleportRingLoop;
  isolateWildSpawns(teleportRingLoop);
  teleportRingLoop.streamScheduler.setSyncMode(true);
  teleportRingLoop.camera.update({0.5f, 0.5f}, {1.0f, 0.0f}, 0.2f);
  assert(std::abs(teleportRingLoop.camera.yaw()) > 0.01f);
  assert(teleportRingLoop.teleportToAnchor(1));
  assert(teleportRingLoop.teleportSafeRingPending.size() == 9);
  teleportRingLoop.streamScheduler.beginBurst(1, 1);
  teleportRingLoop.updateFixed(1, 16);
  assert(!teleportRingLoop.teleportSafeRingPending.empty());
  assert(teleportRingLoop.teleportFlashMs > 0);
  teleportRingLoop.streamScheduler.beginBurst(1, 9);
  teleportRingLoop.updateFixed(2, 16);
  assert(teleportRingLoop.teleportSafeRingPending.empty());
  for (int64_t dy = -1; dy <= 1; ++dy) {
    for (int64_t dx = -1; dx <= 1; ++dx) {
      assert(teleportRingLoop.streamScheduler.isActive(
          {teleportRingLoop.playerWorldPosition.chunk.x + dx,
           teleportRingLoop.playerWorldPosition.chunk.y + dy}));
    }
  }
  assert(teleportRingLoop.teleportFlashMs == 0);
  assert(teleportRingLoop.camera.yaw() ==
         teleportRingLoop.camera.config().defaultYaw);

  // 分块真正回收前必须同步解除锁定、血条滞后与渲染血条关联，不能等到
  // 下一次 WildSpawnSystem 更新后再清理悬空 EntityId。
  Loop recycleAssociationLoop;
  recycleAssociationLoop.streamScheduler.setSyncMode(true);
  recycleAssociationLoop.qualityPreset = 2;
  recycleAssociationLoop.playerWorldPosition =
      NormalizeWorldPosition({3, 0}, 0.5, 0.5);
  recycleAssociationLoop.streamScheduler.beginBurst(1, 1024);
  recycleAssociationLoop.updateFixed(1, 16);
  assert(!recycleAssociationLoop.wildSpawn.snapshot().empty());
  const EntityId recycledEnemyId =
      recycleAssociationLoop.wildSpawn.snapshot().front().id;
  recycleAssociationLoop.currentTarget =
      TargetSelection{static_cast<int32_t>(recycledEnemyId), 0.1f, 0.0f,
                      {1.0f, 0.0f}};
  recycleAssociationLoop.surface.targetMarker3d.active = true;
  recycleAssociationLoop.surface.targetMarker3d.targetId = recycledEnemyId;
  recycleAssociationLoop.enemyHpTrails[recycledEnemyId] = {};
  recycleAssociationLoop.prevAuraMasks[recycledEnemyId] = 1;
  recycleAssociationLoop.surface.enemyHpBars3d.push_back({});
  recycleAssociationLoop.playerWorldPosition =
      NormalizeWorldPosition({50, 0}, 0.5, 0.5);
  recycleAssociationLoop.streamScheduler.beginBurst(1, 1024);
  recycleAssociationLoop.syncInfiniteWorld({0.0f, 1.0f}, {});
  assert(!recycleAssociationLoop.currentTarget.has_value());
  assert(!recycleAssociationLoop.surface.targetMarker3d.active);
  assert(recycleAssociationLoop.surface.targetMarker3d.targetId == 0u);
  assert(recycleAssociationLoop.enemyHpTrails.count(recycledEnemyId) == 0);
  assert(recycleAssociationLoop.prevAuraMasks.count(recycledEnemyId) == 0);
  assert(recycleAssociationLoop.surface.enemyHpBars3d.empty());

  // 动画比例必须跟随控制器平滑后的真实速度，而不是瞬时摇杆幅度。
  Loop locomotionLoop;
  isolateWildSpawns(locomotionLoop);
  locomotionLoop.intent.move = {0.0f, 1.0f};
  locomotionLoop.updateFixed(1, 16);
  const float firstRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
  assert(firstRatio > 0.0f && firstRatio < 0.5f);

  for (Tick tick = 2; tick <= 40; ++tick) {
    locomotionLoop.updateFixed(tick, 16);
  }
  const float settledRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
  assert(settledRatio > 0.95f && settledRatio <= 1.0f);

  locomotionLoop.intent.move = {};
  locomotionLoop.updateFixed(41, 16);
  const float releaseRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
  assert(releaseRatio > 0.0f && releaseRatio < settledRatio);
  for (Tick tick = 42; locomotionLoop.surface.player.moving && tick < 100;
       ++tick) {
    locomotionLoop.updateFixed(tick, 16);
  }
  assert(locomotionLoop.surface.player3dAnimation.moveRatio == 0.0f);

  Loop combatLoop;
  combatLoop.surface.width = 1000;
  combatLoop.surface.height = 800;
  assert(combatLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  combatLoop.processInput();
  assert(combatLoop.touchRouter.activeCount() == 0);
  combatLoop.updateFixed(1, 16);
  assert(combatLoop.snapshot().comboSegment == 1);
  assert(combatLoop.snapshot().targetHp == fp(300));
  combatLoop.updateFixed(10, 144);
  assert(combatLoop.snapshot().targetHp == fp(292));
  combatLoop.stop();
  assert(combatLoop.snapshot().comboSegment == 0);
  assert(combatLoop.snapshot().targetHp == fp(300));

  Loop millisecondLoop;
  isolateWildSpawns(millisecondLoop);
  millisecondLoop.surface.width = 1000;
  millisecondLoop.surface.height = 800;
  millisecondLoop.surface.ready = true;
  for (Tick step = 1; step <= 49; ++step) millisecondLoop.tickOnce(16);
  assert(millisecondLoop.snapshot().hp == fp(100));
  millisecondLoop.tickOnce(16);
  assert(millisecondLoop.snapshot().hp == fp(90));
  assert(millisecondLoop.combatTimeMs() == 800);

  Loop multiStepLoop;
  isolateWildSpawns(multiStepLoop);
  multiStepLoop.surface.width = 1000;
  multiStepLoop.surface.height = 800;
  multiStepLoop.surface.ready = true;
  assert(multiStepLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  multiStepLoop.processInput();
  for (Tick step = 1; step <= 9; ++step) {
    multiStepLoop.updateFixed(step, 16);
  }
  multiStepLoop.tickOnce(64);
  const CombatEventBatch firstFrameEvents = multiStepLoop.combatEvents();
  assert(firstFrameEvents.gameplay.size() == 2);
  assert(firstFrameEvents.gameplay[0].type == GameplayEventType::Hit);
  assert(firstFrameEvents.gameplay[1].type == GameplayEventType::Damage);
  assert(firstFrameEvents.gameplay[0].sequence ==
         firstFrameEvents.gameplay[1].sequence);
  multiStepLoop.tickOnce(16);
  assert(multiStepLoop.combatEvents().gameplay.empty());

  // 命中卡肉：玩家命中后逻辑短暂冻结，tick 不推进；顿帧结束后恢复。
  Loop hitStopLoop;
  isolateWildSpawns(hitStopLoop);
  hitStopLoop.surface.width = 1000;
  hitStopLoop.surface.height = 800;
  hitStopLoop.surface.ready = true;
  assert(hitStopLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  Tick tickAtHit = 0;
  bool hitObserved = false;
  for (int frame = 0; frame < 20 && !hitObserved; ++frame) {
    hitStopLoop.tickOnce(16);
    if (!hitStopLoop.combatEvents().gameplay.empty()) {
      hitObserved = true;
      tickAtHit = hitStopLoop.snapshot().tick;
    }
  }
  assert(hitObserved);
  assert(hitStopLoop.hitStopRemainingMs > 0);
  // 顿帧窗口（40ms）内 tick 冻结。
  hitStopLoop.tickOnce(16);
  assert(hitStopLoop.snapshot().tick == tickAtHit);
  hitStopLoop.tickOnce(16);
  assert(hitStopLoop.snapshot().tick == tickAtHit);
  // 顿帧结束后逻辑恢复推进。
  hitStopLoop.tickOnce(16);
  hitStopLoop.tickOnce(16);
  assert(hitStopLoop.snapshot().tick > tickAtHit);
  // resetInput 清空未消化的顿帧。
  hitStopLoop.hitStopRemainingMs = 40;
  hitStopLoop.resetInput();
  assert(hitStopLoop.hitStopRemainingMs == 0);

  Loop restartCombatLoop;
  isolateWildSpawns(restartCombatLoop);
  restartCombatLoop.surface.width = 1000;
  restartCombatLoop.surface.height = 800;
  restartCombatLoop.surface.ready = true;
  for (Tick step = 1; step <= 49; ++step) {
    restartCombatLoop.updateFixed(step, 16);
  }
  restartCombatLoop.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  assert(restartCombatLoop.snapshot().hp == fp(100));
  restartCombatLoop.stop();

  CombatConfig fragileTargetConfig = CombatConfig::defaults();
  fragileTargetConfig.trainingTargetHp = fp(8);
  Loop lethalLoop;
  isolateWildSpawns(lethalLoop);
  lethalLoop.combat = CombatController(fragileTargetConfig);
  lethalLoop.surface.width = 1000;
  lethalLoop.surface.height = 800;
  lethalLoop.surface.ready = true;
  assert(lethalLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  for (int frame = 0; frame < 10; ++frame) lethalLoop.tickOnce(16);
  const GameSnapshot lethal = lethalLoop.snapshot();
  assert(lethal.targetHp == 0);
  assert(lethal.targetId == 0);
  assert(lethal.targetDist == 0.0f);

  Loop loop;

  assert(loop.enqueueInput(InputAction::PointerDown, 42, 10.0f, 20.0f));

  InputEvent event{};
  assert(loop.input.pop(event));
  assert(event.action == InputAction::PointerDown);
  assert(event.pointerId == 42);
  assert(event.x == 10.0f);
  assert(event.y == 20.0f);
  assert(event.sequence == 0);

  const GameSnapshot initial = loop.snapshot();
  assert(initial.tick == 0);
  assert(initial.targetId == 0);
  assert(initial.bossPhase == 0);
  assert(initial.moveX == 0.0f && initial.moveY == 0.0f);
  assert(initial.cameraYaw == 0.0f);
  assert(initial.targetDist == 0.0f);

  loop.surface.width = 1000;
  loop.surface.height = 800;
  const Vec2 renderProbe{0.8f, 0.7f};
  const Vec2 renderedAtDefaults =
      loop.surface.cameraRenderState.worldToView(renderProbe);
  assert(loop.enqueueInput(InputAction::PointerDown, 1, 100.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 1, 180.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerDown, 2, 700.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 2, 750.0f, 430.0f));
  loop.processInput();
  loop.updateFixed(1, 16);
  assert(loop.intent.move.length() > 0.0f);
  assert(loop.camera.yaw() != 0.0f);
  assert(loop.surface.cameraRenderState.yaw() == loop.camera.yaw());
  assert(loop.surface.cameraRenderState.pitch() == loop.camera.pitch());
  assert(loop.surface.cameraRenderState.distance() == loop.camera.distance());
  const Vec2 renderedBeforeDistance =
      loop.surface.cameraRenderState.worldToView(renderProbe);
  assert(!(renderedBeforeDistance == renderedAtDefaults));
  loop.camera.setDistance(loop.camera.config().maxDistance);
  loop.updateFixed(2, 16);
  const Vec2 renderedAfterDistance =
      loop.surface.cameraRenderState.worldToView(renderProbe);
  assert(!(renderedAfterDistance == renderedBeforeDistance));
  assert(loop.surface.player.moving);
  loop.resetInput();
  assert(loop.intent.move == Vec2{});
  assert(loop.intent.lookDelta == Vec2{});
  assert(loop.touchRouter.activeCount() == 0);

  Loop orderedCameraLoop;
  orderedCameraLoop.surface.width = 1000;
  orderedCameraLoop.surface.height = 800;
  // 期望值从默认灵敏度推导，避免调参后硬编码过期。
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerDown, 10, 700.0f,
                                        400.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerMove, 10, 750.0f,
                                        430.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerUp, 10, 750.0f,
                                        430.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerDown, 11, 800.0f,
                                        400.0f));
  orderedCameraLoop.processInput();
  const CameraGestureConfig gestureDefaults{};
  assert((orderedCameraLoop.intent.lookDelta ==
          Vec2{50.0f * gestureDefaults.sensitivityX,
               30.0f * gestureDefaults.sensitivityY}));

  Loop cancelLoop;
  cancelLoop.surface.width = 1000;
  cancelLoop.surface.height = 800;
  assert(cancelLoop.enqueueInput(InputAction::PointerDown, 20, 100.0f,
                                 400.0f));
  assert(cancelLoop.enqueueInput(InputAction::PointerMove, 20, 150.0f,
                                 400.0f));
  assert(cancelLoop.enqueueInput(InputAction::PointerDown, 21, 700.0f,
                                 400.0f));
  assert(cancelLoop.enqueueInput(InputAction::PointerMove, 21, 730.0f,
                                 420.0f));
  assert(cancelLoop.enqueueInput(InputAction::PointerCancel, 21, 730.0f,
                                 420.0f));
  assert(cancelLoop.enqueueInput(InputAction::PointerMove, 20, 180.0f,
                                 400.0f));
  cancelLoop.processInput();
  assert(cancelLoop.touchRouter.activeCount() == 1);
  assert(cancelLoop.touchRouter.role(20) == TouchRole::Movement);
  assert(std::abs(cancelLoop.intent.move.x - 0.8f) < 0.0001f);
  assert((cancelLoop.intent.lookDelta ==
          Vec2{30.0f * gestureDefaults.sensitivityX,
               20.0f * gestureDefaults.sensitivityY}));

  assert(loop.enqueueInput(InputAction::PointerDown, 3, 100.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 3, 180.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerUp, 3, 180.0f, 400.0f));
  loop.processInput();
  loop.updateFixed(2, 16);
  assert(loop.intent.move == Vec2{});
  // 平滑减速：松开摇杆后速度经数帧衰减至停止，而非瞬间归零。
  for (Tick decelTick = 3; loop.surface.player.moving && decelTick < 100;
       ++decelTick) {
    loop.updateFixed(decelTick, 16);
  }
  assert(!loop.surface.player.moving);
  assert(loop.touchRouter.activeCount() == 0);

  assert(loop.enqueueInput(InputAction::PointerDown, 4, 700.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 4, 750.0f, 430.0f));
  loop.processInput();
  assert(loop.intent.lookDelta.length() > 0.0f);
  loop.stop();
  assert(loop.intent.move == Vec2{});
  assert(loop.intent.lookDelta == Vec2{});
  assert(loop.touchRouter.activeCount() == 0);

  Loop targetingLoop;
  isolateWildSpawns(targetingLoop);
  targetingLoop.surface.width = 1000;
  targetingLoop.surface.height = 800;
  targetingLoop.surface.ready = true;
  // 场景 props 只参与渲染；即使与假人重合，也不能抢占战斗软锁定。
  targetingLoop.surface.props.push_back({0.5f, 0.8f, 0.05f, 1});
  targetingLoop.tickOnce(16);
  const GameSnapshot targeted = targetingLoop.snapshot();
  assert(targeted.targetId == static_cast<int32_t>(CombatController::kTrainingTargetId));
  assert(targetingLoop.surface.props.size() == 1);
  // 训练假人不是敌人原型，焦点框隐藏（archetype = -1）。
  assert(targeted.targetArchetype == -1);
  // 出生点 (0.5, 0.12) 到训练假人 (0.5, 0.8) 的距离；
  // 人工结构不再推出玩家，保持几何直线距离 0.68。
  assert(std::abs(targeted.targetDist - 0.68f) < 0.001f);
  assert(targetingLoop.playerWorldPosition.chunk == (ChunkCoord{0, 0}));
  assert(std::abs(targetingLoop.playerWorldPosition.local.x - 0.5f) <
         0.0001f);
  assert(std::abs(targetingLoop.playerWorldPosition.local.y - 0.12f) <
         0.0001f);

  targetingLoop.resetInput();
  targetingLoop.tickOnce(0);
  const GameSnapshot resetWithoutFixedTick = targetingLoop.snapshot();
  assert(resetWithoutFixedTick.targetId == 0);
  assert(resetWithoutFixedTick.targetDist == 0.0f);

  targetingLoop.tickOnce(16);
  assert(targetingLoop.snapshot().targetId ==
         static_cast<int32_t>(CombatController::kTrainingTargetId));

  targetingLoop.surface.player.angle = 1.5707963f;
  targetingLoop.surface.player.velocity = {};
  targetingLoop.surface.player.moving = false;
  const float facingBeforeOrbit = targetingLoop.surface.player.angle;
  const float yawBeforeOrbit = targetingLoop.camera.yaw();

  assert(targetingLoop.enqueueInput(InputAction::PointerDown, 90,
                                    700.0f, 400.0f));
  assert(targetingLoop.enqueueInput(InputAction::PointerMove, 90,
                                    780.0f, 400.0f));
  targetingLoop.tickOnce(16);
  assert(targetingLoop.camera.yaw() != yawBeforeOrbit);
  assert(std::abs(targetingLoop.surface.player.angle - facingBeforeOrbit) <
         0.0001f);
  assert(targetingLoop.snapshot().targetId ==
         static_cast<int32_t>(CombatController::kTrainingTargetId));

  Loop enemyEncounterLoop;
  isolateWildSpawns(enemyEncounterLoop);
  enemyEncounterLoop.surface.width = 1000;
  enemyEncounterLoop.surface.height = 800;
  enemyEncounterLoop.surface.ready = true;
  assert(enemyEncounterLoop.encounter.snapshot().mode == EncounterMode::Training);
  assert(enemyEncounterLoop.startEncounter(EncounterMode::Mixed));
  enemyEncounterLoop.tickOnce(16);
  const EntityId selectedEnemy =
      static_cast<EntityId>(enemyEncounterLoop.snapshot().targetId);
  assert(selectedEnemy != 0 &&
         selectedEnemy != CombatController::kTrainingTargetId);
  // 焦点框：锁定敌人时暴露原型与血量比例。
  assert(enemyEncounterLoop.snapshot().targetArchetype >= 0);
  assert(enemyEncounterLoop.snapshot().targetHpRatio > 0.0f);
  assert(enemyEncounterLoop.snapshot().targetHpRatio <= 1.0f);
  // Mixed 模式固定 3 名敌人；容量上限 kMaxEnemies 已放开到 8。
  assert(enemyEncounterLoop.encounter.snapshot().enemies.size() == 3);
  assert(enemyEncounterLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  for (int frame = 0; frame < 10; ++frame) enemyEncounterLoop.tickOnce(16);
  const auto damagedEnemy = std::find_if(
      enemyEncounterLoop.encounter.snapshot().enemies.begin(),
      enemyEncounterLoop.encounter.snapshot().enemies.end(),
      [selectedEnemy](const EncounterEnemySnapshot& enemy) {
        return enemy.id == selectedEnemy;
      });
  assert(damagedEnemy != enemyEncounterLoop.encounter.snapshot().enemies.end());
  assert(damagedEnemy->hp < damagedEnemy->maxHp);  // 受击后血量低于上限

  CombatConfig fragileEnemyConfig = CombatConfig::defaults();
  fragileEnemyConfig.comboDamage = {fp(300), fp(300), fp(300), fp(300)};
  Loop staleTargetLoop;
  isolateWildSpawns(staleTargetLoop);
  staleTargetLoop.combat = CombatController(fragileEnemyConfig);
  staleTargetLoop.surface.width = 1000;
  staleTargetLoop.surface.height = 800;
  staleTargetLoop.surface.ready = true;
  assert(staleTargetLoop.startEncounter(EncounterMode::Beast));
  staleTargetLoop.tickOnce(16);
  const int32_t staleTargetId = staleTargetLoop.snapshot().targetId;
  assert(staleTargetId != 0);
  assert(staleTargetLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  for (int frame = 0; frame < 10; ++frame) staleTargetLoop.tickOnce(16);
  assert(staleTargetLoop.encounter.snapshot().state == EncounterState::Victory);
  assert(staleTargetLoop.snapshot().targetId == 0);
  assert(staleTargetLoop.enqueueInput(InputAction::Attack, -1, 0.0f, 0.0f));
  for (int frame = 0; frame < 10; ++frame) staleTargetLoop.tickOnce(16);
  assert(staleTargetLoop.encounter.events().combat.gameplay.empty());

  enemyEncounterLoop.stop();
  assert(enemyEncounterLoop.combatEvents().gameplay.empty());
  assert(enemyEncounterLoop.encounter.snapshot().state == EncounterState::Stopped);
  GameSnapshot activeRenderer = targetingLoop.snapshot();
  activeRenderer.moving = true;
  activeRenderer.moveX = 0.75f;
  activeRenderer.moveY = -0.25f;
  targetingLoop.snapshots.publish(activeRenderer);
  targetingLoop.publishRendererStopped();
  const GameSnapshot stopped = targetingLoop.snapshot();
  assert(!stopped.rendererReady);
  assert(!stopped.moving);
  assert(stopped.moveX == 0.0f && stopped.moveY == 0.0f);
  assert(stopped.targetId == 0);
  assert(stopped.targetDist == 0.0f);

  Loop pausedLoop;
  isolateWildSpawns(pausedLoop);
  pausedLoop.surface.width = 1000;
  pausedLoop.surface.height = 800;
  pausedLoop.surface.ready = true;
  pausedLoop.surface.props.push_back({0.5f, 0.8f, 0.05f, 1});
  assert(pausedLoop.enqueueInput(InputAction::PointerDown, 40, 100.0f,
                                 400.0f));
  assert(pausedLoop.enqueueInput(InputAction::PointerMove, 40, 180.0f,
                                 400.0f));
  pausedLoop.tickOnce(16);
  assert(pausedLoop.snapshot().rendererReady);
  assert(pausedLoop.snapshot().moving);
  assert(pausedLoop.snapshot().targetId ==
         static_cast<int32_t>(CombatController::kTrainingTargetId));
  pausedLoop.stop();
  const GameSnapshot paused = pausedLoop.snapshot();
  assert(paused.rendererReady);
  assert(!paused.moving);
  assert(paused.moveX == 0.0f && paused.moveY == 0.0f);
  assert(paused.targetId == 0);
  assert(paused.targetDist == 0.0f);

  assert(targetingLoop.enqueueInput(InputAction::PointerDown, 5, 100.0f,
                                    400.0f));
  assert(targetingLoop.enqueueInput(InputAction::PointerMove, 5, 180.0f,
                                    400.0f));
  targetingLoop.tickOnce(16);
  const GameSnapshot activeBeforeInvalid = targetingLoop.snapshot();
  assert(activeBeforeInvalid.rendererReady);
  assert(activeBeforeInvalid.moving);
  assert(activeBeforeInvalid.moveX != 0.0f || activeBeforeInvalid.moveY != 0.0f);
  assert(activeBeforeInvalid.targetId ==
         static_cast<int32_t>(CombatController::kTrainingTargetId));
  targetingLoop.surface.ready = false;
  targetingLoop.tickOnce(16);
  assert(targetingLoop.intent.move == Vec2{});
  assert(targetingLoop.touchRouter.activeCount() == 0);
  const GameSnapshot invalidSurface = targetingLoop.snapshot();
  assert(!invalidSurface.rendererReady);
  assert(!invalidSurface.moving);
  assert(invalidSurface.moveX == 0.0f && invalidSurface.moveY == 0.0f);
  assert(invalidSurface.targetId == 0);
  assert(invalidSurface.targetDist == 0.0f);

  Loop particleLoop;
  particleLoop.intent.move = {1.0f, 0.0f};
  particleLoop.updateFixed(1, 60);
  assert(particleLoop.surface.particles.size() == 1);
  assert(std::abs(particleLoop.surface.particles.front().life - 0.34f) <
         0.0001f);
  particleLoop.intent.move = {};
  particleLoop.updateFixed(2, 200);
  assert(std::abs(particleLoop.surface.particles.front().life - 0.14f) <
         0.0001f);
  particleLoop.updateFixed(3, 200);
  assert(particleLoop.surface.particles.empty());

  Loop resetParticleTimerLoop;
  resetParticleTimerLoop.updateFixed(1, 40);
  resetParticleTimerLoop.resetInput();
  resetParticleTimerLoop.intent.move = {1.0f, 0.0f};
  resetParticleTimerLoop.updateFixed(2, 20);
  assert(resetParticleTimerLoop.surface.particles.empty());

  Loop restartedLoop;
  restartedLoop.surface.width = 1000;
  restartedLoop.surface.height = 800;
  restartedLoop.surface.ready = true;
  assert(restartedLoop.enqueueInput(InputAction::PointerDown, 30, 100.0f,
                                    400.0f));
  assert(restartedLoop.enqueueInput(InputAction::PointerMove, 30, 200.0f,
                                    400.0f));
  restartedLoop.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  restartedLoop.stop();
  // start() 会 resetInput 清空排队输入，玩家应保持在环境出生点；
  // 建筑碰撞可能把贴墙出生点向外推出 ≤ 0.002，不视为位移。
  const EnvironmentComposition restartedSpawn =
      EnvironmentController::defaultComposition();
  assert(restartedLoop.surface.player.x == restartedSpawn.spawn.x);
  assert(std::abs(restartedLoop.surface.player.y - restartedSpawn.spawn.z) <=
         0.0021f);

  // 暂停：冻结固定步与输入消费，恢复后继续推进。
  Loop pauseLoop;
  isolateWildSpawns(pauseLoop);
  pauseLoop.surface.width = 1000;
  pauseLoop.surface.height = 800;
  pauseLoop.surface.ready = true;
  pauseLoop.tickOnce(16);
  const Tick tickBeforePause = pauseLoop.snapshot().tick;
  assert(pauseLoop.enqueueInput(InputAction::PointerDown, 60, 100.0f, 400.0f));
  assert(pauseLoop.enqueueInput(InputAction::PointerMove, 60, 180.0f, 400.0f));
  pauseLoop.setPaused(true);  // 暂停同时清空排队输入
  pauseLoop.tickOnce(16);
  pauseLoop.tickOnce(16);
  assert(pauseLoop.snapshot().tick == tickBeforePause);
  pauseLoop.setPaused(false);
  pauseLoop.tickOnce(16);
  assert(pauseLoop.snapshot().tick > tickBeforePause);
  // 暂停期间的陈旧输入已被清除，玩家不移动。
  assert(!pauseLoop.surface.player.moving);
  assert(pauseLoop.intent.move == Vec2{});

  for (int round = 0; round < 20; ++round) {
    Loop concurrentLoop;
    constexpr int producerCount = 8;
    constexpr int eventsPerProducer = 32;
    std::atomic<bool> start{false};
    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer) {
      producers.emplace_back([&, producer]() {
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        for (int index = 0; index < eventsPerProducer; ++index) {
          assert(concurrentLoop.enqueueInput(InputAction::PointerMove,
                                             producer, float(index), 0.0f));
        }
      });
    }
    start.store(true, std::memory_order_release);
    for (auto& producer : producers) producer.join();

    InputEvent concurrentEvent{};
    uint64_t expectedSequence = 0;
    while (concurrentLoop.input.pop(concurrentEvent)) {
      assert(concurrentEvent.sequence == expectedSequence++);
    }
    assert(expectedSequence == producerCount * eventsPerProducer);
  }
}
