#include "native/gameplay/world/interactable.h"

#include "native/engine/math/vec2.h"
#include "native/generated/world_layout.gen.h"

#include <algorithm>
#include <limits>

InteractableRegistry InteractableRegistry::defaultLayout() {
  InteractableRegistry registry;
  // 与主线 Q1 对应的引路灵（出生点附近）。
  registry.addInteractable({1, InteractableKind::Npc, 0.48f, 0.16f,
                            "引路灵", 1});
  // 与主线 Q4 对应的遗迹宝箱（中央祭坛东侧）。
  registry.addInteractable({2, InteractableKind::Chest, 0.58f, 0.52f,
                            "遗迹宝箱", 0});
  // 与主线 Q4 对应的脉流花（祭坛北侧）。
  registry.addInteractable({3, InteractableKind::Collectible, 0.46f, 0.58f,
                            "脉流花", 0});
  // 秘境入口（阶段二验收补齐）：可进入/退出的关卡副本。
  registry.addInteractable({4, InteractableKind::Dungeon, 0.34f, 0.38f,
                            "回声回廊·秘境", 0});
  return registry;
}

InteractableRegistry InteractableRegistry::openWorldLayout() {
  InteractableRegistry registry = defaultLayout();
  for (const WorldLayout::WorldNpcDef& def : WorldLayout::kNpcs) {
    registry.addInteractable({def.id, InteractableKind::Npc, def.x, def.y,
                              std::string(def.label), def.dialogId});
  }
  return registry;
}

void InteractableRegistry::addInteractable(Interactable interactable) {
  items_.push_back(std::move(interactable));
}

InteractableTarget InteractableRegistry::nearest(Vec2 position,
                                                 float interactRadius) const {
  InteractableTarget best;
  if (!position.finite() || interactRadius <= 0.0f) return best;
  float bestDistance = std::numeric_limits<float>::max();
  for (const Interactable& item : items_) {
    if (isConsumed(item.id)) continue;
    const Vec2 delta{item.x - position.x, item.y - position.y};
    const float distance = delta.length();
    if (distance <= interactRadius && distance < bestDistance) {
      bestDistance = distance;
      best.id = item.id;
      best.kind = item.kind;
      best.distance = distance;
      best.label = item.label;
      best.consumed = false;
    }
  }
  return best;
}

bool InteractableRegistry::interact(int32_t id) {
  const auto item =
      std::find_if(items_.begin(), items_.end(),
                   [id](const Interactable& candidate) {
                     return candidate.id == id;
                   });
  if (item == items_.end()) return false;
  if (item->kind == InteractableKind::Npc) return true;
  if (item->kind == InteractableKind::Dungeon) return true;
  if (isConsumed(id)) return false;
  consumedIds_.push_back(id);
  std::sort(consumedIds_.begin(), consumedIds_.end());
  return true;
}

bool InteractableRegistry::isConsumed(int32_t id) const {
  return std::binary_search(consumedIds_.begin(), consumedIds_.end(), id);
}

void InteractableRegistry::reviveConsumed(InteractableKind kind) {
  // 收集该种类全部 id，再从消耗列表中移除。
  std::vector<int32_t> reviveIds;
  for (const Interactable& item : items_) {
    if (item.kind == kind && isConsumed(item.id)) {
      reviveIds.push_back(item.id);
    }
  }
  if (reviveIds.empty()) return;
  std::vector<int32_t> remaining;
  for (int32_t id : consumedIds_) {
    if (std::find(reviveIds.begin(), reviveIds.end(), id) == reviveIds.end()) {
      remaining.push_back(id);
    }
  }
  consumedIds_ = std::move(remaining);
}

int32_t InteractableRegistry::dialogIdFor(int32_t id) const {
  for (const Interactable& item : items_) {
    if (item.id == id && item.kind == InteractableKind::Npc) {
      return item.dialogId;
    }
  }
  return 0;
}

void InteractableRegistry::restoreConsumed(int32_t mask) {
  for (const Interactable& item : items_) {
    if (item.kind == InteractableKind::Npc) continue;
    if (item.kind == InteractableKind::Dungeon) continue;
    if (item.id < 1 || item.id > 31) continue;
    const bool bitSet = (mask & (1 << (item.id - 1))) != 0;
    if (bitSet && !isConsumed(item.id)) {
      consumedIds_.push_back(item.id);
    }
  }
  std::sort(consumedIds_.begin(), consumedIds_.end());
}
