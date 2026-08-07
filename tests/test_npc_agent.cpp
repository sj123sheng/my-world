#include "native/gameplay/world/npc_agent.h"

#include "native/generated/world_layout.gen.h"

#include <cassert>
#include <cmath>

namespace {

constexpr float kDt = 0.016f;

const NpcAgentSnapshot* findById(const NpcAgency& agency, int32_t id) {
  for (const NpcAgentSnapshot& npc : agency.agents()) {
    if (npc.id == id) return &npc;
  }
  return nullptr;
}

}  // namespace

int main() {
  // 注册来自数据管线：6 个 NPC，未超过注册上限。
  NpcAgency agency = NpcAgency::fromWorldLayout();
  assert(agency.agents().size() == WorldLayout::kNpcCount);
  assert(agency.agents().size() <= static_cast<size_t>(NpcAgency::kMaxRegistered));
  assert(agency.talkingNpcId() == -1);

  // 初始位置/朝向/行为与 WorldLayout::kNpcs 一致。
  for (const WorldLayout::WorldNpcDef& def : WorldLayout::kNpcs) {
    const NpcAgentSnapshot* npc = findById(agency, def.id);
    assert(npc != nullptr);
    assert(npc->x == def.x);
    assert(npc->y == def.y);
    assert(npc->angle == def.facing);
    assert(npc->behavior == static_cast<int32_t>(def.behavior));
  }

  // Idle NPC（32）原地驻守：任意推进不移动、不改变朝向。
  const NpcAgentSnapshot* idle = findById(agency, 32);
  const float idleX = idle->x;
  const float idleY = idle->y;
  const float idleAngle = idle->angle;
  for (int i = 0; i < 100; ++i) agency.update(kDt, {0.5f, 0.5f});
  idle = findById(agency, 32);
  assert(idle->x == idleX);
  assert(idle->y == idleY);
  assert(idle->angle == idleAngle);
  assert(!idle->moving);

  // 巡逻停顿序列为确定性 2/3/4 秒（按 id+点序号轮转）。
  for (int32_t npcId = 32; npcId <= 37; ++npcId) {
    for (int32_t point = 0; point < 4; ++point) {
      const float pause = NpcAgency::pauseSecondsFor(npcId, point);
      assert(pause >= 2.0f && pause <= 4.0f);
      assert(pause == 2.0f + static_cast<float>(((npcId + point) % 3 + 3) % 3));
    }
  }

  // NPC 33 出生点即巡逻点 0：第一帧到点停顿（moving=false）。
  NpcAgency patrol = NpcAgency::fromWorldLayout();
  patrol.update(kDt, {0.9f, 0.9f});
  const NpcAgentSnapshot* npc33 = findById(patrol, 33);
  assert(npc33 != nullptr);
  assert(!npc33->moving);
  assert(std::abs(npc33->x - 0.1f) < 1e-6f);
  assert(std::abs(npc33->y - 0.3f) < 1e-6f);
  // 停顿时长 pauseSecondsFor(33, 0)=2s：扫描到首个移动帧，
  // 停顿窗口应在期望时长 ±少量帧内，期间位置不漂移。
  int stillFrames = 0;
  bool started = false;
  for (int i = 0; i < 200; ++i) {
    patrol.update(kDt, {0.9f, 0.9f});
    npc33 = findById(patrol, 33);
    if (npc33->moving) {
      started = true;
      break;
    }
    assert(std::abs(npc33->x - 0.1f) < 1e-6f);
    assert(std::abs(npc33->y - 0.3f) < 1e-6f);
    ++stillFrames;
  }
  assert(started);
  const float actualPauseSeconds = stillFrames * kDt;
  const float expectedPauseSeconds = NpcAgency::pauseSecondsFor(33, 0);
  assert(actualPauseSeconds >= expectedPauseSeconds - 0.05f);
  assert(actualPauseSeconds <= expectedPauseSeconds + 0.05f);
  // 停顿结束后走向下一巡逻点（0.2, 0.35），朝向角约定 atan2(dx, dy)。
  assert(std::abs(npc33->angle - std::atan2(0.1f, 0.05f)) < 1e-4f);

  // Talk 态：停止移动并朝向玩家。
  NpcAgency talk = NpcAgency::fromWorldLayout();
  talk.update(kDt, {0.9f, 0.9f});
  talk.beginTalk(33);
  assert(talk.talkingNpcId() == 33);
  const Vec2 player{0.1f, 0.5f};
  for (int i = 0; i < 200; ++i) talk.update(kDt, player);
  const NpcAgentSnapshot* talking = findById(talk, 33);
  assert(talking->talking);
  assert(!talking->moving);
  // 仍停在出生巡逻点上，未随推进漂移。
  assert(std::abs(talking->x - 0.1f) < 1e-6f);
  assert(std::abs(talking->y - 0.3f) < 1e-6f);
  // 玩家在正前方（+y）：angle 约为 0（atan2(0, +)）。
  assert(std::abs(talking->angle) < 1e-5f);
  talk.endTalk(33);
  assert(talk.talkingNpcId() == -1);
  talk.update(kDt, player);
  talking = findById(talk, 33);
  assert(!talking->talking);

  // 同一时刻至多一个 Talk 态：新会话自动结束旧会话。
  talk.beginTalk(35);
  talk.beginTalk(37);
  assert(talk.talkingNpcId() == 37);
  assert(!findById(talk, 35)->talking);
  assert(findById(talk, 37)->talking);
  // 未知 id 忽略。
  talk.beginTalk(999);
  assert(talk.talkingNpcId() == 37);
  talk.endTalk(999);
  assert(talk.talkingNpcId() == 37);

  // 确定性：同输入双实例逐步对照，所有字段完全一致。
  NpcAgency lhs = NpcAgency::fromWorldLayout();
  NpcAgency rhs = NpcAgency::fromWorldLayout();
  lhs.beginTalk(33);
  rhs.beginTalk(33);
  for (int i = 0; i < 1200; ++i) {
    const Vec2 p{0.5f + 0.001f * (i % 7), 0.2f};
    lhs.update(kDt, p);
    rhs.update(kDt, p);
    if (i == 400) {
      lhs.endTalk(33);
      rhs.endTalk(33);
    }
    assert(lhs.agents().size() == rhs.agents().size());
    for (size_t index = 0; index < lhs.agents().size(); ++index) {
      const NpcAgentSnapshot& a = lhs.agents()[index];
      const NpcAgentSnapshot& b = rhs.agents()[index];
      assert(a.id == b.id);
      assert(a.x == b.x);
      assert(a.y == b.y);
      assert(a.angle == b.angle);
      assert(a.behavior == b.behavior);
      assert(a.moving == b.moving);
      assert(a.talking == b.talking);
    }
  }

  // 非法步长不推进、不崩溃。
  lhs.update(0.0f, {0.5f, 0.5f});
  lhs.update(-1.0f, {0.5f, 0.5f});
  return 0;
}
