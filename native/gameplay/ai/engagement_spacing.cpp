// engagement_spacing.cpp: 敌人交战留白（Plan 2 Task 5）。原型距离、稳定
// 环形槽位与邻居分离全部为纯函数，供 DecisionPolicy/TacticalPlanner/
// Encounter/WildSpawn/Boss 消费，本身不持任何状态。

#include "engagement_spacing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// 原型交战距离（Plan 2 Task 5）：近战/远程/Boss 的 minimum 与 ideal 集中
// 在此；attack/maxPursuit 从现有 ability/region 上限适配，见下方各原型。
constexpr float kMeleeMinimum = 0.08f;
constexpr float kMeleeIdeal = 0.14f;
constexpr float kRangedMinimum = 0.16f;
constexpr float kRangedIdeal = 0.30f;
constexpr float kBossMinimumFloor = 0.14f;
constexpr float kBossMinimumBodyScale = 1.5f;
constexpr float kBossIdealFloor = 0.20f;
constexpr float kBossIdealBodyScale = 2.5f;

// 现有攻击有效性下限：留白不得缩短已有 ability 射程。
constexpr float kMeleeAttackRange = 0.25f;   // decision_policy kMeleeAttackDistance
constexpr float kRangedAttackRange = 4.0f;   // decision_policy kPriestAttackDistance
constexpr float kBossAttackRange = 0.24f;    // BossConfig basicAttackRange

// 追击上限：超出即不再追击（由区域钳制兜底），必须大于 attack。
constexpr float kMeleeMaxPursuit = 0.60f;
constexpr float kRangedMaxPursuit = 6.0f;
constexpr float kBossMaxPursuitFloor = 0.80f;
constexpr float kBossMaxPursuitMargin = 0.30f;

constexpr float kEpsilon = 1.0e-5f;

// SplitMix64 混合：把整型 ID 稳定映射到 [0, 2π)。跨平台/跨运行确定。
float hashToAngle(uint32_t value) {
  uint64_t x = static_cast<uint64_t>(value) + 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  x = x ^ (x >> 31);
  const double unit =
      static_cast<double>(x & 0xFFFFFFFFULL) / 4294967295.0;
  return static_cast<float>(unit * static_cast<double>(kTwoPi));
}

// 无序 ID 对的稳定哈希：用于完全重叠时给出确定的分离方向。
uint32_t pairHash(EntityId a, EntityId b) {
  const uint32_t lo = static_cast<uint32_t>(std::min(a, b));
  const uint32_t hi = static_cast<uint32_t>(std::max(a, b));
  return lo * 0x85EBCA6Bu + hi * 0xC2B2AE35u + 0x27D4EB2Fu;
}

Vec2 directionFromAngle(float angle) {
  return {std::cos(angle), std::sin(angle)};
}

}  // namespace

EngagementRange EngagementRangeFor(EnemyArchetype archetype, float bodyRadius,
                                   bool boss) {
  const float safeRadius =
      std::isfinite(bodyRadius) && bodyRadius > 0.0f ? bodyRadius : 0.0f;
  EngagementRange range;
  if (boss) {
    range.minimum =
        std::max(kBossMinimumFloor, safeRadius * kBossMinimumBodyScale);
    range.ideal =
        std::max(kBossIdealFloor, safeRadius * kBossIdealBodyScale);
    // 不缩短首领普攻射程；ideal 若更大则以 ideal 为准保持 minimum<ideal<=attack。
    range.attack = std::max(kBossAttackRange, range.ideal);
    range.maxPursuit =
        std::max(kBossMaxPursuitFloor, range.attack + kBossMaxPursuitMargin);
    range.lungeOnActive = true;
    return range;
  }
  switch (archetype) {
    case EnemyArchetype::Priest:
    case EnemyArchetype::Caster:
      range.minimum = kRangedMinimum;
      range.ideal = kRangedIdeal;
      range.attack = std::max(kRangedAttackRange, range.ideal);
      range.maxPursuit = kRangedMaxPursuit;
      // 远程站桩输出：Active 不突进，保持理想距离带。
      range.lungeOnActive = false;
      return range;
    case EnemyArchetype::RiftClaw:
    case EnemyArchetype::Guard:
    case EnemyArchetype::Bruiser:
    case EnemyArchetype::Elite:
    default:
      range.minimum = kMeleeMinimum;
      range.ideal = kMeleeIdeal;
      range.attack = std::max(kMeleeAttackRange, range.ideal);
      range.maxPursuit = kMeleeMaxPursuit;
      range.lungeOnActive = true;
      return range;
  }
}

Vec2 EngagementSlotPosition(EntityId id, Vec2 player, float idealRadius,
                            const std::vector<EntityId>& participants) {
  if (!player.finite() || !std::isfinite(idealRadius) || idealRadius <= 0.0f) {
    return player;
  }
  std::vector<EntityId> sorted(participants.begin(), participants.end());
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  const auto it = std::find(sorted.begin(), sorted.end(), id);
  if (it == sorted.end()) {
    // 防御：自身未列入参与者，给出仅依赖自身 ID 的稳定单槽位。
    return player + directionFromAngle(hashToAngle(id)) * idealRadius;
  }
  const auto index = static_cast<float>(it - sorted.begin());
  const float count = static_cast<float>(sorted.size());
  const float baseAngle = hashToAngle(sorted.front());
  const float angle = baseAngle + index * (kTwoPi / count);
  return player + directionFromAngle(angle) * idealRadius;
}

Vec2 SeparationOffset(EntityId self, Vec2 selfPosition,
                      const std::vector<EngagementNeighbor>& neighbors,
                      float minimumSpacing) {
  if (!selfPosition.finite() || !std::isfinite(minimumSpacing) ||
      minimumSpacing <= 0.0f) {
    return {0.0f, 0.0f};
  }
  Vec2 offset{0.0f, 0.0f};
  bool anyClose = false;
  for (const EngagementNeighbor& neighbor : neighbors) {
    if (neighbor.id == self || !neighbor.position.finite()) continue;
    const Vec2 delta = selfPosition - neighbor.position;
    const float distance = delta.length();
    if (distance >= minimumSpacing) continue;
    anyClose = true;
    Vec2 direction;
    if (distance < kEpsilon) {
      // 完全重叠：用无序 ID 对的稳定方向，较小 ID 沿角度正向、较大 ID
      // 沿反向，保证两者互相远离且结果确定。
      const float pairAngle = hashToAngle(pairHash(self, neighbor.id));
      const float sign = (self < neighbor.id) ? 1.0f : -1.0f;
      direction = directionFromAngle(pairAngle) * sign;
    } else {
      direction = delta * (1.0f / distance);
    }
    const float strength = 1.0f - (distance / minimumSpacing);
    offset = offset + direction * strength;
  }
  if (!offset.finite()) offset = {0.0f, 0.0f};
  if (anyClose && offset.length() < kEpsilon) {
    // 多邻居相互抵消时兜底一个确定方向，保证重叠仍有非零分离。
    offset = directionFromAngle(hashToAngle(self));
  }
  // 单帧分离不超过最小间距：有限、有界，消费端再按速度积分。
  return ClampLength(offset, minimumSpacing);
}
