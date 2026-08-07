#include "gameplay/ai/enemy_archetypes.h"
#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>
#include <type_traits>
#include <unordered_set>

namespace {

// 能力冷却序列合法性：冷却为正、各阶段非负、射程为正、权重为正。
void assertAbilityTimingLegal(const EnemyAbility& ability) {
  assert(ability.id != 0);
  assert(!ability.tag.empty());
  assert(ability.range > 0);
  assert(ability.weight > 0);
  assert(ability.cooldownMs > 0);
  assert(ability.windupMs >= 0);
  assert(ability.activeMs >= 0);
  assert(ability.recoveryMs >= 0);
  assert(ability.cooldownMs >= ability.windupMs + ability.activeMs);
}

void testEnumValuesAreFixed() {
  // 数值防重排：渲染层与 world_layout.gen.h SpawnArchetype 按数值对应。
  static_assert(static_cast<unsigned>(EnemyArchetype::RiftClaw) == 0U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Priest) == 1U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Guard) == 2U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Bruiser) == 3U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Caster) == 4U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Elite) == 5U);
}

void testBruiserDefaults() {
  const EnemyAiConfig config = bruiserDefaults();
  assert(config.validated().has_value());
  assert(config.abilities.size() == 1);
  const EnemyAbility& crush = config.abilities.front();
  assert(crush.id == enemy_ability_ids::kBruiserCrush);
  assert(crush.category == EnemyAbilityCategory::Attack);
  assert(crush.targetPolicy == EnemyTargetPolicy::CurrentTarget);
  assert(crush.effect == EnemyAbilityEffect::Damage);
  // 重甲霸体：不可打断，前摇给闪避窗口。
  assert(crush.cancelPolicy == EnemyAbilityCancelPolicy::Uninterruptible);
  assert(crush.interruptThreshold == 0);
  assert(crush.windupMs > 0);
  assertAbilityTimingLegal(crush);
  // 慢速重击：节奏比守卫更慢。
  assert(crush.cooldownMs > 2200);
  assert(config.staggerRecoveryMs > 0);
}

void testCasterDefaults() {
  const EnemyAiConfig config = casterDefaults();
  assert(config.validated().has_value());
  assert(config.abilities.size() == 1);
  const EnemyAbility& bolt = config.abilities.front();
  assert(bolt.id == enemy_ability_ids::kCasterArcaneBolt);
  assert(bolt.category == EnemyAbilityCategory::Attack);
  assert(bolt.effect == EnemyAbilityEffect::Damage);
  // 远程投射：射程比祭司法弹更远，且前摇可被打断。
  assert(bolt.range > fp(4.0));
  assert(bolt.cancelPolicy == EnemyAbilityCancelPolicy::WindupOnly);
  assert(bolt.interruptThreshold > 0);
  assert(bolt.telegraph == EnemyAbilityTelegraph::WarningYellow);
  assertAbilityTimingLegal(bolt);
}

void testEliteDefaults() {
  const EnemyAiConfig config = eliteDefaults();
  assert(config.validated().has_value());
  // 精英：多能力（近战横扫 + 范围冲击波）。
  assert(config.abilities.size() == 2);
  std::unordered_set<EnemyAbilityId> ids;
  const EnemyAbility* cleave = nullptr;
  const EnemyAbility* shockwave = nullptr;
  for (const EnemyAbility& ability : config.abilities) {
    assert(ids.insert(ability.id).second);
    assert(ability.category == EnemyAbilityCategory::Attack);
    assertAbilityTimingLegal(ability);
    if (ability.id == enemy_ability_ids::kEliteCleave) cleave = &ability;
    if (ability.id == enemy_ability_ids::kEliteShockwave) shockwave = &ability;
  }
  assert(cleave != nullptr && shockwave != nullptr);
  // 霸体窗口：横扫不可打断；冲击波为可打断预警技。
  assert(cleave->cancelPolicy == EnemyAbilityCancelPolicy::Uninterruptible);
  assert(cleave->interruptThreshold == 0);
  assert(shockwave->cancelPolicy == EnemyAbilityCancelPolicy::WindupOnly);
  assert(shockwave->interruptThreshold > 0);
  assert(shockwave->effect == EnemyAbilityEffect::AreaDamage);
  assert(shockwave->telegraph == EnemyAbilityTelegraph::WarningYellow);
  assert(config.staggerRecoveryMs > 0);
}

