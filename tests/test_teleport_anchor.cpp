#include "native/gameplay/world/teleport_anchor.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
  TeleportAnchorSystem system = TeleportAnchorSystem::defaultLayout();

  // 默认布局：7 个锚点，出生点锚点已解锁。
  assert(system.anchors().size() == 7);
  assert(system.unlockedCount() == 1);
  assert(system.isUnlocked(1));
  assert(!system.isUnlocked(2));

  // 交互半径内取最近锚点。
  const AnchorInteraction nearSpawn =
      system.nearestInteraction({0.5f, 0.13f}, 0.05f);
  assert(nearSpawn.anchorId == 1);
  assert(nearSpawn.unlocked);
  assert(nearSpawn.distance <= 0.05f);
  assert(!nearSpawn.label.empty());

  // 半径外无交互目标。
  const AnchorInteraction farAway =
      system.nearestInteraction({0.5f, 0.5f - 0.3f}, 0.05f);
  // (0.5, 0.2) 距出生点 (0.5, 0.12) 为 0.08 > 0.05，无目标。
  assert(farAway.anchorId == -1);

  // 非法输入不崩溃。
  assert(system.nearestInteraction({std::nanf(""), 0.5f}, 0.1f).anchorId == -1);
  assert(system.nearestInteraction({0.5f, 0.5f}, -1.0f).anchorId == -1);

  // 首次交互未解锁锚点：解锁成功但不传送。
  const TeleportResult first = system.interact(2, {0.5f, 0.5f});
  assert(!first.success);
  assert(first.anchorId == 2);
  assert(system.isUnlocked(2));
  assert(system.unlockedCount() == 2);

  // 二次交互已解锁锚点：传送成功并返回锚点位置。
  const TeleportResult second = system.interact(2, {0.4f, 0.4f});
  assert(second.success);
  assert(std::abs(second.position.x - 0.5f) < 0.0001f);
  assert(std::abs(second.position.y - 0.5f) < 0.0001f);

  // 不存在的锚点：交互失败且不改变状态。
  const int32_t countBefore = system.unlockedCount();
  const TeleportResult missing = system.interact(999, {0.5f, 0.5f});
  assert(!missing.success);
  assert(missing.anchorId == -1);
  assert(system.unlockedCount() == countBefore);

  // 最近锚点选择：两个锚点都在半径内时取更近者。
  TeleportAnchorSystem custom;
  custom.addAnchor({10, 0.5f, 0.5f, "近"});
  custom.addAnchor({11, 0.6f, 0.5f, "远"});
  const AnchorInteraction closer =
      custom.nearestInteraction({0.52f, 0.5f}, 0.2f);
  assert(closer.anchorId == 10);

  // 重复解锁幂等：interact 对已解锁锚点直接传送，不会重复计数。
  (void)custom.interact(10, {0.5f, 0.5f});
  assert(custom.isUnlocked(10));
  const TeleportResult again = custom.interact(10, {0.5f, 0.5f});
  assert(again.success);
  assert(custom.unlockedCount() == 1);

  // 默认布局锚点覆盖至少 4 个不同世界分块（8x8 网格）。
  bool seen[64] = {false};
  int distinct = 0;
  for (const TeleportAnchor& anchor : system.anchors()) {
    const int cx = static_cast<int>(anchor.x * 8.0f) > 7
                       ? 7
                       : static_cast<int>(anchor.x * 8.0f);
    const int cy = static_cast<int>(anchor.y * 8.0f) > 7
                       ? 7
                       : static_cast<int>(anchor.y * 8.0f);
    const int index = cy * 8 + cx;
    if (!seen[index]) {
      seen[index] = true;
      ++distinct;
    }
  }
  assert(distinct >= 4);
  return 0;
}
