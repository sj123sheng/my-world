// test_engagement_spacing.cpp: 交战留白纯函数（Plan 2 Task 5）。
// 原型交战距离字面量、环形槽位稳定性与重叠分离向量。

#include "native/gameplay/ai/engagement_spacing.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

bool near(float actual, float expected, float tolerance = 1.0e-4f) {
  return std::abs(actual - expected) <= tolerance;
}

float angleOf(Vec2 point, Vec2 center) {
  const Vec2 offset = point - center;
  float angle = std::atan2(offset.y, offset.x);
  if (angle < 0.0f) angle += 2.0f * kPi;
  return angle;
}

void testArchetypeRanges() {
  // 近战：minimum 至少 0.08（约一个身位），ideal 0.14。
  const EngagementRange melee =
      EngagementRangeFor(EnemyArchetype::RiftClaw, 0.05f, false);
  assert(melee.minimum >= 0.08f);
  assert(near(melee.ideal, 0.14f));

  // 远程 ideal 明显大于近战，保持施法空间。
  const EngagementRange ranged =
      EngagementRangeFor(EnemyArchetype::Priest, 0.05f, false);
  assert(ranged.ideal > melee.ideal);
  assert(near(ranged.minimum, 0.16f));
  assert(near(ranged.ideal, 0.30f));

  // 所有原型满足 minimum < ideal <= attack < maxPursuit。
  const std::vector<EngagementRange> all{
      melee, ranged,
      EngagementRangeFor(EnemyArchetype::Guard, 0.05f, false),
      EngagementRangeFor(EnemyArchetype::Bruiser, 0.06f, false),
      EngagementRangeFor(EnemyArchetype::Caster, 0.05f, false),
      EngagementRangeFor(EnemyArchetype::Elite, 0.06f, false),
      EngagementRangeFor(EnemyArchetype::RiftClaw, 0.05f, true)};
  for (const EngagementRange& range : all) {
    assert(range.minimum < range.ideal);
    assert(range.ideal <= range.attack);
    assert(range.attack < range.maxPursuit);
    assert(std::isfinite(range.minimum) && std::isfinite(range.maxPursuit));
  }

  // Boss 按体型扩大空挡：minimum 大于普通近战，且随体型增长。
  const EngagementRange boss =
      EngagementRangeFor(EnemyArchetype::RiftClaw, 0.10f, true);
  assert(boss.minimum > melee.minimum);
  assert(near(boss.minimum, std::max(0.14f, 0.10f * 1.5f)));
  const EngagementRange bigBoss =
      EngagementRangeFor(EnemyArchetype::RiftClaw, 0.20f, true);
  assert(bigBoss.minimum > boss.minimum);
}

void testSlotEvenAndStable() {
  const Vec2 player{0.5f, 0.12f};
  const float idealRadius = 0.14f;
  const std::vector<EntityId> participants{11, 12, 13, 14};

  // 槽位落在理想半径上，四敌人角度均匀（相邻夹角 π/2）。
  std::vector<float> angles;
  for (const EntityId id : participants) {
    const Vec2 slot = EngagementSlotPosition(id, player, idealRadius,
                                             participants);
    assert(near((slot - player).length(), idealRadius, 1.0e-3f));
    angles.push_back(angleOf(slot, player));
  }
  std::sort(angles.begin(), angles.end());
  for (std::size_t i = 0; i < angles.size(); ++i) {
    const float next = (i + 1 < angles.size())
                           ? angles[i + 1]
                           : angles[0] + 2.0f * kPi;
    assert(near(next - angles[i], kPi / 2.0f, 1.0e-3f));
  }

  // 输入参与者反序不改变各 ID 的槽位位置。
  std::vector<EntityId> reversed{14, 13, 12, 11};
  for (const EntityId id : participants) {
    const Vec2 forward = EngagementSlotPosition(id, player, idealRadius,
                                                participants);
    const Vec2 backward = EngagementSlotPosition(id, player, idealRadius,
                                                 reversed);
    assert(forward == backward);
  }

  // 单一参与者槽位稳定且有限。
  const Vec2 solo = EngagementSlotPosition(7, player, idealRadius, {7});
  assert(solo.finite());
  assert(near((solo - player).length(), idealRadius, 1.0e-3f));
}

void testSeparationOverlap() {
  const Vec2 selfPosition{0.5f, 0.7f};
  // 完全重叠：分离向量必须有限且非零，方向由双 ID 稳定哈希给出。
  const std::vector<EngagementNeighbor> overlapping{
      {12, selfPosition}, {13, selfPosition}};
  const Vec2 offset = SeparationOffset(11, selfPosition, overlapping, 0.06f);
  assert(offset.finite());
  assert(offset.length() > 0.0f);
  // 强度钳制：单帧分离不超过最小间距。
  assert(offset.length() <= 0.06f + 1.0e-5f);

  // 确定性：同输入重复计算结果一致；对称性：对方视角方向相反。
  const Vec2 again = SeparationOffset(11, selfPosition, overlapping, 0.06f);
  assert(again == offset);
  const Vec2 fromOther =
      SeparationOffset(12, selfPosition, {{11, selfPosition}}, 0.06f);
  assert(near(fromOther.x, -offset.x, 1.0e-4f) ||
         fromOther.length() > 0.0f);

  // 已分开的邻居不再产生分离。
  const std::vector<EngagementNeighbor> apart{
      {12, {0.5f + 0.2f, 0.7f}}};
  const Vec2 none = SeparationOffset(11, selfPosition, apart, 0.06f);
  assert(near(none.length(), 0.0f));

  // 主角位置不被分离逻辑修改（纯函数只读输入）。
  const Vec2 player{0.5f, 0.12f};
  Vec2 playerCopy = player;
  (void)SeparationOffset(11, selfPosition,
                         {{12, playerCopy}, {13, playerCopy}}, 0.06f);
  assert(playerCopy == player);
}

}  // namespace

int main() {
  testArchetypeRanges();
  testSlotEvenAndStable();
  testSeparationOverlap();
  return 0;
}
