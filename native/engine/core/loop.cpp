#include "loop.h"
#include "native/engine/render/combat_animation.h"
#ifdef OHOS_PLATFORM
#include <hilog/log.h>
#endif
#include <thread>
#include <algorithm>
#include <vector>
#include <limits>
#include <cmath>

#ifdef OHOS_PLATFORM
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#endif

namespace {
void ApplyCombatSnapshot(GameSnapshot& output, const CombatSnapshot& combat) {
  auto applyEncounter = [](GameSnapshot& snap,
                           const EncounterSnapshot& encounter) {
    snap.levelStage = static_cast<int32_t>(encounter.levelStage);
    snap.gateState = static_cast<int32_t>(encounter.gateState);
    snap.supplyState = static_cast<int32_t>(encounter.supplyState);
    snap.bossHp = encounter.boss.hp;
    snap.bossPoise = encounter.boss.poise;
    snap.bossPhase = static_cast<int32_t>(encounter.boss.phase);
    snap.bossMechanic = static_cast<int32_t>(encounter.boss.mechanic);
    snap.bossCastMs = encounter.boss.castRemainingMs;
  };
  (void)applyEncounter;
  output.comboSegment = combat.comboSegment;
  output.hp = combat.playerHp;
  output.poise = combat.playerPoise;
  output.targetHp = combat.targetHp;
  output.targetPoise = combat.targetPoise;
  output.stamina = combat.stamina;
  output.resonance = combat.resonance;
  output.hasInsight = combat.hasInsight;
  output.invulnerable = combat.invulnerable;
  output.insightMs = combat.insightMs;
  output.pulseHitRemainingMs = combat.pulseHitRemainingMs;
  output.lastRejectReason = static_cast<int32_t>(combat.lastRejectReason);
  output.currentAction = combat.currentAction;
  output.comboWindowMs = combat.comboWindowMs;
  output.radianceCooldownMs = combat.radianceCooldownMs;
  output.currentCooldownMs = combat.currentCooldownMs;
  output.corruptionCooldownMs = combat.corruptionCooldownMs;
  output.ultimateWindowMs = combat.ultimateWindowMs;
  output.targetPoiseBroken = combat.targetPoiseBroken;
  output.radianceAttached = combat.radianceAttached;
  output.currentAttached = combat.currentAttached;
  output.corruptionAttached = combat.corruptionAttached;
  output.corroded = combat.corroded;
  output.currentReaction = combat.currentReaction;
  output.pulsePhase = combat.pulsePhase;
}

Tick AdvanceCombatTime(Tick now, int64_t dtMs) {
  if (dtMs <= 0) return now;
  const Tick maximum = std::numeric_limits<Tick>::max();
  return now > maximum - dtMs ? maximum : now + dtMs;
}

// 把当前 EncounterSnapshot 的敌人与首领 2D 位置写入 Surface 的 3D 渲染字段。
// 渲染层只读消费这些状态，不反向修改游戏逻辑。BossSnapshot 无独立位置字段，
// 因此首领位置取固定 (0.5, 0.75)，与 refreshSnapshot 中首领 candidate 位置一致。
void publish3DEncounterState(Surface& surface,
                             const EncounterSnapshot& snapshot,
                             float dtSeconds) {
  surface.enemies3d.clear();
  surface.enemies3d.reserve(snapshot.enemies.size());
  for (const EncounterEnemySnapshot& enemy : snapshot.enemies) {
    Enemy3DRenderState state;
    state.id = enemy.id;
    state.x = enemy.position.x;
    state.y = enemy.position.y;
    state.archetype = static_cast<int>(enemy.archetype);
    state.alive = enemy.alive;
    state.animation.alive = enemy.alive;
    state.animation.action = enemy.attacking ? RenderAnimation::Attack
                                             : RenderAnimation::Idle;
    state.animation.hit = enemy.hit;
    state.animation.moving = enemy.moving;
    // 从敌人 AI 的 facing 向量计算 yaw 弧度，用于 3D 模型旋转。
    // facing 是归一化的 2D 向量，指向玩家方向；atan2(y, x) 给出标准数学角。
    state.angle = std::atan2(enemy.facing.y, enemy.facing.x);
    surface.enemies3d.push_back(state);
  }

  // BossSnapshot 不含位置，按 refreshSnapshot 的首领 candidate 坐标固定。
  surface.boss3d.x = 0.5f;
  surface.boss3d.y = 0.75f;
  surface.boss3d.phase = static_cast<int>(snapshot.boss.phase);
  surface.boss3d.defeated = snapshot.boss.defeated;
  surface.boss3d.active =
      snapshot.mode == EncounterMode::Boss &&
      snapshot.state != EncounterState::Stopped;
  surface.boss3d.animation.alive = !snapshot.boss.defeated;
  surface.boss3d.hitAnimationSeconds = std::max(
      0.0f, surface.boss3d.hitAnimationSeconds - dtSeconds);
  const RenderAnimation bossAnimation = BossRenderAnimation(
      snapshot.boss, surface.boss3d.previousHp);
  if (bossAnimation == RenderAnimation::Hit) {
    surface.boss3d.hitAnimationSeconds = 0.2f;
  }
  surface.boss3d.animation.action =
      bossAnimation == RenderAnimation::Hit ? RenderAnimation::Idle
                                            : bossAnimation;
  surface.boss3d.animation.hit = surface.boss3d.hitAnimationSeconds > 0.0f;
  surface.boss3d.previousHp = snapshot.boss.hp;
  // BossSnapshot 无独立 facing 字段，从首领位置到玩家位置计算朝向角。
  const float bossDx = surface.player.x - surface.boss3d.x;
  const float bossDy = surface.player.y - surface.boss3d.y;
  if (bossDx != 0.0f || bossDy != 0.0f) {
    surface.boss3d.angle = std::atan2(bossDy, bossDx);
  }
}

// 在 surface_draw 前更新 3D 透视相机。yaw/pitch/distance 来自现有 2D
// ThirdPersonCamera，玩家 3D 目标位置取 (player.x, 0.05, player.y)，
// 0.05 为玩家立方体半高，使相机平视角色而非俯视地面。
void update3DCamera(Surface& surface, const ThirdPersonCamera& camera) {
  const glm::vec3 target{surface.player.x, 0.05f, surface.player.y};
  surface.camera3d.follow(target, camera.yaw(), camera.pitch(),
                          camera.distance());
}
}  // namespace

