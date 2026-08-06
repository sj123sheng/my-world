#pragma once

#include "native/engine/math/vec2.h"

#include <cstdint>
#include <string>
#include <vector>

// 传送锚点系统（开放世界探索基础）：
// 世界中的可交互锚点，靠近后可解锁；解锁后可再次交互直接传送。
// 解锁状态与传送结果完全确定性，供存档系统与回归测试消费。
struct TeleportAnchor {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
};

struct AnchorInteraction {
  int32_t anchorId = -1;
  bool unlocked = false;
  float distance = 0.0f;
  std::string label;
};

struct TeleportResult {
  bool success = false;
  Vec2 position{};
  int32_t anchorId = -1;
};

class TeleportAnchorSystem {
 public:
  // 默认世界锚点布局：出生点、中央祭坛与四个方位锚点，
  // 覆盖 ≥4 个世界分块，供流式加载与传送验收使用。
  static TeleportAnchorSystem defaultLayout();

  void addAnchor(TeleportAnchor anchor);

  // 查找交互半径内最近的锚点；无可用锚点返回 anchorId = -1。
  AnchorInteraction nearestInteraction(Vec2 position,
                                       float interactRadius) const;

  // 与锚点交互：未解锁则解锁并返回失败（不传送）；
  // 已解锁则传送成功并返回目标位置。
  TeleportResult interact(int32_t anchorId, Vec2 currentPosition);

  bool isUnlocked(int32_t anchorId) const;
  int32_t unlockedCount() const;
  const std::vector<TeleportAnchor>& anchors() const { return anchors_; }

  // 存档恢复：按位掩码批量解锁（bit(i-1) 对应锚点 id i）。
  void restoreUnlocked(int32_t mask);

 private:
  std::vector<TeleportAnchor> anchors_;
  std::vector<int32_t> unlockedIds_;
};
