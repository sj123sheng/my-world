#include "loop.h"
#include "native/engine/render/combat_animation.h"
#include "native/engine/render/combat_vfx.h"
#include "native/engine/resource/save.h"
#ifdef OHOS_PLATFORM
#include <hilog/log.h>
#endif
#include <thread>
#include <algorithm>
#include <vector>
#include <limits>
#include <cmath>

#ifdef OHOS_PLATFORM
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#endif

namespace {
void ApplyCombatSnapshot(GameSnapshot& output, const CombatSnapshot& combat) {
  auto applyEncounter = [](GameSnapshot& snap,
                           const EncounterSnapshot& encounter) {
    snap.levelStage = static_cast<int32_t>(encounter.levelStage);
    snap.gateState = static_cast<int32_t>(encounter.gateState);
    snap.supplyState = static_cast<int32_t>(encounter.supplyState);
    snap.bossHp = encounter.boss.hp;
    snap.bossPoise = encounter.boss.poise;
    snap.bossPhase = static_cast<int32_t>(encounter.boss.phase);
    snap.bossMechanic = static_cast<int32_t>(encounter.boss.mechanic);
    snap.bossCastMs = encounter.boss.castRemainingMs;
  };
  (void)applyEncounter;
  output.comboSegment = combat.comboSegment;
  output.hp = combat.playerHp;
  output.poise = combat.playerPoise;
  output.targetHp = combat.targetHp;
  output.targetPoise = combat.targetPoise;
  output.stamina = combat.stamina;
  output.resonance = combat.resonance;
  output.hasInsight = combat.hasInsight;
  output.invulnerable = combat.invulnerable;
  output.insightMs = combat.insightMs;
  output.pulseHitRemainingMs = combat.pulseHitRemainingMs;
  output.lastRejectReason = static_cast<int32_t>(combat.lastRejectReason);
  output.currentAction = combat.currentAction;
  output.comboWindowMs = combat.comboWindowMs;
  output.radianceCooldownMs = combat.radianceCooldownMs;
  output.currentCooldownMs = combat.currentCooldownMs;
  output.corruptionCooldownMs = combat.corruptionCooldownMs;
  output.radianceCooldownTotalMs = combat.radianceCooldownTotalMs;
  output.currentCooldownTotalMs = combat.currentCooldownTotalMs;
  output.corruptionCooldownTotalMs = combat.corruptionCooldownTotalMs;
  output.ultimateWindowMs = combat.ultimateWindowMs;
  output.targetPoiseBroken = combat.targetPoiseBroken;
  output.radianceAttached = combat.radianceAttached;
  output.currentAttached = combat.currentAttached;
  output.corruptionAttached = combat.corruptionAttached;
  output.corroded = combat.corroded;
  output.currentReaction = combat.currentReaction;
  output.pulsePhase = combat.pulsePhase;
}

Tick AdvanceCombatTime(Tick now, int64_t dtMs) {
  if (dtMs <= 0) return now;
  const Tick maximum = std::numeric_limits<Tick>::max();
  return now > maximum - dtMs ? maximum : now + dtMs;
}

// 把当前 EncounterSnapshot 的敌人与首领 2D 位置写入 Surface 的 3D 渲染字段。
// 渲染层只读消费这些状态，不反向修改游戏逻辑。敌人与首领位置已在
// 逻辑层（EncounterController/BossController）经建筑碰撞解算，此处直接同步。
void publish3DEncounterState(Surface& surface,
                             const EncounterSnapshot& snapshot,
                             float dtSeconds) {
  surface.enemies3d.clear();
  surface.enemies3d.reserve(snapshot.enemies.size());
  for (const EncounterEnemySnapshot& enemy : snapshot.enemies) {
    Enemy3DRenderState state;
    state.id = enemy.id;
    state.x = enemy.position.x;
    state.y = enemy.position.y;
    state.archetype = static_cast<int>(enemy.archetype);
    state.alive = enemy.alive;
    state.animation.alive = enemy.alive;
    state.animation.action = enemy.attacking ? RenderAnimation::Attack
                                             : RenderAnimation::Idle;
    // 原型攻击 clip 差异化：爪击/仪式/盾击/重斩/法术/旋转斩。
    state.animation.attackClip = EnemyAttackClipFor(state.archetype);
    state.animation.hit = enemy.hit;
    state.animation.moving = enemy.moving;
    state.windingUp = enemy.windingUp;
    state.attacking = enemy.attacking;
    // 模型局部 +Z 为前方，逻辑 (x, y) 映射到 3D (x, z)。
    state.angle = std::atan2(enemy.facing.x, enemy.facing.y);
    // 元素附着位掩码：渲染层据此绘制脚下元素光环（原神式附着指示）。
    state.auraMask = AuraMaskFromFlags(enemy.radianceAttached,
                                       enemy.currentAttached,
                                       enemy.corruptionAttached);
    // 尸体淡出计时：死亡期间持续累加，复活则归零；
    // 渲染层按 DeathFadeAlpha 把尸体线性淡出到完全移除。
    if (!enemy.alive) {
      state.deathSeconds = surface.enemyDeathSeconds[enemy.id] += dtSeconds;
    } else {
      surface.enemyDeathSeconds.erase(enemy.id);
    }
    // 受击/死亡动画变体轮换：存活时按受击次数奇偶切换 hit/Hit_B；
    // 死亡时叠加实体 id，让群体死亡的倒地姿态互不相同，且死亡
    // 期间变体恒定，避免尸体在两个倒地姿态间跳变。
    const auto hitCount = surface.enemyHitCounts.find(enemy.id);
    state.hitCount = hitCount != surface.enemyHitCounts.end() ? hitCount->second : 0u;
    state.animation.variant = enemy.alive
        ? static_cast<uint8_t>(state.hitCount & 1u)
        : static_cast<uint8_t>((state.hitCount + enemy.id) & 1u);
    surface.enemies3d.push_back(state);
  }
  // 清理已离开快照的敌人的淡出计时与受击计数，避免长期泄漏。
  // 存在性同时检查野外敌人列表（共享同一批 id 状态表）。
  const auto presentInAnyEnemyList = [&surface](uint32_t id) {
    return std::any_of(surface.enemies3d.begin(), surface.enemies3d.end(),
                       [id](const Enemy3DRenderState& enemy) {
                         return enemy.id == id;
                       }) ||
           std::any_of(surface.wildEnemies3d.begin(),
                       surface.wildEnemies3d.end(),
                       [id](const WildEnemy3DRenderState& enemy) {
                         return enemy.id == id;
                       });
  };
  for (auto death = surface.enemyDeathSeconds.begin();
       death != surface.enemyDeathSeconds.end();) {
    if (presentInAnyEnemyList(death->first)) {
      ++death;
    } else {
      surface.enemyHitCounts.erase(death->first);
      death = surface.enemyDeathSeconds.erase(death);
    }
  }
  // 受击计数独立清理：未被击杀过的敌人不会进入淡出计时表。
  for (auto count = surface.enemyHitCounts.begin();
       count != surface.enemyHitCounts.end();) {
    if (presentInAnyEnemyList(count->first)) {
      ++count;
    } else {
      count = surface.enemyHitCounts.erase(count);
    }
  }

  // 首领位置/朝向来自逻辑层快照：Boss 可自由移动，渲染层只读跟随。
  surface.boss3d.x = snapshot.boss.position.x;
  surface.boss3d.y = snapshot.boss.position.y;
  surface.boss3d.phase = static_cast<int>(snapshot.boss.phase);
  surface.boss3d.defeated = snapshot.boss.defeated;
  surface.boss3d.moving = snapshot.boss.moving;
  surface.boss3d.basicAttacking = snapshot.boss.basicAttackCastRemainingMs > 0;
  surface.boss3d.basicAttackVariant = snapshot.boss.basicAttackVariant;
  surface.boss3d.active =
      snapshot.mode == EncounterMode::Boss &&
      snapshot.state != EncounterState::Stopped;
  // 出场渐入计时：激活且未击败期间累加，退出或击败归零，
  // 渲染层按 BossEntranceReveal 抬升轮廓光强度。
  if (surface.boss3d.active && !snapshot.boss.defeated) {
    surface.boss3d.entranceSeconds += dtSeconds;
  } else {
    surface.boss3d.entranceSeconds = 0.0f;
  }
  // 首领吟唱机制或普攻前摇期间是玩家的应对窗口：前摇未完时显示预警环。
  surface.boss3d.windingUp =
      (snapshot.boss.mechanic != BossMechanic::None &&
       snapshot.boss.castRemainingMs > 0) ||
      surface.boss3d.basicAttacking;
  surface.boss3d.mechanic = static_cast<int>(snapshot.boss.mechanic);
  surface.boss3d.animation.alive = !snapshot.boss.defeated;
  surface.boss3d.hitAnimationSeconds = std::max(
      0.0f, surface.boss3d.hitAnimationSeconds - dtSeconds);
  const RenderAnimation bossAnimation = BossRenderAnimation(
      snapshot.boss, surface.boss3d.previousHp);
  if (bossAnimation == RenderAnimation::Hit) {
    surface.boss3d.hitAnimationSeconds = 0.2f;
  }
  surface.boss3d.animation.action =
      bossAnimation == RenderAnimation::Hit ? RenderAnimation::Idle
                                            : bossAnimation;
  surface.boss3d.animation.hit = surface.boss3d.hitAnimationSeconds > 0.0f;
  // 首领普攻变体 clip：重劈/吟唱束流/旋转冲击按变体切换。
  surface.boss3d.animation.attackClip =
      BossAttackClipFor(snapshot.boss.basicAttackVariant);
  // 首领移动/普攻状态驱动跑动与攻击动画（ChooseAnimation 按优先级选择）。
  surface.boss3d.animation.moving = snapshot.boss.moving;
  surface.boss3d.previousHp = snapshot.boss.hp;
  // 朝向角直接取逻辑层发布的面向向量，与追击/环绕走位严格同步。
  if (snapshot.boss.facing.finite() &&
      (snapshot.boss.facing.x != 0.0f || snapshot.boss.facing.y != 0.0f)) {
    surface.boss3d.angle =
        std::atan2(snapshot.boss.facing.x, snapshot.boss.facing.y);
  }
}

// 野外敌人快照写入 Surface 3D 渲染字段（仿 enemies3d 模式，共享状态表）。
void publishWildEnemies3d(Surface& surface, const WildSpawnSystem& wild,
                          float dtSeconds) {
  surface.wildEnemies3d.clear();
  for (const WildEnemySnapshot& enemy : wild.snapshot()) {
    WildEnemy3DRenderState state;
    state.id = enemy.id;
    state.x = enemy.position.x;
    state.y = enemy.position.y;
    state.archetype = enemy.archetype;
    state.alive = enemy.alive;
    state.animation.alive = enemy.alive;
    state.animation.action = enemy.attacking ? RenderAnimation::Attack
                                             : RenderAnimation::Idle;
    state.animation.attackClip = EnemyAttackClipFor(state.archetype);
    state.animation.hit = enemy.hit;
    state.animation.moving = enemy.moving;
    state.windingUp = enemy.windingUp;
    state.attacking = enemy.attacking;
    state.angle = std::atan2(enemy.facing.x, enemy.facing.y);
    if (!enemy.alive) {
      state.deathSeconds = surface.enemyDeathSeconds[enemy.id] += dtSeconds;
    } else {
      surface.enemyDeathSeconds.erase(enemy.id);
    }
    const auto hitCount = surface.enemyHitCounts.find(enemy.id);
    state.hitCount = hitCount != surface.enemyHitCounts.end() ? hitCount->second : 0u;
    state.animation.variant =
        enemy.alive ? static_cast<uint8_t>(state.hitCount & 1u)
                    : static_cast<uint8_t>((state.hitCount + enemy.id) & 1u);
    surface.wildEnemies3d.push_back(state);
  }
}

// 流式半径性能联动（Phase 5）：按 PerformanceGuard 视距缩放决定半径——
// 档位对应 viewDistanceScale：Full 1.0 / Light 0.9 / Medium 0.75 → 半径 2，
// Heavy 0.6 / Critical 0.45 → 1；低画质预设（qualityPreset=1）强制取小。
int32_t streamingRadiusForPerf(int32_t qualityPreset,
                               const PerformanceGuard& guard) {
  const int32_t radius = guard.viewDistanceScale() >= 0.7f ? 2 : 1;
  return qualityPreset == 1 ? std::min(radius, 1) : radius;
}

// NPC 渲染上限性能联动（Phase 5）：按 lodLevel 收缩 6→4→3。
// lodLevel 取 PerformanceGuard::lodLevel()（0=完整 1=中等 2=精简）。
int32_t npcVisibleLimitForPerf(int32_t lodLevel) {
  if (lodLevel >= 2) return 3;
  if (lodLevel == 1) return 4;
  return NpcAgency::kMaxVisible;
}

// NPC 渲染发布（Phase 4）：同屏 ≤6，超出按距玩家距离裁剪（常量）。
// maxVisible 为性能联动后的上限（Phase 5），默认保持原行为。
void publishNpcs3d(Surface& surface, const NpcAgency& agency,
                   float playerX, float playerY,
                   int32_t maxVisible = NpcAgency::kMaxVisible) {
  surface.npcs3d.clear();
  const std::vector<NpcAgentSnapshot>& agents = agency.agents();
  if (agents.empty()) return;
  std::vector<size_t> order(agents.size());
  for (size_t index = 0; index < agents.size(); ++index) order[index] = index;
  const auto distSq = [&agents, playerX, playerY](size_t index) {
    const float dx = agents[index].x - playerX;
    const float dy = agents[index].y - playerY;
    return dx * dx + dy * dy;
  };
  std::sort(order.begin(), order.end(),
            [&distSq](size_t a, size_t b) { return distSq(a) < distSq(b); });
  const size_t visibleCount =
      std::min<size_t>(order.size(),
                       static_cast<size_t>(std::max(maxVisible, 0)));
  surface.npcs3d.reserve(visibleCount);
  for (size_t rank = 0; rank < visibleCount; ++rank) {
    const NpcAgentSnapshot& npc = agents[order[rank]];
    Npc3DRenderState state;
    state.id = static_cast<uint32_t>(npc.id);
    state.x = npc.x;
    state.y = npc.y;
    state.angle = npc.angle;
    state.behavior = npc.behavior;
    state.visible = true;
    // NPC 仅 idle/walk：移动时低 moveRatio 使渲染侧选 Walking 片段。
    state.animation.alive = true;
    state.animation.action = RenderAnimation::Idle;
    state.animation.moving = npc.moving;
    state.animation.moveRatio = npc.moving ? 0.2f : 1.0f;
    surface.npcs3d.push_back(state);
  }
}

// 对话会话推进并在结束时处理任务发布（Phase 4）。
// advance 前捕获 offeredQuestId（会话结束后 def_ 被置空）；accept 后
// 立即补发 notifyNpcTalked，使发布对话本身计入首个 TalkToNpc 目标。
void advanceDialogSession(Loop& loop) {
  if (!loop.dialogSession.active()) return;
  const int32_t offeredQuestId = loop.dialogSession.offeredQuestId();
  const int32_t npcId = loop.npcAgency.talkingNpcId();
  if (loop.dialogSession.advance()) return;
  if (npcId >= 0) loop.npcAgency.endTalk(npcId);
  if (offeredQuestId < 0) return;
  if (!loop.openWorldQuests.accept(offeredQuestId)) return;
  loop.openWorldQuests.notifyNpcTalked(npcId);
  loop.npcOfferQuestId = offeredQuestId;
  for (const QuestDef& quest : loop.openWorldQuests.quests()) {
    if (quest.id == offeredQuestId) {
      loop.npcOfferQuestTitle = quest.title;
      break;
    }
  }
}

// 在 surface_draw 前更新 3D 透视相机。yaw/pitch/distance 来自现有 2D
// ThirdPersonCamera，玩家 3D 目标位置取 (player.x, height+0.05, player.y)，
// 0.05 为玩家立方体半高，使相机平视角色而非俯视地面；
// height 来自探索运动状态（地形贴合/跳跃/水面）。
void update3DCamera(Surface& surface, const ThirdPersonCamera& camera,
                    float playerHeight) {
  const float eyeHeight =
      (std::isfinite(playerHeight) ? playerHeight : 0.0f) + 0.05f;
  const glm::vec3 target{surface.player.x + surface.vfxCameraShakeX, eyeHeight,
                         surface.player.y + surface.vfxCameraShakeY};
  surface.camera3d.follow(target, camera.yaw(), camera.pitch(),
                          camera.distance());
  // 共鸣 FOV 冲击：反应触发瞬间收窄视场角（zoom-in punch）再缓出
  // 恢复；无激活时恢复默认 45°。振幅按触发档位写入（轻重分层）。
  const float fovPunch =
      surface.resonanceFovSeconds >= 0.0f
          ? FovPunchOffsetAt(surface.resonanceFovSeconds,
                             surface.fovPunchMaxOffset)
          : 0.0f;
  surface.camera3d.fov = 45.0f + fovPunch;
}

// 按实体 ID 解析世界坐标，供伤害飘字定位。wild 为野外敌人兜底。
std::optional<Vec2> resolveEntityPosition(const Surface& surface,
                                          const EncounterSnapshot& encounter,
                                          EntityId id,
                                          const WildSpawnSystem* wild = nullptr) {
  if (id == CombatController::kPlayerId) {
    return Vec2{surface.player.x, surface.player.y};
  }
  if (id == CombatController::kTrainingTargetId) {
    return Vec2{surface.trainingTarget.x, surface.trainingTarget.y};
  }
  if (id == EncounterController::kBossId) {
    return Vec2{surface.boss3d.x, surface.boss3d.y};
  }
  for (const EncounterEnemySnapshot& enemy : encounter.enemies) {
    if (enemy.id == id) {
      return enemy.position;
    }
  }
  if (wild != nullptr) {
    Vec2 position;
    if (wild->positionOf(id, position)) return position;
  }
  return std::nullopt;
}

// 按实体 ID 解析敌方元素归属（原神式元素可读性）：遭遇敌人与野外
// 敌人同表查询，返回源质编号（0=辉印 1=脉流 2=蚀质）；物理原型 /
// 首领 / 训练假人 / 未知实体返回 nullopt，击杀反馈保持亮金。
std::optional<int> resolveEnemyElement(const EncounterSnapshot& encounter,
                                       EntityId id,
                                       const WildSpawnSystem* wild = nullptr) {
  for (const EncounterEnemySnapshot& enemy : encounter.enemies) {
    if (enemy.id == id) {
      const int element = EnemyElementFor(static_cast<int>(enemy.archetype));
      return element >= 0 ? std::optional<int>{element} : std::nullopt;
    }
  }
  if (wild != nullptr) {
    for (const WildEnemySnapshot& enemy : wild->snapshot()) {
      if (enemy.id == id) {
        const int element = EnemyElementFor(enemy.archetype);
        return element >= 0 ? std::optional<int>{element} : std::nullopt;
      }
    }
  }
  return std::nullopt;
}

// 命中火花发射：LCG 伪随机确定方向/速度（同输入可重现），
// 在命中点爆发一圈向外上扬的短命粒子。
// kind：0=金橙命中，1=红色玩家受击，2=亮金击杀爆裂，
// 4=辉印金白，5=脉流青蓝，6=蚀质暗紫（技能释放）。
// sizeScale：归属实体的模型缩放比例，火花尺寸与扩散速度同步放大。
void spawnHitSparks(Surface& surface, Vec2 position, int kind, int count = 6,
                    float speedScale = 1.0f, float lifeScale = 1.0f,
                    float sizeScale = 1.0f) {
  if (surface.hitSparks3d.size() > 128) return;
  constexpr float kTau = 6.2831853f;
  for (int i = 0; i < count; ++i) {
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r0 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r1 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    const float angle =
        (static_cast<float>(i) / static_cast<float>(count)) * kTau +
        (r0 - 0.5f) * 0.9f;
    const float speed = (0.02f + 0.025f * r1) * speedScale * sizeScale;
    const float life = (0.22f + 0.1f * r0) * lifeScale;
    surface.hitSparks3d.push_back(
        {position.x, 0.02f, position.y, std::cos(angle) * speed,
         (0.04f + 0.04f * r1) * speedScale * sizeScale, std::sin(angle) * speed,
         life, life, kind, sizeScale});
  }
}

// 受击方向性粒子：沿攻击方向（击退方向）喷射的短命火花，
// LCG 决定横向偏转与速度抖动（同输入可重现）；kind<=2 自动受
// 火花重力影响形成抛物线拖尾，强化打击方向感。
// 前摇聚能粒子：从实体周身环带向中心汇聚（寿命结束恰好抵达
// 中心，ConvergingSparkMotion 同源），原神式蓄力前兆。
void spawnConvergingSparks(Surface& surface, Vec2 center, int kind, int count,
                           float radius, float sizeScale) {
  if (surface.hitSparks3d.size() > 120) return;
  constexpr float kLife = 0.24f;
  constexpr float kTau = 6.2831853f;
  for (int i = 0; i < count; ++i) {
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r0 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r1 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    float ox = 0.0f, oz = 0.0f, vx = 0.0f, vz = 0.0f;
    ConvergingSparkMotion(r0 * kTau, radius * (0.75f + 0.5f * r1), kLife, ox,
                          oz, vx, vz);
    surface.hitSparks3d.push_back({center.x + ox, 0.03f, center.y + oz, vx,
                                   0.008f, vz, kLife, kLife, kind, sizeScale});
  }
}

void spawnDirectionalSparks(Surface& surface, Vec2 position, Vec2 direction,
                            int kind, int count, float speedScale,
                            float sizeScale) {
  if (surface.hitSparks3d.size() > 128) return;
  for (int i = 0; i < count; ++i) {
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r0 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
    const float r1 =
        static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) / 65535.0f;
    const float spread = (r0 - 0.5f) * 2.0f;  // -1..1 → ±60°
    const float speed = (0.05f + 0.03f * r1) * speedScale * sizeScale;
    const float lift = (0.03f + 0.03f * r1) * speedScale * sizeScale;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    DirectionalSparkVelocity(direction.x, direction.y, speed, spread, lift,
                             vx, vy, vz);
    const float life = 0.25f + 0.08f * r0;
    surface.hitSparks3d.push_back({position.x, 0.03f, position.y, vx, vy, vz,
                                   life, life, kind, sizeScale});
  }
}

