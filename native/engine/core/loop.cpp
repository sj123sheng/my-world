#include "loop.h"
#ifdef OHOS_PLATFORM
#include <hilog/log.h>
#endif
#include <thread>
#include <algorithm>
#include <vector>

#ifdef OHOS_PLATFORM
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#endif

void Loop::start() {
  withLifecycle([this]() {
    if (!surface.ready) {
      resetInput();
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
    resetInput();
  });
}

void Loop::processInput() {
  InputEvent e;
  while (input.pop(e)) {
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
        if (e.action == InputAction::PointerCancel) {
          resetInput();
          return;
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
  joystick = VirtualJoystick(VirtualJoystickConfig{});
  cameraGesture = CameraGesture(CameraGestureConfig{});
  intent.move = {};
  intent.lookDelta = {};
  InputEvent discarded;
  while (input.pop(discarded)) {
  }
}

void Loop::tickOnce(int64_t elapsedMs) {
  if (!surface.ready) {
    resetInput();
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
  snapshots.publish(snapshot);

  if (tickCount <= 5 || tickCount % 60 == 0) {
    LOGI("tickOnce: %{public}d fps=%{public}.1f", tickCount, fps);
  }
}

void Loop::updateFixed(Tick, int64_t dtMs) {
  const Vec2 lookDelta = intent.lookDelta;
  intent.lookDelta = {};
  const float dtSeconds = static_cast<float>(dtMs) / 1000.0f;
  playerController.update(surface.player, intent.move, camera.yaw(),
                          dtSeconds);
  camera.update({surface.player.x, surface.player.y}, lookDelta, dtSeconds);

  std::vector<TargetCandidate> candidates;
  candidates.reserve(surface.props.size());
  for (std::size_t i = 0; i < surface.props.size(); ++i) {
    const Prop& prop = surface.props[i];
    candidates.push_back(
        {static_cast<int32_t>(i + 1), {prop.x, prop.y}});
  }
  currentTarget = softTargeting.select(
      {surface.player.x, surface.player.y}, camera.yaw(), candidates);
}

void Loop::publishRendererStopped() {
  snapshots.publish(RendererStoppedSnapshot(snapshots.read()));
}
