#include "encounter_controller.h"

#include "combat_region.h"
#include "enemy_agent.h"
#include "enemy_archetypes.h"
#include "gameplay/entities/enemy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace {

bool validMode(EncounterMode mode) {
  switch (mode) {
    case EncounterMode::Training:
    case EncounterMode::Beast:
    case EncounterMode::Mixed:
    case EncounterMode::Guard:
    case EncounterMode::LevelFlow:
    case EncounterMode::Boss:
      return true;
  }
  return false;
}

bool validArchetype(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw:
    case EnemyArchetype::Priest:
    case EnemyArchetype::Guard:
      return true;
  }
  return false;
}

EncounterEnemyConfig enemyConfig(EntityId id, EnemyArchetype archetype,
                                  Vec2 position) {
  return {id, archetype, position, {0.5f, 0.5f}, fp(300), fp(100)};
}

CombatConfig targetConfig(const EncounterEnemyConfig& enemy) {
  CombatConfig config = CombatConfig::defaults();
  config.trainingTargetHp = enemy.hp;
  config.trainingTargetPoise = enemy.poise;
  return config;
}

EnemyAiConfig aiConfig(EnemyArchetype archetype,
                       const CombatRegionConfig& region) {
  EnemyAiConfig config;
  switch (archetype) {
    case EnemyArchetype::RiftClaw:
      config = riftClawDefaults();
      break;
    case EnemyArchetype::Priest:
      config = radiantPriestDefaults();
      break;
    case EnemyArchetype::Guard:
      config = corrosionGuardDefaults();
      break;
  }
  config.region = region;
  return config;
}

uint64_t nextStableSequence(uint64_t& next) {
  const uint64_t value = next;
  if (next != std::numeric_limits<uint64_t>::max()) ++next;
  return value;
}

Vec2 advancePosition(Vec2 position, Vec2 movement, int64_t dtMs,
                     const CombatRegion& region) {
  if (!position.finite() || !movement.finite() || dtMs <= 0) return position;
  const float distance = movement.length();
  if (!std::isfinite(distance) || distance <= 0.0f) return position;
  constexpr float kMovementPerMillisecond = 0.001f;
  const float step = std::min(distance, kMovementPerMillisecond * dtMs);
  return region.projectInside(position + movement * (step / distance));
}

bool effectLess(const CombatEffectRequest& left,
                const CombatEffectRequest& right) {
  if (left.tick != right.tick) return left.tick < right.tick;
  if (left.target != right.target) return left.target < right.target;
  if (left.sequence != right.sequence) return left.sequence < right.sequence;
  return left.transactionId < right.transactionId;
}

bool hitLess(const HitRequest& left, const HitRequest& right) {
  if (left.tick != right.tick) return left.tick < right.tick;
  if (left.attacker != right.attacker) return left.attacker < right.attacker;
  if (left.target != right.target) return left.target < right.target;
  if (left.sequence != right.sequence) return left.sequence < right.sequence;
  return left.transactionId < right.transactionId;
}

}  // namespace

struct EncounterController::EnemySlot {
  EnemySlot(const EncounterEnemyConfig& source,
            const CombatRegionConfig& region)
      : target(targetConfig(source)),
        agent(source.archetype, aiConfig(source.archetype, region)) {
    enemy.id = source.id;
    enemy.hp = source.hp;
    enemy.poise = source.poise;
    enemy.archetype = source.archetype;
    enemy.position = source.position;
    enemy.spawnPosition = source.position;
    enemy.safeReturnPosition = source.safeReturnPosition;
  }

  Enemy enemy;
  TrainingTarget target;
  EnemyAgent agent;
  Vec2 facing = {0.0f, -1.0f};
  EnemyActionPhase phase = EnemyActionPhase::None;
};

EncounterConfig EncounterConfig::forMode(EncounterMode mode) {
  EncounterConfig config;
  config.mode = mode;
  switch (mode) {
    case EncounterMode::Training:
      break;
    case EncounterMode::Beast:
      config.enemies = {
          enemyConfig(EncounterController::kRiftEnemyId,
                      EnemyArchetype::RiftClaw, {0.5f, 0.7f})};
      break;
    case EncounterMode::Mixed:
      config.enemies = {
          enemyConfig(EncounterController::kRiftEnemyId,
                      EnemyArchetype::RiftClaw, {0.46f, 0.7f}),
          enemyConfig(EncounterController::kPriestEnemyId,
                      EnemyArchetype::Priest, {0.54f, 0.7f}),
          enemyConfig(EncounterController::kGuardEnemyId,
                      EnemyArchetype::Guard, {0.5f, 0.72f})};
      break;
    case EncounterMode::Guard:
      config.enemies = {
          enemyConfig(EncounterController::kGuardEnemyId,
                      EnemyArchetype::Guard, {0.5f, 0.7f})};
      break;
    case EncounterMode::LevelFlow:
    case EncounterMode::Boss:
      break;
  }
  return config;
}

