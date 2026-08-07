#pragma once

#include "native/engine/math/vec2.h"

#include <cstdint>
#include <vector>

// NPC 轻量状态机（Phase 4：任务与互动 NPC）：
// Idle/Patrol/Talk 三态，无战斗、无感知，仅输出位置与朝向。
// 刻意不复用 EnemyAgent：NPC 逻辑极简，独立实现避免战斗侧耦合。
// 确定性：固定步长 Tick 驱动，同输入同输出，供回归测试双实例对照。
enum class NpcMotion : uint8_t {
  Idle = 0,
  Patrol = 1,
};

struct NpcAgentSnapshot {
  int32_t id = -1;
  float x = 0.0f;
  float y = 0.0f;
  // 朝向角（弧度）：与敌人朝向约定一致，atan2(dx, dy)，模型局部 +Z 为前方。
  float angle = 0.0f;
  // 0=Idle 1=Patrol（与 WorldLayout::NpcBehavior 数值一致）。
  int32_t behavior = 0;
  bool moving = false;
  bool talking = false;
};

class NpcAgency {
 public:
  // 注册上限：超出 WorldLayout 条目只取前 kMaxRegistered 个。
  static constexpr int32_t kMaxRegistered = 12;
  // 同屏渲染上限：发布渲染状态时按距玩家距离裁剪（发布侧消费）。
  static constexpr int32_t kMaxVisible = 6;
  // 巡逻行走速度（世界单位/秒）。
  static constexpr float kWalkSpeed = 0.02f;
  // 到达巡逻点判定半径（世界单位）。
  static constexpr float kArriveRadius = 0.004f;

  // 从 WorldLayout::kNpcs 构造：位置、朝向、巡逻点全部来自数据管线。
  static NpcAgency fromWorldLayout();

  // 固定步长推进：巡逻移动、到点停顿、对话朝向玩家。
  void update(float dtSeconds, Vec2 playerPosition);

  // 进入/退出对话（Talk 态）：停止移动并朝向玩家；未知 id 忽略。
  // 同一时刻至多一个 NPC 处于 Talk 态，新会话自动结束旧会话。
  void beginTalk(int32_t npcId);
  void endTalk(int32_t npcId);
  int32_t talkingNpcId() const { return talkingNpcId_; }

  const std::vector<NpcAgentSnapshot>& agents() const { return agents_; }

  // 巡逻到点停顿时长（秒）：按 (id + 点序号) 取 2/3/4 秒确定性序列。
  static float pauseSecondsFor(int32_t npcId, int32_t pointIndex);

 private:
  struct AgentState {
    NpcAgentSnapshot snapshot;
    float baseAngle = 0.0f;
    std::vector<Vec2> patrolPoints;
    int32_t patrolIndex = 0;
    float pauseRemainingSeconds = 0.0f;
  };

  std::vector<AgentState> states_;
  std::vector<NpcAgentSnapshot> agents_;
  int32_t talkingNpcId_ = -1;
};
