#pragma once

#include "native/engine/math/vec2.h"
#include "native/engine/core/tick_clock.h"

#include <cstdint>
#include <vector>

// 伤害飘字条目：世界坐标 origin 处生成，随时间上浮并淡出。
enum class DamageNumberKind : uint8_t {
  Normal,    // 普通伤害（白色）
  Heavy,     // 大额伤害（金色）
  PlayerHit, // 玩家受击（红色）
};

struct DamageNumber {
  Vec2 origin{};                 // 世界 2D 坐标（x, y）
  int32_t value = 0;             // 显示数值
  DamageNumberKind kind = DamageNumberKind::Normal;
  Tick elapsedMs = 0;
  Tick lifetimeMs = 700;
  float driftX = 0.0f;           // 水平散布偏移，避免多条飘字重叠
  uint64_t sequence = 0;

  // 归一化生命进度 [0, 1]。
  float progress() const {
    return lifetimeMs > 0
               ? static_cast<float>(elapsedMs) / static_cast<float>(lifetimeMs)
               : 1.0f;
  }
};

// 伤害飘字系统：消费战斗伤害事件，维护上浮淡出的飘字列表。
// 纯逻辑实现，渲染层只读消费 active() 列表。
class DamageNumberSystem {
 public:
  // 同时存在的飘字上限，超出时淘汰最旧条目。
  static constexpr std::size_t kCapacity = 24;
  // 上浮总高度（世界单位）：缩小后避免遮挡敌人本体。
  static constexpr float kRiseHeight = 0.05f;
  // 入场弹出动画时长：期间字号从 0.6 倍放大到 1.0 倍。
  static constexpr Tick kPopInMs = 120;

  void spawn(Vec2 position, float value, DamageNumberKind kind);
  void update(int64_t dtMs);
  void clear();

  const std::vector<DamageNumber>& active() const { return numbers_; }

  // 缓动上浮偏移：先快后慢（ease-out）。
  static float riseOffset(const DamageNumber& number);
  // 透明度：前 60% 生命保持不透明，后 40% 线性淡出。
  static float alpha(const DamageNumber& number);
  // 入场弹出缩放：0.6 → 1.0（ease-out），提升生成瞬间的可读性。
  static float popScale(const DamageNumber& number);

 private:
  std::vector<DamageNumber> numbers_;
  uint64_t counter_ = 0;
};