// 特效尺寸比例：按实体归属的模型档案缩放 / 基准缩放派生，
// 模型放大后命中火花、技能爆发与投射物尺寸同步跟随。
float actorVfxRatio(const Surface& surface, EntityId id) {
  if (id == CombatController::kPlayerId) {
    return VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player);
  }
  if (id == EncounterController::kBossId) {
    return VfxSizeRatio(surface.bossAssetProfile, ModelKind::Boss);
  }
  return VfxSizeRatio(surface.enemyAssetProfile, ModelKind::Enemy);
}

// 释放过程投射物：从主角胸口朝目标发射沿直线飞行的粒子，
// 飞行时长按距离取 0.12~0.26s；寿命结束恰与命中点爆裂火花衔接，
// 形成“释放→飞行→命中”的完整动效。kind 复用火花配色。
// count>1 时沿飞行方向法线横向错开起点，呈轻微束流状。
void spawnAttackProjectiles(Surface& surface, Vec2 from, Vec2 to, int kind,
                            float sizeScale, int count = 1) {
  const Vec2 delta = to - from;
  const float distance = delta.length();
  if (!delta.finite() || distance < 0.02f || surface.hitSparks3d.size() > 120) {
    return;
  }
  const float travelSeconds = std::clamp(distance / 1.6f, 0.12f, 0.26f);
  const Vec2 velocity = delta * (1.0f / travelSeconds);
  const Vec2 side{-delta.y / distance, delta.x / distance};
  for (int i = 0; i < count; ++i) {
    const float offset = (static_cast<float>(i) -
                          static_cast<float>(count - 1) * 0.5f) * 0.02f;
    const Vec2 origin = from + side * offset;
    surface.hitSparks3d.push_back(
        {origin.x, 0.035f, origin.y, velocity.x, 0.012f, velocity.y,
         travelSeconds, travelSeconds, kind, sizeScale * 2.0f});
  }
}

// 释放冲击波：施法者脚下生成扩散光环（上限防溢出），颜色随源质。
void spawnShockwave(Surface& surface, Vec2 position, glm::vec3 color,
                    float maxRadius) {
  if (surface.shockwaveRings.size() > 24) return;
  surface.shockwaveRings.push_back(
      {position.x, position.y, 0.0f, maxRadius, color});
}

// 命中冲击贴花：受击点脚下生成短促源质色光斑（上限防溢出）。
void spawnImpactDecal(Surface& surface, Vec2 position, glm::vec3 color,
                      float maxRadius) {
  if (surface.impactDecals.size() > 24) return;
  surface.impactDecals.push_back(
      {position.x, position.y, 0.0f, maxRadius, color});
}

// 共鸣爆发光柱：受击点升起垂直元素光柱（上限防溢出），
// 高度已含实体缩放与反应类型档位。
void spawnLightPillar(Surface& surface, Vec2 position, glm::vec3 color,
                      float maxHeight) {
  if (surface.lightPillars.size() > 16) return;
  surface.lightPillars.push_back(
      {position.x, position.y, 0.0f, maxHeight, color});
}

// 元素技能符文环：施法者脚下生成旋转双新月符阵（上限防溢出）。
void spawnSkillRune(Surface& surface, Vec2 position, glm::vec3 color,
                    float maxRadius) {
  if (surface.skillRunes.size() > 16) return;
  surface.skillRunes.push_back(
      {position.x, position.y, 0.0f, maxRadius, color});
}

// 敌方普攻刀光：挥击上升沿生成新月弧线，尺寸随原型缩放，
// 颜色按原型元素染色（原神式敌方元素可读性）。
void spawnEnemySlashArc(Surface& surface, uint32_t id, Vec2 position,
                        float yaw, int archetype) {
  if (surface.enemySlashArcs.size() > 16) return;
  surface.enemySlashArcs.push_back(
      {id, position.x, position.y, yaw, 0.0f, EnemyArchetypeScale(archetype),
       EnemySkillColorFor(archetype)});
}

// 敌方释放动效：与主角侧对称——敌人前摇开始时在自身位置爆出
// 蓄力火花（施法前兆），挥击瞬间朝主角发射红色投射物并挥出
// 红色刀光；首领吟唱开始时爆出更大规模的暗紫火花环并向主角齐射束流。
// 返回本步是否发生首领阶段转换（供调用侧施加卡肉/FOV 冲击）。
// 敌方释放动效结果：调用侧据此施加镜头反馈（分层）。
struct EnemyReleaseVfxResult {
  bool bossCameraFeedback = false;  // 出场/转阶段：重卡肉 + FOV 冲击
  bool bossSlamLanded = false;      // 普攻挥击落地：相机震动
};

EnemyReleaseVfxResult spawnEnemyReleaseVfx(Surface& surface) {
  // 首领戏剧性事件（出场/转阶段）→ 调用侧施加卡肉 + FOV 冲击。
  EnemyReleaseVfxResult result;
  const Vec2 playerPos{surface.player.x, surface.player.y};
  // 遭遇敌人与野外敌人字段布局一致，共用同一边沿检测模板。
  const auto processEnemy = [&surface, playerPos](const auto& enemy) {
    const auto prevWindup = surface.enemyPrevWindingUp.find(enemy.id);
    const auto prevAttack = surface.enemyPrevAttacking.find(enemy.id);
    const bool wasWinding =
        prevWindup != surface.enemyPrevWindingUp.end() && prevWindup->second;
    const bool wasAttacking =
        prevAttack != surface.enemyPrevAttacking.end() && prevAttack->second;
    if (enemy.alive) {
      const float ratio =
          actorVfxRatio(surface, static_cast<EntityId>(enemy.id));
      const Vec2 enemyPos{enemy.x, enemy.y};
      // 敌方技能元素色化：蓄力火花与投射物 kind 按原型元素染色
      //（物理原型保持红 kind 1）。
      const int skillKind = EnemySkillSparkKindFor(enemy.archetype);
      if (enemy.windingUp && !wasWinding) {
        spawnHitSparks(surface, enemyPos, skillKind, 6, 1.0f, 1.0f, ratio);
      }
      if (enemy.attacking && !wasAttacking) {
        spawnAttackProjectiles(surface, enemyPos, playerPos, skillKind, ratio,
                               2);
        spawnEnemySlashArc(surface, enemy.id, enemyPos, enemy.angle,
                           enemy.archetype);
      }
    }
    surface.enemyPrevWindingUp[enemy.id] = enemy.windingUp;
    surface.enemyPrevAttacking[enemy.id] = enemy.attacking;
  };
  for (const Enemy3DRenderState& enemy : surface.enemies3d) {
    processEnemy(enemy);
  }
  for (const WildEnemy3DRenderState& enemy : surface.wildEnemies3d) {
    processEnemy(enemy);
  }
  // 清理已离开快照的敌人的边沿状态，避免长期泄漏（两个列表同查）。
  const auto presentInAny = [&surface](uint32_t id) {
    return std::any_of(surface.enemies3d.begin(), surface.enemies3d.end(),
                       [id](const Enemy3DRenderState& enemy) {
                         return enemy.id == id;
                       }) ||
           std::any_of(surface.wildEnemies3d.begin(),
                       surface.wildEnemies3d.end(),
                       [id](const WildEnemy3DRenderState& enemy) {
                         return enemy.id == id;
                       });
  };
  for (auto state = surface.enemyPrevWindingUp.begin();
       state != surface.enemyPrevWindingUp.end();) {
    if (presentInAny(state->first)) {
      ++state;
    } else {
      state = surface.enemyPrevWindingUp.erase(state);
    }
  }
  for (auto state = surface.enemyPrevAttacking.begin();
       state != surface.enemyPrevAttacking.end();) {
    if (presentInAny(state->first)) {
      ++state;
    } else {
      state = surface.enemyPrevAttacking.erase(state);
    }
  }
  // 击败边沿（存活→击败）：首领死亡爆发（出场仪式的收尾呼应）——
  // 击败瞬间周身爆发阶段元素色大火花 + 冲击波 + 光柱 + 符阵，
  // 镜头反馈与转阶段同源，把击杀拎成高光时刻。
  if (surface.boss3d.active && surface.boss3d.defeated &&
      !surface.bossPrevDefeated) {
    const float deathRatio =
        actorVfxRatio(surface, EncounterController::kBossId);
    const Vec2 deathPos{surface.boss3d.x, surface.boss3d.y};
    const BossPhaseVfx deathVfx = BossPhaseVfxFor(surface.boss3d.phase);
    spawnHitSparks(surface, deathPos, deathVfx.sparkKind, 32, 2.4f, 1.7f,
                   deathRatio * deathVfx.scale * 1.3f);
    spawnShockwave(surface, deathPos, deathVfx.color,
                   0.24f * deathRatio * deathVfx.scale);
    spawnLightPillar(surface, deathPos, deathVfx.color,
                     0.20f * deathRatio * deathVfx.scale);
    spawnSkillRune(surface, deathPos, deathVfx.color,
                   0.14f * deathRatio * deathVfx.scale);
    result.bossCameraFeedback = true;
  }
  surface.bossPrevDefeated = surface.boss3d.defeated;
  // 首领：机制吟唱与普攻分别做边沿检测，释放差异化动效。
  if (surface.boss3d.active && !surface.boss3d.defeated) {
    const float bossRatio =
        actorVfxRatio(surface, EncounterController::kBossId);
    const Vec2 bossPos{surface.boss3d.x, surface.boss3d.y};
    // 出场边沿（未激活→激活）：首领周身爆发阶段元素色大火花 +
    // 冲击波 + 光柱 + 符文环（原神首领出场仪式），与出场渐入轮廓
    // 光同帧叠加；镜头反馈与转阶段同源。
    if (!surface.bossPrevActive) {
      const BossPhaseVfx entranceVfx =
          BossPhaseVfxFor(surface.boss3d.phase);
      spawnHitSparks(surface, bossPos, entranceVfx.sparkKind, 24, 2.0f,
                     1.5f, bossRatio * entranceVfx.scale * 1.1f);
      spawnShockwave(surface, bossPos, entranceVfx.color,
                     0.18f * bossRatio * entranceVfx.scale);
      spawnLightPillar(surface, bossPos, entranceVfx.color,
                       0.14f * bossRatio * entranceVfx.scale);
      spawnSkillRune(surface, bossPos, entranceVfx.color,
                     0.11f * bossRatio * entranceVfx.scale);
      result.bossCameraFeedback = true;
    }
    surface.bossPrevActive = true;
    // 阶段转换边沿（1→2→3）：首领周身爆发阶段元素色大火花 +
    // 冲击波 + 光柱 + 符文环（原神首领转阶段仪式）。激活 0→1
    // 由出场渐入表达，不视为转阶段。
    if (surface.boss3d.phase != surface.bossPrevPhase) {
      if (surface.bossPrevPhase > 0 && surface.boss3d.phase > 0) {
        const BossPhaseVfx phaseVfx =
            BossPhaseVfxFor(surface.boss3d.phase);
        spawnHitSparks(surface, bossPos, phaseVfx.sparkKind, 28, 2.2f,
                       1.6f, bossRatio * phaseVfx.scale * 1.2f);
        spawnShockwave(surface, bossPos, phaseVfx.color,
                       0.20f * bossRatio * phaseVfx.scale);
        spawnLightPillar(surface, bossPos, phaseVfx.color,
                         0.16f * bossRatio * phaseVfx.scale);
        spawnSkillRune(surface, bossPos, phaseVfx.color,
                       0.12f * bossRatio * phaseVfx.scale);
        result.bossCameraFeedback = true;
      }
      surface.bossPrevPhase = surface.boss3d.phase;
    }
    // 火花 kind → 冲击波环配色（与 drawHitSparks 同源）。
    const auto kindColor = [](int kind) {
      if (kind == 6) return glm::vec3{0.72f, 0.45f, 0.95f};
      if (kind == 5) return glm::vec3{0.45f, 0.85f, 1.0f};
      return glm::vec3{1.0f, 0.78f, 0.32f};
    };
    // 机制吟唱（审判光束等）：暗紫大火花环 + 朝主角齐射束流。
    const bool mechanicWinding =
        surface.boss3d.windingUp && !surface.boss3d.basicAttacking;
    if (mechanicWinding && !surface.bossPrevWindingUp) {
      spawnHitSparks(surface, bossPos, 6, 24, 1.8f, 1.4f, bossRatio * 1.2f);
      spawnAttackProjectiles(surface, bossPos, playerPos, 6, bossRatio, 7);
      spawnShockwave(surface, bossPos, kindColor(6), 0.16f * bossRatio);
    }
    surface.bossPrevWindingUp = mechanicWinding;

    // 普攻上升沿（蓄力起手）：按变体爆出对应色蓄力火花，
    // 0=金橙挥击、1=暗紫束流、2=青蓝冲击，配合预警环提示闪避窗口。
    if (surface.boss3d.basicAttacking && !surface.bossPrevBasicAttacking) {
      constexpr int kChargeKinds[3] = {0, 6, 5};
      const int kind = kChargeKinds[surface.boss3d.basicAttackVariant % 3];
      spawnHitSparks(surface, bossPos, kind, 14, 1.5f, 1.2f,
                     bossRatio * 1.1f);
    }
    // 普攻下降沿（挥击落地）：周身爆发 + 按变体规模朝主角齐射，
    // 形成“蓄力→挥击→弹幕飞行→命中火花”的完整释放链。
    if (!surface.boss3d.basicAttacking && surface.bossPrevBasicAttacking) {
      constexpr int kVolleyKinds[3] = {0, 6, 5};
      constexpr int kVolleyCounts[3] = {3, 5, 7};
      const int variant = surface.boss3d.basicAttackVariant % 3;
      spawnHitSparks(surface, bossPos, kVolleyKinds[variant], 10, 1.4f, 1.0f,
                     bossRatio);
      spawnAttackProjectiles(surface, bossPos, playerPos,
                             kVolleyKinds[variant], bossRatio,
                             kVolleyCounts[variant]);
      // 挥击落地冲击波：按变体配色，体量随首领缩放。
      spawnShockwave(surface, bossPos, kindColor(kVolleyKinds[variant]),
                     0.14f * bossRatio);
      // 挥击落地相机震动：闪避成功也能感到重击落地的冲击。
      result.bossSlamLanded = true;
    }
    surface.bossPrevBasicAttacking = surface.boss3d.basicAttacking;
  } else {
    surface.bossPrevWindingUp = false;
    surface.bossPrevBasicAttacking = false;
    surface.bossPrevPhase = 0;
    surface.bossPrevActive = false;
  }
  return result;
}

// 开放世界探索字段发布：体力、运动状态、分块统计、锚点交互与小地图。
void ApplyExplorationSnapshot(GameSnapshot& output, const Loop& loop) {
  output.explorationStamina = loop.motionState.stamina;
  output.motionState = static_cast<int32_t>(loop.motionState.state);
  output.playerHeight = loop.motionState.height;
  output.activeChunkCount =
      static_cast<int32_t>(loop.worldGrid.activeChunks().size());
  output.chunkLoadCount = loop.chunkLoadCount;
  output.interactionAnchorId = loop.currentAnchorInteraction.anchorId;
  output.interactionUnlocked = loop.currentAnchorInteraction.unlocked;
  output.interactionLabel = loop.currentAnchorInteraction.anchorId >= 0
                                ? loop.currentAnchorInteraction.label
                                : std::string{};
  output.unlockedAnchorCount = loop.anchors.unlockedCount();
  output.sprintActive = loop.motionState.sprinting ? 1 : 0;
  output.cameraExploration = loop.camera.exploration();
  output.teleportFlashMs = loop.teleportFlashMs;
  const ExplorationProgress exploration = loop.explorationContent.progress();
  output.explorationPoiCount = exploration.discoveredPoiCount;
  output.explorationPuzzleCount = exploration.activatedPuzzleCount;
  output.explorationRewardCount = exploration.claimedRewardCount;
  output.explorationGateCount = exploration.openGateCount;
  output.explorationTraversalMask = loop.explorationContent.traversalMask();
  output.explorationCurrentPoiId = -1;
  output.explorationCurrentTargetLabel.clear();
  output.explorationCurrentTargetDistrict.clear();
  output.explorationBlockedGateId = -1;
  output.explorationBlockedGateLabel.clear();
  output.explorationBlockedByPuzzleLabel.clear();
  const ExplorationFeedback& feedback = loop.explorationFeedback.snapshot();
  output.explorationFeedbackType = static_cast<int32_t>(feedback.type);
  output.explorationFeedbackId = feedback.id;
  output.explorationFeedbackTitle = feedback.title;
  output.explorationFeedbackSubtitle = feedback.subtitle;
  output.explorationFeedbackRemainingMs = feedback.remainingMs;
  const ExplorationTarget nearbyTarget = loop.explorationContent.nearestTarget(
      {loop.surface.player.x, loop.surface.player.y}, 0.045f);
  if (nearbyTarget.kind == ExplorationTargetKind::TraversalGate &&
      !loop.explorationContent.isGateOpen(nearbyTarget.id)) {
    output.explorationBlockedGateId = nearbyTarget.id;
    output.explorationBlockedGateLabel = nearbyTarget.label;
    for (const PuzzleNode& puzzle : loop.explorationContent.puzzles()) {
      if (puzzle.opensGateId == nearbyTarget.id) {
        output.explorationBlockedByPuzzleLabel = puzzle.label;
        break;
      }
    }
  }
  for (const PointOfInterest& poi : loop.explorationContent.pointsOfInterest()) {
    if (!loop.explorationContent.isPointDiscovered(poi.id) && poi.mainRoute) {
      output.explorationCurrentPoiId = poi.id;
      output.explorationCurrentTargetLabel = poi.label;
      output.explorationCurrentTargetDistrict = poi.districtId;
      break;
    }
  }
  output.minimapAnchorX.clear();
  output.minimapAnchorY.clear();
  output.minimapAnchorUnlocked.clear();
  for (const TeleportAnchor& anchor : loop.anchors.anchors()) {
    output.minimapAnchorX.push_back(anchor.x);
    output.minimapAnchorY.push_back(anchor.y);
    output.minimapAnchorUnlocked.push_back(
        loop.anchors.isUnlocked(anchor.id) ? 1 : 0);
  }
  // 小地图可交互物标记（优化）：未消耗的宝箱/采集物/秘境入口。
  output.minimapItemX.clear();
  output.minimapItemY.clear();
  output.minimapItemKind.clear();
  for (const Interactable& item : loop.interactables.items()) {
    if (item.kind == InteractableKind::Npc) continue;
    if (loop.interactables.isConsumed(item.id)) continue;
    output.minimapItemX.push_back(item.x);
    output.minimapItemY.push_back(item.y);
    output.minimapItemKind.push_back(item.kind == InteractableKind::Chest
                                         ? 3
                                         : item.kind == InteractableKind::Collectible
                                               ? 4
                                               : 5);
  }
}

