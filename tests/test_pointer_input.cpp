#include "native/engine/input/pointer_input.h"

#include <cassert>
#include <limits>

int main() {
  InputAction action = InputAction::Attack;
  assert(TryMapPointerAction(0, action) && action == InputAction::PointerDown);
  assert(TryMapPointerAction(1, action) && action == InputAction::PointerUp);
  assert(TryMapPointerAction(2, action) && action == InputAction::PointerMove);
  assert(TryMapPointerAction(3, action) && action == InputAction::PointerCancel);
  assert(!TryMapPointerAction(-1, action));
  assert(!TryMapPointerAction(4, action));

  int32_t integer = -1;
  assert(TryConvertInt32(0.0, integer) && integer == 0);
  assert(TryConvertInt32(42.0, integer) && integer == 42);
  assert(!TryConvertInt32(1.5, integer));
  assert(!TryConvertInt32(std::numeric_limits<double>::infinity(), integer));
  assert(!TryConvertInt32(static_cast<double>(std::numeric_limits<int32_t>::max()) + 1.0, integer));

  float coordinate = 0.0f;
  assert(TryConvertFloat(12.5, coordinate) && coordinate == 12.5f);
  assert(!TryConvertFloat(std::numeric_limits<double>::infinity(), coordinate));
  assert(!TryConvertFloat(static_cast<double>(std::numeric_limits<float>::max()) * 2.0, coordinate));
}
