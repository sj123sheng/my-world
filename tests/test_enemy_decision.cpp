#include "gameplay/ai/decision_policy.h"
#include "gameplay/ai/perception_system.h"

#include <cassert>

namespace {

PerceptionSnapshot observeDefaults() {
  EnemyWorldView world = EnemyWorldView::testDefaults();
  world.selfPosition = {0.0f, 0.0f};
  world.playerPosition = {0.3f, 0.4f};
  return PerceptionSystem{}.observe(world);
}

}  // namespace

int main() {
  DecisionPolicy policy;

  PerceptionSnapshot facts = observeDefaults();
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Chase);

  // 近战交战留白（minimum 0.08 / attack 0.25）：贴身过近后撤、能力射程
  // 内攻击、超出射程追击。
  facts.playerDistance = 0.04f;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Retreat);
  facts.playerDistance = 0.14f;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Attack);
  facts.playerDistance = 0.2f;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Attack);
  facts.playerDistance = 0.5f;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Chase);

  facts.selfAlive = false;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Idle);

  facts.selfInsideRegion = false;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Idle);

  facts = observeDefaults();
  facts.selfInsideRegion = false;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::ReturnToArea);

  facts.playerReachable = false;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::ReturnToArea);

  facts = observeDefaults();
  facts.playerReachable = false;
  assert(policy.choose(facts, EnemyArchetype::Guard) == EnemyIntent::BreakFree);

  facts.staggered = true;
  assert(policy.choose(facts, EnemyArchetype::Guard) == EnemyIntent::BreakFree);

  facts = observeDefaults();
  facts.staggered = true;
  assert(policy.choose(facts, EnemyArchetype::RiftClaw) == EnemyIntent::Idle);

  facts = observeDefaults();
  facts.allies = {
      {30, EnemyArchetype::Guard, fp(80), 0, {1.5f, 0.0f}, 1.5f, true, true},
      {10, EnemyArchetype::RiftClaw, fp(50), 0, {1.0f, 0.0f}, 1.0f, true, true},
      {20, EnemyArchetype::RiftClaw, fp(0), 0, {1.0f, 0.0f}, 1.0f, false, true},
      {40, EnemyArchetype::RiftClaw, fp(50), 0, {1.0f, 0.0f}, 1.0f, true, false},
  };
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Support);

  facts.playerVisible = false;
  facts.targetVisible = false;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Support);
  facts.playerVisible = true;
  facts.targetVisible = true;

  facts.staggered = true;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Idle);
  facts.staggered = false;

  facts.allies[0].shield = fp(10);
  facts.allies[1].shield = fp(10);
  // 远程交战留白（minimum 0.16 / attack 4.0）：近身后撤、能力射程内施法、
  // 超出射程拉回。
  facts.playerDistance = 0.10f;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Retreat);

  facts.playerDistance = 0.30f;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Attack);

  facts.playerDistance = 0.5f;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Attack);

  facts.playerDistance = 5.0f;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Chase);

  facts.playerDistance = 5.0f;
  assert(policy.choose(facts, EnemyArchetype::Guard) == EnemyIntent::Chase);

  // 快照注入的留白优先于原型推导：Boss 空挡口径直接驱动分支。
  PerceptionSnapshot custom = observeDefaults();
  custom.engagementRange = {0.2f, 0.4f, 0.5f, 1.0f};
  custom.playerDistance = 0.15f;
  assert(policy.choose(custom, EnemyArchetype::RiftClaw) == EnemyIntent::Retreat);
  custom.playerDistance = 0.3f;
  assert(policy.choose(custom, EnemyArchetype::RiftClaw) == EnemyIntent::Attack);
  custom.playerDistance = 0.7f;
  assert(policy.choose(custom, EnemyArchetype::RiftClaw) == EnemyIntent::Chase);

  const EnemyIntent expected = policy.choose(facts, EnemyArchetype::Guard);
  for (int iteration = 0; iteration < 100; ++iteration) {
    assert(policy.choose(facts, EnemyArchetype::Guard) == expected);
  }
}
