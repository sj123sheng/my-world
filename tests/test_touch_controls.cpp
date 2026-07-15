#include "native/engine/input/camera_gesture.h"
#include "native/engine/input/touch_router.h"
#include "native/engine/input/virtual_joystick.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
  TouchRouter router;
  assert(!router.handle({InputAction::PointerDown, 1, 100, 200, 0}, 0, 800));
  assert(router.handle({InputAction::PointerDown, 1, 100, 200, 1}, 1000, 800));
  assert(router.activeCount() == 1);
  assert(router.role(1) == TouchRole::Movement);
  assert(router.handle({InputAction::PointerDown, 2, 800, 200, 2}, 1000, 800));
  assert(router.role(2) == TouchRole::Camera);
  assert(router.handle({InputAction::PointerMove, 1, 900, 200, 3}, 1000, 800));
  assert(router.role(1) == TouchRole::Movement);
  assert(router.handle({InputAction::PointerUp, 2, 820, 210, 4}, 1000, 800));
  assert(router.activeCount() == 1);
  assert(router.role(1) == TouchRole::Movement);
  assert(router.role(2) == TouchRole::None);
  assert(router.handle({InputAction::PointerDown, 3, 200, 300, 5}, 1000, 800));
  assert(router.role(3) == TouchRole::Ignored);
  assert(router.handle({InputAction::PointerDown, 4, 900, 300, 6}, 1000, 800));
  assert(router.role(4) == TouchRole::Camera);
  assert(router.handle({InputAction::PointerDown, 5, 850, 300, 7}, 1000, 800));
  assert(router.role(5) == TouchRole::Ignored);
  assert(router.handle({InputAction::PointerUp, 5, 850, 300, 8}, 1000, 800));
  assert(router.role(4) == TouchRole::Camera);
  assert(!router.handle({InputAction::PointerMove, 99, 1, 1, 9}, 1000, 800));
  assert(!router.handle({InputAction::PointerUp, 99, 1, 1, 10}, 1000, 800));
  assert(!router.handle({InputAction::Attack, 4, 900, 300, 11}, 1000, 800));
  assert(router.role(4) == TouchRole::Camera);
  assert(!router.handle(
      {static_cast<InputAction>(255), 4, 900, 300, 12}, 1000, 800));
  assert(!router.handle({InputAction::PointerDown, 4,
                         std::numeric_limits<float>::quiet_NaN(), 300, 13},
                        1000, 800));
  assert(router.role(4) == TouchRole::Camera);
  assert(!router.handle({InputAction::PointerUp, 4,
                         std::numeric_limits<float>::infinity(), 300, 14},
                        1000, 800));
  assert(router.role(4) == TouchRole::Camera);
  assert(!router.handle({InputAction::PointerDown, 6, 100, 300, 15},
                        std::numeric_limits<float>::infinity(), 800));
  assert(!router.handle({InputAction::PointerDown, 6, 100, 300, 16}, 1000,
                        std::numeric_limits<float>::quiet_NaN()));
  assert(router.role(6) == TouchRole::None);
  router.clear();
  assert(router.activeCount() == 0);
  assert(router.role(1) == TouchRole::None);
  assert(router.role(3) == TouchRole::None);
  assert(router.role(4) == TouchRole::None);

  VirtualJoystick joystick({0.1f, 100.0f});
  joystick.begin(1, {100, 200});
  joystick.move(1, {105, 200});
  assert(joystick.value() == Vec2{});
  joystick.move(1, {200, 300});
  assert(std::abs(joystick.value().length() - 1.0f) < 0.0001f);
  joystick.end(1);
  assert(joystick.value() == Vec2{});
  VirtualJoystick invalidJoystick(
      {0.1f, std::numeric_limits<float>::quiet_NaN()});
  invalidJoystick.begin(5, {0, 0});
  invalidJoystick.move(5, {10, 10});
  assert(invalidJoystick.value().finite());
  assert(invalidJoystick.value() == Vec2{});

  CameraGesture camera({0.01f, 0.01f});
  camera.begin(2, {800, 200});
  camera.move(2, {820, 190});
  assert((camera.consumeDelta() == Vec2{0.2f, -0.1f}));
  camera.move(2, {830, 180});
  assert((camera.consumeDelta() == Vec2{0.1f, -0.1f}));
  assert(camera.consumeDelta() == Vec2{});
  CameraGesture invalidCamera(
      {std::numeric_limits<float>::infinity(), 0.01f});
  invalidCamera.begin(6, {0, 0});
  invalidCamera.move(6, {10, 10});
  assert(invalidCamera.consumeDelta().finite());
}
