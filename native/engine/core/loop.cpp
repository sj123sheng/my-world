#include "loop.h"
#include <hilog/log.h>
#include <thread>
#include <cmath>
#include <algorithm>

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void Loop::start() {
  withLifecycle([this]() {
    if (!surface.ready) {
      LOGI("Loop start skipped: running=%{public}d ready=%{public}d", (int)running, (int)surface.ready);
      return;
    }
    if (!lifecycle.start([this]() {
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
  });
}

void Loop::processInput() {
  InputEvent e;
  while (input.pop(e)) {
    switch (e.action) {
      case InputAction::PointerDown:
      case InputAction::PointerMove:
        surface.player.moving = true;
        surface.player.targetX = clamp01(e.x / static_cast<float>(surface.width));
        surface.player.targetY = clamp01(e.y / static_cast<float>(surface.height));
        break;
      case InputAction::PointerUp:
      case InputAction::PointerCancel:
        surface.player.moving = false;
        break;
      default:
        break;
    }
  }
}

void Loop::updatePlayer(float dt) {
  if (surface.player.moving) {
    const float dx = surface.player.targetX - surface.player.x;
    const float dy = surface.player.targetY - surface.player.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > 0.001f) {
      surface.player.angle = std::atan2(dy, dx);
      const float speed = 2.0f * dt;
      float step = std::min(dist, speed);
      surface.player.x = clamp01(surface.player.x + std::cos(surface.player.angle) * step);
      surface.player.y = clamp01(surface.player.y + std::sin(surface.player.angle) * step);
    }
  }

  // trail particles
  static float emitTimer = 0.0f;
  emitTimer += dt;
  if (surface.player.moving && emitTimer > 0.05f) {
    emitTimer = 0.0f;
    Particle p;
    p.x = surface.player.x;
    p.y = surface.player.y;
    p.life = 0.4f;
    p.maxLife = 0.4f;
    surface.particles.push_back(p);
  }

  for (auto& p : surface.particles) {
    p.life -= dt;
  }
  surface.particles.erase(
    std::remove_if(surface.particles.begin(), surface.particles.end(), [](const Particle& p) { return p.life <= 0.0f; }),
    surface.particles.end());
}

void Loop::tickOnce(int64_t elapsedMs) {
  processInput();
  fixedStep.advance(elapsedMs, [this](Tick tick, int64_t dtMs) {
    updateFixed(tick, dtMs);
  });
  surface_draw(surface);
  surface_swap(surface);

  snapshots.publish({
    fixedStep.tick(),
    100,
    100,
    surface.player.x,
    surface.player.y,
    fps,
    surface.player.moving,
    0,
    0,
    surface.ready,
  });

  tickCount++;
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime).count();
  if (elapsed >= 1000) {
    fps = tickCount * 1000.0f / (float)elapsed;
    tickCount = 0;
    lastFpsTime = now;
  }
  if (tickCount <= 5 || tickCount % 60 == 0) {
    LOGI("tickOnce: %{public}d fps=%{public}.1f", tickCount, fps);
  }
}

void Loop::updateFixed(Tick, int64_t dtMs) {
  updatePlayer(static_cast<float>(dtMs) / 1000.0f);
}

void Loop::publishRendererStopped() {
  snapshots.publish(RendererStoppedSnapshot(snapshots.read()));
}
