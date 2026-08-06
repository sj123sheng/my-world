#include "native/gameplay/quest/side_quests.h"

#include <cassert>

int main() {
  // 默认布局恰好三条支线（阶段二验收：≥3 条）。
  SideQuestSystem side = SideQuestSystem::defaults();
  assert(side.quests().size() == 3);
  assert(side.completedCount() == 0);

  // 击杀支线：累计 3 次完成；单次事件可携带多计数。
  side.notifyEvent(SideQuestEvent::Kill, 2);
  assert(side.progressOf(1) == 2);
  assert(!side.isCompleted(1));
  side.notifyEvent(SideQuestEvent::Kill, 5);
  assert(side.progressOf(1) == 3);  // 钳制到需求值。
  assert(side.isCompleted(1));
  assert(side.completedCount() == 1);

  // 采集支线：逐次推进。
  side.notifyEvent(SideQuestEvent::Collect);
  side.notifyEvent(SideQuestEvent::Collect);
  assert(side.isCompleted(2));

  // 锚点支线：完成后不再累计。
  side.notifyEvent(SideQuestEvent::ReachAnchor, 2);
  assert(side.isCompleted(3));
  side.notifyEvent(SideQuestEvent::ReachAnchor, 5);
  assert(side.completedCount() == 3);

  // 完成掩码与恢复：掩码往返一致。
  const int32_t mask = side.completedMask();
  assert(mask == 0b111);
  SideQuestSystem restored = SideQuestSystem::defaults();
  restored.restoreMask(mask);
  assert(restored.completedCount() == 3);
  assert(restored.progressOf(1) == 3);
  SideQuestSystem partial = SideQuestSystem::defaults();
  partial.restoreMask(0b010);
  assert(partial.completedCount() == 1);
  assert(partial.isCompleted(2));
  assert(!partial.isCompleted(1));

  // 非法输入：负计数与未知 id 无副作用。
  partial.notifyEvent(SideQuestEvent::Kill, -3);
  assert(partial.progressOf(3) == 0);
  assert(partial.progressOf(99) == 0);
  assert(!partial.isCompleted(99));

  return 0;
}
