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
  output.radianceCooldownTotalMs = combat.radianceCooldownTotalMs;
  output.currentCooldownTotalMs = combat.currentCooldownTotalMs;
  output.corruptionCooldownTotalMs = combat.corruptionCooldownTotalMs;
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
    state.windingUp = enemy.windingUp;
    // 模型局部 +Z 为前方，逻辑 (x, y) 映射到 3D (x, z)。
    state.angle = std::atan2(enemy.facing.x, enemy.facing.y);
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
  // 首领吟唱机制期间是玩家的应对窗口：有机制且吟唱未完时显示预警环。
  surface.boss3d.windingUp =
      snapshot.boss.mechanic != BossMechanic::None &&
      snapshot.boss.castRemainingMs > 0;
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
    surface.boss3d.angle = std::atan2(bossDx, bossDy);
  }
}

// 在 surface_draw 前更新 3D 透视相机。yaw/pitch/distance 来自现有 2D
// ThirdPersonCamera，玩家 3D 目标位置取 (player.x, 0.05, player.y)，
// 0.05 为玩家立方体半高，使相机平视角色而非俯视地面。
void update3DCamera(Surface& surface, const ThirdPersonCamera& camera) {
  const glm::vec3 target{surface.player.x + surface.vfxCameraShakeX, 0.05f,
                         surface.player.y + surface.vfxCameraShakeY};
  surface.camera3d.follow(target, camera.yaw(), camera.pitch(),
                          camera.distance());
}

// 按实体 ID 解析世界坐标，供伤害飘字定位。
std::optional<Vec2> resolveEntityPosition(const Surface& surface,
                                          const EncounterSnapshot& encounter,
                                          EntityId id) {
  if (id == CombatController::kPlayerId) {
    return Vec2{surface.player.x, surface.player.y};
  }
  if (id == CombatController::kTrainingTargetId) {
    return Vec2{surface.trainingTarget.x, surface.trainingTarget.y};
  }
  if (id == EncounterController::kBossId) {
    return Vec2{surface.boss3d.x, surface.boss3d.y};
  }
  for (const EncounterEnemySnapshot& enemy : encounter.enemies) {
    if (enemy.id == id) {
      return enemy.position;
    }
  }
  return std::nullopt;
}