// 内容与任务字段发布：任务进度、对话会话与交互提示种类。
void ApplyContentSnapshot(GameSnapshot& output, const Loop& loop) {
  const QuestProgressSnapshot quest = loop.quests.snapshot();
  output.questId = quest.questId;
  output.questStatus = static_cast<int32_t>(quest.status);
  output.questTitle = quest.title;
  output.questObjectiveLabel = quest.objectiveLabel;
  output.questObjectiveProgress = quest.objectiveProgress;
  output.questObjectiveRequired = quest.objectiveRequired;
  output.completedQuestCount = loop.quests.completedCount();
  output.completedSideQuestCount = loop.sideQuests.completedCount();
  output.dailyCompletedCount = loop.dailyQuests.completedCount();
  output.dailyQuestClaimed = loop.dailyRewarded ? 1 : 0;
  output.sideQuestProgress.clear();
  output.sideQuestRequired.clear();
  for (const SideQuestDef& quest : loop.sideQuests.quests()) {
    output.sideQuestProgress.push_back(loop.sideQuests.progressOf(quest.id));
    output.sideQuestRequired.push_back(quest.required);
  }
  output.dungeonState = static_cast<int32_t>(loop.dungeon.state());
  output.dungeonProgress = loop.dungeon.kills();
  output.dungeonRequired = loop.dungeon.def().killsRequired;
  output.dialogActive = loop.dialogSession.active();
  output.dialogSpeaker.clear();
  output.dialogText.clear();
  output.dialogLineIndex = 0;
  output.dialogLineCount = 0;
  if (output.dialogActive) {
    const DialogLine* line = loop.dialogSession.current();
    if (line != nullptr) {
      output.dialogSpeaker = line->speaker;
      output.dialogText = line->text;
    }
    output.dialogLineIndex = loop.dialogSession.index();
    output.dialogLineCount = loop.dialogSession.lineCount();
  } else {
    // 开场剧情字幕（演出导演）：无激活对话时占用字幕通道，自动推进。
    const StoryCue* cue = loop.storyDirector.current();
    if (cue != nullptr) {
      output.dialogActive = true;
      output.dialogSpeaker = cue->speaker;
      output.dialogText = cue->text;
    }
  }
  // 交互提示种类：按距离就近在锚点与可交互物间选择。
  output.interactionKind = 0;
  const bool hasAnchor = loop.currentAnchorInteraction.anchorId >= 0;
  const bool hasItem = loop.currentInteractable.id >= 0;
  if (hasAnchor &&
      (!hasItem || loop.currentAnchorInteraction.distance <=
                       loop.currentInteractable.distance)) {
    output.interactionKind = 1;
  } else if (hasItem) {
    switch (loop.currentInteractable.kind) {
      case InteractableKind::Npc:
        output.interactionKind = 2;
        break;
      case InteractableKind::Chest:
        output.interactionKind = 3;
        break;
      case InteractableKind::Collectible:
        output.interactionKind = 4;
        break;
      case InteractableKind::Dungeon:
        output.interactionKind = 5;
        break;
    }
    output.interactionLabel = loop.currentInteractable.label;
  }
}

// 养成与抽卡字段发布：背包货币、保底计数、抽卡结果与角色图鉴。
void ApplyGrowthSnapshot(GameSnapshot& output, const Loop& loop) {
  output.fateCount = loop.inventory.countOf(static_cast<int32_t>(ItemId::Fate));
  output.goldCount = loop.inventory.countOf(static_cast<int32_t>(ItemId::Gold));
  output.expMaterialCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::ExpMaterial));
  output.ascensionMaterialCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::AscensionMaterial));
  // 原神式养成新增物品计数。
  output.oreLowCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::OreLow));
  output.oreMidCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::OreMid));
  output.oreHighCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::OreHigh));
  output.expSmallCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::ExpSmall));
  output.expMediumCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::ExpMedium));
  output.expLargeCount =
      loop.inventory.countOf(static_cast<int32_t>(ItemId::ExpLarge));
  // 冒险等级与世界等级。
  output.adventureRank = loop.adventureRank.rank();
  output.adventureExp = loop.adventureRank.exp();
  output.adventureExpRequired =
      AdventureRank::expRequired(loop.adventureRank.rank());
  output.worldLevel = loop.adventureRank.worldLevel();
  output.gachaPity5 = loop.gachaState.since5;
  output.gachaResultIds.clear();
  output.gachaResultRarities.clear();
  output.gachaResultIsNew.clear();
  for (size_t i = 0; i < loop.lastGachaResults.size(); ++i) {
    output.gachaResultIds.push_back(loop.lastGachaResults[i].characterId);
    output.gachaResultRarities.push_back(loop.lastGachaResults[i].rarity);
    output.gachaResultIsNew.push_back(
        i < loop.lastGachaIsNew.size() && loop.lastGachaIsNew[i] ? 1 : 0);
  }
  output.rosterIds.clear();
  output.rosterLevels.clear();
  output.rosterAscensions.clear();
  for (const OwnedCharacter& character : loop.characters.owned()) {
    output.rosterIds.push_back(character.characterId);
    output.rosterLevels.push_back(character.level);
    output.rosterAscensions.push_back(character.ascension);
    // 派生属性（优化）：命之座加成 + 武器白值叠加到攻击，
  // 圣遗物固定/百分比加成与套装效果叠加（原神式）。
    output.rosterHp.push_back(CharacterGrowth::hpFor(
        character.characterId, character.level, character.ascension,
        character.constellation) +
        loop.artifacts.flatHpFor(character.characterId));
    const int32_t baseAtk =
        CharacterGrowth::atkFor(character.characterId, character.level,
                                character.ascension,
                                character.constellation) +
        loop.weapons.equippedBonusFor(character.characterId);
    const int32_t percentAtk =
        loop.artifacts.percentAtkFor(character.characterId);
    output.rosterAtk.push_back(baseAtk * (100 + percentAtk) / 100 +
                               loop.artifacts.flatAtkFor(character.characterId));
    output.rosterConstellations.push_back(character.constellation);
  }
  // 武器清单（养成深化）：id/等级/装备者 + 突破/精炼/精炼素材/经验。
  output.weaponIds.clear();
  output.weaponLevels.clear();
  output.weaponEquippedBy.clear();
  output.weaponAscensions.clear();
  output.weaponRefines.clear();
  output.weaponRefineStocks.clear();
  output.weaponExps.clear();
  for (const OwnedWeapon& weapon : loop.weapons.owned()) {
    output.weaponIds.push_back(weapon.weaponId);
    output.weaponLevels.push_back(weapon.level);
    output.weaponEquippedBy.push_back(weapon.equippedBy);
    output.weaponAscensions.push_back(weapon.ascension);
    output.weaponRefines.push_back(weapon.refine);
    output.weaponRefineStocks.push_back(weapon.refineStock);
    output.weaponExps.push_back(weapon.exp);
  }
  // 圣遗物清单（平行数组，按实例 id 升序）。
  output.artifactInstanceIds.clear();
  output.artifactDefIds.clear();
  output.artifactRarities.clear();
  output.artifactLevels.clear();
  output.artifactEquippedBy.clear();
  output.artifactSeeds.clear();
  for (const OwnedArtifact& artifact : loop.artifacts.owned()) {
    output.artifactInstanceIds.push_back(artifact.instanceId);
    output.artifactDefIds.push_back(artifact.defId);
    output.artifactRarities.push_back(artifact.rarity);
    output.artifactLevels.push_back(artifact.level);
    output.artifactEquippedBy.push_back(artifact.equippedBy);
    output.artifactSeeds.push_back(static_cast<int32_t>(artifact.substatSeed));
  }
  // 阶段四：出战角色、昼夜小时、画质预设、天气与 BGM 区域。
  output.activeCharacterId = loop.activeCharacterId;
  output.dayNightHour = loop.timeOfDaySeconds / 10.0f;
  output.qualityPreset = loop.qualityPreset;
  output.gachaPoolKind = loop.gachaPoolKind;
  output.weatherId = WeatherSystem::weatherAt(loop.timeOfDaySeconds).id;
  output.musicRegionId = loop.musicRegionId;
  // NPC 任务发布（Phase 4）：快照尾部纯追加 2 字段。
  output.npcOfferQuestId = loop.npcOfferQuestId;
  output.npcOfferQuestTitle = loop.npcOfferQuestTitle;
}
}  // namespace

void Loop::refreshExplorationGateCollision() {
  explorationGateCollision =
      ExplorationGateCollision::fromContent(explorationContent);
}

void Loop::publishExplorationFeedback(ExplorationFeedbackType type, int32_t id,
                                      const std::string& title,
                                      const std::string& subtitle,
                                      Tick durationMs) {
  explorationFeedback.publish(type, id, title, subtitle, durationMs);
}

BuildingContact Loop::resolvePlayerWorldCollision(float& x, float& y,
                                                  float radius, float height) {
  BuildingContact contact = buildingCollision.resolve(x, y, radius, height);
  const BuildingContact gateContact =
      explorationGateCollision.resolve(x, y, radius, height);
  contact.touching = contact.touching || gateContact.touching;
  contact.normal = gateContact.touching ? gateContact.normal : contact.normal;
  contact.highestTop = std::max(contact.highestTop, gateContact.highestTop);
  return contact;
}

void Loop::start() {
  withLifecycle([this]() {
    if (!surface.ready) {
      resetInput();
      encounter.reset();
      combat.reset();
      combatTimeMs_ = 0;
      {
        std::lock_guard<std::mutex> lock(combatEventMutex);
        frameCombatEvents_ = {};
      }
      LOGI("Loop start skipped: running=%{public}d ready=%{public}d", (int)running, (int)surface.ready);
      return;
    }
    if (!lifecycle.start([this]() {
      resetInput();
      paused.store(false);
      (void)encounter.start(EncounterMode::Training);
      audioBridge.start();
      combatTimeMs_ = 0;
      {
        std::lock_guard<std::mutex> lock(combatEventMutex);
        frameCombatEvents_ = {};
      }
      shouldStop = false;
      running = true;
      tickCount = 0;
      fps = 60.0f;
      lastFpsTime = std::chrono::steady_clock::now();
      runner = std::thread([this]() {
        auto lastTickTime = std::chrono::steady_clock::now();
        while (!shouldStop) {
          const auto now = std::chrono::steady_clock::now();
          const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickTime).count();
          lastTickTime = now;
          tickOnce(std::min<int64_t>(elapsedMs, 250));
          std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
        running = false;
      });
    })) {
      LOGI("Loop start skipped: already running");
    }
  });
}

void Loop::setPaused(bool value) {
  paused.store(value);
  if (value) {
    // 暂停时清空排队输入与摇杆状态，避免恢复后消费陈旧事件。
    resetInput();
  }
}

void Loop::stop() {
  withLifecycle([this]() {
    lifecycle.stop([this]() {
      shouldStop = true;
      if (runner.joinable()) runner.join();
      running = false;
    });
    resetInput();
    encounter.stop();
    audioBridge.stop();
    combat.reset();
    damageNumbers.clear();
    surface.damageNumbers3d.clear();
    combatTimeMs_ = 0;
    {
      std::lock_guard<std::mutex> lock(combatEventMutex);
      frameCombatEvents_ = {};
    }
    surface.trainingTarget.alive = true;
    GameSnapshot paused = snapshots.read();
    paused.moving = false;
    paused.targetId = 0;
    paused.moveX = 0.0f;
    paused.moveY = 0.0f;
    paused.targetDist = 0.0f;
    paused.rendererReady = surface.ready;
    ApplyCombatSnapshot(paused, combat.snapshot());
    snapshots.publish(paused);
  });
}

bool Loop::startEncounter(EncounterMode mode) {
  return withLifecycle([this, mode]() {
    currentTarget.reset();
    intent.actions.clear();
    damageNumbers.clear();
    surface.damageNumbers3d.clear();
    combatTimeMs_ = 0;
    {
      std::lock_guard<std::mutex> lock(combatEventMutex);
      frameCombatEvents_ = {};
    }
    const bool started = encounter.start(mode);
    surface.trainingTarget.alive =
        started && mode == EncounterMode::Training;
    // 新遭遇清空附着表现：光环掩码归零，updateFixed 会按需重建。
    surface.trainingTargetAuraMask = 0;
    return started;
  });
}

bool Loop::advanceLevel() {
  return withLifecycle([this]() { return encounter.advanceLevel(); });
}

bool Loop::useSupply() {
  return withLifecycle([this]() { return encounter.useSupply(); });
}

bool Loop::retryBoss() {
  return withLifecycle([this]() {
    currentTarget.reset();
    intent.actions.clear();
    return encounter.retryBoss();
  });
}

void Loop::toggleDebugHud() {
  debugHud_ = !debugHud_;
}

void Loop::advanceDialog() {
  withLifecycle([this]() {
    if (dialogSession.active()) {
      advanceDialogSession(*this);
    } else if (storyDirector.active()) {
      // 开场剧情字幕（无会话）：“继续”按钮手动推进演出。
      storyDirector.advance(loopTimeMs);
    }
  });
}

bool Loop::saveProgress(const std::string& path) {
  return withLifecycle([this, &path]() {
    SaveState state;
    state.completedQuestCount = quests.completedCount();
    state.activeQuestId = quests.activeQuestId();
    int32_t anchorMask = 0;
    for (const TeleportAnchor& anchor : anchors.anchors()) {
      if (anchor.id >= 1 && anchor.id <= 31 && anchors.isUnlocked(anchor.id)) {
        anchorMask |= (1 << (anchor.id - 1));
      }
    }
    state.unlockedAnchorMask = anchorMask;
    int32_t consumedMask = 0;
    for (const Interactable& item : interactables.items()) {
      if (item.kind == InteractableKind::Npc) continue;
      if (item.id >= 1 && item.id <= 31 && interactables.isConsumed(item.id)) {
        consumedMask |= (1 << (item.id - 1));
      }
    }
    state.consumedInteractableMask = consumedMask;
    // 养成字段。
    state.fateCount =
        inventory.countOf(static_cast<int32_t>(ItemId::Fate));
    state.goldCount =
        inventory.countOf(static_cast<int32_t>(ItemId::Gold));
    state.expCount =
        inventory.countOf(static_cast<int32_t>(ItemId::ExpMaterial));
    state.ascensionCount =
        inventory.countOf(static_cast<int32_t>(ItemId::AscensionMaterial));
    state.gachaPity5 = gachaState.since5;
    state.gachaPity4 = gachaState.since4;
    state.gachaSeed = gachaState.seed;
    state.sideQuestMask = sideQuests.completedMask();
    state.collectRespawnMs = collectRespawnRemainingMs;
    for (const OwnedCharacter& character : characters.owned()) {
      state.rosterTriples.push_back(character.characterId);
      state.rosterTriples.push_back(character.level);
      state.rosterTriples.push_back(character.ascension);
    }
    for (const OwnedWeapon& weapon : weapons.owned()) {
      state.weaponTriples.push_back(weapon.weaponId);
      state.weaponTriples.push_back(weapon.level);
      state.weaponTriples.push_back(weapon.equippedBy);
    }
    // V7 原神式养成字段。
    state.adventureRank = adventureRank.rank();
    state.adventureExp = adventureRank.exp();
    state.dropSeed = dropSeed;
    state.oreLowCount =
        inventory.countOf(static_cast<int32_t>(ItemId::OreLow));
    state.oreMidCount =
        inventory.countOf(static_cast<int32_t>(ItemId::OreMid));
    state.oreHighCount =
        inventory.countOf(static_cast<int32_t>(ItemId::OreHigh));
    state.expSmallCount =
        inventory.countOf(static_cast<int32_t>(ItemId::ExpSmall));
    state.expMediumCount =
        inventory.countOf(static_cast<int32_t>(ItemId::ExpMedium));
    state.expLargeCount =
        inventory.countOf(static_cast<int32_t>(ItemId::ExpLarge));
    for (const OwnedWeapon& weapon : weapons.owned()) {
      state.weaponRecords.push_back(weapon.weaponId);
      state.weaponRecords.push_back(weapon.level);
      state.weaponRecords.push_back(weapon.ascension);
      state.weaponRecords.push_back(weapon.refine);
      state.weaponRecords.push_back(weapon.refineStock);
      state.weaponRecords.push_back(weapon.exp);
      state.weaponRecords.push_back(weapon.equippedBy);
    }
    for (const OwnedArtifact& artifact : artifacts.owned()) {
      state.artifactRecords.push_back(artifact.instanceId);
      state.artifactRecords.push_back(artifact.defId);
      state.artifactRecords.push_back(artifact.rarity);
      state.artifactRecords.push_back(artifact.level);
      state.artifactRecords.push_back(artifact.equippedBy);
      state.artifactRecords.push_back(
          static_cast<int32_t>(artifact.substatSeed));
    }
    state.claimedRanks = claimedRankRewards;
    // V8：开放世界支线进度——并行支线用完成位掩码（bit i 对应声明顺序
    // 第 i 个任务）+ 当前接取任务 id，restoreLinear 的"前 N 个"语义不适用。
    int32_t openWorldMask = 0;
    const std::vector<QuestDef>& openQuests = openWorldQuests.quests();
    for (size_t i = 0; i < openQuests.size() && i < 31; ++i) {
      if (openWorldQuests.isCompleted(openQuests[i].id)) {
        openWorldMask |= (1 << static_cast<int32_t>(i));
      }
    }
    state.openWorldQuestMask = openWorldMask;
    state.openWorldQuestActiveId = openWorldQuests.activeQuestId();
    state.explorationPoiMask = explorationContent.discoveredPoiMask();
    state.explorationPuzzleMask = explorationContent.activatedPuzzleMask();
    state.explorationRewardMask = explorationContent.claimedRewardMask();
    state.explorationGateMask = explorationContent.openGateMask();
    state.explorationTraversalMask = explorationContent.traversalMask();
    Save save;
    return save.write(state, path.c_str());
  });
}

bool Loop::performGacha(int32_t count) {
  return withLifecycle([this, count]() {
    if (count != 1 && count != 10) return false;
    const int32_t fateId = static_cast<int32_t>(ItemId::Fate);
    if (!inventory.removeItem(fateId, count)) return false;
    lastGachaResults = gacha.draw(gachaState, count);
    gachaPoolKind = 0;
    lastGachaIsNew.clear();
    for (const GachaPull& pull : lastGachaResults) {
      const bool isNew = !characters.owns(pull.characterId);
      lastGachaIsNew.push_back(isNew);
      if (!characters.addCharacter(pull.characterId)) {
        // 重复角色转化：提升命之座（原神式命座）+ 返还契约与金币。
        characters.boostConstellation(pull.characterId);
        inventory.addItem(static_cast<int32_t>(ItemId::Fate),
                          pull.rarity == 5 ? 4 : 2);
        inventory.addItem(static_cast<int32_t>(ItemId::Gold),
                          pull.rarity == 5 ? 200 : 50);
      }
    }
    audioBridge.playUiSound(SoundEffect::Resonance);
    return true;
  });
}

bool Loop::performWeaponGacha(int32_t count) {
  return withLifecycle([this, count]() {
    if (count != 1 && count != 10) return false;
    const int32_t fateId = static_cast<int32_t>(ItemId::Fate);
    if (!inventory.removeItem(fateId, count)) return false;
    lastGachaResults = gacha.drawWeapon(gachaState, count);
    gachaPoolKind = 1;
    lastGachaIsNew.clear();
    for (const GachaPull& pull : lastGachaResults) {
      const bool isNew = !weapons.owns(pull.characterId);
      lastGachaIsNew.push_back(isNew);
      if (!weapons.addWeapon(pull.characterId)) {
        // 重复武器：累计精炼素材 + 折算金币。
        weapons.addRefineStock(pull.characterId);
        inventory.addItem(static_cast<int32_t>(ItemId::Gold),
                          pull.rarity == 5 ? 300 : 100);
      }
    }
    audioBridge.playUiSound(SoundEffect::Resonance);
    return true;
  });
}

bool Loop::useExpMaterial(int32_t characterId, int32_t materialCount) {
  return withLifecycle([this, characterId, materialCount]() {
    if (materialCount <= 0 || !characters.owns(characterId)) return false;
    const int32_t expId = static_cast<int32_t>(ItemId::ExpMaterial);
    if (!inventory.removeItem(expId, materialCount)) return false;
    // 每份经验材料折算 10 点经验。
    const int32_t levels = characters.addExp(characterId, materialCount * 10);
    if (levels > 0) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return true;
  });
}

bool Loop::ascendCharacter(int32_t characterId) {
  return withLifecycle([this, characterId]() {
    const OwnedCharacter* character = characters.find(characterId);
    if (character == nullptr ||
        character->level <
            CharacterGrowth::levelCap(character->ascension) ||
        character->ascension >= CharacterGrowth::kMaxAscension) {
      return false;
    }
    // 突破成本：2 份源晶碎片 + 100 金币。
    if (!inventory.removeItem(
            static_cast<int32_t>(ItemId::AscensionMaterial), 2) ||
        !inventory.removeItem(static_cast<int32_t>(ItemId::Gold), 100)) {
      return false;
    }
    const bool ascended = characters.ascend(characterId);
    if (ascended) {
      audioBridge.playUiSound(SoundEffect::Resonance);
    }
    return ascended;
  });
}

bool Loop::upgradeWeapon(int32_t weaponId) {
  return withLifecycle([this, weaponId]() {
    const OwnedWeapon* weapon = weapons.find(weaponId);
    if (weapon == nullptr || weapon->level >= WeaponSystem::kMaxLevel) {
      return false;
    }
    // 强化成本：随等级递增的金币。
    const int32_t cost = WeaponSystem::upgradeCost(weapon->level);
    if (!inventory.removeItem(static_cast<int32_t>(ItemId::Gold), cost)) {
      return false;
    }
    const bool upgraded = weapons.upgrade(weaponId);
    if (upgraded) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return upgraded;
  });
}

bool Loop::equipWeapon(int32_t weaponId, int32_t characterId) {
  return withLifecycle([this, weaponId, characterId]() {
    if (!characters.owns(characterId)) return false;
    const bool equipped = weapons.equip(weaponId, characterId);
    if (equipped) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return equipped;
  });
}