void Loop::start() {
  withLifecycle([this]() {
    if (!surface.ready) {
      resetInput();
      encounter.reset();
      combat.reset();
      combatTimeMs_ = 0;
      {
        std::lock_guard<std::mutex> lock(combatEventMutex);
        frameCombatEvents_ = {};
      }
      LOGI("Loop start skipped: running=%{public}d ready=%{public}d", (int)running, (int)surface.ready);
      return;
    }
    if (!lifecycle.start([this]() {
      resetInput();
      (void)encounter.start(EncounterMode::Training);
      audioBridge.start();
      combatTimeMs_ = 0;
      {
        std::lock_guard<std::mutex> lock(combatEventMutex);
        frameCombatEvents_ = {};
      }
      shouldStop = false;
      running = true;
      tickCount = 0;
      fps = 60.0f;
      lastFpsTime = std::chrono::steady_clock::now();
      runner = std::thread([this]() {
        auto lastTickTime = std::chrono::steady_clock::now();
        while (!shouldStop) {
          const auto now = std::chrono::steady_clock::now();
          const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickTime).count();
          lastTickTime = now;
          tickOnce(std::min<int64_t>(elapsedMs, 250));
          std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
        running = false;
      });
    })) {
      LOGI("Loop start skipped: already running");
    }
  });
}

void Loop::stop() {
  withLifecycle([this]() {
    lifecycle.stop([this]() {
      shouldStop = true;
      if (runner.joinable()) runner.join();
      running = false;
    });
    resetInput();
    encounter.stop();
    audioBridge.stop();
    combat.reset();
    combatTimeMs_ = 0;
    {
      std::lock_guard<std::mutex> lock(combatEventMutex);
      frameCombatEvents_ = {};
    }
    surface.trainingTarget.alive = true;
    GameSnapshot paused = snapshots.read();
    paused.moving = false;
    paused.targetId = 0;
    paused.moveX = 0.0f;
    paused.moveY = 0.0f;
    paused.targetDist = 0.0f;
    paused.rendererReady = surface.ready;
    ApplyCombatSnapshot(paused, combat.snapshot());
    snapshots.publish(paused);
  });
}

