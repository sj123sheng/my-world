#include "native/engine/input/camera_gesture.h"
#include "native/engine/input/changed_pointer_forwarder.h"
#include "native/engine/input/touch_router.h"
#include "native/engine/input/virtual_joystick.h"
#include <cassert>
#include <cstdint>
#include <vector>

struct ActivePoint {
  int32_t id;
};

struct NativeTouchSnapshot {
  int32_t changedType;
  int32_t changedId;
  float changedX;
  float changedY;
  std::vector<ActivePoint> activePoints;
};

int main() {
  const std::vector<NativeTouchSnapshot> snapshots = {
      {0, 1, 100.0f, 200.0f, {{1}}},
      {0, 2, 800.0f, 200.0f, {{1}, {2}}},
      {2, 1, 180.0f, 200.0f, {{1}, {2}}},
      {1, 1, 180.0f, 200.0f, {{2}}},
      {3, 2, 800.0f, 200.0f, {}},
      {4, 9, 500.0f, 200.0f, {}},
  };

  TouchRouter router;
  VirtualJoystick joystick({0.0f, 100.0f});
  CameraGesture camera({0.01f, 0.01f});
  std::vector<InputEvent> forwarded;

  auto sink = [&](InputAction action, int32_t id, float x, float y) {
    const InputEvent event{action, id, x, y, forwarded.size()};
    forwarded.push_back(event);
    const TouchRole roleBefore = router.role(id);
    assert(router.handle(event, 1000.0f, 800.0f));
    const TouchRole roleAfter = router.role(id);
    if (action == InputAction::PointerDown) {
      if (roleAfter == TouchRole::Movement) joystick.begin(id, {x, y});
      if (roleAfter == TouchRole::Camera) camera.begin(id, {x, y});
    } else if (action == InputAction::PointerMove) {
      if (roleBefore == TouchRole::Movement) joystick.move(id, {x, y});
      if (roleBefore == TouchRole::Camera) camera.move(id, {x, y});
    } else {
      if (roleBefore == TouchRole::Movement) joystick.end(id);
      if (roleBefore == TouchRole::Camera) camera.end(id);
    }
    return true;
  };

  for (std::size_t index = 0; index < snapshots.size(); ++index) {
    const NativeTouchSnapshot& snapshot = snapshots[index];
    const std::size_t before = forwarded.size();
    const bool accepted = ForwardChangedPointer(
        snapshot.changedType, snapshot.changedId, snapshot.changedX,
        snapshot.changedY, sink);
    if (index < 5) {
      assert(accepted);
      assert(forwarded.size() == before + 1);
    } else {
      assert(!accepted);
      assert(forwarded.size() == before);
    }

    if (index == 1) {
      assert(snapshot.activePoints.size() == 2);
      assert(forwarded.size() == 2);
      assert(router.role(1) == TouchRole::Movement);
      assert(router.role(2) == TouchRole::Camera);
    } else if (index == 2) {
      assert((joystick.value() == Vec2{0.8f, 0.0f}));
      assert(router.role(2) == TouchRole::Camera);
    } else if (index == 3) {
      assert(snapshot.activePoints.size() == 1);
      assert(snapshot.activePoints.front().id == 2);
      assert(joystick.value() == Vec2{});
      assert(router.role(1) == TouchRole::None);
      assert(router.role(2) == TouchRole::Camera);
    } else if (index == 4) {
      assert(router.role(2) == TouchRole::None);
      assert(router.activeCount() == 0);
      assert(camera.consumeDelta() == Vec2{});
    }
  }

  assert(forwarded.size() == 5);
}
