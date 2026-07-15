#include "native/engine/core/loop.h"

#include <cassert>
#include <cmath>
#include <type_traits>

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
  assert(loop.enqueueInput(InputAction::PointerDown, 1, 100.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 1, 180.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerDown, 2, 700.0f, 400.0f));
  assert(loop.enqueueInput(InputAction::PointerMove, 2, 750.0f, 430.0f));
  loop.processInput();
  loop.updateFixed(1, 16);
  assert(loop.intent.move.length() > 0.0f);
  assert(loop.camera.yaw() != 0.0f);
  assert(loop.surface.player.moving);
  loop.resetInput();
  assert(loop.intent.move == Vec2{});
  assert(loop.intent.lookDelta == Vec2{});
  assert(loop.touchRouter.activeCount() == 0);

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

  assert(targetingLoop.enqueueInput(InputAction::PointerDown, 5, 100.0f,
                                    400.0f));
  assert(targetingLoop.enqueueInput(InputAction::PointerMove, 5, 180.0f,
                                    400.0f));
  targetingLoop.processInput();
  assert(targetingLoop.intent.move.length() > 0.0f);
  targetingLoop.surface.ready = false;
  targetingLoop.tickOnce(16);
  assert(targetingLoop.intent.move == Vec2{});
  assert(targetingLoop.touchRouter.activeCount() == 0);
}
