#include "native/gameplay/world/interactable.h"

#include "native/engine/math/vec2.h"

#include <cassert>
#include <cmath>

int main() {
  InteractableRegistry registry = InteractableRegistry::defaultLayout();
  assert(registry.items().size() == 4);

  // 出生点附近最近目标是引路灵 NPC。
  const InteractableTarget npc = registry.nearest({0.48f, 0.17f}, 0.06f);
  assert(npc.id == 1);
  assert(npc.kind == InteractableKind::Npc);
  assert(!npc.label.empty());
  assert(npc.distance <= 0.06f);

  // NPC 永不消耗，可重复交互并解析对话 id。
  assert(registry.interact(1));
  assert(registry.interact(1));
  assert(!registry.isConsumed(1));
  assert(registry.dialogIdFor(1) == 1);
  assert(registry.nearest({0.48f, 0.17f}, 0.06f).id == 1);

  // 宝箱首次交互消耗，二次交互失败且不再出现在最近查询。
  const InteractableTarget chest = registry.nearest({0.58f, 0.52f}, 0.05f);
  assert(chest.id == 2);
  assert(chest.kind == InteractableKind::Chest);
  assert(registry.interact(2));
  assert(registry.isConsumed(2));
  assert(!registry.interact(2));
  assert(registry.nearest({0.58f, 0.52f}, 0.05f).id == -1);

  // 采集物同样一次性消耗。
  assert(registry.interact(3));
  assert(registry.isConsumed(3));
  assert(!registry.interact(3));

  // 秘境入口（id=4）：可重复交互，永不消耗。
  const InteractableTarget gate = registry.nearest({0.34f, 0.38f}, 0.05f);
  assert(gate.id == 4);
  assert(gate.kind == InteractableKind::Dungeon);
  assert(registry.interact(4));
  assert(registry.interact(4));
  assert(!registry.isConsumed(4));
  assert(registry.nearest({0.34f, 0.38f}, 0.05f).id == 4);

  // 非 NPC 无对话 id；未知 id 返回 false/0。
  assert(registry.dialogIdFor(2) == 0);
  assert(!registry.interact(99));
  assert(registry.dialogIdFor(99) == 0);

  // 非法输入安全。
  assert(registry.nearest({std::nanf(""), 0.5f}, 0.1f).id == -1);
  assert(registry.nearest({0.5f, 0.5f}, -1.0f).id == -1);

  // 多目标在半径内取最近。
  InteractableRegistry dense;
  dense.addInteractable({10, InteractableKind::Chest, 0.5f, 0.5f, "近", 0});
  dense.addInteractable({11, InteractableKind::Chest, 0.55f, 0.5f, "远", 0});
  assert(dense.nearest({0.51f, 0.5f}, 0.1f).id == 10);

  // 重生（优化）：仅恢复指定种类的消耗记录。
  InteractableRegistry respawn;
  respawn.addInteractable({1, InteractableKind::Collectible, 0.2f, 0.2f, "花", 0});
  respawn.addInteractable({2, InteractableKind::Chest, 0.7f, 0.7f, "箱", 0});
  assert(respawn.interact(1));
  assert(respawn.interact(2));
  respawn.reviveConsumed(InteractableKind::Collectible);
  assert(!respawn.isConsumed(1));   // 采集物已重生。
  assert(respawn.isConsumed(2));    // 宝箱不受影响。
  assert(respawn.nearest({0.2f, 0.2f}, 0.05f).id == 1);
  // 无消耗记录时重生无副作用。
  respawn.reviveConsumed(InteractableKind::Dungeon);
  assert(respawn.isConsumed(2));
  return 0;
}
