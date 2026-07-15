#include "native/engine/core/loop.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

int main() {
  static_assert(std::is_same_v<decltype(&Loop::tickOnce), void (Loop::*)(int64_t)>);
  static_assert(std::is_same_v<decltype(&Loop::updateFixed), void (Loop::*)(Tick, int64_t)>);

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
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerDown, 10, 700.0f,
                                        400.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerMove, 10, 750.0f,
                                        430.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerUp, 10, 750.0f,
                                        430.0f));
  assert(orderedCameraLoop.enqueueInput(InputAction::PointerDown, 11, 800.0f,
                                        400.0f));
  orderedCameraLoop.processInput();
  assert((orderedCameraLoop.intent.lookDelta == Vec2{0.5f, 0.3f}));

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
  assert((cancelLoop.intent.lookDelta == Vec2{0.3f, 0.2f}));

  assert(loop.enqueueInput(InputAction::PointerDown, 3, 100.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 3, 180.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerUp, 3, 180.0f, 400.0f));
  loop.processInput();
  loop.updateFixed(2, 16);
  assert(loop.intent.move == Vec2{});
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
  targetingLoop.surface.width = 1000;
  targetingLoop.surface.height = 800;
  targetingLoop.surface.ready = true;
  targetingLoop.surface.props.push_back({0.5f, 0.8f, 0.05f, 1});
  targetingLoop.tickOnce(16);
  const GameSnapshot targeted = targetingLoop.snapshot();
  assert(targeted.targetId == 1);
  assert(std::abs(targeted.targetDist - 0.3f) < 0.001f);

  targetingLoop.resetInput();
  targetingLoop.tickOnce(0);
  const GameSnapshot resetWithoutFixedTick = targetingLoop.snapshot();
  assert(resetWithoutFixedTick.targetId == 0);
  assert(resetWithoutFixedTick.targetDist == 0.0f);

  targetingLoop.tickOnce(16);
  assert(targetingLoop.snapshot().targetId == 1);
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
  assert(pausedLoop.snapshot().targetId == 1);
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
  assert(activeBeforeInvalid.targetId == 1);
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
  assert(restartedLoop.surface.player.x == 0.5f);
  assert(restartedLoop.surface.player.y == 0.5f);

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