bool EncounterEnemySnapshot::operator==(
    const EncounterEnemySnapshot& other) const {
  return id == other.id && archetype == other.archetype &&
         position == other.position && hp == other.hp &&
         poise == other.poise && shield == other.shield &&
         alive == other.alive && radianceAttached == other.radianceAttached &&
         currentAttached == other.currentAttached &&
         corruptionAttached == other.corruptionAttached &&
         corroded == other.corroded;
}

bool EncounterSnapshot::operator==(const EncounterSnapshot& other) const {
  return mode == other.mode && state == other.state && victory == other.victory &&
         playerHp == other.playerHp && levelStage == other.levelStage &&
         gateState == other.gateState && supplyState == other.supplyState &&
         enemies == other.enemies &&
         candidates == other.candidates;
}

EncounterController::EncounterController(CombatController& combat)
    : combat_(combat) {}

EncounterController::~EncounterController() = default;

bool EncounterController::validConfig(const EncounterConfig& config) {
  if (!validMode(config.mode) || config.maxEnemies == 0 ||
      config.maxEnemies > EnemyAiConfig::kMaxEnemies ||
      !config.region.center.finite() || !std::isfinite(config.region.radius) ||
      config.region.radius <= 0.0f ||
      config.enemies.size() > config.maxEnemies) {
    return false;
  }
  const bool allowsEmptyEnemies =
      config.mode == EncounterMode::Training ||
      config.mode == EncounterMode::LevelFlow ||
      config.mode == EncounterMode::Boss;
  if (allowsEmptyEnemies != config.enemies.empty()) {
    return false;
  }

  std::unordered_set<EntityId> ids;
  for (const EncounterEnemyConfig& enemy : config.enemies) {
    if (enemy.id == 0 || enemy.id == CombatController::kPlayerId ||
        enemy.id == CombatController::kTrainingTargetId ||
        enemy.id > static_cast<EntityId>(std::numeric_limits<int32_t>::max()) ||
        !ids.insert(enemy.id).second || !validArchetype(enemy.archetype) ||
        !enemy.position.finite() || !enemy.safeReturnPosition.finite() ||
        enemy.hp <= 0 || enemy.poise <= 0) {
      return false;
    }
  }
  return true;
}

bool EncounterController::start(EncounterMode mode) {
  if (!validMode(mode)) return false;
  if (mode == EncounterMode::LevelFlow) {
    if (!start(EncounterMode::Training)) return false;
    levelFlowActive_ = true;
    snapshot_.mode = EncounterMode::LevelFlow;
    snapshot_.levelStage = LevelStage::Training;
    snapshot_.gateState = GateState::Closed;
    snapshot_.supplyState = SupplyState::Unavailable;
    return true;
  }
  return start(EncounterConfig::forMode(mode));
}

bool EncounterController::start(const EncounterConfig& config) {
  if (!validConfig(config)) return false;

  std::vector<std::unique_ptr<EnemySlot>> replacement;
  replacement.reserve(config.enemies.size());
  for (const EncounterEnemyConfig& enemy : config.enemies) {
    replacement.push_back(std::make_unique<EnemySlot>(enemy, config.region));
  }
  std::sort(replacement.begin(), replacement.end(),
            [](const std::unique_ptr<EnemySlot>& left,
               const std::unique_ptr<EnemySlot>& right) {
              return left->enemy.id < right->enemy.id;
            });

  combat_.reset();
  config_ = config;
  enemies_ = std::move(replacement);
  events_ = {};
  lastTick_ = 0;
  nextSequence_ = 1;
  snapshot_ = {};
  levelFlowActive_ = false;
  snapshot_.mode = config.mode;
  snapshot_.state = EncounterState::Running;
  refreshSnapshot(true);
  return true;
}

void EncounterController::reset() {
  for (const std::unique_ptr<EnemySlot>& slot : enemies_) slot->agent.reset();
  enemies_.clear();
  config_ = {};
  snapshot_ = {};
  events_ = {};
  lastTick_ = 0;
  nextSequence_ = 1;
  levelFlowActive_ = false;
  combat_.reset();
}

void EncounterController::stop() {
  events_ = {};
  for (const std::unique_ptr<EnemySlot>& slot : enemies_) slot->agent.reset();
  snapshot_.state = EncounterState::Stopped;
  refreshSnapshot(false);
}