bool Loop::startEncounter(EncounterMode mode) {
  return withLifecycle([this, mode]() {
    currentTarget.reset();
    intent.actions.clear();
    combatTimeMs_ = 0;
    {
      std::lock_guard<std::mutex> lock(combatEventMutex);
      frameCombatEvents_ = {};
    }
    const bool started = encounter.start(mode);
    surface.trainingTarget.alive =
        started && mode == EncounterMode::Training;
    return started;
  });
}

bool Loop::advanceLevel() {
  return withLifecycle([this]() { return encounter.advanceLevel(); });
}

bool Loop::useSupply() {
  return withLifecycle([this]() { return encounter.useSupply(); });
}

bool Loop::retryBoss() {
  return withLifecycle([this]() {
    currentTarget.reset();
    intent.actions.clear();
    return encounter.retryBoss();
  });
}

void Loop::toggleDebugHud() {
  debugHud_ = !debugHud_;
}
void Loop::processInput() {
  InputEvent e;
  while (input.pop(e)) {
    CombatAction combatAction;
    if (TryMapCombatAction(e.action, combatAction)) {
      intent.actions.push_back({combatAction, e.sequence});
      continue;
    }
    const TouchRole releaseRole = touchRouter.role(e.pointerId);
    switch (e.action) {
      case InputAction::PointerDown: {
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        const TouchRole role = touchRouter.role(e.pointerId);
        if (role == TouchRole::Movement) {
          joystick.begin(e.pointerId, {e.x, e.y});
        } else if (role == TouchRole::Camera) {
          cameraGesture.begin(e.pointerId, {e.x, e.y});
        }
        break;
      }
      case InputAction::PointerMove:
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        if (releaseRole == TouchRole::Movement) {
          joystick.move(e.pointerId, {e.x, e.y});
        } else if (releaseRole == TouchRole::Camera) {
          cameraGesture.move(e.pointerId, {e.x, e.y});
        }
        break;
      case InputAction::PointerUp:
      case InputAction::PointerCancel:
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        if (releaseRole == TouchRole::Movement) {
          joystick.end(e.pointerId);
        } else if (releaseRole == TouchRole::Camera) {
          cameraGesture.end(e.pointerId);
        }
        break;
      default:
        break;
    }
  }
  intent.move = joystick.value();
  intent.lookDelta = intent.lookDelta + cameraGesture.consumeDelta();
}

void Loop::resetInput() {
  touchRouter.clear();
  joystick.clear();
  cameraGesture.clear();
  intent.move = {};
  intent.lookDelta = {};
  intent.actions.clear();
  surface.player.moving = false;
  surface.playerHitAnimationSeconds = 0.0f;
  surface.player3dAnimation.attacking = false;
  surface.player3dAnimation.hit = false;
  surface.player3dAnimation.moving = false;
  particleEmitTimer = 0.0f;
  currentTarget.reset();
  input.clear();
}

void Loop::tickOnce(int64_t elapsedMs) {
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    frameCombatEvents_ = {};
  }
  if (!surface.ready) {
    resetInput();
    encounter.stop();
    combat.reset();
    combatTimeMs_ = 0;
    surface.trainingTarget.alive = true;
    publishRendererStopped();
    return;
  }
  if (encounter.snapshot().state == EncounterState::Stopped) {
    (void)encounter.start(EncounterMode::Training);
  }
  processInput();
  fixedStep.advance(elapsedMs, [this](Tick tick, int64_t dtMs) {
    updateFixed(tick, dtMs);
  });
#ifdef OHOS_PLATFORM
  update3DCamera(surface, camera);
  surface_draw(surface);
  surface_swap(surface);