EncounterEnemyConfig spawnEnemy(EntityId id, EnemyArchetype archetype,
                                Vec2 position) {
  EncounterEnemyConfig enemy;
  enemy.id = id;
  enemy.archetype = archetype;
  enemy.position = position;
  enemy.safeReturnPosition = position;
  // 手动构造的配置自带 hp/poise（validConfig 要求为正）；
  // EncounterController 原样透传到快照。
  return enemy;
}

void testNewArchetypesSpawnWithPositiveStats() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  EncounterConfig config;
  config.mode = EncounterMode::Mixed;
  config.enemies = {
      spawnEnemy(5001, EnemyArchetype::RiftClaw, {0.40f, 0.7f}),
      spawnEnemy(5002, EnemyArchetype::Priest, {0.43f, 0.7f}),
      spawnEnemy(5003, EnemyArchetype::Guard, {0.46f, 0.7f}),
      spawnEnemy(5004, EnemyArchetype::Bruiser, {0.49f, 0.7f}),
      spawnEnemy(5005, EnemyArchetype::Caster, {0.52f, 0.7f}),
      spawnEnemy(5006, EnemyArchetype::Elite, {0.55f, 0.7f}),
  };
  assert(encounter.start(config));
  const EncounterSnapshot& snapshot = encounter.snapshot();
  assert(snapshot.enemies.size() == 6);

  FixedPoint hp[6] = {};
  FixedPoint poise[6] = {};
  for (const EncounterEnemySnapshot& enemy : snapshot.enemies) {
    const auto index =
        static_cast<std::underlying_type_t<EnemyArchetype>>(enemy.archetype);
    assert(index < 6);
    hp[index] = enemy.maxHp;
    poise[index] = enemy.poise;
    assert(enemy.alive);
    assert(enemy.hp > 0);
    assert(enemy.poise > 0);
    assert(enemy.maxHp > 0);
    // 配置数值原样透传：新原型可正常进入战斗并存活。
    assert(enemy.maxHp == fp(300));
    assert(enemy.poise == fp(100));
  }
  // 六个原型全部落场（含新增的 Bruiser/Caster/Elite）。
  for (int i = 0; i < 6; ++i) {
    assert(hp[i] > 0);
    assert(poise[i] > 0);
  }
}

void testCapacityAllowsEightEnemies() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  EncounterConfig config;
  config.mode = EncounterMode::Mixed;
  config.maxEnemies = EnemyAiConfig::kMaxEnemies;
  assert(EnemyAiConfig::kMaxEnemies == 8);
  const EnemyArchetype archetypes[8] = {
      EnemyArchetype::RiftClaw, EnemyArchetype::Priest, EnemyArchetype::Guard,
      EnemyArchetype::Bruiser,  EnemyArchetype::Caster, EnemyArchetype::Elite,
      EnemyArchetype::RiftClaw, EnemyArchetype::Caster,
  };
  for (std::size_t i = 0; i < 8; ++i) {
    const float x = 0.4f + static_cast<float>(i) * 0.025f;
    config.enemies.push_back(
        spawnEnemy(static_cast<EntityId>(6001 + i), archetypes[i],
                   {x, 0.7f}));
  }
  assert(encounter.start(config));
  assert(encounter.snapshot().enemies.size() == 8);

  // 超出容量上限仍被拒绝。
  config.enemies.push_back(
      spawnEnemy(6009, EnemyArchetype::RiftClaw, {0.6f, 0.72f}));
  assert(!encounter.start(config));
}

}  // namespace

int main() {
  testEnumValuesAreFixed();
  testBruiserDefaults();
  testCasterDefaults();
  testEliteDefaults();
  testNewArchetypesSpawnWithPositiveStats();
  testCapacityAllowsEightEnemies();
}