void EncounterController::update(const EncounterFrameInput& input) {
  events_ = {};
  if (snapshot_.state != EncounterState::Running) return;

  const Tick tick = std::max(lastTick_, input.tick);
  const int64_t dtMs = std::max<int64_t>(0, input.dtMs);
  lastTick_ = tick;
  if (config_.mode == EncounterMode::Training) {
    combat_.update({tick, dtMs, input.playerMoving, input.targetId,
                    input.targetId == CombatController::kTrainingTargetId &&
                        combat_.snapshot().targetAlive});
    events_.combat = combat_.events();
    if (levelFlowActive_ && !combat_.snapshot().targetAlive) {
      snapshot_.state = EncounterState::Victory;
      snapshot_.victory = true;
      snapshot_.gateState = GateState::Open;
    }
    refreshSnapshot(true);
    return;
  }

  for (const std::unique_ptr<EnemySlot>& slot : enemies_) {
    if (slot->target.alive()) slot->target.advance(tick);
    slot->enemy.hp = slot->target.hp();
    slot->enemy.poise = slot->target.poise();
  }

  EnemySlot* selected = nullptr;
  for (const std::unique_ptr<EnemySlot>& slot : enemies_) {
    if (slot->enemy.id == input.targetId && slot->target.alive()) {
      selected = slot.get();
      break;
    }
  }

  CombatTargetBinding binding;
  if (selected != nullptr) {
    binding.id = selected->enemy.id;
    binding.target = &selected->target;
    binding.shield = &selected->enemy.shield;
    binding.damageContext.attackerPosition = input.playerPosition;
    binding.damageContext.defenderPosition = selected->enemy.position;
    binding.damageContext.defenderFacing = selected->facing;
    if (selected->enemy.archetype == EnemyArchetype::Guard) {
      binding.damageContext.directionalDefense = corrosionGuardDefense();
    }
  }
  combat_.updateEnemy(
      {tick, dtMs, input.playerMoving, input.targetId, selected != nullptr},
      binding);
  if (selected != nullptr) {
    selected->enemy.hp = selected->target.hp();
    selected->enemy.poise = selected->target.poise();
  }

  std::vector<HitRequest> hits;
  std::vector<CombatEffectRequest> effects;
  std::vector<std::pair<EnemySlot*, EnemyUpdateResult>> results;
  results.reserve(enemies_.size());
  const CombatRegion region(config_.region);
  for (const std::unique_ptr<EnemySlot>& slot : enemies_) {
    EnemyWorldView world;
    world.tick = tick;
    world.selfId = slot->enemy.id;
    world.selfAlive = slot->target.alive();
    world.selfPosition = slot->enemy.position;
    world.selfFacing = slot->facing;
    world.spawnPosition = slot->enemy.spawnPosition;
    world.safeReturnPosition = slot->enemy.safeReturnPosition;
    world.region = config_.region;
    world.playerId = CombatController::kPlayerId;
    world.playerPosition = input.playerPosition;
    world.playerVisible = combat_.snapshot().playerHp > 0;
    world.playerReachable = true;
    world.poise = slot->target.poise();
    world.staggered = slot->target.poiseBroken(tick);
    world.actionPhase = slot->phase;
    for (const std::unique_ptr<EnemySlot>& ally : enemies_) {
      if (ally.get() == slot.get()) continue;
      world.allies.push_back(
          {ally->enemy.id, ally->enemy.archetype, ally->target.hp(),
           ally->enemy.shield, ally->enemy.position, ally->target.alive(),
           region.contains(ally->enemy.position)});
    }

    EnemyExecutionContext execution;
    execution.targetAlive = combat_.snapshot().playerHp > 0;
    execution.baseDamage = fp(10);
    execution.poiseDamage = fp(5);
    execution.sequence = nextStableSequence(nextSequence_);
    EnemyUpdateResult result =
        slot->agent.update({world, dtMs, execution, std::nullopt});
    slot->phase = result.phase;
    if (result.hit.has_value()) hits.push_back(*result.hit);
    if (result.effect.has_value()) effects.push_back(*result.effect);
    results.emplace_back(slot.get(), std::move(result));
  }

  std::sort(effects.begin(), effects.end(), effectLess);
  for (const CombatEffectRequest& effect : effects) {
    if (effect.type != CombatEffectType::Shield || effect.amount <= 0) continue;
    const auto target = std::find_if(
        enemies_.begin(), enemies_.end(),
        [&effect](const std::unique_ptr<EnemySlot>& slot) {
          return slot->enemy.id == effect.target && slot->target.alive();
        });
    if (target == enemies_.end()) continue;
    const FixedPoint maximum = std::numeric_limits<FixedPoint>::max();
    (*target)->enemy.shield =
        (*target)->enemy.shield > maximum - effect.amount
            ? maximum
            : (*target)->enemy.shield + effect.amount;
    events_.effects.push_back(effect);
  }

  std::sort(hits.begin(), hits.end(), hitLess);
  for (const HitRequest& hit : hits) combat_.applyEnemyHit(hit);

  for (auto& [slot, result] : results) {
    if (!slot->target.alive()) continue;
    const Vec2 previous = slot->enemy.position;
    slot->enemy.position =
        advancePosition(previous, result.movement, dtMs, region);
    const Vec2 facing = input.playerPosition - slot->enemy.position;
    const float length = facing.length();
    if (facing.finite() && std::isfinite(length) && length > 0.0f) {
      slot->facing = facing * (1.0f / length);
    }
  }

  events_.combat = combat_.events();
  const bool anyAlive = std::any_of(
      enemies_.begin(), enemies_.end(),
      [](const std::unique_ptr<EnemySlot>& slot) {
        return slot->target.alive();
      });
  if (!anyAlive) {
    snapshot_.state = EncounterState::Victory;
    snapshot_.victory = true;
    if (levelFlowActive_) snapshot_.gateState = GateState::Open;
  }
  refreshSnapshot(true);
}