bool Loop::upgradeWeaponWithOre(int32_t weaponId, int32_t oreItemId,
                                int32_t oreCount) {
  return withLifecycle([this, weaponId, oreItemId, oreCount]() {
    if (oreCount <= 0) return false;
    const OwnedWeapon* weapon = weapons.find(weaponId);
    if (weapon == nullptr ||
        weapon->level >= WeaponSystem::levelCap(weapon->ascension)) {
      return false;
    }
    const int32_t expPerOre = Inventory::weaponExpValue(oreItemId);
    if (expPerOre <= 0 ||
        !inventory.removeItem(oreItemId, oreCount)) {
      return false;
    }
    // 强化金币费用：经验价值的十分之一（原神式摩拉折算）。
    const int32_t goldCost =
        WeaponSystem::expRequired(weapon->level) > 0
            ? (expPerOre * oreCount) / 10
            : 0;
    if (goldCost > 0 &&
        !inventory.removeItem(static_cast<int32_t>(ItemId::Gold), goldCost)) {
      return false;
    }
    const int32_t levels = weapons.addWeaponExp(weaponId, expPerOre * oreCount);
    if (levels > 0) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return true;
  });
}

bool Loop::ascendWeapon(int32_t weaponId) {
  return withLifecycle([this, weaponId]() {
    const OwnedWeapon* weapon = weapons.find(weaponId);
    if (weapon == nullptr ||
        weapon->level < WeaponSystem::levelCap(weapon->ascension) ||
        weapon->ascension >= WeaponSystem::kMaxAscension) {
      return false;
    }
    // 突破成本：随阶段递增的源晶碎片与金币。
    const int32_t materials =
        WeaponSystem::ascensionMaterialCost(weapon->ascension);
    const int32_t gold = WeaponSystem::ascensionGoldCost(weapon->ascension);
    if (!inventory.removeItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                              materials) ||
        !inventory.removeItem(static_cast<int32_t>(ItemId::Gold), gold)) {
      return false;
    }
    const bool ascended = weapons.ascend(weaponId);
    if (ascended) {
      audioBridge.playUiSound(SoundEffect::Resonance);
    }
    return ascended;
  });
}

bool Loop::refineWeapon(int32_t weaponId) {
  return withLifecycle([this, weaponId]() {
    const bool refined = weapons.refine(weaponId);
    if (refined) {
      audioBridge.playUiSound(SoundEffect::Resonance);
    }
    return refined;
  });
}

bool Loop::useExpItem(int32_t characterId, int32_t itemId, int32_t count) {
  return withLifecycle([this, characterId, itemId, count]() {
    if (count <= 0 || !characters.owns(characterId)) return false;
    const int32_t expPerItem = Inventory::characterExpValue(itemId);
    if (expPerItem <= 0 || !inventory.removeItem(itemId, count)) {
      return false;
    }
    const int32_t levels = characters.addExp(characterId, expPerItem * count);
    if (levels > 0) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return true;
  });
}

bool Loop::upgradeArtifact(int32_t targetInstanceId,
                           const std::vector<int32_t>& feedInstanceIds) {
  return withLifecycle([this, targetInstanceId, &feedInstanceIds]() {
    if (artifacts.find(targetInstanceId) == nullptr ||
        feedInstanceIds.empty()) {
      return false;
    }
    // 预计算经验与金币费用：跳过非法素材。
    int32_t expGain = 0;
    for (const int32_t feedId : feedInstanceIds) {
      if (feedId == targetInstanceId) continue;
      const OwnedArtifact* feed = artifacts.find(feedId);
      if (feed == nullptr || feed->equippedBy != 0) continue;
      expGain += ArtifactSystem::feedExpValue(feed->rarity, feed->level);
    }
    if (expGain <= 0) return false;
    const int32_t goldCost = ArtifactSystem::upgradeGoldCost(expGain);
    if (goldCost > 0 &&
        !inventory.removeItem(static_cast<int32_t>(ItemId::Gold), goldCost)) {
      return false;
    }
    const int32_t gained = artifacts.feedUpgrade(targetInstanceId,
                                                 feedInstanceIds);
    if (gained > 0) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return gained > 0;
  });
}

bool Loop::equipArtifact(int32_t instanceId, int32_t characterId) {
  return withLifecycle([this, instanceId, characterId]() {
    if (!characters.owns(characterId)) return false;
    const bool equipped = artifacts.equip(instanceId, characterId);
    if (equipped) {
      audioBridge.playUiSound(SoundEffect::AuraApplied);
    }
    return equipped;
  });
}

bool Loop::claimRankReward(int32_t rank) {
  return withLifecycle([this, rank]() {
    const RankReward* reward = AdventureRank::rankReward(rank);
    if (reward == nullptr || adventureRank.rank() < rank) return false;
    for (const int32_t claimed : claimedRankRewards) {
      if (claimed == rank) return false;
    }
    if (reward->gold > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::Gold), reward->gold);
    }
    if (reward->fate > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::Fate), reward->fate);
    }
    if (reward->expMaterial > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::ExpSmall),
                        reward->expMaterial);
    }
    if (reward->ascensionMaterial > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                        reward->ascensionMaterial);
    }
    claimedRankRewards.push_back(rank);
    audioBridge.playUiSound(SoundEffect::Resonance);
    return true;
  });
}

bool Loop::switchCharacter() {
  return withLifecycle([this]() {
    const std::vector<OwnedCharacter>& owned = characters.owned();
    if (owned.size() < 2) return false;
    // 找到当前出战角色的下一位，循环切换。
    size_t currentIndex = owned.size();
    for (size_t i = 0; i < owned.size(); ++i) {
      if (owned[i].characterId == activeCharacterId) {
        currentIndex = i;
        break;
      }
    }
    const size_t nextIndex =
        currentIndex >= owned.size() ? 0 : (currentIndex + 1) % owned.size();
    activeCharacterId = owned[nextIndex].characterId;
    // 附魔重置：新角色出场时武器附魔切到该角色自身源质（原神切人
    // 语言：附魔跟随角色而非全局状态）；物理角色无附魔，刀光/
    // 拖尾/附魔光环随之回到中性。
    surface.playerSlashSource = CharacterSourceFor(activeCharacterId);
    // 出场动效（原神切人仪式）：按角色所属源质释放对应色火花 +
    // 冲击波 + 光柱 + 符阵（1辉印 2脉流 3蚀质，其余角色通用金橙），
    // 配合轻微 FOV 冲击与卡肉，把"换人"从静默切换拎成一次出场。
    const CharacterSwitchVfx switchVfx =
        CharacterSwitchVfxFor(activeCharacterId);
    const Vec2 playerPos{surface.player.x, surface.player.y};
    const float playerRatio =
        VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player);
    spawnHitSparks(surface, playerPos, switchVfx.sparkKind, 16, 1.5f, 1.3f,
                   playerRatio);
    spawnShockwave(surface, playerPos, switchVfx.color, 0.10f * playerRatio);
    spawnLightPillar(surface, playerPos, switchVfx.color, 0.10f * playerRatio);
    spawnSkillRune(surface, playerPos, switchVfx.color, 0.07f * playerRatio);
    surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(1);
    surface.resonanceFovSeconds = 0.0f;
    hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 48, 96);
    audioBridge.playUiSound(SoundEffect::AuraApplied);
    return true;
  });
}

bool Loop::teleportToAnchor(int32_t anchorId) {
  return withLifecycle([this, anchorId]() {
    if (!anchors.isUnlocked(anchorId)) return false;
    for (const TeleportAnchor& anchor : anchors.anchors()) {
      if (anchor.id != anchorId) continue;
      surface.player.x = anchor.x;
      surface.player.y = anchor.y;
      motionState = explorationMotion.reset(
          terrain.heightAt(anchor.x, anchor.y));
      // 传送：强制刷新流式集合同步准备目标分块一圈，配合黑屏转场。
      worldGrid.updateStreaming({anchor.x, anchor.y});
      chunkLoadCount += static_cast<int32_t>(worldGrid.pendingLoads().size());
      streamScheduler.loadRingSync(
          worldGrid.chunkIndexAt({anchor.x, anchor.y}),
          worldGrid.config().streamingRadius, performanceGuard.lodLevel());
      // 传送圈分块在 Ready 队列等待上传：黑屏窗口内临时放宽每帧
      // 配额（25 块 / 4 块每帧 ≈ 7 帧 ≪ 1200ms 转场）。
      streamScheduler.beginBurst(10, 4);
      teleportFlashMs = 1200;
      audioBridge.playUiSound(SoundEffect::Resonance);
      return true;
    }
    return false;
  });
}

bool Loop::loadProgress(const std::string& path) {
  return withLifecycle([this, &path]() {
    SaveState state;
    Save save;
    if (!save.read(state, path.c_str())) {
      return false;
    }
    quests.restoreLinear(state.completedQuestCount, state.activeQuestId);
    // V8：开放世界支线按完成掩码恢复（V1-V7 旧存档字段为默认值，
    // restoreByMask(0,-1) 等价于初始全部可接取态）。
    openWorldQuests.restoreByMask(state.openWorldQuestMask,
                                  state.openWorldQuestActiveId);
    explorationContent.restoreMasks(
        state.explorationPoiMask, state.explorationPuzzleMask,
        state.explorationRewardMask, state.explorationGateMask,
        static_cast<uint8_t>(state.explorationTraversalMask));
    sideQuests.restoreMask(state.sideQuestMask);
    lastRewardedSideCount = sideQuests.completedCount();
    collectRespawnRemainingMs = state.collectRespawnMs;
    anchors.restoreUnlocked(state.unlockedAnchorMask);
    interactables.restoreConsumed(state.consumedInteractableMask);
    lastRewardedQuestCount = quests.completedCount();
    // v3 养成字段：背包、抽卡状态与角色图鉴。
    inventory = Inventory();
    if (state.fateCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::Fate), state.fateCount);
    }
    if (state.goldCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::Gold), state.goldCount);
    }
    if (state.expCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::ExpMaterial),
                        state.expCount);
    }
    if (state.ascensionCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                        state.ascensionCount);
    }
    // V7 新增物品计数。
    if (state.oreLowCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::OreLow),
                        state.oreLowCount);
    }
    if (state.oreMidCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::OreMid),
                        state.oreMidCount);
    }
    if (state.oreHighCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::OreHigh),
                        state.oreHighCount);
    }
    if (state.expSmallCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::ExpSmall),
                        state.expSmallCount);
    }
    if (state.expMediumCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::ExpMedium),
                        state.expMediumCount);
    }
    if (state.expLargeCount > 0) {
      inventory.addItem(static_cast<int32_t>(ItemId::ExpLarge),
                        state.expLargeCount);
    }
    // V7 冒险等级与掉落种子。
    adventureRank.restore(state.adventureRank, state.adventureExp);
    if (state.dropSeed != 0) {
      dropSeed = state.dropSeed;
    }
    claimedRankRewards = state.claimedRanks;
    if (state.gachaSeed != 0 || state.gachaPity5 != 0 ||
        state.gachaPity4 != 0) {
      gachaState.seed = state.gachaSeed;
      gachaState.since5 = state.gachaPity5;
      gachaState.since4 = state.gachaPity4;
    }
    if (!state.rosterTriples.empty() && state.rosterTriples.size() % 3 == 0) {
      characters = CharacterGrowth();
      for (size_t i = 0; i + 2 < state.rosterTriples.size(); i += 3) {
        characters.restoreCharacter(state.rosterTriples[i],
                                    state.rosterTriples[i + 1],
                                    state.rosterTriples[i + 2]);
      }
    }
    // v6 武器字段：武器三元组（id/等级/装备者）；
    // V7 七元组优先（含突破/精炼/经验）。
    if (!state.weaponRecords.empty() &&
        state.weaponRecords.size() % 7 == 0) {
      weapons = WeaponSystem();
      for (size_t i = 0; i + 6 < state.weaponRecords.size(); i += 7) {
        weapons.restoreWeapon(state.weaponRecords[i],
                              state.weaponRecords[i + 1],
                              state.weaponRecords[i + 2],
                              state.weaponRecords[i + 3],
                              state.weaponRecords[i + 4],
                              state.weaponRecords[i + 5],
                              state.weaponRecords[i + 6]);
      }
    } else if (!state.weaponTriples.empty() &&
               state.weaponTriples.size() % 3 == 0) {
      weapons = WeaponSystem();
      for (size_t i = 0; i + 2 < state.weaponTriples.size(); i += 3) {
        weapons.restoreWeapon(state.weaponTriples[i],
                              state.weaponTriples[i + 1],
                              state.weaponTriples[i + 2]);
      }
    }
    // V7 圣遗物六元组 [instanceId, defId, rarity, level, equippedBy, seed]。
    artifacts = ArtifactSystem();
    if (!state.artifactRecords.empty() &&
        state.artifactRecords.size() % 6 == 0) {
      for (size_t i = 0; i + 5 < state.artifactRecords.size(); i += 6) {
        artifacts.restoreArtifact(
            state.artifactRecords[i], state.artifactRecords[i + 1],
            state.artifactRecords[i + 2], state.artifactRecords[i + 3],
            state.artifactRecords[i + 4],
            static_cast<uint32_t>(state.artifactRecords[i + 5]));
      }
    }
    return true;
  });
}
void Loop::processInput() {
  InputEvent e;
  while (input.pop(e)) {
    CombatAction combatAction;
    if (TryMapCombatAction(e.action, combatAction)) {
      intent.actions.push_back({combatAction, e.sequence});
      continue;
    }
    // 探索动作：不进入战斗管线，直接驱动探索状态。
    if (e.action == InputAction::Jump) {
      jumpQueued = true;
      continue;
    }
    if (e.action == InputAction::Interact) {
      interactQueued = true;
      continue;
    }
    if (e.action == InputAction::GlidePress) {
      glideHeld = true;
      continue;
    }
    if (e.action == InputAction::GlideRelease) {
      glideHeld = false;
      continue;
    }
    if (e.action == InputAction::SwitchCharacter) {
      switchCharacter();
      continue;
    }
    const TouchRole releaseRole = touchRouter.role(e.pointerId);
    switch (e.action) {
      case InputAction::PointerDown: {
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        const TouchRole role = touchRouter.role(e.pointerId);
        if (role == TouchRole::Movement) {
          joystick.begin(e.pointerId, {e.x, e.y});
        } else if (role == TouchRole::Camera) {
          cameraGesture.begin(e.pointerId, {e.x, e.y});
        }
        break;
      }
      case InputAction::PointerMove:
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        if (releaseRole == TouchRole::Movement) {
          joystick.move(e.pointerId, {e.x, e.y});
        } else if (releaseRole == TouchRole::Camera) {
          cameraGesture.move(e.pointerId, {e.x, e.y});
        }
        break;
      case InputAction::PointerUp:
      case InputAction::PointerCancel:
        if (!touchRouter.handle(e, static_cast<float>(surface.width),
                                static_cast<float>(surface.height))) {
          break;
        }
        if (releaseRole == TouchRole::Movement) {
          joystick.end(e.pointerId);
        } else if (releaseRole == TouchRole::Camera) {
          cameraGesture.end(e.pointerId);
        }
        break;
      default:
        break;
    }
  }
  intent.move = joystick.value();
  intent.lookDelta = intent.lookDelta + cameraGesture.consumeDelta();
}

void Loop::resetInput() {
  touchRouter.clear();
  joystick.clear();
  cameraGesture.clear();
  intent.move = {};
  intent.lookDelta = {};
  intent.actions.clear();
  jumpQueued = false;
  interactQueued = false;
  glideHeld = false;
  surface.player.moving = false;
  surface.playerHitAnimationSeconds = 0.0f;
  surface.playerAirSeconds = 0.0f;
  surface.playerLandSeconds = 0.0f;
  surface.enemyHitFlash.clear();
  surface.enemyPrevWindingUp.clear();
  surface.enemyPrevAttacking.clear();
  surface.bossPrevWindingUp = false;
  surface.bossPrevBasicAttacking = false;
  surface.bossPrevPhase = 0;
  surface.bossPrevActive = false;
  surface.bossPrevDefeated = false;
  prevFinalForgeCasting = false;
  surface.ultimateDimSeconds = 0.0f;
  surface.infusionEmitSeconds = 0.0f;
  surface.playerGhostHistory.clear();
  surface.playerGhostFadeSeconds = 0.0f;
  surface.hitSparks3d.clear();
  surface.playerSlashSeconds = -1.0f;
  surface.playerSlashCombo = 0;
  surface.playerSlashYaw = 0.0f;
  surface.playerSlashSource = -1;
  surface.enemySlashArcs.clear();
  surface.shockwaveRings.clear();
  surface.impactDecals.clear();
  surface.lightPillars.clear();
  surface.resonanceFovSeconds = -1.0f;
  surface.fovPunchMaxOffset = -7.0f;
  surface.skillRunes.clear();
  hitStopRemainingMs = 0;
  surface.player3dAnimation.action = RenderAnimation::Idle;
  surface.player3dAnimation.hit = false;
  surface.player3dAnimation.moving = false;
  particleEmitTimer = 0.0f;
  trailEmitTimer = 0.0f;
  prevComboSegment = 0;
  prevRadianceCdMs = 0;
  prevCurrentCdMs = 0;
  prevCorruptionCdMs = 0;
  prevComboSegmentForVfx = 0;
  prevActionForVfx = 0;
  prevAuraMasks.clear();
  currentTarget.reset();
  input.clear();
}

void Loop::tickOnce(int64_t elapsedMs) {
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    frameCombatEvents_ = {};
  }
  if (!surface.ready) {
    resetInput();
    encounter.stop();
    combat.reset();
    combatTimeMs_ = 0;
    surface.trainingTarget.alive = true;
    surface.trainingTargetAuraMask = 0;
    publishRendererStopped();
    return;
  }
  if (paused.load()) {
    // 暂停：冻结输入与固定步逻辑，画面与快照保持最后一帧。
    return;
  }
  if (encounter.snapshot().state == EncounterState::Stopped) {
    (void)encounter.start(EncounterMode::Training);
  }
  processInput();
  if (hitStopRemainingMs > 0) {
    // 命中卡肉：吞掉本帧时间不进入固定步累加器，逻辑冻结而渲染继续，
    // 命中瞬间的顿帧强化打击感；结束后从干净累加器恢复。
    hitStopRemainingMs -= elapsedMs > 0 ? elapsedMs : 0;
  } else {
    fixedStep.advance(elapsedMs, [this](Tick tick, int64_t dtMs) {
      updateFixed(tick, dtMs);
    });
  }
#ifdef OHOS_PLATFORM
  update3DCamera(surface, camera, motionState.height);
  // 主角脚底高度与渲染时钟：模型随地形/跳跃同步，水面涟漪随时间流动。
  surface.playerGroundHeight = motionState.height;
  surface.renderSeconds +=
      static_cast<float>(elapsedMs > 0 ? elapsedMs : 0) / 1000.0f;
  if (surface.renderSeconds > 3600.0f) surface.renderSeconds -= 3600.0f;
  surface.environmentPerfLevel = performanceGuard.level();
  // bloom 后处理仅高画质预设启用（低画质设备跳过整条管线）。
  surface.bloomEnabled = qualityPreset == 0;
  surface_draw(surface);
  surface_swap(surface);
