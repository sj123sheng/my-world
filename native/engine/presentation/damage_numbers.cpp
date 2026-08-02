#include "native/engine/presentation/damage_numbers.h"

#include <algorithm>
#include <cmath>
#include <limits>

void DamageNumberSystem::spawn(Vec2 position, float value,
                               DamageNumberKind kind) {
  if (!position.finite() || !std::isfinite(value) || value <= 0.0f) {
    return;
  }

  DamageNumber number;
  number.origin = position;
  number.value = std::max(1, static_cast<int32_t>(std::lround(value)));
  number.kind = kind;
  number.sequence = counter_;
  // 确定性水平散布：按序号在 [-0.018, 0.018] 内取偏移，避免重叠。
  const int32_t bucket = static_cast<int32_t>(counter_ % 7u);
  number.driftX = (static_cast<float>(bucket) - 3.0f) * 0.006f;
  ++counter_;

  if (numbers_.size() >= kCapacity) {
    numbers_.erase(numbers_.begin());
  }
  numbers_.push_back(number);
}

void DamageNumberSystem::update(int64_t dtMs) {
  if (dtMs <= 0) {
    return;
  }
  for (DamageNumber& number : numbers_) {
    const Tick maxRemaining =
        std::numeric_limits<Tick>::max() - number.elapsedMs;
    number.elapsedMs +=
        static_cast<Tick>(std::min<int64_t>(dtMs, maxRemaining));
  }
  numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(),
                                [](const DamageNumber& number) {
                                  return number.elapsedMs >= number.lifetimeMs;
                                }),
                 numbers_.end());
}

void DamageNumberSystem::clear() { numbers_.clear(); }

float DamageNumberSystem::riseOffset(const DamageNumber& number) {
  const float t = std::clamp(number.progress(), 0.0f, 1.0f);
  // ease-out：1 - (1 - t)^2，起始上浮快、末端趋停。
  const float remaining = 1.0f - t;
  return kRiseHeight * (1.0f - remaining * remaining);
}

float DamageNumberSystem::alpha(const DamageNumber& number) {
  const float t = std::clamp(number.progress(), 0.0f, 1.0f);
  return t < 0.6f ? 1.0f : (1.0f - t) / 0.4f;
}