void EncounterController::refreshSnapshot(bool includeCandidates) {
  snapshot_.mode = levelFlowActive_ ? EncounterMode::LevelFlow : config_.mode;
  snapshot_.playerHp = combat_.snapshot().playerHp;
  snapshot_.enemies.clear();
  snapshot_.candidates.clear();
  snapshot_.enemies.reserve(enemies_.size());
  snapshot_.candidates.reserve(enemies_.size());

  if (config_.mode == EncounterMode::Training) {
    if (includeCandidates && snapshot_.state == EncounterState::Running &&
        combat_.snapshot().targetAlive) {
      snapshot_.candidates.push_back(
          {static_cast<int32_t>(CombatController::kTrainingTargetId),
           {0.5f, 0.8f}});
    }
    return;
  }

  for (const std::unique_ptr<EnemySlot>& slot : enemies_) {
    EncounterEnemySnapshot enemy;
    enemy.id = slot->enemy.id;
    enemy.archetype = slot->enemy.archetype;
    enemy.position = slot->enemy.position;
    enemy.hp = slot->target.hp();
    enemy.poise = slot->target.poise();
    enemy.shield = slot->enemy.shield;
    enemy.alive = slot->target.alive();
    for (const SourceAura& aura : slot->target.sourceAuras().active()) {
      if (aura.type == SourceType::Radiance) enemy.radianceAttached = true;
      if (aura.type == SourceType::Current) enemy.currentAttached = true;
      if (aura.type == SourceType::Corruption) enemy.corruptionAttached = true;
    }
    enemy.corroded = slot->target.corroded();
    snapshot_.enemies.push_back(enemy);
    if (includeCandidates && snapshot_.state == EncounterState::Running &&
        enemy.alive) {
      snapshot_.candidates.push_back(
          {static_cast<int32_t>(enemy.id), enemy.position});
    }
  }
}

bool EncounterController::advanceLevel() {
  if (!levelFlowActive_ || snapshot_.gateState != GateState::Open) return false;

  const LevelStage current = snapshot_.levelStage;
  LevelStage next = current;
  EncounterMode nextMode = EncounterMode::Training;
  switch (current) {
    case LevelStage::Training:
      next = LevelStage::RiftClawFight;
      nextMode = EncounterMode::Beast;
      break;
    case LevelStage::RiftClawFight:
      next = LevelStage::PriestMixedFight;
      nextMode = EncounterMode::Mixed;
      break;
    case LevelStage::PriestMixedFight:
      next = LevelStage::GuardElite;
      nextMode = EncounterMode::Guard;
      break;
    case LevelStage::GuardElite:
      combat_.reset();
      enemies_.clear();
      config_ = EncounterConfig::forMode(EncounterMode::Boss);
      snapshot_ = {};
      levelFlowActive_ = true;
      snapshot_.mode = EncounterMode::LevelFlow;
      snapshot_.state = EncounterState::Running;
      snapshot_.levelStage = LevelStage::Supply;
      snapshot_.gateState = GateState::Open;
      snapshot_.supplyState = SupplyState::Available;
      return true;
    case LevelStage::Supply:
      next = LevelStage::Boss;
      nextMode = EncounterMode::Boss;
      break;
    case LevelStage::Boss:
      return false;
  }

  if (!start(nextMode)) return false;
  levelFlowActive_ = true;
  snapshot_.mode = EncounterMode::LevelFlow;
  snapshot_.levelStage = next;
  snapshot_.gateState = GateState::Closed;
  snapshot_.supplyState = SupplyState::Unavailable;
  return true;
}

bool EncounterController::useSupply() {
  if (!levelFlowActive_ || snapshot_.levelStage != LevelStage::Supply ||
      snapshot_.supplyState != SupplyState::Available) {
    return false;
  }
  combat_.reset();
  snapshot_.playerHp = combat_.snapshot().playerHp;
  snapshot_.supplyState = SupplyState::Consumed;
  return true;
}
