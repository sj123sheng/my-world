// test_target_lock_controller.cpp: 统一目标锁定控制器（Plan 2 Task 1/2）。
// 自动模式距离优先、连招稳定、死亡重选、攻击停止后锁定环淡出。

#include "native/gameplay/targeting/target_lock_controller.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

bool near(float actual, float expected, float tolerance = 1.0e-4f) {
  return std::abs(actual - expected) <= tolerance;
}

TargetLockCandidate makeCandidate(EntityId id, Vec2 position,
                                  bool alive = true, bool attackable = true,
                                  bool boss = false) {
  TargetLockCandidate candidate;
  candidate.id = id;
  candidate.position = position;
  candidate.alive = alive;
  candidate.attackable = attackable;
  candidate.boss = boss;
  return candidate;
}

// 宽松配置：角度上限放到 π，聚焦距离/行为断言。
TargetLockConfig wideConfig() { return TargetLockConfig{0.75f, kPi, 800}; }

void testDistanceBeatsCameraAngle() {
  // 防止按角度优先而跳过更近敌人：距离 0.3 但角度 90° 的目标，
  // 优于距离 0.5 正前方的目标。
  TargetLockController controller(wideConfig());
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.3f, 0.0f}),   // 距离 0.3，角度 90°
      makeCandidate(2, {0.0f, 0.5f}),   // 距离 0.5，角度 0°
  };
  const TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 1000);
  assert(result.id.has_value() && *result.id == 1u);
  assert(result.mode == TargetLockMode::Automatic);
  assert(near(result.distance, 0.3f));
  assert(result.showMarker);
}

void testSameDistancePrefersForward() {
  // 相同距离时镜头前方（角度小）优先。
  TargetLockController controller(wideConfig());
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.4f, 0.0f}),   // 距离 0.4，角度 90°
      makeCandidate(2, {0.0f, 0.4f}),   // 距离 0.4，角度 0°
  };
  const TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 1000);
  assert(result.id.has_value() && *result.id == 2u);
}

void testComboKeepsCurrentTarget() {
  // 连招活跃期间保持当前目标，防止抖动换目标；连招结束后恢复距离优先。
  TargetLockController controller(wideConfig());
  TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, {makeCandidate(1, {0.0f, 0.5f})}, true, false, 100);
  assert(result.id.has_value() && *result.id == 1u);

  // 更近的敌人 2 出现：连招中仍保持 1。
  const std::vector<TargetLockCandidate> both{
      makeCandidate(1, {0.0f, 0.5f}),
      makeCandidate(2, {0.0f, 0.3f}),
  };
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, both, false, true,
                                      116);
  assert(result.id.has_value() && *result.id == 1u);

  // 连招结束：恢复距离优先，切到更近的 2。
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, both, false, false,
                                      200);
  assert(result.id.has_value() && *result.id == 2u);
}

void testComboHoldDistanceLimit() {
  // 连招维持距离 = 自动获取距离 × 1.25（0.75 → 0.9375）：
  // 超出后即使连招中也不再保持。
  TargetLockController controller(wideConfig());
  TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, {makeCandidate(1, {0.0f, 0.5f})}, true, false, 0);
  assert(result.id.has_value() && *result.id == 1u);

  // 0.90 <= 0.9375：连招保持。
  result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(1, {0.0f, 0.90f}), makeCandidate(2, {0.0f, 0.3f})},
      false, true, 16);
  assert(result.id.has_value() && *result.id == 1u);

  // 0.95 > 0.9375：连招也重选到更近候选。
  result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(1, {0.0f, 0.95f}), makeCandidate(2, {0.0f, 0.3f})},
      false, true, 32);
  assert(result.id.has_value() && *result.id == 2u);
}

void testDeadTargetReselects() {
  // 目标死亡（alive=false 或离场）立即重选，连招中也不例外。
  TargetLockController controller(wideConfig());
  TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(1, {0.0f, 0.3f}), makeCandidate(2, {0.0f, 0.5f})},
      true, false, 0);
  assert(result.id.has_value() && *result.id == 1u);

  result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(1, {0.0f, 0.3f}, /*alive=*/false),
       makeCandidate(2, {0.0f, 0.5f})},
      false, true, 16);
  assert(result.id.has_value() && *result.id == 2u);

  // 死亡候选完全离场同样重选。
  result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, {makeCandidate(2, {0.0f, 0.5f})}, false, true, 32);
  assert(result.id.has_value() && *result.id == 2u);
}

void testMarkerFadesAfterAttackStops() {
  // 攻击活跃窗口 800ms：窗口内显示锁定环，窗口外淡出；无攻击永不显示。
  TargetLockController controller(wideConfig());
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.0f, 0.5f})};

  // 从未攻击：有候选也不显示环。
  TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, false, false, 0);
  assert(result.id.has_value());
  assert(!result.showMarker);

  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, candidates,
                                      /*attackTriggered=*/true, false, 1000);
  assert(result.showMarker);

  // 攻击停止后 800ms 窗口内保持显示（含边界）。
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, candidates, false,
                                      false, 1500);
  assert(result.id.has_value() && result.showMarker);
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, candidates, false,
                                      false, 1800);
  assert(result.showMarker);

  // 超出窗口：目标仍在但环淡出。
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f, candidates, false,
                                      false, 1801);
  assert(result.id.has_value());
  assert(!result.showMarker);
}