// 命中火花发射：LCG 伪随机确定方向/速度（同输入可重现），
// 在命中点爆发一圈向外上扬的短命粒子。kind：0=金橙，1=红。
void spawnHitSparks(Surface& surface, Vec2 position, int kind) {
  if (surface.hitSparks3d.size() > 128) return;
  constexpr int kSparkCount = 6;
  constexpr float kTau = 6.2831853f;
  for (int i = 0; i < kSparkCount; ++i) {
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r0 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r1 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    const float angle =
        (static_cast<float>(i) / static_cast<float>(kSparkCount)) * kTau +
        (r0 - 0.5f) * 0.9f;
    const float speed = 0.02f + 0.025f * r1;
    const float life = 0.22f + 0.1f * r0;
    surface.hitSparks3d.push_back(
        {position.x, 0.02f, position.y, std::cos(angle) * speed,
         0.04f + 0.04f * r1, std::sin(angle) * speed, life, life, kind});
  }
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
      paused.store(false);
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

void Loop::setPaused(bool value) {
  paused.store(value);
  if (value) {
    // 暂停时清空排队输入与摇杆状态，避免恢复后消费陈旧事件。
    resetInput();
  }
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
    damageNumbers.clear();
    surface.damageNumbers3d.clear();
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
    damageNumbers.clear();
    surface.damageNumbers3d.clear();
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
  surface.enemyHitFlash.clear();
  surface.hitSparks3d.clear();
  surface.player3dAnimation.action = RenderAnimation::Idle;
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
  if (paused.load()) {
    // 暂停：冻结输入与固定步逻辑，画面与快照保持最后一帧。
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
  surface.environmentPerfLevel = performanceGuard.level();
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
    LOGI("PROFILE fps=%{public}.1f perf_level=%{public}d environment_ready=%{public}d "
         "environment_draw_calls=%{public}u environment_triangles=%{public}u "
         "environment_texture_tier=%{public}s encounter_mode=%{public}d",
         fps, performanceGuard.level(), static_cast<int>(surface.environmentReady),
         surface.environmentDrawCalls, surface.environmentTriangles,
         performanceGuard.level() >= 4 ? "half" : "full",
         static_cast<int>(encounter.snapshot().mode));
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
  snapshot.environmentReady = surface.environmentReady;
  snapshot.environmentDrawCalls = surface.environmentDrawCalls;
  snapshot.environmentTriangles = surface.environmentTriangles;
  snapshot.encounterMode = static_cast<int32_t>(encounter.snapshot().mode);
  snapshot.encounterState = static_cast<int32_t>(encounter.snapshot().state);
  snapshot.moveX = intent.move.x;
  snapshot.moveY = intent.move.y;
  snapshot.cameraYaw = camera.yaw();
  snapshot.cameraPitch = camera.pitch();
  snapshot.targetDist = currentTarget ? currentTarget->distance : 0.0f;
  // 锁定目标焦点框：解析锁定敌人的原型与血量比例（首领已有专属血条，
  // 不在敌人列表中，保持 archetype = -1 由 HUD 隐藏焦点框）。
  snapshot.targetArchetype = -1;
  snapshot.targetHpRatio = 0.0f;
  if (currentTarget.has_value()) {
    for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
      if (enemy.id == static_cast<EntityId>(currentTarget->id) &&
          enemy.maxHp > 0) {
        snapshot.targetArchetype = static_cast<int32_t>(enemy.archetype);
        snapshot.targetHpRatio = static_cast<float>(enemy.hp) /
                                 static_cast<float>(enemy.maxHp);
        break;
      }
    }
  }
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
  snapshot.bossHpRatio = BossCinematicState::healthRatio(
      encounter.snapshot().boss.hp, BossConfig::karounDefaults().maxHp);
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    snapshot.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  snapshot.debugHud = debugHud_;
  switch (encounter.snapshot().mode) {
    case EncounterMode::Training:
      snapshot.objectiveLabel = "唤醒第一道共鸣";
      break;
    case EncounterMode::Boss:
      snapshot.objectiveLabel = "击破共鸣核心";
      break;
    default:
      snapshot.objectiveLabel = "前往共鸣祭坛";
      break;
  }
  snapshot.resonanceSlots = {
      static_cast<uint8_t>(snapshot.radianceAttached),
      static_cast<uint8_t>(snapshot.currentAttached),
      static_cast<uint8_t>(snapshot.corruptionAttached)};
  snapshot.showDebugHud = debugHud_;
  snapshot.inputEventCount = inputEventCount_;
  snapshots.publish(snapshot);

  if (tickCount <= 5 || tickCount % 60 == 0) {
    LOGI("tickOnce: %{public}d fps=%{public}.1f", tickCount, fps);
  }
}

void Loop::updateFixed(Tick tick, int64_t dtMs) {
  const Vec2 lookDelta = intent.lookDelta;
  intent.lookDelta = {};
  const float dtSeconds = static_cast<float>(dtMs) / 1000.0f;

  // 相机先更新：玩家移动使用本帧最新 yaw，保证操控方向与画面严格一致。
  camera.update({surface.player.x, surface.player.y}, lookDelta, dtSeconds);
  surface.cameraRenderState = camera.renderState();

  playerController.update(surface.player, intent.move, camera.yaw(),
                          dtSeconds);

  // 仅在摇杆有有效输入时发射脚步粒子，避免减速滑行期间误发。
  particleEmitTimer += dtSeconds;
  if (surface.player.moving && intent.move.length() > 0.0f &&
      particleEmitTimer > 0.05f) {
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

  const std::vector<TargetCandidate>& candidates =
      encounter.snapshot().candidates;
  currentTarget = softTargeting.select(
      {surface.player.x, surface.player.y}, camera.yaw(), candidates,
      currentTarget ? std::optional<int32_t>{currentTarget->id} : std::nullopt);

  // 锁定目标指示器：发布目标位置与脉冲相位，目标脚下绘制脉冲环。
  surface.targetMarker3d.active = currentTarget.has_value();
  if (currentTarget.has_value()) {
    const std::optional<Vec2> markerPosition = resolveEntityPosition(
        surface, encounter.snapshot(),
        static_cast<EntityId>(currentTarget->id));
    if (markerPosition.has_value()) {
      surface.targetMarker3d.x = markerPosition->x;
      surface.targetMarker3d.z = markerPosition->y;
    } else {
      surface.targetMarker3d.active = false;
    }
  }
  surface.targetMarker3d.pulsePhase =
      static_cast<float>(combatTimeMs_.load()) * 0.004f;

  for (const ActionRequest& action : intent.actions) combat.enqueue(action);
  intent.actions.clear();
  const Tick combatTime = AdvanceCombatTime(combatTimeMs_.load(), dtMs);
  combatTimeMs_.store(combatTime);
  encounter.update({combatTime, dtMs,
                    {surface.player.x, surface.player.y},
                    surface.player.moving,
                    currentTarget ? static_cast<EntityId>(currentTarget->id) : 0});
  const EncounterSnapshot& encounterState = encounter.snapshot();
  DemoSignals demoSignals;
  demoSignals.introComplete = combatTime >= 1000;
  demoSignals.reachedCombatAnchor = surface.player.y >= 0.45f;
  demoSignals.encounterComplete =
      encounterState.state == EncounterState::Victory;
  demoSignals.allSourcesActive = combat.snapshot().radianceAttached &&
                                 combat.snapshot().currentAttached &&
                                 combat.snapshot().corruptionAttached;
  demoSignals.cinematicComplete = false;
  demoSignals.bossDefeated = encounterState.state == EncounterState::Victory &&
                             encounterState.mode == EncounterMode::Boss;
  demoDirector.tick(combatTime, demoSignals);
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
  std::size_t gameplayEventStart = 0;
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    const CombatEventBatch& stepEvents = encounter.events().combat;
    playerHitObserved = std::any_of(
        stepEvents.presentation.begin(), stepEvents.presentation.end(),
        [](const PresentationEvent& event) {
          return event.target == CombatController::kPlayerId &&
                 event.type == PresentationEventType::HitFlash;
        });
    gameplayEventStart = frameCombatEvents_.gameplay.size();
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

  // 伤害飘字：仅从本步新增的 Damage 事件生成（避免同帧多步重复），
  // 按目标实体定位；玩家受击红色、大额伤害金色、其余近白。
  for (std::size_t eventIndex = gameplayEventStart;
       eventIndex < frameCombatEvents_.gameplay.size(); ++eventIndex) {
    const GameplayEvent& event = frameCombatEvents_.gameplay[eventIndex];
    if (event.type != GameplayEventType::Damage) continue;
    // 非玩家目标受击：启动模型闪白计时器，渲染层据此提亮配色。
    if (event.target != CombatController::kPlayerId) {
      surface.enemyHitFlash[static_cast<uint32_t>(event.target)] = 0.15f;
    }
    const std::optional<Vec2> position = resolveEntityPosition(
        surface, encounter.snapshot(), event.target);
    if (!position.has_value()) continue;
    const float amount =
        static_cast<float>(event.value) / static_cast<float>(FP_ONE);
    DamageNumberKind kind = DamageNumberKind::Normal;
    if (event.target == CombatController::kPlayerId) {
      kind = DamageNumberKind::PlayerHit;
    } else if (amount >= 15.0f) {
      kind = DamageNumberKind::Heavy;
    }
    damageNumbers.spawn(*position, amount, kind);
    // 命中火花：与飘字同源，玩家受击用红色火花，其余金橙。
    spawnHitSparks(surface, *position,
                   event.target == CombatController::kPlayerId ? 1 : 0);
  }
  damageNumbers.update(dtMs);
  surface.damageNumbers3d.clear();
  for (const DamageNumber& number : damageNumbers.active()) {
    DamageNumberRenderState state;
    state.x = number.origin.x;
    state.z = number.origin.y;
    state.rise = DamageNumberSystem::riseOffset(number);
    state.driftX = number.driftX;
    state.alpha = DamageNumberSystem::alpha(number);
    state.value = number.value;
    state.kind = static_cast<int>(number.kind);
    surface.damageNumbers3d.push_back(state);
  }

  // 敌人头顶血条：仅发布存活敌人，比例 = 当前 HP / 最大 HP。
  surface.enemyHpBars3d.clear();
  for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
    if (!enemy.alive || enemy.maxHp <= 0) continue;
    EnemyHpBarRenderState bar;
    bar.x = enemy.position.x;
    bar.z = enemy.position.y;
    bar.ratio = static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp);
    surface.enemyHpBars3d.push_back(bar);
  }

  // 只从 gameplay 快照/事件投影动画意图，不反向写入战斗、AI 或玩家控制器。
  surface.playerHitAnimationSeconds = std::max(
      0.0f, surface.playerHitAnimationSeconds - dtSeconds);
  if (playerHitObserved) surface.playerHitAnimationSeconds = 0.2f;
  // 受击闪白计时器逐帧衰减并清理到期项。
  for (auto flash = surface.enemyHitFlash.begin();
       flash != surface.enemyHitFlash.end();) {
    flash->second -= dtSeconds;
    if (flash->second <= 0.0f) {
      flash = surface.enemyHitFlash.erase(flash);
    } else {
      ++flash;
    }
  }
  // 预警环脉冲时钟：按 0.8s 周期回绕，避免浮点长时间累加精度退化。
  surface.windupPulseSeconds += dtSeconds;
  if (surface.windupPulseSeconds >= 0.8f) surface.windupPulseSeconds -= 0.8f;
  // 命中火花：速度积分 + 重力回落，寿命到期或落地后清理。
  constexpr float kSparkGravity = 0.35f;
  for (HitSpark3D& spark : surface.hitSparks3d) {
    spark.life -= dtSeconds;
    spark.vy -= kSparkGravity * dtSeconds;
    spark.x += spark.vx * dtSeconds;
    spark.y += spark.vy * dtSeconds;
    spark.z += spark.vz * dtSeconds;
  }
  surface.hitSparks3d.erase(
      std::remove_if(surface.hitSparks3d.begin(), surface.hitSparks3d.end(),
                     [](const HitSpark3D& spark) {
                       return spark.life <= 0.0f || spark.y < 0.0f;
                     }),
      surface.hitSparks3d.end());
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  surface.player3dAnimation.alive = combatSnapshot.playerHp > 0;
  surface.player3dAnimation.action = PlayerRenderAnimation(
      static_cast<ActionState>(combatSnapshot.currentAction),
      combatSnapshot.activeCombatAction);
  surface.player3dAnimation.hit = surface.playerHitAnimationSeconds > 0.0f;
  surface.player3dAnimation.moving = surface.player.moving;
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
  surface.vfxFlags = vfxSystem.snapshot().vfxFlags;
  surface.vfxHitFlash = static_cast<float>(vfxSystem.snapshot().hitFlashMs) / 200.0f;
  surface.vfxDodgeFlash = static_cast<float>(vfxSystem.snapshot().dodgeFlashMs) / 400.0f;
  surface.vfxResonanceBurst = static_cast<float>(vfxSystem.snapshot().resonanceBurstMs) / 800.0f;
  surface.vfxCameraShakeX = vfxSystem.snapshot().cameraShakeX;
  surface.vfxCameraShakeY = vfxSystem.snapshot().cameraShakeY;
  updated.bossHpRatio = BossCinematicState::healthRatio(
      encounter.snapshot().boss.hp, BossConfig::karounDefaults().maxHp);
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    updated.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  updated.debugHud = debugHud_;
  switch (demoDirector.phase()) {
    case DemoPhase::Intro:
      updated.objectiveLabel = "进入失落遗迹";
      break;
    case DemoPhase::Explore:
      updated.objectiveLabel = "前往共鸣祭坛";
      break;
    case DemoPhase::Encounter:
      updated.objectiveLabel = "清除遗迹守卫";
      break;
    case DemoPhase::Resonance:
      updated.objectiveLabel = "激活三源共鸣";
      break;
    case DemoPhase::BossIntro:
      updated.objectiveLabel = "共鸣核心正在苏醒";
      break;
    case DemoPhase::BossFight:
      updated.objectiveLabel = "击破共鸣核心";
      break;
    case DemoPhase::Outro:
      updated.objectiveLabel = "遗迹共鸣已完成";
      break;
  }
  updated.showDebugHud = debugHud_;
  updated.inputEventCount = inputEventCount_;
  const BossCinematicState cinematic = BossCinematicState::fromBossHp(
      updated.bossHpRatio);
  const BossCinematicState& introCinematic = demoDirector.bossCinematic();
  updated.bossCinematicProgress = introCinematic.ringProgress;
  updated.bossShardCount = introCinematic.shardCount;
  updated.bossSourceColor = static_cast<uint8_t>(introCinematic.sourceColor);
  updated.bossRingBroken = cinematic.broken;
  surface.boss3d.cinematicProgress = updated.bossCinematicProgress;
  surface.boss3d.shardCount = updated.bossShardCount;
  surface.boss3d.sourceColor = updated.bossSourceColor;
  surface.boss3d.ringBroken = updated.bossRingBroken;
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
