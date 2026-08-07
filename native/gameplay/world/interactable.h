#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Vec2;

// 通用可交互物注册表（阶段二）：宝箱、采集物与 NPC。
// 锚点传送仍由 TeleportAnchorSystem 专管；本注册表负责其余交互物
// 的最近查询、一次性消耗状态与对话 id 解析。
enum class InteractableKind : uint8_t {
  Chest = 0,
  Collectible = 1,
  Npc = 2,
  Dungeon = 3,  // 秘境入口：可重复交互，不消耗。
};

struct Interactable {
  int32_t id = 0;
  InteractableKind kind = InteractableKind::Chest;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  // NPC 关联的对话 id；其余类型为 0。
  int32_t dialogId = 0;
};

struct InteractableTarget {
  int32_t id = -1;
  InteractableKind kind = InteractableKind::Chest;
  float distance = 0.0f;
  std::string label;
  bool consumed = false;
};

class InteractableRegistry {
 public:
  // 默认世界布局：与主线任务目标对应的宝箱、采集物与引路灵。
  static InteractableRegistry defaultLayout();

  // 开放世界布局（Phase 4）：defaultLayout 基础上追加
  // WorldLayout::kNpcs 的 6 个 NPC 交互点（id 32-37、dialogId 100-105），
  // 位置与 NpcAgency 严格一致。defaultLayout 保持冻结供旧测试使用。
  static InteractableRegistry openWorldLayout();

  void addInteractable(Interactable interactable);

  // 半径内最近的可交互物；无目标返回 id = -1。
  InteractableTarget nearest(Vec2 position, float interactRadius) const;

  // 交互：宝箱/采集物首次交互消耗并返回 true；NPC 与秘境入口永不消耗。
  // 未知 id 返回 false。
  bool interact(int32_t id);

  bool isConsumed(int32_t id) const;
  // 重生（采集物重置）：把指定种类的已消耗记录全部恢复可交互。
  void reviveConsumed(InteractableKind kind);
  // NPC 交互返回关联对话 id；非 NPC 或未知 id 返回 0。
  int32_t dialogIdFor(int32_t id) const;
  const std::vector<Interactable>& items() const { return items_; }

  // 存档恢复：按位掩码批量标记消耗（bit(id-1)）。
  void restoreConsumed(int32_t mask);

 private:
  std::vector<Interactable> items_;
  std::vector<int32_t> consumedIds_;
};