#endif

  tickCount++;
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime).count();
  if (elapsed >= 1000) {
    fps = tickCount * 1000.0f / (float)elapsed;
    tickCount = 0;
    lastFpsTime = now;
  }
  performanceGuard.sample(fixedStep.tick(), 16, fps);

  GameSnapshot snapshot;
  snapshot.tick = fixedStep.tick();
  snapshot.playerX = surface.player.x;
  snapshot.playerY = surface.player.y;
  snapshot.fps = fps;
  snapshot.moving = surface.player.moving;
  snapshot.targetId = currentTarget ? currentTarget->id : 0;
  snapshot.rendererReady = surface.ready;
  snapshot.encounterMode = static_cast<int32_t>(encounter.snapshot().mode);
  snapshot.encounterState = static_cast<int32_t>(encounter.snapshot().state);
  snapshot.moveX = intent.move.x;
  snapshot.moveY = intent.move.y;
  snapshot.cameraYaw = camera.yaw();
  snapshot.cameraPitch = camera.pitch();
  snapshot.targetDist = currentTarget ? currentTarget->distance : 0.0f;
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  ApplyCombatSnapshot(snapshot, combatSnapshot);
  snapshot.levelStage = static_cast<int32_t>(encounter.snapshot().levelStage);
  snapshot.gateState = static_cast<int32_t>(encounter.snapshot().gateState);
  snapshot.supplyState = static_cast<int32_t>(encounter.snapshot().supplyState);
  snapshot.bossHp = encounter.snapshot().boss.hp;
  snapshot.bossPoise = encounter.snapshot().boss.poise;
  snapshot.bossPhase = static_cast<int32_t>(encounter.snapshot().boss.phase);
  snapshot.bossMechanic = static_cast<int32_t>(encounter.snapshot().boss.mechanic);
  snapshot.bossCastMs = encounter.snapshot().boss.castRemainingMs;
  snapshot.perfLevel = performanceGuard.level();
  snapshot.vfxFlags = vfxSystem.snapshot().vfxFlags;
  snapshot.cameraShakeX = vfxSystem.snapshot().cameraShakeX;
  snapshot.cameraShakeY = vfxSystem.snapshot().cameraShakeY;
  snapshot.bossHpRatio = static_cast<float>(encounter.snapshot().boss.hp) /
                          static_cast<float>(encounter.snapshot().boss.hp + fp(1));
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    snapshot.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  snapshot.debugHud = debugHud_;
  snapshots.publish(snapshot);

  if (tickCount <= 5 || tickCount % 60 == 0) {
    LOGI("tickOnce: %{public}d fps=%{public}.1f", tickCount, fps);
  }
}

void Loop::updateFixed(Tick tick, int64_t dtMs) {
  const Vec2 lookDelta = intent.lookDelta;
  intent.lookDelta = {};
  const float dtSeconds = static_cast<float>(dtMs) / 1000.0f;
  playerController.update(surface.player, intent.move, camera.yaw(),
                          dtSeconds);

  particleEmitTimer += dtSeconds;
  if (surface.player.moving && particleEmitTimer > 0.05f) {
    particleEmitTimer = 0.0f;
    surface.particles.push_back({surface.player.x, surface.player.y, 0.4f,
                                 0.4f});
  }
  for (Particle& particle : surface.particles) {
    particle.life -= dtSeconds;
  }
  surface.particles.erase(
      std::remove_if(surface.particles.begin(), surface.particles.end(),
                     [](const Particle& particle) {
                       return particle.life <= 0.0f;
                     }),
      surface.particles.end());

  camera.update({surface.player.x, surface.player.y}, lookDelta, dtSeconds);
  surface.cameraRenderState = camera.renderState();

  const std::vector<TargetCandidate>& candidates =
      encounter.snapshot().candidates;
  currentTarget = softTargeting.select(
      {surface.player.x, surface.player.y}, camera.yaw(), candidates,
      currentTarget ? std::optional<int32_t>{currentTarget->id} : std::nullopt);

  for (const ActionRequest& action : intent.actions) combat.enqueue(action);
  intent.actions.clear();
  const Tick combatTime = AdvanceCombatTime(combatTimeMs_.load(), dtMs);
  combatTimeMs_.store(combatTime);
  encounter.update({combatTime, dtMs,
                    {surface.player.x, surface.player.y},
                    surface.player.moving,
                    currentTarget ? static_cast<EntityId>(currentTarget->id) : 0});
  surface.trainingTarget.alive =
      encounter.snapshot().mode == EncounterMode::Training &&
      combat.snapshot().targetAlive;
  if (currentTarget.has_value() &&
      std::none_of(encounter.snapshot().candidates.begin(),
                   encounter.snapshot().candidates.end(),
                   [this](const TargetCandidate& candidate) {
                     return candidate.id == currentTarget->id;
                   })) {
    currentTarget.reset();
  }

  bool playerHitObserved = false;
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    const CombatEventBatch& stepEvents = encounter.events().combat;
    playerHitObserved = std::any_of(
        stepEvents.presentation.begin(), stepEvents.presentation.end(),
        [](const PresentationEvent& event) {
          return event.target == CombatController::kPlayerId &&
                 event.type == PresentationEventType::HitFlash;
        });
    frameCombatEvents_.gameplay.insert(frameCombatEvents_.gameplay.end(),
                                       stepEvents.gameplay.begin(),
                                       stepEvents.gameplay.end());
    frameCombatEvents_.presentation.insert(frameCombatEvents_.presentation.end(),
                                           stepEvents.presentation.begin(),
                                           stepEvents.presentation.end());
    const auto less = [](const auto& left, const auto& right) {
      if (left.tick != right.tick) return left.tick < right.tick;
      if (left.source != right.source) return left.source < right.source;
      if (left.target != right.target) return left.target < right.target;
      return left.sequence < right.sequence;
    };
    std::stable_sort(frameCombatEvents_.gameplay.begin(), frameCombatEvents_.gameplay.end(), less);
   std::stable_sort(frameCombatEvents_.presentation.begin(), frameCombatEvents_.presentation.end(), less);
  }

  vfxSystem.consume(frameCombatEvents_);
  vfxSystem.update(combatTime, dtMs);
  audioBridge.dispatch(frameCombatEvents_);

  // 只从 gameplay 快照/事件投影动画意图，不反向写入战斗、AI 或玩家控制器。
  surface.playerHitAnimationSeconds = std::max(
      0.0f, surface.playerHitAnimationSeconds - dtSeconds);
  if (playerHitObserved) surface.playerHitAnimationSeconds = 0.2f;
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  surface.player3dAnimation.alive = combatSnapshot.playerHp > 0;
  surface.player3dAnimation.action = PlayerRenderAnimation(
      static_cast<ActionState>(combatSnapshot.currentAction),
      combatSnapshot.activeCombatAction);
  surface.player3dAnimation.hit = surface.playerHitAnimationSeconds > 0.0f;
  surface.player3dAnimation.moving = surface.player.moving;