#endif

  tickCount++;
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime).count();
  if (elapsed >= 1000) {
    fps = tickCount * 1000.0f / (float)elapsed;
    tickCount = 0;
    lastFpsTime = now;
    // 分块流式与野外敌人计数（性能仪表扩展）：打点处仅只读现有状态，
    // wild_enemies 当前恒为 0，由后续 WildSpawnSystem 填充。
    const int activeChunkCount =
        static_cast<int>(worldGrid.activeChunks().size());
    const int streamingPendingCount =
        static_cast<int>(worldGrid.pendingLoads().size() +
                         worldGrid.pendingUnloads().size());
    LOGI("PROFILE fps=%{public}.1f perf_level=%{public}d environment_ready=%{public}d "
         "environment_draw_calls=%{public}u environment_triangles=%{public}u "
         "environment_texture_tier=%{public}s encounter_mode=%{public}d "
         "active_chunks=%{public}d streaming_pending=%{public}d "
         "wild_enemies=%{public}d",
         fps, performanceGuard.level(), static_cast<int>(surface.environmentReady),
         surface.environmentDrawCalls, surface.environmentTriangles,
         performanceGuard.level() >= 4 ? "half" : "full",
         static_cast<int>(encounter.snapshot().mode),
         activeChunkCount, streamingPendingCount, wildEnemyCount);
  }
  performanceGuard.sample(fixedStep.tick(), 16, fps);

  // 结算音效：遭遇状态进入胜利/失败的转移沿播放确认音。
  const int encounterStateNow = static_cast<int>(encounter.snapshot().state);
  if (encounterStateNow != lastEncounterStateForAudio) {
    if (encounterStateNow == static_cast<int>(EncounterState::Victory)) {
      audioBridge.playUiSound(SoundEffect::Resonance);
    } else if (encounterStateNow == static_cast<int>(EncounterState::Defeat)) {
      audioBridge.playUiSound(SoundEffect::PhaseChanged);
    }
    lastEncounterStateForAudio = encounterStateNow;
  }

  // 连击升阶音效：连击段数上升且达到 2 段及以上时播放升阶确认音。
  const int comboNow = static_cast<int>(combat.snapshot().comboSegment);
  if (comboNow > prevComboSegment && comboNow >= 2) {
    audioBridge.playUiSound(SoundEffect::AuraApplied);
  }
  prevComboSegment = comboNow;

  GameSnapshot snapshot;
  snapshot.tick = fixedStep.tick();
  snapshot.playerX = surface.player.x;
  snapshot.playerY = surface.player.y;
  snapshot.fps = fps;
  snapshot.moving = surface.player.moving;
  snapshot.targetId = currentTarget ? currentTarget->id : 0;
  snapshot.rendererReady = surface.ready;
  snapshot.environmentReady = surface.environmentReady;
  snapshot.environmentDrawCalls = surface.environmentDrawCalls;
  snapshot.environmentTriangles = surface.environmentTriangles;
  snapshot.encounterMode = static_cast<int32_t>(encounter.snapshot().mode);
  snapshot.encounterState = static_cast<int32_t>(encounter.snapshot().state);
  snapshot.moveX = intent.move.x;
  snapshot.moveY = intent.move.y;
  snapshot.cameraYaw = camera.yaw();
  snapshot.cameraPitch = camera.pitch();
  snapshot.targetDist = currentTarget ? currentTarget->distance : 0.0f;
  // 锁定目标焦点框：解析锁定敌人的原型与血量比例（首领已有专属血条，
  // 不在敌人列表中，保持 archetype = -1 由 HUD 隐藏焦点框）。
  snapshot.targetArchetype = -1;
  snapshot.targetHpRatio = 0.0f;
  if (currentTarget.has_value()) {
    for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
      if (enemy.id == static_cast<EntityId>(currentTarget->id) &&
          enemy.maxHp > 0) {
        snapshot.targetArchetype = static_cast<int32_t>(enemy.archetype);
        snapshot.targetHpRatio = static_cast<float>(enemy.hp) /
                                 static_cast<float>(enemy.maxHp);
        break;
      }
    }
  }
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  ApplyCombatSnapshot(snapshot, combatSnapshot);
  snapshot.levelStage = static_cast<int32_t>(encounter.snapshot().levelStage);
  snapshot.gateState = static_cast<int32_t>(encounter.snapshot().gateState);
  snapshot.supplyState = static_cast<int32_t>(encounter.snapshot().supplyState);
  snapshot.bossHp = encounter.snapshot().boss.hp;
  snapshot.bossPoise = encounter.snapshot().boss.poise;
  snapshot.bossPhase = static_cast<int32_t>(encounter.snapshot().boss.phase);
  snapshot.bossMechanic = static_cast<int32_t>(encounter.snapshot().boss.mechanic);
  snapshot.bossCastMs = encounter.snapshot().boss.castRemainingMs;
  // 首领吟唱警示音：机制开始（上升沿）时播放低沉警示，
  // 与预警环/吟唱条同步给玩家应对提示。
  if (snapshot.bossMechanic != lastBossMechanicForAudio &&
      snapshot.bossMechanic != 0) {
    audioBridge.playUiSound(SoundEffect::PhaseChanged);
  }
  lastBossMechanicForAudio = snapshot.bossMechanic;
  snapshot.perfLevel = performanceGuard.level();
  snapshot.vfxFlags = vfxSystem.snapshot().vfxFlags;
  snapshot.cameraShakeX = vfxSystem.snapshot().cameraShakeX;
  snapshot.cameraShakeY = vfxSystem.snapshot().cameraShakeY;
  snapshot.bossHpRatio = BossCinematicState::healthRatio(
      encounter.snapshot().boss.hp, BossConfig::karounDefaults().maxHp);
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    snapshot.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  snapshot.debugHud = debugHud_;
  switch (encounter.snapshot().mode) {
    case EncounterMode::Training:
      snapshot.objectiveLabel = "唤醒第一道共鸣";
      break;
    case EncounterMode::Boss:
      if (encounter.snapshot().boss.phase == BossPhase::CurrentStorm &&
          !encounter.snapshot().boss.defeated) {
        snapshot.objectiveLabel =
            std::string("击碎电流节点 ") +
            std::to_string(encounter.snapshot().boss.nodesBroken) + "/" +
            std::to_string(BossConfig::karounDefaults().currentNodeCount);
      } else {
        snapshot.objectiveLabel = "击破共鸣核心";
      }
      break;
    default:
      snapshot.objectiveLabel = "前往共鸣祭坛";
      break;
  }
  snapshot.resonanceSlots = {
      static_cast<uint8_t>(snapshot.radianceAttached),
      static_cast<uint8_t>(snapshot.currentAttached),
      static_cast<uint8_t>(snapshot.corruptionAttached)};
  snapshot.showDebugHud = debugHud_;
  snapshot.inputEventCount = inputEventCount_;
  ApplyExplorationSnapshot(snapshot, *this);
  ApplyContentSnapshot(snapshot, *this);
  ApplyGrowthSnapshot(snapshot, *this);
  // 任务目标优先于演示目标文案。
  if (snapshot.questStatus ==
          static_cast<int32_t>(QuestStatus::Active) &&
      !snapshot.questObjectiveLabel.empty()) {
    snapshot.objectiveLabel = snapshot.questObjectiveLabel;
  }
  snapshots.publish(snapshot);

  if (tickCount <= 5 || tickCount % 60 == 0) {
    LOGI("tickOnce: %{public}d fps=%{public}.1f", tickCount, fps);
  }
}

