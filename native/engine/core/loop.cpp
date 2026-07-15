#include "loop.h"
#ifdef OHOS_PLATFORM
#include <hilog/log.h>
#endif
#include <thread>
#include <algorithm>
#include <vector>
#include <limits>

#ifdef OHOS_PLATFORM
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#endif

namespace {
void ApplyCombatSnapshot(GameSnapshot& output, const CombatSnapshot& combat) {
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
  output.pulseWarningMs = combat.pulseWarningMs;
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
}  // namespace

void Loop::start() {
  withLifecycle([this]() {
    if (!surface.ready) {
      resetInput();
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
      combat.reset();
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
    combat.reset();
    combatTimeMs_ = 0;
    surface.trainingTarget.alive = true;
    publishRendererStopped();
    return;
  }
  processInput();
  fixedStep.advance(elapsedMs, [this](Tick tick, int64_t dtMs) {
    updateFixed(tick, dtMs);
  });
#ifdef OHOS_PLATFORM
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

  GameSnapshot snapshot;
  snapshot.tick = fixedStep.tick();
  snapshot.playerX = surface.player.x;
  snapshot.playerY = surface.player.y;
  snapshot.fps = fps;
  snapshot.moving = surface.player.moving;
  snapshot.targetId = currentTarget ? currentTarget->id : 0;
  snapshot.rendererReady = surface.ready;
  snapshot.moveX = intent.move.x;
  snapshot.moveY = intent.move.y;
  snapshot.cameraYaw = camera.yaw();
  snapshot.cameraPitch = camera.pitch();
  snapshot.targetDist = currentTarget ? currentTarget->distance : 0.0f;
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  ApplyCombatSnapshot(snapshot, combatSnapshot);
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

  std::vector<TargetCandidate> candidates;
  candidates.reserve(1);
  if (surface.trainingTarget.alive) {
    candidates.push_back({static_cast<int32_t>(surface.trainingTarget.id),
                          {surface.trainingTarget.x, surface.trainingTarget.y}});
  }
  currentTarget = softTargeting.select(
      {surface.player.x, surface.player.y}, camera.yaw(), candidates);

  for (const ActionRequest& action : intent.actions) combat.enqueue(action);
  intent.actions.clear();
  const Tick combatTime = AdvanceCombatTime(combatTimeMs_.load(), dtMs);
  combatTimeMs_.store(combatTime);
  combat.update({combatTime, dtMs, surface.player.moving,
                 currentTarget ? static_cast<EntityId>(currentTarget->id) : 0,
                 currentTarget.has_value() && surface.trainingTarget.alive});
  surface.trainingTarget.alive = combat.snapshot().targetAlive;
  if (!surface.trainingTarget.alive) currentTarget.reset();

  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    const CombatEventBatch& stepEvents = combat.events();
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

  GameSnapshot updated = snapshots.read();
  ApplyCombatSnapshot(updated, combat.snapshot());
  snapshots.publish(updated);
}

void Loop::publishRendererStopped() {
  currentTarget.reset();
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
