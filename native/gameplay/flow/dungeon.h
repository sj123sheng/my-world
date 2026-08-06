#pragma once

#include <cstdint>
#include <string>

// 秘境实例（阶段二验收补齐）：可进入/退出的关卡副本，带确定性结算。
// 进入后以击杀数推进；达标即通关，退出时一次性发放奖励。
enum class DungeonState : uint8_t {
  Idle = 0,      // 未进入
  Active = 1,    // 秘境进行中
  Cleared = 2,   // 已通关，等待退出结算
};

struct DungeonDef {
  int32_t id = 1;
  std::string name = "回声回廊";
  int32_t killsRequired = 3;
  // 结算奖励（进入养成经济回路）。
  int32_t rewardGold = 200;
  int32_t rewardExp = 4;
  int32_t rewardAscension = 1;
};

class DungeonSession {
 public:
  explicit DungeonSession(DungeonDef def = DungeonDef{}) : def_(def) {}

  // 进入秘境：仅 Idle 时有效。返回是否成功。
  bool enter() {
    if (state_ != DungeonState::Idle) return false;
    kills_ = 0;
    rewarded_ = false;
    state_ = DungeonState::Active;
    return true;
  }

  // 退出秘境：Active 直接离开（无结算）；Cleared 标记可结算并回到 Idle。
  // 返回本次退出是否触发结算。
  bool leave() {
    if (state_ == DungeonState::Idle) return false;
    const bool settle = state_ == DungeonState::Cleared && !rewarded_;
    if (settle) rewarded_ = true;
    state_ = DungeonState::Idle;
    kills_ = 0;
    return settle;
  }

  // 击杀推进：仅 Active 生效，达标自动进入 Cleared。
  void notifyEnemiesKilled(int32_t count) {
    if (state_ != DungeonState::Active || count <= 0) return;
    kills_ += count;
    if (kills_ >= def_.killsRequired) {
      kills_ = def_.killsRequired;
      state_ = DungeonState::Cleared;
    }
  }

  DungeonState state() const { return state_; }
  int32_t kills() const { return kills_; }
  const DungeonDef& def() const { return def_; }

 private:
  DungeonDef def_;
  DungeonState state_ = DungeonState::Idle;
  int32_t kills_ = 0;
  bool rewarded_ = false;
};
