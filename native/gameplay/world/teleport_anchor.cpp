#include "native/gameplay/world/teleport_anchor.h"

#include <algorithm>
#include <cmath>
#include <limits>

TeleportAnchorSystem TeleportAnchorSystem::defaultLayout() {
  TeleportAnchorSystem system;
  // 出生点锚点默认解锁：玩家开局即有一个可用传送目标。
  system.addAnchor({1, 0.5f, 0.12f, "遗迹营地"});
  system.addAnchor({2, 0.5f, 0.5f, "共鸣祭坛"});
  system.addAnchor({3, 0.14f, 0.5f, "西侧断崖"});
  system.addAnchor({4, 0.86f, 0.5f, "东侧石阵"});
  system.addAnchor({5, 0.5f, 0.86f, "北部高地"});
  system.addAnchor({6, 0.18f, 0.82f, "雾谷入口"});
  system.addAnchor({7, 0.82f, 0.18f, "沉没湖岸"});
  system.unlockedIds_.push_back(1);
  return system;
}

void TeleportAnchorSystem::addAnchor(TeleportAnchor anchor) {
  anchors_.push_back(std::move(anchor));
}

AnchorInteraction TeleportAnchorSystem::nearestInteraction(
    Vec2 position, float interactRadius) const {
  AnchorInteraction best;
  if (!position.finite() || interactRadius <= 0.0f) return best;
  float bestDistance = std::numeric_limits<float>::max();
  for (const TeleportAnchor& anchor : anchors_) {
    const Vec2 delta{anchor.x - position.x, anchor.y - position.y};
    const float distance = delta.length();
    if (distance <= interactRadius && distance < bestDistance) {
      bestDistance = distance;
      best.anchorId = anchor.id;
      best.unlocked = isUnlocked(anchor.id);
      best.distance = distance;
      best.label = anchor.label;
    }
  }
  return best;
}

TeleportResult TeleportAnchorSystem::interact(int32_t anchorId,
                                              Vec2 currentPosition) {
  (void)currentPosition;
  TeleportResult result;
  const auto anchor = std::find_if(
      anchors_.begin(), anchors_.end(),
      [anchorId](const TeleportAnchor& candidate) {
        return candidate.id == anchorId;
      });
  if (anchor == anchors_.end()) return result;
  result.anchorId = anchorId;
  if (!isUnlocked(anchorId)) {
    // 首次交互解锁锚点，不执行传送。
    unlockedIds_.push_back(anchorId);
    std::sort(unlockedIds_.begin(), unlockedIds_.end());
    return result;
  }
  result.success = true;
  result.position = {anchor->x, anchor->y};
  return result;
}

bool TeleportAnchorSystem::isUnlocked(int32_t anchorId) const {
  return std::binary_search(unlockedIds_.begin(), unlockedIds_.end(),
                            anchorId);
}

void TeleportAnchorSystem::restoreUnlocked(int32_t mask) {
  for (const TeleportAnchor& anchor : anchors_) {
    if (anchor.id < 1 || anchor.id > 31) continue;
    const bool bitSet = (mask & (1 << (anchor.id - 1))) != 0;
    if (bitSet && !isUnlocked(anchor.id)) {
      unlockedIds_.push_back(anchor.id);
    }
  }
  std::sort(unlockedIds_.begin(), unlockedIds_.end());
}

int32_t TeleportAnchorSystem::unlockedCount() const {
  return static_cast<int32_t>(unlockedIds_.size());
}
