#pragma once

#include "enemy_ai_config.h"
#include "gameplay/combat/combat_controller.h"
#include "gameplay/targeting/soft_targeting.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum class EncounterMode : uint8_t {
  Training,
  Beast,
  Mixed,
  Guard,
  LevelFlow,
  Boss,
};

enum class EncounterState : uint8_t {
  Stopped,
  Running,
  Victory,
};

enum class LevelStage : uint8_t {
  Training,
  RiftClawFight,
  PriestMixedFight,
  GuardElite,
  Supply,
  Boss,
};

enum class GateState : uint8_t { Closed, Open };

enum class SupplyState : uint8_t { Unavailable, Available, Consumed };

struct EncounterEnemyConfig {
  EntityId id = 0;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  Vec2 position;
  Vec2 safeReturnPosition;
  FixedPoint hp = fp(300);
  FixedPoint poise = fp(100);
};

struct EncounterConfig {
  EncounterMode mode = EncounterMode::Training;
  std::size_t maxEnemies = EnemyAiConfig::kMaxEnemies;
  CombatRegionConfig region{{0.5f, 0.5f}, 2.0f};
  std::vector<EncounterEnemyConfig> enemies;

  static EncounterConfig forMode(EncounterMode mode);
};

struct EncounterFrameInput {
  Tick tick = 0;
  int64_t dtMs = 0;
  Vec2 playerPosition;
  bool playerMoving = false;
  EntityId targetId = 0;
};

struct EncounterEnemySnapshot {
  EntityId id = 0;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  Vec2 position;
  FixedPoint hp = 0;
  FixedPoint poise = 0;
  FixedPoint shield = 0;
  bool alive = false;
  bool radianceAttached = false;
  bool currentAttached = false;
  bool corruptionAttached = false;
  bool corroded = false;

  bool operator==(const EncounterEnemySnapshot& other) const;
};

struct EncounterSnapshot {
  EncounterMode mode = EncounterMode::Training;
  EncounterState state = EncounterState::Stopped;
  bool victory = false;
  FixedPoint playerHp = fp(100);
  LevelStage levelStage = LevelStage::Training;
  GateState gateState = GateState::Closed;
  SupplyState supplyState = SupplyState::Unavailable;
  std::vector<EncounterEnemySnapshot> enemies;
  std::vector<TargetCandidate> candidates;

  bool operator==(const EncounterSnapshot& other) const;
};

struct EncounterEventBatch {
  CombatEventBatch combat;
  std::vector<CombatEffectRequest> effects;
};

class EncounterController {
 public:
  static constexpr EntityId kRiftEnemyId = 2001;
  static constexpr EntityId kPriestEnemyId = 2002;
  static constexpr EntityId kGuardEnemyId = 2003;

  explicit EncounterController(CombatController& combat);
  ~EncounterController();

  EncounterController(const EncounterController&) = delete;
  EncounterController& operator=(const EncounterController&) = delete;

  bool start(EncounterMode mode);
  bool start(const EncounterConfig& config);
  void reset();
  void stop();
  void update(const EncounterFrameInput& input);
  bool advanceLevel();
  bool useSupply();

  const EncounterSnapshot& snapshot() const { return snapshot_; }
  const EncounterEventBatch& events() const { return events_; }

 private:
  struct EnemySlot;

  static bool validConfig(const EncounterConfig& config);
  void refreshSnapshot(bool includeCandidates);

  CombatController& combat_;
  EncounterConfig config_;
  std::vector<std::unique_ptr<EnemySlot>> enemies_;
  EncounterSnapshot snapshot_;
  EncounterEventBatch events_;
  Tick lastTick_ = 0;
  uint64_t nextSequence_ = 1;
  bool levelFlowActive_ = false;
};
