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
    if (!decision.accepted) continue;
    lastAcceptedSequence_ = request.sequence;
    if (request.action == CombatAction::Attack) {
      comboSegment_ = static_cast<uint8_t>(comboSegment_ % 4 + 1);
    } else {
      comboSegment_ = 0;
    }
    if (request.action == CombatAction::Dodge &&
        pulse_.classifyDodge(input.tick) == DodgeGrade::Precise) {
      actions_.grantInsight(input.tick);
    }
  }
  pendingActions_.clear();

  if (input.moving) comboSegment_ = 0;
  for (const PulseEvent& pulseEvent : pulse_.advance(input.tick)) {
    if (pulseEvent.kind != PulseEventKind::Hit) continue;
    if (actions_.isInvulnerable()) {
      events_.gameplay.push_back({pulseEvent.tick, kPlayerId, kPlayerId,
                                  GameplayEventType::Dodge, 0, 0});
      events_.presentation.push_back({pulseEvent.tick, kPlayerId, kPlayerId,
                                      PresentationEventType::DodgeFlash, FP_ONE, 0});
      continue;
    }
    const FixedPoint applied = std::min(playerHp_, config_.trainingPulseDamage);
    playerHp_ -= applied;
    comboSegment_ = 0;
    events_.gameplay.push_back({pulseEvent.tick, kTrainingTargetId, kPlayerId,
                                GameplayEventType::Damage, applied, 0});
  }

  const std::optional<HitRequest> hit = actions_.update(input.tick, input.dtMs, context);
  if (hit) {
    const DamageOutcome outcome = damage_.resolve(target_, *hit);
    const bool landed = outcome.hpDamage > 0 || outcome.poiseDamage > 0;
    emitDamageEvents(*hit, outcome);

    std::optional<ReactionOutcome> reaction;
    if (landed && hit->source) {
      reaction = reactions_.apply(
          target_, *hit->source, hit->sourceAmount, hit->tick, hit->attacker);
      if (reaction->type) {
        events_.gameplay.push_back({hit->tick, hit->attacker, hit->target,
                                    GameplayEventType::Resonance,
                                    reaction->hpDamage + reaction->poiseDamage,
                                    static_cast<uint32_t>(hit->sequence)});
        events_.presentation.push_back({hit->tick, hit->attacker, hit->target,
                                        PresentationEventType::ResonanceBurst,
                                        FP_ONE,
                                        static_cast<uint32_t>(hit->sequence)});
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
}

void CombatController::emitDamageEvents(const HitRequest& hit,
                                        const DamageOutcome& damage) {
  if (damage.hpDamage == 0 && damage.poiseDamage == 0) return;
  const uint32_t sequence = static_cast<uint32_t>(hit.sequence);
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
  snapshot_.targetHp = target_.hp();
  snapshot_.targetPoise = target_.poise();
  snapshot_.stamina = actions_.stamina();
  snapshot_.resonance = actions_.resonance();
  snapshot_.hasInsight = actions_.hasInsight();
  snapshot_.targetAlive = target_.alive();
  snapshot_.lastAcceptedSequence = lastAcceptedSequence_;
}

void CombatController::reset() {
  actions_.reset();
  pulse_ = TrainingPulse(config_);
  target_.reset();
  pendingActions_.clear();
  events_ = {};
  playerHp_ = config_.trainingPlayerHp;
  comboSegment_ = 0;
  lastAcceptedSequence_ = 0;
  refreshSnapshot();
}
