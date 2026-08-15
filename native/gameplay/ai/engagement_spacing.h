#pragma once

#include "enemy_ai_types.h"

#include <vector>

// 敌人交战留白（Plan 2 Task 5）：原型距离参数、环形槽位与邻居分离。
// 纯函数、无状态，Encounter/WildSpawn/Boss 与 AI 决策共享同一口径。
// EngagementRange 类型定义在 enemy_ai_types.h（供 PerceptionSnapshot 携带）。

// 原型交战距离：近战/远程用字面量参数，Boss 按体型放大最小空挡。
EngagementRange EngagementRangeFor(EnemyArchetype archetype, float bodyRadius,
                                   bool boss);

// 环形槽位：参与者按 ID 排序取角间距 2π/N，基准角由最小 ID 的稳定哈希
// 决定；输入顺序不影响结果。退化输入回退到 player，不产生 NaN。
Vec2 EngagementSlotPosition(EntityId id, Vec2 player, float idealRadius,
                            const std::vector<EntityId>& participants);

struct EngagementNeighbor {
  EntityId id = 0;
  Vec2 position;
};

// 邻居分离：minimumSpacing 内的邻居产生推离向量，完全重叠时用双方 ID
// 的稳定哈希方向（互为镜像）；强度钳制，输出恒为有限值。
Vec2 SeparationOffset(EntityId self, Vec2 selfPosition,
                      const std::vector<EngagementNeighbor>& neighbors,
                      float minimumSpacing);
