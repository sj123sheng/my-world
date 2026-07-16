#include "combat_controller.h"

#include <algorithm>

CombatController::CombatController(CombatConfig config)
    : config_(config.validated()),
      actions_(config_),
      pulse_(config_),
      target_(config_),
      damage_(config_),
      reactions_(config_) {
  reset();
}

void CombatController::enqueue(ActionRequest request) {
  pendingActions_.push_back(request);
}

ActionContext CombatController::contextFor(const CombatFrameInput& input) const {
  const bool selected = input.targetId == kTrainingTargetId;
  return {kPlayerId,
          selected ? input.targetId : 0,
          selected && input.targetAlive && target_.alive(),
          input.moving,
          false};
}

void CombatController::update(const CombatFrameInput& input) {
  events_ = {};
  currentTick_ = input.tick;
  target_.advance(input.tick);
  ActionContext context = contextFor(input);

  // 资源时间线先于本帧动作决策推进；零时长不会推进动作命中点。
  (void)actions_.update(input.tick, 0, context);

  std::stable_sort(pendingActions_.begin(), pendingActions_.end(),
                   [](const ActionRequest& left, const ActionRequest& right) {
                     return left.sequence < right.sequence;
                   });
  for (const ActionRequest& request : pendingActions_) {
    const ActionDecision decision = actions_.request(request, context);
    if (!decision.accepted) {
      lastRejectReason_ = decision.reason;
      continue;
    }
    lastAcceptedSequence_ = request.sequence;
    if (request.action == CombatAction::Attack) {
      comboSegment_ = static_cast<uint8_t>(comboSegment_ % 4 + 1);
    } else {
      comboSegment_ = 0;
    }
    if (request.action == CombatAction::Dodge) {
      const std::optional<Tick> preciseHit = pulse_.preciseDodgeHitTick(input.tick);
      if (!preciseHit) continue;
      preciseDodgedPulseTick_ = preciseHit;
      actions_.grantInsight(input.tick);
    }
  }
  pendingActions_.clear();

  if (input.moving) comboSegment_ = 0;
  const std::optional<HitRequest> hit = actions_.update(input.tick, input.dtMs, context);
  bool encounterReset = false;
  for (const PulseEvent& pulseEvent : pulse_.advance(input.tick)) {
    pulsePhase_ = pulseEvent.kind;
    if (pulseEvent.kind != PulseEventKind::Hit) continue;
    const bool preciselyDodged = preciseDodgedPulseTick_ == pulseEvent.tick;
    if (preciselyDodged) preciseDodgedPulseTick_.reset();
    if (preciselyDodged || actions_.wasInvulnerableAt(pulseEvent.tick)) {
      events_.gameplay.push_back({pulseEvent.tick, kPlayerId, kPlayerId,
                                  GameplayEventType::Dodge, 0, 0});
      events_.presentation.push_back({pulseEvent.tick, kPlayerId, kPlayerId,
                                      PresentationEventType::DodgeFlash, FP_ONE, 0});
      continue;
    }
    const FixedPoint applied = std::min(playerHp_, config_.trainingPulseDamage);
    playerHp_ -= applied;
    comboSegment_ = 0;
    actions_.resetCombo();
    events_.gameplay.push_back({pulseEvent.tick, kTrainingTargetId, kPlayerId,
                                GameplayEventType::Damage, applied, 0});
    if (playerHp_ == 0) {
      resetTrainingEncounter();
      events_.gameplay.push_back({pulseEvent.tick, kTrainingTargetId, kPlayerId,
                                  GameplayEventType::EncounterReset, 0, 0});
      encounterReset = true;
      break;
    }
  }

  if (hit && !encounterReset) {
    lastAbility_ = hit->ability;
    const DamageOutcome outcome = damage_.resolve(target_, *hit);
    const bool landed = outcome.hpDamage > 0 || outcome.poiseDamage > 0;
    emitDamageEvents(*hit, outcome);

    std::optional<ReactionOutcome> reaction;
    if (landed && hit->source) {
      reaction = reactions_.apply(
          target_, *hit->source, hit->sourceAmount, hit->tick, hit->attacker);
      events_.gameplay.push_back({hit->tick, hit->attacker, hit->target,
                                  GameplayEventType::AuraApplied,
                                  hit->sourceAmount, hit->sequence});
      if (reaction->type) {
        currentReaction_ = static_cast<int32_t>(*reaction->type);
        events_.gameplay.push_back({hit->tick, hit->attacker, hit->target,
                                    GameplayEventType::Resonance,
                                    reaction->hpDamage + reaction->poiseDamage,
                                    hit->sequence});
        events_.presentation.push_back({hit->tick, hit->attacker, hit->target,
                                        PresentationEventType::ResonanceBurst,
                                        FP_ONE,
                                        hit->sequence});
      }
    }

    if (hit->transactionId != 0) {
      (void)actions_.confirmHit(hit->transactionId, landed);
    }
    if (reaction && reaction->type) {
      actions_.addResonance(reaction->resonanceGain);
    }
  }

  refreshSnapshot();
  sortEvents();
}