void Loop::updateFixed(Tick tick, int64_t dtMs) {
  const Vec2 lookDelta = intent.lookDelta;
  intent.lookDelta = {};
  const float dtSeconds = static_cast<float>(dtMs) / 1000.0f;

  // 相机先更新：玩家移动使用本帧最新 yaw，保证操控方向与画面严格一致。
  camera.update({surface.player.x, surface.player.y}, lookDelta, dtSeconds);
  surface.cameraRenderState = camera.renderState();

  playerController.update(surface.player, intent.move, camera.yaw(),
                          dtSeconds,
                          motionState.sprinting
                              ? explorationMotion.config().sprintSpeedMultiplier
                              : 1.0f);

  // 建筑碰撞：把主角从城墙/塔楼盒内推出并沿墙滑动，不再穿模；
  // 接触信息驱动墙面攀爬判定（高度越过盒顶时不再阻挡，可翻上墙头）。
  const BuildingContact wallContact = resolvePlayerWorldCollision(
      surface.player.x, surface.player.y, playerCollisionRadius,
      motionState.height);

  // ---- 开放世界探索（阶段一）----
  // 性能降级联动视距（Phase 5 接入 viewDistanceScale）：档位越低
  // 视距越短，流式半径越小，减少激活分块与刷怪压力。
  worldGrid.setStreamingRadius(
      streamingRadiusForPerf(qualityPreset, performanceGuard));
  streamScheduler.setKeepRadius(worldGrid.config().streamingRadius + 1);
  // 分块流式：按玩家位置维护激活分块集合，累计加载次数供验收。
  if (worldGrid.updateStreaming({surface.player.x, surface.player.y})) {
    chunkLoadCount += static_cast<int32_t>(worldGrid.pendingLoads().size());
    // 转发流式调度器：驱动分块地形内容加载/卸载（滞后带由调度器评估）。
    streamScheduler.requestLoads(
        worldGrid.pendingLoads(),
        worldGrid.chunkIndexAt({surface.player.x, surface.player.y}),
        performanceGuard.lodLevel());
    streamScheduler.requestUnloads(worldGrid.pendingUnloads());
  }
  // 垂直运动：跳跃/滑翔/攀爬/游泳与探索体力。
  {
    MotionInput motionInput;
    motionInput.jumpPressed = jumpQueued;
    motionInput.glideHeld = glideHeld;
    motionInput.moving = surface.player.moving;
    // 墙面攀爬：贴墙朝墙推进且尚未登顶时，与地形陡坡同等进入攀爬，
    // 消耗体力并按固定速度上升；登顶后由地面覆盖接管站上墙头。
    motionInput.wallClimbing =
        wallContact.touching && surface.player.moving &&
        (motionState.state == MotionState::Grounded ||
         motionState.state == MotionState::Climbing) &&
        motionState.height < wallContact.highestTop - 0.002f &&
        motionState.stamina > 0.0f;
    // 合成支撑高度：已翻上建筑盒顶时，地面取盒顶而非地形采样，
    // 保证墙头站立/落地贴合；贴墙站立时不会误判（standingTopAt
    // 仅在盒顶不高于当前高度时计入）。
    const float terrainGround =
        terrain.heightAt(surface.player.x, surface.player.y);
    const float standingTop = buildingCollision.standingTopAt(
        surface.player.x, surface.player.y, playerCollisionRadius * 0.5f,
        motionState.height, 0.006f);
    MotionGroundOverride groundOverride;
    if (std::isfinite(standingTop) && standingTop > terrainGround) {
      groundOverride.active = true;
      groundOverride.groundHeight = standingTop;
    }
    motionState = explorationMotion.update(
        motionState, motionInput, terrain, surface.player.x,
        surface.player.y, dtSeconds,
        groundOverride.active ? &groundOverride : nullptr);
    const auto recordTraversal = [&](TraversalAbility ability) {
      if (explorationContent.traversalUsed(ability)) return;
      explorationContent.recordTraversal(ability);
      quests.notifyTraversalUsed(static_cast<int32_t>(ability));
    };
    if (motionState.state == MotionState::Airborne) {
      recordTraversal(TraversalAbility::Jump);
    }
    if (motionState.sprinting) recordTraversal(TraversalAbility::Sprint);
    if (motionState.state == MotionState::Gliding) {
      recordTraversal(TraversalAbility::Glide);
    }
    if (motionState.state == MotionState::Climbing) {
      recordTraversal(TraversalAbility::Climb);
    }
    if (motionState.state == MotionState::Swimming) {
      recordTraversal(TraversalAbility::Swim);
    }
    jumpQueued = false;
  }
  // 开场剧情演出（阶段二验收补齐）：首次固定步懒启动，按时钟自动推进。
  loopTimeMs += dtMs;
  if (!storyDirector.started()) {
    storyDirector.start(loopTimeMs);
  }
  storyDirector.tick(loopTimeMs);

  // NPC 行为推进（Phase 4）：巡逻/驻守/对话朝向，只输出位置与朝向。
  npcAgency.update(dtSeconds, {surface.player.x, surface.player.y});

  // 锚点与可交互物检测：交互键按距离就近选择目标。
  currentAnchorInteraction = anchors.nearestInteraction(
      {surface.player.x, surface.player.y}, 0.06f);
  currentInteractable = interactables.nearest(
      {surface.player.x, surface.player.y}, 0.06f);
  const ExplorationTarget explorationTarget = explorationContent.nearestTarget(
      {surface.player.x, surface.player.y}, 0.045f);
  if (explorationTarget.kind == ExplorationTargetKind::PointOfInterest &&
      explorationContent.discoverPoint(explorationTarget.id)) {
    quests.notifyPointReached(explorationTarget.id);
    publishExplorationFeedback(ExplorationFeedbackType::PoiDiscovered,
                               explorationTarget.id, explorationTarget.label,
                               "发现新地标", 1200);
    teleportFlashMs = std::max<Tick>(teleportFlashMs, 300);
    audioBridge.playUiSound(SoundEffect::AuraApplied);
  }
  if (interactQueued) {
    interactQueued = false;
    if (dialogSession.active()) {
      // 对话进行中：交互键推进台词。
      advanceDialogSession(*this);
    } else if (storyDirector.active()) {
      // 开场字幕进行中：交互键推进演出，不触发世界交互。
      storyDirector.advance(loopTimeMs);
    } else {
      const bool anchorCloser =
          currentAnchorInteraction.anchorId >= 0 &&
          (currentInteractable.id < 0 ||
           currentAnchorInteraction.distance <= currentInteractable.distance);
      if (anchorCloser) {
        const TeleportResult result = anchors.interact(
            currentAnchorInteraction.anchorId,
            {surface.player.x, surface.player.y});
        if (result.success) {
          surface.player.x = result.position.x;
          surface.player.y = result.position.y;
          motionState = explorationMotion.reset(
              terrain.heightAt(surface.player.x, surface.player.y));
          teleportFlashMs = 1200;
          audioBridge.playUiSound(SoundEffect::Resonance);
        } else if (result.anchorId >= 0) {
          // 首次交互解锁锚点，并推进到达类任务目标与支线。
          quests.notifyAnchorReached(result.anchorId);
          sideQuests.notifyEvent(SideQuestEvent::ReachAnchor);
          dailyQuests.notifyEvent(DailyQuestKind::Anchor);
          teleportFlashMs = 600;
          audioBridge.playUiSound(SoundEffect::AuraApplied);
        }
      } else if (explorationTarget.kind == ExplorationTargetKind::Puzzle) {
        if (explorationContent.activatePuzzle(explorationTarget.id,
                                              motionState.state)) {
          quests.notifyPuzzleActivated(explorationTarget.id);
          publishExplorationFeedback(ExplorationFeedbackType::PuzzleActivated,
                                     explorationTarget.id,
                                     explorationTarget.label, "机关已激活",
                                     1200);
          const PuzzleNode* puzzle =
              explorationContent.puzzleById(explorationTarget.id);
          const TraversalGate* openedGate =
              puzzle != nullptr
                  ? explorationContent.gateById(puzzle->opensGateId)
                  : nullptr;
          if (openedGate != nullptr &&
              explorationContent.isGateOpen(openedGate->id)) {
            publishExplorationFeedback(ExplorationFeedbackType::GateOpened,
                                       openedGate->id, openedGate->label,
                                       "路径已开启", 1400);
          }
          refreshExplorationGateCollision();
          teleportFlashMs = 700;
          audioBridge.playUiSound(SoundEffect::Resonance);
        }
      } else if (explorationTarget.kind == ExplorationTargetKind::Reward) {
        const ExplorationReward* reward = nullptr;
        for (const ExplorationReward& candidate :
             explorationContent.rewards()) {
          if (candidate.id == explorationTarget.id) {
            reward = &candidate;
            break;
          }
        }
        if (reward != nullptr && explorationContent.claimReward(reward->id)) {
          publishExplorationFeedback(ExplorationFeedbackType::RewardClaimed,
                                     reward->id, reward->label,
                                     "获得探索奖励", 1200);
          if (reward->gold > 0) {
            inventory.addItem(static_cast<int32_t>(ItemId::Gold), reward->gold);
          }
          if (reward->fate > 0) {
            inventory.addItem(static_cast<int32_t>(ItemId::Fate), reward->fate);
          }
          if (reward->itemId > 0 && reward->itemCount > 0) {
            inventory.addItem(reward->itemId, reward->itemCount);
          }
          if (reward->sourceTraces > 0) {
            adventureRank.addExp(reward->sourceTraces);
          }
          if (reward->itemCount > 0 && reward->itemId == 0) {
            dropSeed = dropSeed * 1664525u + 1013904223u;
            artifacts.addArtifact(ArtifactSystem::dropDefId(dropSeed), 4,
                                  dropSeed);
          }
          teleportFlashMs = 800;
          audioBridge.playUiSound(SoundEffect::Resonance);
        }
      } else if (currentInteractable.id >= 0) {
        const InteractableKind kind = currentInteractable.kind;
        if (interactables.interact(currentInteractable.id)) {
          if (kind == InteractableKind::Npc) {
            // NPC：开启对话并推进对话类任务目标。
            dialogSession.start(DialogLibrary::defaults().find(
                interactables.dialogIdFor(currentInteractable.id)));
            quests.notifyNpcTalked(currentInteractable.id);
            // NPC 进入 Talk 态：停步并朝向玩家；清除上一次的任务发布提示。
            npcAgency.beginTalk(currentInteractable.id);
            npcOfferQuestId = -1;
            npcOfferQuestTitle.clear();
            audioBridge.playUiSound(SoundEffect::AuraApplied);
          } else if (kind == InteractableKind::Chest) {
            quests.notifyChestOpened(currentInteractable.id);
            dailyQuests.notifyEvent(DailyQuestKind::Chest);
            teleportFlashMs = 600;
            audioBridge.playUiSound(SoundEffect::Resonance);
          } else if (kind == InteractableKind::Dungeon) {
            // 秘境入口：未进入则进入副本；进行中交互提示击杀目标。
            if (dungeon.enter()) {
              teleportFlashMs = 800;
              audioBridge.playUiSound(SoundEffect::Resonance);
            }
          } else {
            quests.notifyCollect(currentInteractable.id);
            sideQuests.notifyEvent(SideQuestEvent::Collect);
            dailyQuests.notifyEvent(DailyQuestKind::Collect);
            // 首次采集启动重生倒计时（采集物重置）。
            if (collectRespawnRemainingMs <= 0) {
              collectRespawnRemainingMs = kCollectRespawnMs;
            }
            teleportFlashMs = 400;
            audioBridge.playUiSound(SoundEffect::AuraApplied);
          }
        }
      }
      currentAnchorInteraction = anchors.nearestInteraction(
          {surface.player.x, surface.player.y}, 0.06f);
      currentInteractable = interactables.nearest(
          {surface.player.x, surface.player.y}, 0.06f);
    }
  }
  if (teleportFlashMs > 0) {
    teleportFlashMs = dtMs >= teleportFlashMs ? 0 : teleportFlashMs - dtMs;
  }
  explorationFeedback.update(dtMs);
  // 采集物重生（优化）：倒计时归零后恢复全部已消耗采集物可交互。
  if (collectRespawnRemainingMs > 0) {
    collectRespawnRemainingMs -= dtMs;
    if (collectRespawnRemainingMs <= 0) {
      collectRespawnRemainingMs = 0;
      interactables.reviveConsumed(InteractableKind::Collectible);
    }
  }

  // ---- 昼夜循环（阶段四）：240 秒一个游戏日，光照随时钟插值 ----
  constexpr float kSecondsPerGameDay = 240.0f;
  timeOfDaySeconds += dtSeconds;
  while (timeOfDaySeconds >= kSecondsPerGameDay) {
    timeOfDaySeconds -= kSecondsPerGameDay;
    // 新游戏日（每日委托优化）：重置委托组合与奖励。
    gameDayCount += 1;
    dailyQuests = DailyQuestSystem(gameDayCount);
    dailyRewarded = false;
  }
  {
    const float hour = timeOfDaySeconds / (kSecondsPerGameDay / 24.0f);
    // 亮度曲线：正午最高、午夜最低，用余弦平滑过渡。
    constexpr float kPi = 3.14159265358979323846f;
    const float brightness =
        0.5f + 0.5f * std::cos((hour - 12.0f) / 24.0f * 2.0f * kPi);
    const float dayBoost = 0.35f + 0.65f * brightness;
    const float nightBoost = 0.55f + 0.45f * brightness;
    // 天气（阶段四）：由时钟确定性推导，叠乘光照衰减。
    const float weatherScale =
        WeatherSystem::weatherAt(timeOfDaySeconds).lightScale;
    surface.lightColor = glm::vec3(0.8f * dayBoost * weatherScale,
                                   0.8f * dayBoost * weatherScale,
                                   0.75f * dayBoost * weatherScale);
    surface.ambient = glm::vec3(0.25f * nightBoost, 0.25f * nightBoost,
                                0.3f * nightBoost);
  }

  // ---- BGM 区域切换（阶段四）：按纬度三分世界，跨界重启环境垫底 ----
  {
    const float y = surface.player.y;
    const int32_t region = y < 1.0f / 3.0f ? 0 : (y < 2.0f / 3.0f ? 1 : 2);
    if (region != musicRegionId) {
      musicRegionId = region;
      audioBridge.setAmbientRegion(region);
    }
  }

  // 朝向锁定：仅在主角停步时平滑转向软锁定目标，保持面对面对峙姿态；
  // 跑动中不覆盖朝向，脸部始终跟随移动方向（由 PlayerController 驱动）。
  if (!surface.player.moving && currentTarget.has_value() &&
      currentTarget->direction.length() > 0.0f) {
    const Vec2 facing = currentTarget->direction;
    const float targetAngle = std::atan2(facing.x, facing.y);
    constexpr float kTwoPi = 6.2831853071795864769f;
    const float maxTurn = 8.0f * dtSeconds;
    float delta = std::remainder(targetAngle - surface.player.angle, kTwoPi);
    delta = std::clamp(delta, -maxTurn, maxTurn);
    surface.player.angle += delta;
  }

  // 仅在摇杆有有效输入时发射脚步粒子，避免减速滑行期间误发。
  particleEmitTimer += dtSeconds;
  if (surface.player.moving && intent.move.length() > 0.0f &&
      particleEmitTimer > 0.05f) {
    particleEmitTimer = 0.0f;
    surface.particles.push_back({surface.player.x, surface.player.y, 0.4f,
                                 0.4f});
  }
  for (Particle& particle : surface.particles) {
    particle.life -= dtSeconds;
  }
  surface.particles.erase(
      std::remove_if(surface.particles.begin(), surface.particles.end(),
                     [](const Particle& particle) {
                       return particle.life <= 0.0f;
                     }),
      surface.particles.end());

  // 3D 移动尾迹：移动中每 0.09s 在脚下发射一颗缓升淡蓝粒子（kind=3），
  // 在 3D 场景中给出运动轨迹感；与 2D 脚步粒子同源条件。
  trailEmitTimer += dtSeconds;
  if (surface.player.moving && intent.move.length() > 0.0f &&
      trailEmitTimer > 0.09f) {
    trailEmitTimer = 0.0f;
    if (surface.hitSparks3d.size() <= 128) {
      surface.hitSparks3d.push_back(
          {surface.player.x, 0.006f, surface.player.y, 0.0f, 0.012f, 0.0f,
           0.45f, 0.45f, 3,
           VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player)});
    }
  }

  // 源技能释放特效：冷却开始（上升沿）时在主角周身爆发一圈技能色火花，
  // 辉印=金白、脉流=青蓝、蚀质=暗紫，让施法瞬间可见；
  // 同时向目标发射飞行投射物，给出“主角→目标”的释放过程动效。
  {
    const CombatSnapshot& skillSnapshot = combat.snapshot();
    const Vec2 playerPos{surface.player.x, surface.player.y};
    const float playerRatio =
        actorVfxRatio(surface, CombatController::kPlayerId);
    // 释放目标：优先软锁定目标；未锁定时退回首领（首领战），
    // 保证投射物始终有明确去处。
    std::optional<Vec2> releaseTarget;
    if (currentTarget.has_value()) {
      releaseTarget = resolveEntityPosition(
          surface, encounter.snapshot(),
          static_cast<EntityId>(currentTarget->id), &wildSpawn);
    }
    if (!releaseTarget.has_value() &&
        encounter.snapshot().mode == EncounterMode::Boss &&
        encounter.snapshot().boss.hp > 0) {
      releaseTarget = Vec2{surface.boss3d.x, surface.boss3d.y};
    }
    // 三系技能释放点缀（剪影差异化，原神技能语言）：辉印=光柱
    // （辉印降临）、脉流=追加束流（流动投射物）、蚀质=贴地蚀斑，
    // 由 SkillCastAccentFor 同源驱动，三技能释放轮廓不再同形。
    const auto applySkillAccent = [&](int source, glm::vec3 color) {
      switch (SkillCastAccentFor(source)) {
        case SkillCastAccent::Pillar:
          spawnLightPillar(surface, playerPos, color, 0.09f * playerRatio);
          break;
        case SkillCastAccent::Stream:
          if (releaseTarget.has_value()) {
            spawnAttackProjectiles(surface, playerPos, *releaseTarget,
                                   AuraSparkKindFor(source), playerRatio, 3);
          }
          break;
        case SkillCastAccent::Decal:
          spawnImpactDecal(surface, playerPos, color, 0.06f * playerRatio);
          break;
        case SkillCastAccent::None:
          break;
      }
    };
    if (skillSnapshot.radianceCooldownMs > 0 && prevRadianceCdMs <= 0) {
      spawnHitSparks(surface, playerPos, 4, 12, 1.3f, 1.3f, playerRatio);
      if (releaseTarget.has_value()) {
        spawnAttackProjectiles(surface, playerPos, *releaseTarget, 4,
                               playerRatio, 5);
      }
      spawnShockwave(surface, playerPos, glm::vec3{1.0f, 0.96f, 0.72f},
                     0.07f * playerRatio);
      spawnSkillRune(surface, playerPos, glm::vec3{1.0f, 0.96f, 0.72f},
                     0.05f * playerRatio);
      applySkillAccent(0, glm::vec3{1.0f, 0.96f, 0.72f});
      // 元素技能镜头语言：轻档 FOV 冲击 + 40ms 卡肉（重于普攻、
      // 轻于终结技/反应），把技能释放从普攻节奏里分层拎出。
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(0);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 40, 96);
      // 元素附魔：武器附着辉印，普攻刀光/拖尾随之染金白。
      surface.playerSlashSource = 0;
    }
    if (skillSnapshot.currentCooldownMs > 0 && prevCurrentCdMs <= 0) {
      spawnHitSparks(surface, playerPos, 5, 12, 1.3f, 1.3f, playerRatio);
      if (releaseTarget.has_value()) {
        spawnAttackProjectiles(surface, playerPos, *releaseTarget, 5,
                               playerRatio, 5);
      }
      spawnShockwave(surface, playerPos, glm::vec3{0.45f, 0.85f, 1.0f},
                     0.07f * playerRatio);
      spawnSkillRune(surface, playerPos, glm::vec3{0.45f, 0.85f, 1.0f},
                     0.05f * playerRatio);
      applySkillAccent(1, glm::vec3{0.45f, 0.85f, 1.0f});
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(0);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 40, 96);
      // 元素附魔：武器附着脉流，普攻刀光/拖尾随之染青蓝。
      surface.playerSlashSource = 1;
    }
    if (skillSnapshot.corruptionCooldownMs > 0 && prevCorruptionCdMs <= 0) {
      spawnHitSparks(surface, playerPos, 6, 12, 1.3f, 1.3f, playerRatio);
      if (releaseTarget.has_value()) {
        spawnAttackProjectiles(surface, playerPos, *releaseTarget, 6,
                               playerRatio, 5);
      }
      spawnShockwave(surface, playerPos, glm::vec3{0.72f, 0.45f, 0.95f},
                     0.07f * playerRatio);
      spawnSkillRune(surface, playerPos, glm::vec3{0.72f, 0.45f, 0.95f},
                     0.05f * playerRatio);
      applySkillAccent(2, glm::vec3{0.72f, 0.45f, 0.95f});
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(0);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 40, 96);
      // 元素附魔：武器附着蚀质，普攻刀光/拖尾随之染暗紫。
      surface.playerSlashSource = 2;
    }
    // 终结技释放动效：进入吟唱状态瞬间在主角周身爆发大规模金白火花，
    // 并向目标齐射更粗更亮的密集束流，强化“终结一击”的仪式感。
    const uint8_t actionNow = skillSnapshot.currentAction;
    if (actionNow == static_cast<uint8_t>(ActionState::CastingUltimate) &&
        prevActionForVfx !=
            static_cast<uint8_t>(ActionState::CastingUltimate)) {
      // 终结技按出战角色源质着色（原神元素爆发语言）：元素角色
      // 释放自身源质色爆发，物理角色保持通用亮金。
      const UltimateVfx ultimateVfx = UltimateVfxFor(activeCharacterId);
      spawnHitSparks(surface, playerPos, ultimateVfx.sparkKind, 20, 2.0f, 1.6f,
                     playerRatio * 1.5f);
      if (releaseTarget.has_value()) {
        spawnAttackProjectiles(surface, playerPos, *releaseTarget,
                               ultimateVfx.sparkKind,
                               playerRatio * 1.5f, 7);
      }
      // 终结技冲击波：半径更大，强化“终结一击”的地面震荡感。
      spawnShockwave(surface, playerPos, ultimateVfx.color,
                     0.11f * playerRatio);
      // 终结技符文环：更大更亮的旋转符阵，强化爆发仪式感。
      spawnSkillRune(surface, playerPos, ultimateVfx.color,
                     0.08f * playerRatio);
      // 终结技光柱：施法者位置升起亮金元素光柱（原神元素爆发语言），
      // 高度随模型缩放，与共鸣光柱同源曲线。
      spawnLightPillar(surface, playerPos, ultimateVfx.color,
                       0.15f * playerRatio);
      // 终结技镜头语言：FOV 收窄冲击 + 64ms 卡肉（重于普攻命中、
      // 略轻于转阶段），把终结一击从普通技能里拎出来。
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 64, 96);
    }
    // 闪避释放动效：进入闪避状态瞬间主角脚下爆出淡蓝冲刺尘土
    //（原神冲刺语言），与既有全屏蓝闪呼应，强化侧身闪避的灵动感。
    if (actionNow == static_cast<uint8_t>(ActionState::Dodging) &&
        prevActionForVfx != static_cast<uint8_t>(ActionState::Dodging)) {
      spawnHitSparks(surface, playerPos, 3, 10, 1.1f, 1.0f, playerRatio);
      spawnImpactDecal(surface, playerPos, DodgeDustColor(),
                       0.04f * playerRatio);
    }
    prevActionForVfx = actionNow;
    // 普攻释放动效：连击段数变化（每次挥击升阶/回绕）时，主角周身
    // 爆出小型挥击火花，并朝目标发射双投射物，命中点由爆裂火花收束。
    const int comboSegmentNow = static_cast<int>(skillSnapshot.comboSegment);
    if (comboSegmentNow != prevComboSegmentForVfx && comboSegmentNow > 0) {
      // 附魔普攻释放：挥击火花/投射物也按源质着色，与刀光/拖尾
      // 同语言（无附魔保持金橙）。
      const int swingKind =
          InfusedHitSparkKindFor(surface.playerSlashSource, 0);
      spawnHitSparks(surface, playerPos, swingKind, 4, 0.8f, 0.8f,
                     playerRatio);
      if (releaseTarget.has_value()) {
        spawnAttackProjectiles(surface, playerPos, *releaseTarget, swingKind,
                               playerRatio, 2);
      }
      // 普攻刀光：记录挥击瞬间朝向并启动弧线计时；第 4 段为终结段，
      // SlashArcPoseAt 会放大弧线并提亮。
      surface.playerSlashSeconds = 0.0f;
      surface.playerSlashCombo = comboSegmentNow;
      surface.playerSlashYaw = surface.player.angle;
      // 终结段（第 4 击）地面反馈：挥击瞬间主角脚下爆出金橙冲击波 +
      // 贴地贴花，与放大的终结刀光呼应，强化连段收尾仪式感。
      if (comboSegmentNow >= 4) {
        spawnShockwave(surface, playerPos, glm::vec3{1.0f, 0.78f, 0.38f},
                       0.08f * playerRatio);
        spawnImpactDecal(surface, playerPos, glm::vec3{1.0f, 0.78f, 0.38f},
                         0.045f * playerRatio);
        // 终结段相机震动：重劈分量对应的轻震（幅度与受击同级、
        // 轻于首领砸地），补全终结段"刀光+地面+镜头"分量链。
        vfxSystem.triggerCameraShake(FP_ONE);
      }
    }
    prevComboSegmentForVfx = comboSegmentNow;
    prevRadianceCdMs = skillSnapshot.radianceCooldownMs;
    prevCurrentCdMs = skillSnapshot.currentCooldownMs;
    prevCorruptionCdMs = skillSnapshot.corruptionCooldownMs;
  }

  // 软锁定候选合并：遭遇敌人 + 野外敌人（WildSpawnSystem）。
  std::vector<TargetCandidate> candidates = encounter.snapshot().candidates;
  const std::vector<TargetCandidate> wildCandidates = wildSpawn.candidates();
  candidates.insert(candidates.end(), wildCandidates.begin(), wildCandidates.end());
  currentTarget = softTargeting.select(
      {surface.player.x, surface.player.y}, camera.yaw(), candidates,
      currentTarget ? std::optional<int32_t>{currentTarget->id} : std::nullopt);

  // 相机双模式：无锁定目标时切探索视角（拉远），有目标时收回战斗视角。
  camera.setExploration(!currentTarget.has_value());

  // 锁定目标指示器：发布目标位置与脉冲相位，目标脚下绘制脉冲环。
  surface.targetMarker3d.active = currentTarget.has_value();
  surface.targetMarker3d.targetId =
      currentTarget.has_value() ? static_cast<uint32_t>(currentTarget->id) : 0u;
  // 首领锁定态同步发布，供渲染层轮廓光常亮增强。
  surface.boss3d.targeted =
      currentTarget.has_value() &&
      static_cast<EntityId>(currentTarget->id) == EncounterController::kBossId;
  if (currentTarget.has_value()) {
    const std::optional<Vec2> markerPosition = resolveEntityPosition(
        surface, encounter.snapshot(),
        static_cast<EntityId>(currentTarget->id), &wildSpawn);
    if (markerPosition.has_value()) {
      surface.targetMarker3d.x = markerPosition->x;
      surface.targetMarker3d.z = markerPosition->y;
    } else {
      surface.targetMarker3d.active = false;
      surface.targetMarker3d.targetId = 0u;
    }
    // 锁定标记元素归属：供渲染层把指示环混入目标元素色。
    const std::optional<int> markerElement = resolveEnemyElement(
        encounter.snapshot(), static_cast<EntityId>(currentTarget->id),
        &wildSpawn);
    surface.targetMarker3d.element =
        markerElement.has_value() ? *markerElement : -1;
  }
  if (!currentTarget.has_value()) {
    surface.targetMarker3d.element = -1;
  }
  surface.targetMarker3d.pulsePhase =
      static_cast<float>(combatTimeMs_.load()) * 0.004f;

  for (const ActionRequest& action : intent.actions) combat.enqueue(action);
  intent.actions.clear();
  const Tick combatTime = AdvanceCombatTime(combatTimeMs_.load(), dtMs);
  combatTimeMs_.store(combatTime);
  // 敌人碰撞：遭遇敌人与野外敌人共用同一建筑碰撞集。
  const auto enemyPositionResolver = [this](Vec2& position, float radius) {
    const float ground = terrain.heightAt(position.x, position.y);
    buildingCollision.resolve(position.x, position.y, radius, ground);
    explorationGateCollision.resolve(position.x, position.y, radius, ground);
  };
  // 野外刷怪（Phase 3.2/3.3）：worldGrid 流式之后推进；生成/回收/重生/
  // 巡逻/LOD 在此结算，敌方命中转发 combat 外部通道。
  // 性能档位转发（Phase 5）：降级时活跃上限 8→6→4、感知半径收缩。
  wildSpawn.setPerformanceLevel(performanceGuard.lodLevel());
  wildSpawn.update({combatTime, dtMs, {surface.player.x, surface.player.y},
                    combat.snapshot().playerHp > 0,
                    &worldGrid.activeChunks(), enemyPositionResolver});
  wildEnemyCount = wildSpawn.activeCount();
  // 仅遭遇运行时转发：避免队列在非战斗态滞留。
  if (encounter.snapshot().state == EncounterState::Running) {
    for (const HitRequest& hit : wildSpawn.playerHits()) {
      combat.enqueueExternalEnemyHit(hit);
    }
  }
  // 绑定玩家锁定的野外目标：玩家攻击改道结算到它（非野外目标时空绑定）。
  combat.setExternalTargetBinding(wildSpawn.combatBinding(
      currentTarget ? static_cast<EntityId>(currentTarget->id) : 0,
      {surface.player.x, surface.player.y}));
  encounter.update({combatTime, dtMs,
                    {surface.player.x, surface.player.y},
                    surface.player.moving,
                    currentTarget ? static_cast<EntityId>(currentTarget->id) : 0,
                    enemyPositionResolver});
  const EncounterSnapshot& encounterState = encounter.snapshot();
  DemoSignals demoSignals;
  demoSignals.introComplete = combatTime >= 1000;
  demoSignals.reachedCombatAnchor = surface.player.y >= 0.45f;
  // Victory 门控：野外敌人走独立 WildSpawnSystem 通道，天然隔离。
  demoSignals.encounterComplete =
      encounterState.state == EncounterState::Victory;
  demoSignals.allSourcesActive = combat.snapshot().radianceAttached &&
                                 combat.snapshot().currentAttached &&
                                 combat.snapshot().corruptionAttached;
  demoSignals.cinematicComplete = false;
  demoSignals.bossDefeated = encounterState.state == EncounterState::Victory &&
                             encounterState.mode == EncounterMode::Boss;
  demoDirector.tick(combatTime, demoSignals);
  surface.trainingTarget.alive =
      encounter.snapshot().mode == EncounterMode::Training &&
      combat.snapshot().targetAlive;
  // 训练假人元素附着掩码：仅训练模式取自战斗快照附着位（此时
  // 快照附着位即假人身上源质）；其余模式锁定目标的附着已由
  // EncounterEnemySnapshot 逐敌人发布，避免重复绘制。
  surface.trainingTargetAuraMask =
      encounter.snapshot().mode == EncounterMode::Training
          ? AuraMaskFromFlags(combat.snapshot().radianceAttached,
                              combat.snapshot().currentAttached,
                              combat.snapshot().corruptionAttached)
          : 0;
  // 元素附着施加爆发（原神式）：目标新附着源质瞬间在其位置爆出
  // 小型元素火花 + 贴地元素贴花，与呼吸附着光环同元素语言；附着
  // 掩码差分边沿检测（不新增事件），死亡/离场实体掩码归零清理。
  {
    const auto auraBurst = [this](EntityId id, int newMask, Vec2 position,
                                  float ratio) {
      const uint32_t key = static_cast<uint32_t>(id);
      const auto prev = prevAuraMasks.find(key);
      const int prevMask = prev != prevAuraMasks.end() ? prev->second : 0;
      const int freshMask = newMask & ~prevMask;
      prevAuraMasks[key] = newMask;
      for (int sourceType = 0; sourceType < 3; ++sourceType) {
        if ((freshMask & (1 << sourceType)) == 0) continue;
        spawnHitSparks(surface, position, AuraSparkKindFor(sourceType), 8,
                       1.2f, 1.1f, ratio);
        spawnImpactDecal(surface, position, AuraColorFor(sourceType),
                         0.05f * ratio);
      }
    };
    for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
      const int mask = enemy.alive
                           ? AuraMaskFromFlags(enemy.radianceAttached,
                                               enemy.currentAttached,
                                               enemy.corruptionAttached)
                           : 0;
      auraBurst(enemy.id, mask, enemy.position,
                actorVfxRatio(surface, enemy.id));
    }
    if (encounter.snapshot().mode == EncounterMode::Training) {
      const int mask = AuraMaskFromFlags(combat.snapshot().radianceAttached,
                                         combat.snapshot().currentAttached,
                                         combat.snapshot().corruptionAttached);
      auraBurst(CombatController::kTrainingTargetId, mask,
                {surface.trainingTarget.x, surface.trainingTarget.y},
                actorVfxRatio(surface,
                              CombatController::kTrainingTargetId));
    }
    // 清理已离开快照的实体边沿状态，避免长期泄漏。
    for (auto entry = prevAuraMasks.begin(); entry != prevAuraMasks.end();) {
      const EntityId id = static_cast<EntityId>(entry->first);
      const bool present =
          (id == CombatController::kTrainingTargetId &&
           encounter.snapshot().mode == EncounterMode::Training) ||
          std::any_of(encounter.snapshot().enemies.begin(),
                      encounter.snapshot().enemies.end(),
                      [id](const EncounterEnemySnapshot& enemy) {
                        return enemy.id == id;
                      });
      if (present) {
        ++entry;
      } else {
        entry = prevAuraMasks.erase(entry);
      }
    }
  }
  // 锁定释放复核必须用本步结算后的新鲜候选：帧首候选是上一步快照，
  // 不含本步 encounter.update 的击杀结果，已死目标会多挂一帧锁定；
  // 而击杀触发的命中卡肉又会冻结后续固定步，把延迟放大成幽灵锁定。
  // 释放时同步关闭锁定标记并切回探索视角，避免残留一帧幽灵标记。
  if (currentTarget.has_value()) {
    std::vector<TargetCandidate> freshCandidates =
        encounter.snapshot().candidates;
    const std::vector<TargetCandidate> freshWildCandidates =
        wildSpawn.candidates();
    freshCandidates.insert(freshCandidates.end(),
                           freshWildCandidates.begin(),
                           freshWildCandidates.end());
    if (std::none_of(freshCandidates.begin(), freshCandidates.end(),
                     [this](const TargetCandidate& candidate) {
                       return candidate.id == currentTarget->id;
                     })) {
      currentTarget.reset();
      surface.targetMarker3d.active = false;
      surface.targetMarker3d.targetId = 0u;
      camera.setExploration(true);
    }
  }

  bool playerHitObserved = false;
  std::size_t gameplayEventStart = 0;
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    const CombatEventBatch& stepEvents = encounter.events().combat;
    playerHitObserved = std::any_of(
        stepEvents.presentation.begin(), stepEvents.presentation.end(),
        [](const PresentationEvent& event) {
          return event.target == CombatController::kPlayerId &&
                 event.type == PresentationEventType::HitFlash;
        });
    gameplayEventStart = frameCombatEvents_.gameplay.size();
    frameCombatEvents_.gameplay.insert(frameCombatEvents_.gameplay.end(),
                                       stepEvents.gameplay.begin(),
                                       stepEvents.gameplay.end());
    frameCombatEvents_.presentation.insert(frameCombatEvents_.presentation.end(),
                                           stepEvents.presentation.begin(),
                                           stepEvents.presentation.end());
    const auto less = [](const auto& left, const auto& right) {
      if (left.tick != right.tick) return left.tick < right.tick;
      if (left.source != right.source) return left.source < right.source;
      if (left.target != right.target) return left.target < right.target;
      return left.sequence < right.sequence;
    };
    std::stable_sort(frameCombatEvents_.gameplay.begin(), frameCombatEvents_.gameplay.end(), less);
   std::stable_sort(frameCombatEvents_.presentation.begin(), frameCombatEvents_.presentation.end(), less);
  }

  vfxSystem.consume(frameCombatEvents_);
  vfxSystem.update(combatTime, dtMs);
  audioBridge.dispatch(frameCombatEvents_);

  // 伤害飘字：仅从本步新增的 Damage 事件生成（避免同帧多步重复）；野外
  // 击杀的 Death/Damage 事件由 CombatController 产出（target = 野外 id），
  // 自动计入下方击杀/掉落/任务管线。
  int32_t killsThisStep = 0;
  for (std::size_t eventIndex = gameplayEventStart;
       eventIndex < frameCombatEvents_.gameplay.size(); ++eventIndex) {
    const GameplayEvent& event = frameCombatEvents_.gameplay[eventIndex];
    // 击杀爆裂：非玩家死亡时爆发更大规模的亮金火花，强化击杀确认感。
    // 玩家死亡爆发（原神角色死亡语言）：主角倒下瞬间周身暗红
    // 火花 + 冲击波 + 重镜头反馈，把死亡拎成重击时刻；死亡动画
    // 与后续复活/重置流程不变。
    if (event.type == GameplayEventType::Death &&
        event.target == CombatController::kPlayerId) {
      const PlayerDeathVfx deathVfx = PlayerDeathVfxFor();
      const Vec2 playerDeathPos{surface.player.x, surface.player.y};
      const float playerDeathRatio =
          VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player);
      spawnHitSparks(surface, playerDeathPos, deathVfx.sparkKind, 18, 1.8f,
                     1.4f, playerDeathRatio);
      spawnShockwave(surface, playerDeathPos, deathVfx.color,
                     0.12f * playerDeathRatio);
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 72, 96);
      vfxSystem.triggerCameraShake(2 * FP_ONE);
      continue;
    }
    if (event.type == GameplayEventType::Death &&
        event.target != CombatController::kPlayerId) {
      killsThisStep += 1;
      // 打怪升级闭环（原神式）：击杀掉落经验/金币/材料，
      // 受世界等级倍率放大；Boss 额外必掉五星圣遗物。
      dropSeed = dropSeed * 1664525u + 1013904223u;
      const int32_t multPct =
          AdventureRank::dropMultiplierPct(adventureRank.worldLevel());
      const bool isBoss = event.target == EncounterController::kBossId;
      const int32_t charExp = (isBoss ? 300 : 20) * multPct / 100;
      const int32_t gold = (isBoss ? 500 : 30) * multPct / 100;
      characters.addExp(activeCharacterId, std::max(charExp, 1));
      adventureRank.addExp(std::max(charExp / 10, 1));
      inventory.addItem(static_cast<int32_t>(ItemId::Gold), std::max(gold, 1));
      if (isBoss) {
        // Boss 掉落：经验书 x5 + 突破材料 x3 + 必掉五星圣遗物。
        inventory.addItem(static_cast<int32_t>(ItemId::ExpSmall),
                          5 * multPct / 100);
        inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                          3 * multPct / 100);
        inventory.addItem(static_cast<int32_t>(ItemId::OreHigh),
                          2 * multPct / 100);
        dropSeed = dropSeed * 1664525u + 1013904223u;
        artifacts.addArtifact(ArtifactSystem::dropDefId(dropSeed), 5,
                              dropSeed);
      } else {
        // 普通敌人：20% 概率经验书、10% 概率突破材料、30% 概率矿石。
        const int32_t roll = static_cast<int32_t>(dropSeed % 100u);
        if (roll < 20) {
          inventory.addItem(static_cast<int32_t>(ItemId::ExpSmall),
                            std::max(1 * multPct / 100, 1));
        } else if (roll < 30) {
          inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                            std::max(1 * multPct / 100, 1));
        } else if (roll < 60) {
          inventory.addItem(static_cast<int32_t>(ItemId::OreLow),
                            std::max(1 * multPct / 100, 1));
        }
      }
      const std::optional<Vec2> deathPos = resolveEntityPosition(
          surface, encounter.snapshot(), event.target, &wildSpawn);
      if (deathPos.has_value()) {
        const float deathRatio = actorVfxRatio(surface, event.target);
        // 元素死亡爆发（原神式）：元素系敌人按原型元素色爆散
        //（元素火花 + 小型元素冲击波），物理系/首领/假人保持亮金
        // 击杀爆裂，颜色语言与附着/技能同源。
        const std::optional<int> deathElement =
            resolveEnemyElement(encounter.snapshot(), event.target,
                                &wildSpawn);
        if (deathElement.has_value()) {
          spawnHitSparks(surface, *deathPos,
                         AuraSparkKindFor(*deathElement), 16, 1.8f, 1.4f,
                         deathRatio);
          spawnShockwave(surface, *deathPos, AuraColorFor(*deathElement),
                         0.09f * deathRatio);
          // 元素死亡光柱：死亡点升起小型元素色光柱（元素死亡的
          // 垂直签名，与共鸣/终结光柱同曲线），物理系亮金击杀
          // 爆裂保持原样不叠加，避免群怪死亡刷屏抢戏。
          spawnLightPillar(surface, *deathPos, AuraColorFor(*deathElement),
                           0.10f * deathRatio);
        } else {
          spawnHitSparks(surface, *deathPos, 2, 14, 1.8f, 1.4f, deathRatio);
        }
      }
      continue;
    }
    // 完美闪避（原神完美闪避）：无敌帧内闪过敌人攻击瞬间主角周身
    // 爆出淡蓝火花 + 冲击波，配合中档 FOV 冲击 + 56ms 卡肉替代原版
    // 慢镜感；蓝闪与闪避音效来自既有 DodgeFlash/音频通道。
    if (event.type == GameplayEventType::Dodge) {
      const PerfectDodgeVfx dodgeVfx = PerfectDodgeVfxFor();
      const Vec2 dodgePos{surface.player.x, surface.player.y};
      const float dodgeRatio =
          VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player);
      spawnHitSparks(surface, dodgePos, dodgeVfx.sparkKind, 14, 1.4f, 1.2f,
                     dodgeRatio);
      spawnShockwave(surface, dodgePos, dodgeVfx.color, 0.09f * dodgeRatio);
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(1);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 56, 96);
      continue;
    }
    // 破韧爆发（原神式破韧）：敌人韧性击碎瞬间在其位置爆发亮金
    // 碎裂火花 + 冲击波，伴随更重卡肉与 FOV 冲击，传达"防线破碎、
    // 全力输出"窗口；此前破韧仅有音效无视觉反馈。
    if (event.type == GameplayEventType::PoiseBreak &&
        event.target != CombatController::kPlayerId) {
      const std::optional<Vec2> poisePos = resolveEntityPosition(
          surface, encounter.snapshot(), event.target, &wildSpawn);
      if (poisePos.has_value()) {
        const float poiseRatio = actorVfxRatio(surface, event.target);
        spawnHitSparks(surface, *poisePos, 2, 18, 1.9f, 1.4f,
                       poiseRatio * 1.2f);
        spawnShockwave(surface, *poisePos, glm::vec3{1.0f, 0.92f, 0.62f},
                       0.12f * poiseRatio);
        hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 72, 96);
        surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
        surface.resonanceFovSeconds = 0.0f;
        // 破韧相机震动：韧性破碎瞬间的轻震，与卡肉/FOV 同节奏。
        vfxSystem.triggerCameraShake(FP_ONE);
      }
      continue;
    }
    // 元素反应爆发：三源共鸣触发时在受击点爆发大规模元素色火花 +
    // 冲击波环 + 贴花，颜色/火花 kind 按反应类型区分（原神式
    // 元素反应反馈）；反应类型取自战斗快照（同帧单次反应准确）。
    if (event.type == GameplayEventType::Resonance) {
      // 元素反应镜头/卡肉：FOV 收窄冲击 + 60ms 顿帧，把反应
      // 从普通命中里拎出来（原神元素爆发仪式感）。
      surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
      surface.resonanceFovSeconds = 0.0f;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 60, 96);
      // 元素反应相机震动：反应是战斗里最高光时刻，幅度与首领
      // 砸地同级（2×受击），与光柱/冲击波/卡肉同步落地。
      vfxSystem.triggerCameraShake(2 * FP_ONE);
      const std::optional<Vec2> reactionPos = resolveEntityPosition(
          surface, encounter.snapshot(), event.target, &wildSpawn);
      if (reactionPos.has_value()) {
        const ReactionVfx reactionVfx =
            ReactionVfxFor(combat.snapshot().currentReaction);
        const float ratio = actorVfxRatio(surface, event.target);
        spawnHitSparks(surface, *reactionPos, reactionVfx.sparkKind, 16,
                       1.6f, 1.3f, ratio);
        spawnShockwave(surface, *reactionPos, reactionVfx.color,
                       0.11f * ratio);
        spawnImpactDecal(surface, *reactionPos, reactionVfx.color,
                         0.05f * ratio);
        // 共鸣爆发（3）光柱加高 60%，其余反应标准高度。
        const float pillarBoost =
            combat.snapshot().currentReaction == 3 ? 1.6f : 1.0f;
        spawnLightPillar(surface, *reactionPos, reactionVfx.color,
                         0.13f * ratio * pillarBoost);
      }
      continue;
    }
    if (event.type != GameplayEventType::Damage) continue;
    // 非玩家目标受击：启动模型闪白计时器，渲染层据此提亮配色；
    // 累计受击次数按奇偶驱动受击动画变体轮换。
    if (event.target != CombatController::kPlayerId) {
      surface.enemyHitFlash[static_cast<uint32_t>(event.target)] = 0.15f;
      surface.enemyHitCounts[static_cast<uint32_t>(event.target)] += 1u;
    }
    const std::optional<Vec2> position = resolveEntityPosition(
        surface, encounter.snapshot(), event.target, &wildSpawn);
    if (!position.has_value()) continue;
    const float amount =
        static_cast<float>(event.value) / static_cast<float>(FP_ONE);
    DamageNumberKind kind = DamageNumberKind::Normal;
    if (event.target == CombatController::kPlayerId) {
      kind = DamageNumberKind::PlayerHit;
    } else if (event.source == CombatController::kPlayerId &&
               surface.playerSlashSource >= 0) {
      // 附魔期间主角伤害按源质着色（原神元素伤害飘字语言）：
      // 元素色优先于大额金色，与刀光/拖尾染色同状态。
      kind = static_cast<DamageNumberKind>(
          static_cast<int>(DamageNumberKind::Radiance) +
          surface.playerSlashSource);
    } else if (amount >= 15.0f) {
      kind = DamageNumberKind::Heavy;
    }
    // 命中卡肉：玩家命中敌人时短暂顿帧；重击/击杀更重。
    if (event.source == CombatController::kPlayerId) {
      const int64_t stopMs = amount >= 15.0f ? 64 : 40;
      hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + stopMs, 80);
    }
    damageNumbers.spawn(*position, amount, kind);
    // 命中火花：与飘字同源，玩家受击红色、物理命中金橙；命中元素
    // 敌人时火花按目标元素色爆散（与附着/死亡语言同源），尺寸按
    // 受击实体的模型缩放同步，大体型目标火花更大。
    const bool playerHit = event.target == CombatController::kPlayerId;
    int hitKind = playerHit ? 1 : 0;
    glm::vec3 decalColor = playerHit ? glm::vec3{1.0f, 0.35f, 0.30f}
                                     : glm::vec3{1.0f, 0.78f, 0.32f};
    if (!playerHit) {
      const int infusionSource =
          event.source == CombatController::kPlayerId
              ? surface.playerSlashSource
              : -1;
      if (infusionSource >= 0) {
        // 附魔普攻命中：攻击元素优先（原神元素附魔语言），与
        // 刀光/拖尾/飘字染色同状态。
        hitKind = InfusedHitSparkKindFor(infusionSource, hitKind);
        decalColor = InfusedHitDecalColorFor(infusionSource, decalColor);
      } else {
        const std::optional<int> hitElement = resolveEnemyElement(
            encounter.snapshot(), event.target, &wildSpawn);
        if (hitElement.has_value()) {
          hitKind = AuraSparkKindFor(*hitElement);
          decalColor = AuraColorFor(*hitElement);
        }
      }
    }
    spawnHitSparks(surface, *position, hitKind, 6, 1.0f, 1.0f,
                   actorVfxRatio(surface, event.target));
    // 命中贴地冲击贴花：玩家受击红色、物理命中金橙、元素命中元素色，
    // 重击半径更大，与火花/飘字共同构成“打中了”的地面反馈。
    const float decalRatio = actorVfxRatio(surface, event.target);
    spawnImpactDecal(surface, *position, decalColor,
                     (amount >= 15.0f ? 0.045f : 0.03f) * decalRatio);
    // 受击方向性粒子：沿攻击方向（攻击者→受击者）喷射，强化打击
    // 方向感；攻击者位置不可解析（环境伤害等）时跳过。
    const std::optional<Vec2> sourcePosition = resolveEntityPosition(
        surface, encounter.snapshot(), event.source, &wildSpawn);
    if (sourcePosition.has_value()) {
      const Vec2 hitDirection = *position - *sourcePosition;
      if (hitDirection.length() > 0.001f) {
        spawnDirectionalSparks(surface, *position, hitDirection,
                               hitKind, 4, 1.0f, decalRatio);
      }
    }
  }
  damageNumbers.update(dtMs);
  // 击杀事件推进任务与秘境：仅统计敌人/首领死亡，排除玩家自身。
  if (killsThisStep > 0) {
    quests.notifyEnemiesKilled(killsThisStep);
    openWorldQuests.notifyEnemiesKilled(killsThisStep);
    sideQuests.notifyEvent(SideQuestEvent::Kill, killsThisStep);
    dailyQuests.notifyEvent(DailyQuestKind::Kill, killsThisStep);
    if (dungeon.state() == DungeonState::Active) {
      dungeon.notifyEnemiesKilled(killsThisStep);
      if (dungeon.state() == DungeonState::Cleared) {
        // 秘境结算：达标即发放奖励并退出副本（进入养成经济回路）。
        const DungeonDef& def = dungeon.def();
        inventory.addItem(static_cast<int32_t>(ItemId::Gold), def.rewardGold);
        inventory.addItem(static_cast<int32_t>(ItemId::ExpMaterial), def.rewardExp);
        inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial),
                          def.rewardAscension);
        // 冒险经验（原神式）：秘境通关同步发放。
        adventureRank.addExp(120);
        (void)dungeon.leave();
        teleportFlashMs = 900;
        audioBridge.playUiSound(SoundEffect::Resonance);
      }
    }
  }
  // 支线完成奖励：按完成数补发（确定性配置）。
  while (lastRewardedSideCount < sideQuests.completedCount()) {
    lastRewardedSideCount += 1;
    inventory.addItem(static_cast<int32_t>(ItemId::Gold), 100);
    inventory.addItem(static_cast<int32_t>(ItemId::Fate), 2);
  }
  // 每日委托奖励（内容优化）：全部完成一次性发放。
  if (!dailyRewarded && dailyQuests.allCompleted()) {
    dailyRewarded = true;
    inventory.addItem(static_cast<int32_t>(ItemId::Fate), 3);
    inventory.addItem(static_cast<int32_t>(ItemId::Gold), 150);
    teleportFlashMs = 600;
    audioBridge.playUiSound(SoundEffect::Resonance);
  }
  // 任务完成奖励：按完成任务数补发未领取的奖励（确定性配置表）。
  while (lastRewardedQuestCount < quests.completedCount()) {
    lastRewardedQuestCount += 1;
    switch (lastRewardedQuestCount) {
      case 1:
        inventory.addItem(static_cast<int32_t>(ItemId::Fate), 5);
        break;
      case 2:
        inventory.addItem(static_cast<int32_t>(ItemId::Fate), 5);
        break;
      case 3:
        inventory.addItem(static_cast<int32_t>(ItemId::Gold), 50);
        inventory.addItem(static_cast<int32_t>(ItemId::ExpMaterial), 5);
        // 武器奖励（养成深化）：重复获取折算金币。
        if (!weapons.addWeapon(4)) {
          inventory.addItem(static_cast<int32_t>(ItemId::Gold), 200);
        }
        break;
      case 4:
        inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial), 3);
        break;
      default:
        inventory.addItem(static_cast<int32_t>(ItemId::Fate), 10);
        if (!weapons.addWeapon(5)) {
          inventory.addItem(static_cast<int32_t>(ItemId::Gold), 200);
        }
        break;
    }
  }
  surface.damageNumbers3d.clear();
  for (const DamageNumber& number : damageNumbers.active()) {
    DamageNumberRenderState state;
    state.x = number.origin.x;
    state.z = number.origin.y;
    state.rise = DamageNumberSystem::riseOffset(number);
    state.driftX = number.driftX;
    state.alpha = DamageNumberSystem::alpha(number);
    state.scale = DamageNumberSystem::popScale(number);
    state.value = number.value;
    state.kind = static_cast<int>(number.kind);
    surface.damageNumbers3d.push_back(state);
  }

  // 敌人头顶血条：仅发布存活敌人，比例 = 当前 HP / 最大 HP。
  // 滞后条在受击后停留片刻再匀速收缩，突出单次扣血量。
  constexpr Tick kTrailHoldMs = 150;
  constexpr float kTrailChasePerSec = 2.4f;  // 约 0.42 秒排空整条
  const float trailDtSeconds = static_cast<float>(dtMs) / 1000.0f;
  surface.enemyHpBars3d.clear();
  // 遭遇敌人与野外敌人共用同一滞后条逻辑（id 无冲突，共享 trail 表）。
  const auto publishHpBar = [&](uint32_t id, Vec2 position, FixedPoint hp,
                                FixedPoint maxHp, int archetype) {
    EnemyHpBarRenderState bar;
    bar.x = position.x;
    bar.z = position.y;
    bar.ratio = static_cast<float>(hp) / static_cast<float>(maxHp);
    // 元素归属（-1=物理）：渲染层据此给元素敌人血条加元素色边框，
    // 与技能/附着/死亡爆发同源，血条填充仍按血量渐变保持可读。
    bar.element = EnemyElementFor(archetype);
    EnemyHpTrailState& trail = enemyHpTrails[id];
    if (bar.ratio >= trail.trail) {
      // 回血或满血：立即贴合，不残留滞后条。
      trail.trail = bar.ratio;
      trail.holdMs = 0;
      trail.chasing = false;
    } else {
      if (!trail.chasing && trail.holdMs <= 0) {
        // 新一次扣血：先进入停留期，让玩家看清整段损失。
        trail.holdMs = kTrailHoldMs;
      }
      if (trail.holdMs > 0) {
        trail.holdMs -= dtMs;
      } else {
        trail.chasing = true;
        trail.trail -= kTrailChasePerSec * trailDtSeconds;
        if (trail.trail <= bar.ratio) {
          trail.trail = bar.ratio;
          trail.chasing = false;
        }
      }
    }
    bar.trailRatio = trail.trail;
    surface.enemyHpBars3d.push_back(bar);
  };
  const auto publishEnemyHpBars = [&](const auto& enemies) {
    for (const auto& enemy : enemies) {
      if (!enemy.alive || enemy.maxHp <= 0) {
        enemyHpTrails.erase(enemy.id);
        continue;
      }
      publishHpBar(enemy.id, enemy.position, enemy.hp, enemy.maxHp,
                   static_cast<int>(enemy.archetype));
    }
  };
  publishEnemyHpBars(encounter.snapshot().enemies);
  publishEnemyHpBars(wildSpawn.snapshot());

  // 只从 gameplay 快照/事件投影动画意图，不反向写入战斗、AI 或玩家控制器。
  surface.playerHitAnimationSeconds = std::max(
      0.0f, surface.playerHitAnimationSeconds - dtSeconds);
  if (playerHitObserved) surface.playerHitAnimationSeconds = 0.2f;
  // 受击闪白计时器逐帧衰减并清理到期项。
  for (auto flash = surface.enemyHitFlash.begin();
       flash != surface.enemyHitFlash.end();) {
    flash->second -= dtSeconds;
    if (flash->second <= 0.0f) {
      flash = surface.enemyHitFlash.erase(flash);
    } else {
      ++flash;
    }
  }
  // 预警环脉冲时钟：按 0.8s 周期回绕，避免浮点长时间累加精度退化。
  surface.windupPulseSeconds += dtSeconds;
  if (surface.windupPulseSeconds >= 0.8f) surface.windupPulseSeconds -= 0.8f;
  // 元素附着光环时钟：呼吸周期回绕 + 附着粒子发射节奏累加。
  surface.auraPulseSeconds += dtSeconds;
  if (surface.auraPulseSeconds >= AuraRingPeriod()) {
    surface.auraPulseSeconds -= AuraRingPeriod();
  }
  surface.auraEmitSeconds += dtSeconds;
  if (surface.auraEmitSeconds >= AuraParticleInterval()) {
    surface.auraEmitSeconds -= AuraParticleInterval();
    // 为每个附着源质的存活目标各发射一颗上升元素粒子：从脚下环带
    // 外飘上升，形成"元素能量上涌"的附着指示（kind>=4 不受重力）。
    const float enemyRatio =
        VfxSizeRatio(surface.enemyAssetProfile, ModelKind::Enemy);
    const auto emitAuraParticles = [&](Vec2 position, int auraMask) {
      if (auraMask == 0 || surface.hitSparks3d.size() > 128) return;
      for (int source = 0; source < 3; ++source) {
        if ((auraMask & (1 << source)) == 0) continue;
        surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
        const float r0 =
            static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) /
            65535.0f;
        surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
        const float r1 =
            static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) /
            65535.0f;
        const float angle = r0 * 6.2831853f;
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
        AuraParticleVelocity(angle, 0.0025f * enemyRatio,
                             (0.028f + 0.012f * r1) * enemyRatio, vx, vy, vz);
        const float life = 0.5f + 0.25f * r0;
        const float spawnRadius = 0.003f * enemyRatio;
        surface.hitSparks3d.push_back(
            {position.x + std::cos(angle) * spawnRadius, 0.002f,
             position.y + std::sin(angle) * spawnRadius, vx, vy, vz, life,
             life, AuraSparkKindFor(source), enemyRatio});
      }
    };
    for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
      if (!enemy.alive) continue;
      emitAuraParticles(enemy.position,
                        AuraMaskFromFlags(enemy.radianceAttached,
                                          enemy.currentAttached,
                                          enemy.corruptionAttached));
    }
    if (surface.trainingTarget.alive) {
      emitAuraParticles({surface.trainingTarget.x, surface.trainingTarget.y},
                        surface.trainingTargetAuraMask);
    }
  }
  // 武器附魔粒子：附魔期间主角周身持续元素粒子上涌（原神武器附魔
  // 语言），与敌人附着光环上升粒子同源（AuraParticleVelocity/
  // AuraSparkKindFor），附魔状态从刀光/染色延伸到角色周身。
  surface.infusionEmitSeconds += dtSeconds;
  if (surface.infusionEmitSeconds >= AuraParticleInterval()) {
    surface.infusionEmitSeconds -= AuraParticleInterval();
    if (surface.playerSlashSource >= 0 && surface.hitSparks3d.size() <= 128) {
      const float infusionRatio =
          VfxSizeRatio(surface.playerAssetProfile, ModelKind::Player);
      surface.hitSparkSeed = surface.hitSparkSeed * 1664525u + 1013904223u;
      const float r0 =
          static_cast<float>((surface.hitSparkSeed >> 8) & 0xFFFFu) /
          65535.0f;
      const float infusionAngle = r0 * 6.2831853f;
      float ivx = 0.0f, ivy = 0.0f, ivz = 0.0f;
      AuraParticleVelocity(infusionAngle, 0.002f * infusionRatio,
                           (0.026f + 0.012f * r0) * infusionRatio, ivx, ivy,
                           ivz);
      const float infusionLife = 0.45f + 0.2f * r0;
      const float infusionSpawnRadius = 0.02f * infusionRatio;
      surface.hitSparks3d.push_back(
          {surface.player.x + std::cos(infusionAngle) * infusionSpawnRadius,
           0.04f,
           surface.player.y + std::sin(infusionAngle) * infusionSpawnRadius,
           ivx, ivy, ivz, infusionLife, infusionLife,
           AuraSparkKindFor(surface.playerSlashSource), infusionRatio});
    }
  }
  // 前摇聚能粒子：敌人/首领吟唱期间持续向自身汇聚粒子（原神蓄力
  // 语言），给玩家连续的"正在蓄力"前兆，与预警环/蓄力火花同色；
  // 首领按阶段火花 kind，敌人按原型元素 kind。
  surface.windupConvergeSeconds += dtSeconds;
  if (surface.windupConvergeSeconds >= WindupConvergeInterval()) {
    surface.windupConvergeSeconds -= WindupConvergeInterval();
    const float convergeRatio =
        VfxSizeRatio(surface.enemyAssetProfile, ModelKind::Enemy);
    for (const EncounterEnemySnapshot& enemy : encounter.snapshot().enemies) {
      if (!enemy.alive || !enemy.windingUp) continue;
      spawnConvergingSparks(surface, enemy.position,
                            EnemySkillSparkKindFor(
                                static_cast<int>(enemy.archetype)),
                            2, 0.10f * convergeRatio, convergeRatio);
    }
    for (const WildEnemySnapshot& enemy : wildSpawn.snapshot()) {
      if (!enemy.alive || !enemy.windingUp) continue;
      spawnConvergingSparks(surface, enemy.position,
                            EnemySkillSparkKindFor(enemy.archetype), 2,
                            0.10f * convergeRatio, convergeRatio);
    }
    if (surface.boss3d.active && !surface.boss3d.defeated &&
        surface.boss3d.windingUp) {
      const float bossRatio =
          VfxSizeRatio(surface.bossAssetProfile, ModelKind::Boss);
      spawnConvergingSparks(surface,
                            {surface.boss3d.x, surface.boss3d.y},
                            BossPhaseVfxFor(surface.boss3d.phase).sparkKind, 3,
                            0.16f * bossRatio, bossRatio);
    }
  }
  // 命中火花：速度积分 + 重力回落，寿命到期或落地后清理；
  // 尾迹与技能粒子（kind>=3）不受重力，保持上扬消散。
  constexpr float kSparkGravity = 0.35f;
  for (HitSpark3D& spark : surface.hitSparks3d) {
    spark.life -= dtSeconds;
    if (spark.kind <= 2) spark.vy -= kSparkGravity * dtSeconds;
    spark.x += spark.vx * dtSeconds;
    spark.y += spark.vy * dtSeconds;
    spark.z += spark.vz * dtSeconds;
  }
  surface.hitSparks3d.erase(
      std::remove_if(surface.hitSparks3d.begin(), surface.hitSparks3d.end(),
                     [](const HitSpark3D& spark) {
                       return spark.life <= 0.0f || spark.y < 0.0f;
                     }),
      surface.hitSparks3d.end());
  // 普攻刀光/技能冲击波计时推进与过期清理（时长由纯函数统一决定）。
  if (surface.playerSlashSeconds >= 0.0f) {
    // 主角武器拖尾：挥击窗口内沿刀光扫掠角每帧发射一颗金白尾迹
    // 粒子（kind=7 不受重力），形成原神式武器挥舞流光。
    const WeaponTrailPose playerTrail = WeaponTrailPoseAt(
        surface.playerSlashSeconds, surface.playerSlashCombo);
    if (playerTrail.active && surface.hitSparks3d.size() <= 128) {
      const float scale = surface.playerAssetProfile.scale;
      const float phi = surface.playerSlashYaw + playerTrail.angleRadians;
      const float radius = playerTrail.radiusFactor * scale;
      float vx = 0.0f, vy = 0.0f, vz = 0.0f;
      WeaponTrailVelocity(phi, radius * 2.5f, vx, vy, vz);
      surface.hitSparks3d.push_back(
          {surface.player.x + std::sin(phi) * radius,
           surface.playerGroundHeight + playerTrail.heightFactor * scale,
           surface.player.y + std::cos(phi) * radius, vx, vy, vz, 0.2f, 0.2f,
           WeaponTrailKindFor(surface.playerSlashSource), 1.0f});
    }
    surface.playerSlashSeconds += dtSeconds;
    if (surface.playerSlashSeconds >= SlashArcDuration()) {
      surface.playerSlashSeconds = -1.0f;
    }
  }
  for (Surface::EnemySlashArc& arc : surface.enemySlashArcs) {
    // 敌方武器拖尾：每条激活刀光每帧发射一颗红色尾迹粒子（kind=8）。
    const WeaponTrailPose enemyTrail = WeaponTrailPoseAt(arc.seconds, 0);
    if (enemyTrail.active && surface.hitSparks3d.size() <= 128) {
      const float scale = surface.enemyAssetProfile.scale * arc.scale;
      const float phi = arc.yaw + enemyTrail.angleRadians;
      const float radius = enemyTrail.radiusFactor * scale;
      float vx = 0.0f, vy = 0.0f, vz = 0.0f;
      WeaponTrailVelocity(phi, radius * 2.5f, vx, vy, vz);
      surface.hitSparks3d.push_back(
          {arc.x + std::sin(phi) * radius,
           0.02f + enemyTrail.heightFactor * scale,
           arc.y + std::cos(phi) * radius, vx, vy, vz, 0.2f, 0.2f, 8, 1.0f});
    }
    arc.seconds += dtSeconds;
  }
  surface.enemySlashArcs.erase(
      std::remove_if(surface.enemySlashArcs.begin(),
                     surface.enemySlashArcs.end(),
                     [](const Surface::EnemySlashArc& arc) {
                       return arc.seconds >= SlashArcDuration();
                     }),
      surface.enemySlashArcs.end());
  for (Surface::ShockwaveRing& ring : surface.shockwaveRings) {
    ring.seconds += dtSeconds;
  }
  surface.shockwaveRings.erase(
      std::remove_if(surface.shockwaveRings.begin(),
                     surface.shockwaveRings.end(),
                     [](const Surface::ShockwaveRing& ring) {
                       return ring.seconds >= ShockwaveDuration();
                     }),
      surface.shockwaveRings.end());
  for (Surface::ImpactDecal& decal : surface.impactDecals) {
    decal.seconds += dtSeconds;
  }
  surface.impactDecals.erase(
      std::remove_if(surface.impactDecals.begin(),
                     surface.impactDecals.end(),
                     [](const Surface::ImpactDecal& decal) {
                       return decal.seconds >= ImpactDecalDuration();
                     }),
      surface.impactDecals.end());
  for (Surface::LightPillar& pillar : surface.lightPillars) {
    pillar.seconds += dtSeconds;
  }
  surface.lightPillars.erase(
      std::remove_if(surface.lightPillars.begin(),
                     surface.lightPillars.end(),
                     [](const Surface::LightPillar& pillar) {
                       return pillar.seconds >= LightPillarDuration();
                     }),
      surface.lightPillars.end());
  for (Surface::SkillRune& rune : surface.skillRunes) {
    rune.seconds += dtSeconds;
  }
  surface.skillRunes.erase(
      std::remove_if(surface.skillRunes.begin(), surface.skillRunes.end(),
                     [](const Surface::SkillRune& rune) {
                       return rune.seconds >= SkillRuneDuration();
                     }),
      surface.skillRunes.end());
  // 共鸣 FOV 冲击计时推进：到期归位（渲染层据此恢复默认视场角）。
  if (surface.resonanceFovSeconds >= 0.0f) {
    surface.resonanceFovSeconds += dtSeconds;
    if (surface.resonanceFovSeconds >= FovPunchDuration()) {
      surface.resonanceFovSeconds = -1.0f;
    }
  }
  const CombatSnapshot& combatSnapshot = combat.snapshot();
  surface.player3dAnimation.alive = combatSnapshot.playerHp > 0;
  // 玩家血量比例：供低血量边缘脉冲警示推导强度。
  const float playerMaxHp = static_cast<float>(combat.config().trainingPlayerHp) /
                            static_cast<float>(FP_ONE);
  const float playerHp = static_cast<float>(combatSnapshot.playerHp) /
                         static_cast<float>(FP_ONE);
  surface.playerHpRatio = playerMaxHp > 0.0f
                              ? std::clamp(playerHp / playerMaxHp, 0.0f, 1.0f)
                              : 1.0f;
  // 无敌帧状态：闪避期间渲染层半透明化玩家模型。
  surface.playerInvulnerable = combatSnapshot.invulnerable;
  // 闪避残影：无敌帧窗口内逐帧采样位置，窗口结束后留短暂余韵
  //（原神闪避运动语言）。
  surface.updatePlayerGhostTrail(dtSeconds, combatSnapshot.invulnerable);
  surface.player3dAnimation.action = PlayerRenderAnimation(
      static_cast<ActionState>(combatSnapshot.currentAction),
      combatSnapshot.activeCombatAction);
  // 连段攻击 clip 差异化：斜劈/横斩/突刺/终结重劈按段数切换。
  surface.player3dAnimation.attackClip = PlayerAttackClipFor(
      PlayerComboSegmentFor(
          static_cast<ActionState>(combatSnapshot.currentAction)));
  // 跳跃/落地/滑翔动画（KayKit 跳跃语言）：空中播放 Jump_Start
  //（前 0.18s）/Jump_Idle，滑翔复用空中姿态，落地播放 0.25s
  // Jump_Land + 脚下轻尘，补全角色离地运动语言。
  const bool playerAirborneNow =
      motionState.state == MotionState::Airborne ||
      motionState.state == MotionState::Gliding;
  if (playerAirborneNow) {
    surface.playerAirSeconds += dtSeconds;
    surface.playerLandSeconds = 0.0f;
    surface.player3dAnimation.action = RenderAnimation::Jump;
    surface.player3dAnimation.attackClip =
        PlayerJumpClipFor(surface.playerAirSeconds);
  } else {
    if (surface.playerAirSeconds > 0.0f) {
      // 落地边沿：启动落地动画 + 脚下淡蓝轻尘（移动语言同源）。
      surface.playerLandSeconds = 0.25f;
      spawnHitSparks(surface, {surface.player.x, surface.player.y}, 3, 4,
                     0.6f, 0.8f, 0.7f);
    }
    surface.playerAirSeconds = 0.0f;
    surface.playerLandSeconds =
        std::max(0.0f, surface.playerLandSeconds - dtSeconds);
    if (surface.playerLandSeconds > 0.0f) {
      surface.player3dAnimation.action = RenderAnimation::Land;
      surface.player3dAnimation.attackClip.clear();
    }
  }
  // 终结技暗场聚焦：吟唱中累加（0.3 封顶），结束后双倍速回落，
  // 渲染层按 UltimateDimAlphaFor 压暗全屏突出爆发（原神爆发演出）。
  if (combatSnapshot.currentAction ==
      static_cast<uint8_t>(ActionState::CastingUltimate)) {
    surface.ultimateDimSeconds =
        std::min(surface.ultimateDimSeconds + dtSeconds, 0.3f);
  } else {
    surface.ultimateDimSeconds =
        std::max(surface.ultimateDimSeconds - dtSeconds * 2.0f, 0.0f);
  }
  surface.player3dAnimation.hit = surface.playerHitAnimationSeconds > 0.0f;
  surface.player3dAnimation.moving = surface.player.moving;
  // 摇杆幅度驱动跑动步频缩放：地面移速与输入幅度成正比，
  // 动画步频同比缩放才能消除半推摇杆时的滑步。
  surface.player3dAnimation.moveRatio =
      std::clamp(intent.move.length(), 0.0f, 1.0f);
  surface.trainingTarget3dAnimation.alive = surface.trainingTarget.alive;
  // 野外敌人先发布：publish3DEncounterState 的共享状态表清理依赖它已填充。
  publishWildEnemies3d(surface, wildSpawn, dtSeconds);
  // NPC 发布侧按 lodLevel 收缩同屏上限（Phase 5）：6→4→3。
  publishNpcs3d(surface, npcAgency, surface.player.x, surface.player.y,
                npcVisibleLimitForPerf(performanceGuard.lodLevel()));
  publish3DEncounterState(surface, encounter.snapshot(), dtSeconds);
  // 敌方释放动效：依赖 publish 后的 enemies3d/boss3d 状态做边沿检测。
  // 首领出场/转阶段镜头语言：更重的卡肉 + FOV 冲击（与元素反应同源）。
  const EnemyReleaseVfxResult releaseVfx = spawnEnemyReleaseVfx(surface);
  if (releaseVfx.bossCameraFeedback) {
    hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 80, 96);
    surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
    surface.resonanceFovSeconds = 0.0f;
    // 首领仪式时刻相机震动（出场/转阶段/死亡爆发同源）：与首领
    // 砸地同级幅度，补全首领大时刻的镜头位移维度。
    vfxSystem.triggerCameraShake(2 * FP_ONE);
  }
  // 首领挥击落地相机震动：与受击震动同源通道，幅度加倍突出体量。
  if (releaseVfx.bossSlamLanded) {
    vfxSystem.triggerCameraShake(2 * FP_ONE);
  }
  // 终锻打断边沿：吟唱中被共鸣终结技打断（机制→None 且非超时
  // 失败）时爆发吟唱条碎裂高光——亮金火花 + 冲击波 + 光柱 + 符阵
  // + 重镜头反馈，给玩家"成功打断"操作一个高光时刻；超时失败
  // （failedMechanic）不触发。
  const BossSnapshot& bossInterruptSnap = encounter.snapshot().boss;
  const bool finalForgeCasting =
      bossInterruptSnap.mechanic == BossMechanic::FinalForge &&
      bossInterruptSnap.castRemainingMs > 0;
  if (prevFinalForgeCasting && !finalForgeCasting &&
      bossInterruptSnap.mechanic == BossMechanic::None &&
      !bossInterruptSnap.failedMechanic && !bossInterruptSnap.defeated) {
    const float interruptRatio =
        VfxSizeRatio(surface.bossAssetProfile, ModelKind::Boss);
    const Vec2 interruptPos{surface.boss3d.x, surface.boss3d.y};
    spawnHitSparks(surface, interruptPos, 2, 24, 2.2f, 1.6f,
                   interruptRatio * 1.2f);
    spawnShockwave(surface, interruptPos, glm::vec3{1.0f, 0.92f, 0.62f},
                   0.20f * interruptRatio);
    spawnLightPillar(surface, interruptPos, glm::vec3{1.0f, 0.92f, 0.62f},
                     0.16f * interruptRatio);
    spawnSkillRune(surface, interruptPos, glm::vec3{1.0f, 0.92f, 0.62f},
                   0.12f * interruptRatio);
    hitStopRemainingMs = std::min<int64_t>(hitStopRemainingMs + 80, 96);
    surface.fovPunchMaxOffset = FovPunchMaxOffsetFor(2);
    surface.resonanceFovSeconds = 0.0f;
    vfxSystem.triggerCameraShake(2 * FP_ONE);
    // 打断音效：噪声碎裂（此前仅事件映射存在、无触发源）。
    audioBridge.playUiSound(SoundEffect::CastBarBroken);
  }
  prevFinalForgeCasting = finalForgeCasting;

  GameSnapshot updated = snapshots.read();
  ApplyCombatSnapshot(updated, combat.snapshot());
  updated.encounterMode = static_cast<int32_t>(encounter.snapshot().mode);
  updated.encounterState = static_cast<int32_t>(encounter.snapshot().state);
  updated.levelStage = static_cast<int32_t>(encounter.snapshot().levelStage);
  updated.gateState = static_cast<int32_t>(encounter.snapshot().gateState);
  updated.supplyState = static_cast<int32_t>(encounter.snapshot().supplyState);
  updated.bossHp = encounter.snapshot().boss.hp;
  updated.bossPoise = encounter.snapshot().boss.poise;
  updated.bossPhase = static_cast<int32_t>(encounter.snapshot().boss.phase);
  updated.bossMechanic = static_cast<int32_t>(encounter.snapshot().boss.mechanic);
  updated.bossCastMs = encounter.snapshot().boss.castRemainingMs;
  updated.perfLevel = performanceGuard.level();
  updated.vfxFlags = vfxSystem.snapshot().vfxFlags;
  updated.cameraShakeX = vfxSystem.snapshot().cameraShakeX;
  updated.cameraShakeY = vfxSystem.snapshot().cameraShakeY;
  surface.vfxFlags = vfxSystem.snapshot().vfxFlags;
  surface.vfxHitFlash = static_cast<float>(vfxSystem.snapshot().hitFlashMs) / 200.0f;
  surface.vfxDodgeFlash = static_cast<float>(vfxSystem.snapshot().dodgeFlashMs) / 400.0f;
  surface.vfxResonanceBurst = static_cast<float>(vfxSystem.snapshot().resonanceBurstMs) / 800.0f;
  surface.vfxCameraShakeX = vfxSystem.snapshot().cameraShakeX;
  surface.vfxCameraShakeY = vfxSystem.snapshot().cameraShakeY;
  updated.bossHpRatio = BossCinematicState::healthRatio(
      encounter.snapshot().boss.hp, BossConfig::karounDefaults().maxHp);
  if (encounter.snapshot().boss.castRemainingMs > 0) {
    updated.bossCastRatio = 1.0f - static_cast<float>(encounter.snapshot().boss.castRemainingMs) /
                                  static_cast<float>(5000);
  }
  updated.debugHud = debugHud_;
  switch (demoDirector.phase()) {
    case DemoPhase::Intro:
      updated.objectiveLabel = "进入失落遗迹";
      break;
    case DemoPhase::Explore:
      updated.objectiveLabel = "前往共鸣祭坛";
      break;
    case DemoPhase::Encounter:
      updated.objectiveLabel = "清除遗迹守卫";
      break;
    case DemoPhase::Resonance:
      updated.objectiveLabel = "激活三源共鸣";
      break;
    case DemoPhase::BossIntro:
      updated.objectiveLabel = "共鸣核心正在苏醒";
      break;
    case DemoPhase::BossFight:
      updated.objectiveLabel = "击破共鸣核心";
      break;
    case DemoPhase::Outro:
      updated.objectiveLabel = "遗迹共鸣已完成";
      break;
  }
  updated.showDebugHud = debugHud_;
  updated.inputEventCount = inputEventCount_;
  const BossCinematicState cinematic = BossCinematicState::fromBossHp(
      updated.bossHpRatio);
  const BossCinematicState& introCinematic = demoDirector.bossCinematic();
  updated.bossCinematicProgress = introCinematic.ringProgress;
  updated.bossShardCount = introCinematic.shardCount;
  updated.bossSourceColor = static_cast<uint8_t>(introCinematic.sourceColor);
  updated.bossRingBroken = cinematic.broken;
  surface.boss3d.cinematicProgress = updated.bossCinematicProgress;
  surface.boss3d.shardCount = updated.bossShardCount;
  surface.boss3d.sourceColor = updated.bossSourceColor;
  surface.boss3d.ringBroken = updated.bossRingBroken;
  snapshots.publish(updated);
}

void Loop::publishRendererStopped() {
  currentTarget.reset();
  encounter.stop();
  combat.reset();
  combatTimeMs_ = 0;
  {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    frameCombatEvents_ = {};
  }
  surface.trainingTarget.alive = true;
  surface.trainingTargetAuraMask = 0;
  GameSnapshot stopped = RendererStoppedSnapshot(snapshots.read());
  ApplyCombatSnapshot(stopped, combat.snapshot());
  snapshots.publish(stopped);
}
