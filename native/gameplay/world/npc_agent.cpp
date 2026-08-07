#include "native/gameplay/world/npc_agent.h"

#include "native/generated/world_layout.gen.h"

#include <algorithm>
#include <cmath>

namespace {

// 朝向角约定与敌人一致：模型局部 +Z 为前方，angle = atan2(dx, dy)。
float facingAngle(Vec2 direction) {
  if (!direction.finite() ||
      (direction.x == 0.0f && direction.y == 0.0f)) {
    return 0.0f;
  }
  return std::atan2(direction.x, direction.y);
}

}  // namespace

NpcAgency NpcAgency::fromWorldLayout() {
  NpcAgency agency;
  const size_t count =
      std::min<size_t>(WorldLayout::kNpcCount, kMaxRegistered);
  agency.states_.reserve(count);
  agency.agents_.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    const WorldLayout::WorldNpcDef& def = WorldLayout::kNpcs[index];
    AgentState state;
    state.snapshot.id = def.id;
    state.snapshot.x = def.x;
    state.snapshot.y = def.y;
    state.snapshot.angle = def.facing;
    state.snapshot.behavior = static_cast<int32_t>(def.behavior);
    state.baseAngle = def.facing;
    if (def.behavior == WorldLayout::NpcBehavior::Patrol) {
      for (int32_t point = 0; point < def.patrolCount &&
                               point < WorldLayout::kMaxNpcPatrolPoints;
           ++point) {
        state.patrolPoints.push_back(
            {def.patrolX[point], def.patrolY[point]});
      }
    }
    agency.agents_.push_back(state.snapshot);
    agency.states_.push_back(std::move(state));
  }
  return agency;
}

float NpcAgency::pauseSecondsFor(int32_t npcId, int32_t pointIndex) {
  // 确定性停顿序列：2/3/4 秒，按 npc id 与点序号轮转。
  const int32_t bucket = ((npcId + pointIndex) % 3 + 3) % 3;
  return 2.0f + static_cast<float>(bucket);
}

void NpcAgency::update(float dtSeconds, Vec2 playerPosition) {
  if (dtSeconds <= 0.0f) return;
  for (size_t index = 0; index < states_.size(); ++index) {
    AgentState& state = states_[index];
    NpcAgentSnapshot& snapshot = state.snapshot;
    if (snapshot.talking) {
      // Talk 态：停止移动，朝向玩家。
      snapshot.moving = false;
      if (playerPosition.finite()) {
        const Vec2 toPlayer{playerPosition.x - snapshot.x,
                            playerPosition.y - snapshot.y};
        if (toPlayer.x != 0.0f || toPlayer.y != 0.0f) {
          snapshot.angle = facingAngle(toPlayer);
        }
      }
      agents_[index] = snapshot;
      continue;
    }
    if (snapshot.behavior != static_cast<int32_t>(NpcMotion::Patrol) ||
        state.patrolPoints.empty()) {
      // Idle：驻守原地，保持默认朝向。
      snapshot.moving = false;
      snapshot.angle = state.baseAngle;
      agents_[index] = snapshot;
      continue;
    }
    if (state.pauseRemainingSeconds > 0.0f) {
      // 到点停顿：不走动，倒数结束后切到下一个巡逻点。
      state.pauseRemainingSeconds -= dtSeconds;
      snapshot.moving = false;
      if (state.pauseRemainingSeconds <= 0.0f) {
        state.pauseRemainingSeconds = 0.0f;
        state.patrolIndex =
            (state.patrolIndex + 1) %
            static_cast<int32_t>(state.patrolPoints.size());
      }
      agents_[index] = snapshot;
      continue;
    }
    const Vec2 target = state.patrolPoints[state.patrolIndex];
    const Vec2 delta{target.x - snapshot.x, target.y - snapshot.y};
    const float distance = delta.length();
    const float step = kWalkSpeed * dtSeconds;
    if (distance <= kArriveRadius || distance <= step) {
      // 到达巡逻点：吸附到点上并进入确定性停顿。
      snapshot.x = target.x;
      snapshot.y = target.y;
      snapshot.moving = false;
      state.pauseRemainingSeconds =
          pauseSecondsFor(snapshot.id, state.patrolIndex);
    } else {
      const Vec2 direction{delta.x / distance, delta.y / distance};
      snapshot.x += direction.x * step;
      snapshot.y += direction.y * step;
      snapshot.angle = facingAngle(direction);
      snapshot.moving = true;
    }
    agents_[index] = snapshot;
  }
}

void NpcAgency::beginTalk(int32_t npcId) {
  if (npcId < 0) return;
  // 未知 id 忽略：不能先结束旧会话再发现目标不存在。
  AgentState* target = nullptr;
  size_t targetIndex = 0;
  for (size_t index = 0; index < states_.size(); ++index) {
    if (states_[index].snapshot.id == npcId) {
      target = &states_[index];
      targetIndex = index;
      break;
    }
  }
  if (target == nullptr || talkingNpcId_ == npcId) return;
  if (talkingNpcId_ >= 0) {
    endTalk(talkingNpcId_);
  }
  target->snapshot.talking = true;
  target->snapshot.moving = false;
  agents_[targetIndex] = target->snapshot;
  talkingNpcId_ = npcId;
}

void NpcAgency::endTalk(int32_t npcId) {
  for (size_t index = 0; index < states_.size(); ++index) {
    if (states_[index].snapshot.id != npcId) continue;
    states_[index].snapshot.talking = false;
    agents_[index] = states_[index].snapshot;
    break;
  }
  if (talkingNpcId_ == npcId) talkingNpcId_ = -1;
}