void testInvalidateAndClear() {
  TargetLockController controller(wideConfig());
  TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(1, {0.0f, 0.5f}), makeCandidate(2, {0.0f, 0.4f})},
      true, false, 0);
  // 距离优先先选 2；invalidate(2) 立即放弃当前目标。
  assert(result.id.has_value() && *result.id == 2u);
  controller.invalidate(2u);
  assert(!controller.currentId().has_value());

  // 目标 2 离场后连招中也不能保持已失效目标，重选到 1。
  result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, {makeCandidate(1, {0.0f, 0.5f})}, false, true, 16);
  assert(result.id.has_value() && *result.id == 1u);

  // clear 复位当前目标与活跃窗口：无攻击时不再显示环。
  controller.clear();
  assert(!controller.currentId().has_value());
  result = controller.updateAutomatic({0.0f, 0.0f}, 0.0f,
                                      {makeCandidate(1, {0.0f, 0.5f})},
                                      false, false, 32);
  assert(result.id.has_value());
  assert(!result.showMarker);
  assert(controller.mode() == TargetLockMode::Automatic);
}

void testFiltersInvalidCandidates() {
  TargetLockController controller(wideConfig());
  const float infinity = std::numeric_limits<float>::infinity();
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(0, {0.0f, 0.5f}),
      makeCandidate(1, {0.0f, 0.5f}, /*alive=*/false),
      makeCandidate(2, {0.0f, 0.5f}, true, /*attackable=*/false),
      makeCandidate(3, {infinity, 0.0f}),
      makeCandidate(4, {0.0f, 0.76f}),  // 超出获取距离 0.75
  };
  const TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 0);
  assert(!result.id.has_value());
  assert(!result.showMarker);

  // 重复 id 全部拒绝（与 SoftTargeting 同契约）。
  const TargetLockResult duplicates = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f,
      {makeCandidate(5, {0.0f, 0.3f}), makeCandidate(5, {0.0f, 0.4f})},
      true, false, 16);
  assert(!duplicates.id.has_value());
}

void testBossParticipatesWithoutPriority() {
  // Boss 统一参与候选但不强制抢锁：更近的普通敌人仍然优先。
  TargetLockController controller(wideConfig());
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.0f, 0.6f}, true, true, /*boss=*/true),
      makeCandidate(2, {0.0f, 0.3f}),
  };
  const TargetLockResult result = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 0);
  assert(result.id.has_value() && *result.id == 2u);

  // 只剩 Boss 时正常锁定。
  const TargetLockResult bossOnly = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, {candidates[0]}, true, false, 16);
  assert(bossOnly.id.has_value() && *bossOnly.id == 1u);
}

void testIndependentOfInputOrder() {
  TargetLockController controller(wideConfig());
  std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.0f, 0.5f}),
      makeCandidate(2, {0.0f, 0.3f}),
      makeCandidate(3, {0.3f, 0.0f}),
  };
  const TargetLockResult forward = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 0);
  std::reverse(candidates.begin(), candidates.end());
  const TargetLockResult reversed = controller.updateAutomatic(
      {0.0f, 0.0f}, 0.0f, candidates, true, false, 16);
  assert(forward.id.has_value() && reversed.id.has_value());
  assert(*forward.id == *reversed.id);
  assert(near(forward.distance, reversed.distance));
  assert(near(forward.angle, reversed.angle));
}

void testRejectsNonFiniteContext() {
  TargetLockController controller(wideConfig());
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const std::vector<TargetLockCandidate> candidates{
      makeCandidate(1, {0.0f, 0.5f})};

  const TargetLockResult badPlayer = controller.updateAutomatic(
      {infinity, 0.0f}, 0.0f, candidates, true, false, 0);
  assert(!badPlayer.id.has_value() && !badPlayer.showMarker);

  const TargetLockResult badYaw = controller.updateAutomatic(
      {0.0f, 0.0f}, nan, candidates, true, false, 16);
  assert(!badYaw.id.has_value() && !badYaw.showMarker);
}

}  // namespace

int main() {
  testDistanceBeatsCameraAngle();
  testSameDistancePrefersForward();
  testComboKeepsCurrentTarget();
  testComboHoldDistanceLimit();
  testDeadTargetReselects();
  testMarkerFadesAfterAttackStops();
  testInvalidateAndClear();
  testFiltersInvalidCandidates();
  testBossParticipatesWithoutPriority();
  testIndependentOfInputOrder();
  testRejectsNonFiniteContext();
  return 0;
}