void CombatController::emitDamageEvents(const HitRequest& hit,
                                        const DamageOutcome& damage) {
  if (damage.hpDamage == 0 && damage.poiseDamage == 0) return;
  const uint64_t sequence = hit.sequence;
  events_.gameplay.push_back(
      {hit.tick, hit.attacker, hit.target, GameplayEventType::Hit, hit.baseDamage, sequence});
  events_.gameplay.push_back(
      {hit.tick, hit.attacker, hit.target, GameplayEventType::Damage, damage.hpDamage, sequence});
  events_.presentation.push_back({hit.tick, hit.attacker, hit.target,
                                  PresentationEventType::HitFlash, FP_ONE, sequence});
  if (damage.poiseBroken) {
    events_.gameplay.push_back({hit.tick, hit.attacker, hit.target,
                                GameplayEventType::PoiseBreak, damage.poiseDamage, sequence});
    events_.presentation.push_back({hit.tick, hit.attacker, hit.target,
                                    PresentationEventType::PoiseBreakBurst, FP_ONE, sequence});
  }
  if (damage.killed) {
    events_.gameplay.push_back(
        {hit.tick, hit.attacker, hit.target, GameplayEventType::Death, 0, sequence});
  }
}

void CombatController::refreshSnapshot() {
  snapshot_.comboSegment = comboSegment_;
  snapshot_.playerHp = playerHp_;
  snapshot_.playerPoise = playerPoise_;
  snapshot_.targetHp = target_.hp();
  snapshot_.targetPoise = target_.poise();
  snapshot_.stamina = actions_.stamina();
  snapshot_.resonance = actions_.resonance();
  snapshot_.hasInsight = actions_.hasInsight();
  snapshot_.invulnerable = actions_.isInvulnerable();
  snapshot_.insightMs = actions_.insightRemainingMs();
  snapshot_.pulseHitRemainingMs = pulse_.hitRemainingMs(currentTick_);
  snapshot_.lastRejectReason = lastRejectReason_;
  snapshot_.targetAlive = target_.alive();
  snapshot_.lastAcceptedSequence = lastAcceptedSequence_;
  snapshot_.currentAction = static_cast<uint8_t>(actions_.state());
  snapshot_.comboWindowMs = actions_.comboWindowRemainingMs();
  snapshot_.radianceCooldownMs = actions_.sourceCooldownRemainingMs(SourceType::Radiance);
  snapshot_.currentCooldownMs = actions_.sourceCooldownRemainingMs(SourceType::Current);
  snapshot_.corruptionCooldownMs = actions_.sourceCooldownRemainingMs(SourceType::Corruption);
  snapshot_.ultimateWindowMs = actions_.ultimateWindowRemainingMs();
  snapshot_.targetPoiseBroken = target_.poiseBroken(currentTick_);
  snapshot_.radianceAttached = false;
  snapshot_.currentAttached = false;
  snapshot_.corruptionAttached = false;
  for (const SourceAura& aura : target_.sourceAuras().active()) {
    if (aura.type == SourceType::Radiance) snapshot_.radianceAttached = true;
    if (aura.type == SourceType::Current) snapshot_.currentAttached = true;
    if (aura.type == SourceType::Corruption) snapshot_.corruptionAttached = true;
  }
  snapshot_.corroded = target_.corroded();
  snapshot_.currentReaction = currentReaction_;
  snapshot_.pulsePhase = static_cast<uint8_t>(pulse_.phase(currentTick_));
  snapshot_.lastAbility = lastAbility_;
}

void CombatController::reset() {
  actions_.reset();
  pulse_ = TrainingPulse(config_);
  target_.reset();
  pendingActions_.clear();
  events_ = {};
  playerHp_ = config_.trainingPlayerHp;
  playerPoise_ = config_.trainingPlayerPoise;
  comboSegment_ = 0;
  lastAcceptedSequence_ = 0;
  lastRejectReason_ = ActionRejectReason::None;
  currentTick_ = 0;
  currentReaction_ = -1;
  pulsePhase_ = PulseEventKind::None;
  preciseDodgedPulseTick_.reset();
  lastAbility_ = 0;
  refreshSnapshot();
}

void CombatController::resetTrainingEncounter() {
  actions_.reset();
  pulse_.resetAt(currentTick_);
  target_.reset();
  pendingActions_.clear();
  playerHp_ = config_.trainingPlayerHp;
  playerPoise_ = config_.trainingPlayerPoise;
  comboSegment_ = 0;
  lastAcceptedSequence_ = 0;
  lastRejectReason_ = ActionRejectReason::None;
  currentReaction_ = -1;
  pulsePhase_ = PulseEventKind::None;
  preciseDodgedPulseTick_.reset();
  lastAbility_ = 0;
}

void CombatController::sortEvents() {
  const auto less = [](const auto& left, const auto& right) {
    if (left.tick != right.tick) return left.tick < right.tick;
    if (left.source != right.source) return left.source < right.source;
    if (left.target != right.target) return left.target < right.target;
    return left.sequence < right.sequence;
  };
  std::stable_sort(events_.gameplay.begin(), events_.gameplay.end(), less);
  std::stable_sort(events_.presentation.begin(), events_.presentation.end(), less);
}