#ifdef OHOS_PLATFORM
  if (surface.player3dAnimation.action != RenderAnimation::Idle) {
    LOGI("player3dAnimation action=%{public}s state=%{public}d",
         RenderAnimationName(surface.player3dAnimation.action),
         static_cast<int>(combatSnapshot.currentAction));
  }
#endif
  surface.trainingTarget3dAnimation.alive = surface.trainingTarget.alive;
  publish3DEncounterState(surface, encounter.snapshot(), dtSeconds);

  GameSnapshot updated = snapshots.read();
  ApplyCombatSnapshot(updated, combat.snapshot());
  updated.encounterMode = static_cast<int32_t>(encounter.snapshot().mode);
  updated.encounterState = static_cast<int32_t>(encounter.snapshot().state);
  updated.levelStage = static_cast<int32_t>(encounter.snapshot().levelStage);
  updated.gateState = static_cast<int32_t>(encounter.snapshot().gateState);
  updated.supplyState = static_cast<int32_t>(encounter.snapshot().supplyState);
  updated.bossHp = encounter.snapshot().boss.hp;
  updated.bossPoise = encounter.snapshot().boss.poise;
  updated.bossPhase = static_cast<int32_t>(encounter.snapshot().boss.phase);
  updated.bossMechanic = static_cast<int32_t>(encounter.snapshot().boss.mechanic);
  updated.bossCastMs = encounter.snapshot().boss.castRemainingMs;
  updated.perfLevel = performanceGuard.level();
  updated.vfxFlags = vfxSystem.snapshot().vfxFlags;
  updated.cameraShakeX = vfxSystem.snapshot().cameraShakeX;
  updated.cameraShakeY = vfxSystem.snapshot().cameraShakeY;
  updated.bossHpRatio = static_cast<float>(encounter.snapshot().boss.hp) /
                        static_cast<float>(encounter.snapshot().boss.hp + fp(1));
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    updated.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  updated.debugHud = debugHud_;
  snapshots.publish(updated);
}

void Loop::publishRendererStopped() {
  currentTarget.reset();
  encounter.stop();
  combat.reset();
  combatTimeMs_ = 0;
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    frameCombatEvents_ = {};
  }
  surface.trainingTarget.alive = true;
  GameSnapshot stopped = RendererStoppedSnapshot(snapshots.read());
  ApplyCombatSnapshot(stopped, combat.snapshot());
  snapshots.publish(stopped);
}
