#include "native/gameplay/flow/dungeon.h"

#include <cassert>

int main() {
  DungeonSession dungeon;  // 默认 def：回声回廊，需 3 击杀。
  assert(dungeon.state() == DungeonState::Idle);
  assert(dungeon.def().killsRequired == 3);

  // 进入：Idle→Active；重复进入被拒。
  assert(dungeon.enter());
  assert(dungeon.state() == DungeonState::Active);
  assert(!dungeon.enter());

  // 击杀推进：未达标保持 Active。
  dungeon.notifyEnemiesKilled(2);
  assert(dungeon.state() == DungeonState::Active);
  assert(dungeon.kills() == 2);

  // 达标自动 Cleared；过量击杀钳制。
  dungeon.notifyEnemiesKilled(5);
  assert(dungeon.state() == DungeonState::Cleared);
  assert(dungeon.kills() == 3);
  // Cleared 后击杀不再累计。
  dungeon.notifyEnemiesKilled(2);
  assert(dungeon.kills() == 3);

  // 退出结算：Cleared 退出返回 true（发放奖励信号），回到 Idle。
  assert(dungeon.leave());
  assert(dungeon.state() == DungeonState::Idle);
  assert(dungeon.kills() == 0);

  // 未通关退出无结算；Idle 退出无效。
  assert(dungeon.enter());
  dungeon.notifyEnemiesKilled(1);
  assert(!dungeon.leave());
  assert(dungeon.state() == DungeonState::Idle);
  assert(!dungeon.leave());

  // 通关奖励只发一次：再次进入通关后退出才有结算。
  assert(dungeon.enter());
  dungeon.notifyEnemiesKilled(3);
  assert(dungeon.state() == DungeonState::Cleared);
  assert(dungeon.leave());
  // 负计数与 Idle 期击杀无效。
  dungeon.notifyEnemiesKilled(10);
  assert(dungeon.kills() == 0);
  dungeon.notifyEnemiesKilled(-1);
  assert(dungeon.kills() == 0);

  return 0;
}
