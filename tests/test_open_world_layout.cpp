#include "native/gameplay/quest/dialog.h"
#include "native/gameplay/world/interactable.h"
#include "native/gameplay/world/teleport_anchor.h"
#include "native/generated/world_layout.gen.h"

#include <cassert>
#include <cmath>

int main() {
  // 旧工厂冻结：defaultLayout 仍为 7 锚点（旧测试断言不冲突）。
  TeleportAnchorSystem defaults = TeleportAnchorSystem::defaultLayout();
  assert(defaults.anchors().size() == 7);

  // openWorldLayout：12 锚点，id 1-12 无冲突。
  TeleportAnchorSystem openWorld = TeleportAnchorSystem::openWorldLayout();
  assert(openWorld.anchors().size() == 12);
  bool seenAnchor[13] = {false};
  for (const TeleportAnchor& anchor : openWorld.anchors()) {
    assert(anchor.id >= 1 && anchor.id <= 12);
    assert(!seenAnchor[anchor.id]);
    seenAnchor[anchor.id] = true;
  }
  for (int id = 1; id <= 12; ++id) {
    assert(seenAnchor[id]);
  }
  // 追加的 5 个锚点（id 8-12）位置与数据管线一致。
  for (const WorldLayout::WorldAnchorDef& def : WorldLayout::kAnchors) {
    bool found = false;
    for (const TeleportAnchor& anchor : openWorld.anchors()) {
      if (anchor.id != def.id) continue;
      found = true;
      assert(std::abs(anchor.x - def.x) < 1e-6f);
      assert(std::abs(anchor.y - def.y) < 1e-6f);
    }
    assert(found);
  }
  // 出生点锚点仍解锁，新增锚点未解锁。
  assert(openWorld.isUnlocked(1));
  assert(openWorld.unlockedCount() == 1);
  assert(!openWorld.isUnlocked(8));

  // 可交互物：defaultLayout 冻结，openWorldLayout 追加 6 个 NPC 交互点。
  InteractableRegistry base = InteractableRegistry::defaultLayout();
  InteractableRegistry world = InteractableRegistry::openWorldLayout();
  assert(world.items().size() == base.items().size() + WorldLayout::kNpcCount);
  // id 全局无冲突。
  for (size_t i = 0; i < world.items().size(); ++i) {
    for (size_t j = i + 1; j < world.items().size(); ++j) {
      assert(world.items()[i].id != world.items()[j].id);
    }
  }
  // NPC 交互点 id 32-37：位置与 NpcAgency 数据源一致，dialogId 100-105。
  for (const WorldLayout::WorldNpcDef& def : WorldLayout::kNpcs) {
    bool found = false;
    for (const Interactable& item : world.items()) {
      if (item.id != def.id) continue;
      found = true;
      assert(item.kind == InteractableKind::Npc);
      assert(item.x == def.x);
      assert(item.y == def.y);
      assert(item.dialogId == def.dialogId);
      assert(world.dialogIdFor(def.id) == def.dialogId);
    }
    assert(found);
  }
  // NPC 永不消耗：重复交互均成功且不标记消耗。
  assert(world.interact(32));
  assert(world.interact(32));
  assert(!world.isConsumed(32));
  // 旧 defaultLayout 不含新 NPC。
  assert(base.dialogIdFor(32) == 0);

  // 对话库：dialogId 100-105 全部注册；101/103/105 发布支线 201/202/203，
  // 其余无任务。
  const DialogLibrary& library = DialogLibrary::defaults();
  for (int32_t dialogId = 100; dialogId <= 105; ++dialogId) {
    const DialogDef* def = library.find(dialogId);
    assert(def != nullptr);
    assert(def->id == dialogId);
    assert(!def->lines.empty());
  }
  assert(library.find(100)->offeredQuestId == -1);
  assert(library.find(101)->offeredQuestId == 201);
  assert(library.find(102)->offeredQuestId == -1);
  assert(library.find(103)->offeredQuestId == 202);
  assert(library.find(104)->offeredQuestId == -1);
  assert(library.find(105)->offeredQuestId == 203);
  return 0;
}
