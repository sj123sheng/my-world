// test_encounter_building_collision.cpp: 敌人与首领接入建筑碰撞的集成测试。
// 覆盖：EncounterController 移动积分后经注入解算器推出墙体、BossController
// 追击移动同样被阻挡、未注入解算器时行为不变（解算器可空）。

#include "native/gameplay/ai/encounter_controller.h"

#include "native/engine/world/environment_collision.h"

#include <cassert>
#include <cmath>

namespace {

// 覆盖玩家位置 (0.5, 0.5) 的方形墙体：追击者必须停在膨胀盒外。
BuildingCollision playerWall() {
  BuildingBox box;
  box.cx = 0.5f;
  box.cz = 0.5f;
  box.hx = 0.06f;
  box.hz = 0.06f;
  box.yaw = 0.0f;
  box.top = 0.05f;
  return BuildingCollision{{box}};
}

// 位置是否处于膨胀盒外：再次解算不再产生接触即视为合法。
bool outsideWall(const BuildingCollision& wall, Vec2 position, float radius) {
  float x = position.x;
  float y = position.y;
  const BuildingContact contact = wall.resolve(x, y, radius, 0.0f);
  return !contact.touching;
}

struct ResolverStats {
  int enemyCalls = 0;
  int bossCalls = 0;
};

std::function<void(Vec2&, float)> makeResolver(const BuildingCollision& wall,
                                               ResolverStats& stats) {
  return [&wall, &stats](Vec2& position, float radius) {
    if (radius == EncounterController::kEnemyCollisionRadius) {
      ++stats.enemyCalls;
    } else if (radius == BossController::kBossCollisionRadius) {
      ++stats.bossCalls;
    }
    wall.resolve(position.x, position.y, radius, 0.0f);
  };
}

}  // namespace

int main() {
  // ---- 敌人：追击玩家时被墙体阻挡，停在膨胀盒外 ----
  {
    const BuildingCollision wall = playerWall();
    ResolverStats stats;
    CombatController combat(CombatConfig::defaults());
    EncounterController encounter(combat);
    assert(encounter.start(EncounterMode::Beast));
    const std::function<void(Vec2&, float)> resolver =
        makeResolver(wall, stats);

    Tick tick = 0;
    for (int i = 0; i < 120; ++i) {
      encounter.update({tick, 16, {0.5f, 0.5f}, false, 0, resolver});
      tick += 16;
      for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
        if (!enemy.alive) continue;
        assert(outsideWall(wall, enemy.position,
                           EncounterController::kEnemyCollisionRadius));
      }
    }
    assert(stats.enemyCalls > 0);  // 每帧每个存活敌人都经过解算。
  }

  // ---- 首领：自由移动/追击同样被阻挡 ----
  {
    const BuildingCollision wall = playerWall();
    ResolverStats stats;
    CombatController combat(CombatConfig::defaults());
    EncounterController encounter(combat);
    assert(encounter.start(EncounterMode::Boss));
    const std::function<void(Vec2&, float)> resolver =
        makeResolver(wall, stats);

    Tick tick = 0;
    for (int i = 0; i < 100 && encounter.snapshot().state ==
                                   EncounterState::Running;
         ++i) {
      encounter.update({tick, 16, {0.5f, 0.5f}, false, 0, resolver});
      tick += 16;
      const BossSnapshot& boss = encounter.snapshot().boss;
      if (!boss.defeated) {
        assert(outsideWall(wall, boss.position,
                           BossController::kBossCollisionRadius));
      }
    }
    assert(stats.bossCalls > 0);  // 首领每帧经过解算。
  }

  // ---- 未注入解算器：行为与升级前一致（无阻挡，无崩溃）----
  {
    CombatController combat(CombatConfig::defaults());
    EncounterController encounter(combat);
    assert(encounter.start(EncounterMode::Beast));
    Tick tick = 0;
    for (int i = 0; i < 20; ++i) {
      encounter.update({tick, 16, {0.5f, 0.5f}, false, 0});
      tick += 16;
    }
    assert(encounter.snapshot().state == EncounterState::Running ||
           encounter.snapshot().state == EncounterState::Defeat);
  }

  return 0;
}
