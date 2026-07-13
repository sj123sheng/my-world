#include "loop.h"
#include <hilog/log.h>
#include <thread>
#include <cmath>

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void Loop::start() {
  if (running || !surface.ready) {
    LOGI("Loop start skipped: running=%{public}d ready=%{public}d", (int)running, (int)surface.ready);
    return;
  }
  shouldStop = false;
  running = true;
  tickCount = 0;
  fps = 60.0f;
  lastFpsTime = std::chrono::steady_clock::now();
  runner = std::thread([this]() {
    while (!shouldStop) {
      tickOnce();
      std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
    running = false;
  });
}

void Loop::stop() {
  shouldStop = true;
  if (runner.joinable()) runner.join();
  running = false;
}

void Loop::processInput() {
  InputEvent e;
  while (input.pop(e)) {
    if (e.type == 0 || e.type == 2) { // down / move
      surface.player.moving = true;
      surface.player.targetX = clamp01(e.x / (float)surface.width);
      surface.player.targetY = clamp01(e.y / (float)surface.height);
    } else if (e.type == 1 || e.type == 3) { // up / cancel
      surface.player.moving = false;
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

void Loop::tickOnce() {
  processInput();
  updatePlayer(0.016f);
  surface_draw(surface);
  surface_swap(surface);

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
