#include "native/engine/presentation/damage_numbers.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool close(float actual, float expected, float tolerance = 0.0001f) {
  return std::abs(actual - expected) < tolerance;
}

}  // namespace

int main() {
  DamageNumberSystem system;
  assert(system.active().empty());

  // 非法输入被拒绝。
  system.spawn({0.5f, 0.5f}, 0.0f, DamageNumberKind::Normal);
  system.spawn({0.5f, 0.5f}, -5.0f, DamageNumberKind::Normal);
  system.spawn({std::numeric_limits<float>::quiet_NaN(), 0.5f}, 10.0f,
               DamageNumberKind::Normal);
  assert(system.active().empty());

  // 正常生成：数值四舍五入且至少为 1。
  system.spawn({0.5f, 0.5f}, 11.6f, DamageNumberKind::Normal);
  assert(system.active().size() == 1);
  assert(system.active()[0].value == 12);
  system.spawn({0.4f, 0.6f}, 0.4f, DamageNumberKind::Normal);
  assert(system.active().size() == 2);
  assert(system.active()[1].value == 1);

  // 更新推进时间并移除到期条目。
  system.update(500);
  assert(system.active().size() == 2);
  assert(system.active()[0].elapsedMs == 500);
  system.update(400);  // 900ms 累计超过 700ms 寿命，到期移除
  assert(system.active().empty());

  // 容量上限：超出时淘汰最旧条目。
  for (int index = 0; index < 30; ++index) {
    system.spawn({0.5f, 0.5f}, static_cast<float>(index + 1),
                 DamageNumberKind::Normal);
  }
  assert(system.active().size() == DamageNumberSystem::kCapacity);
  // 最旧的 6 条（1..6）已被淘汰，剩余 7..30。
  assert(system.active().front().value == 7);
  assert(system.active().back().value == 30);
  system.clear();
  assert(system.active().empty());

  // 上浮缓动：起点为 0，单调递增至 kRiseHeight。
  DamageNumber number;
  number.lifetimeMs = 1000;
  number.elapsedMs = 0;
  assert(close(DamageNumberSystem::riseOffset(number), 0.0f));
  float previous = 0.0f;
  for (Tick t = 100; t <= 1000; t += 100) {
    number.elapsedMs = t;
    const float rise = DamageNumberSystem::riseOffset(number);
    assert(rise > previous);
    previous = rise;
  }
  assert(close(previous, DamageNumberSystem::kRiseHeight));

  // 透明度：前 60% 保持 1，之后线性淡出到 0。
  number.elapsedMs = 0;
  assert(close(DamageNumberSystem::alpha(number), 1.0f));
  number.elapsedMs = 600;
  assert(close(DamageNumberSystem::alpha(number), 1.0f));
  number.elapsedMs = 800;
  assert(close(DamageNumberSystem::alpha(number), 0.5f));
  number.elapsedMs = 1000;
  assert(close(DamageNumberSystem::alpha(number), 0.0f));

  // 入场弹出缩放：起点 0.6，kPopInMs 后收敛到 1.0，期间单调递增。
  number.elapsedMs = 0;
  assert(close(DamageNumberSystem::popScale(number), 0.6f));
  float prevScale = 0.0f;
  for (Tick t = 20; t <= DamageNumberSystem::kPopInMs; t += 20) {
    number.elapsedMs = t;
    const float s = DamageNumberSystem::popScale(number);
    assert(s > prevScale);
    prevScale = s;
  }
  assert(close(DamageNumberSystem::popScale(number), 1.0f));
  number.elapsedMs = 500;
  assert(close(DamageNumberSystem::popScale(number), 1.0f));

  // 水平散布确定性：相同序号模式产生相同偏移，且范围受限。
  DamageNumberSystem first;
  DamageNumberSystem second;
  for (int index = 0; index < 7; ++index) {
    first.spawn({0.5f, 0.5f}, 10.0f, DamageNumberKind::Normal);
    second.spawn({0.5f, 0.5f}, 10.0f, DamageNumberKind::Normal);
  }
  for (std::size_t index = 0; index < 7; ++index) {
    assert(first.active()[index].driftX == second.active()[index].driftX);
    assert(std::abs(first.active()[index].driftX) <= 0.018f);
  }

  // 非正 dt 不推进时间。
  DamageNumberSystem paused;
  paused.spawn({0.5f, 0.5f}, 10.0f, DamageNumberKind::Normal);
  paused.update(0);
  paused.update(-16);
  assert(paused.active()[0].elapsedMs == 0);
  return 0;
}
