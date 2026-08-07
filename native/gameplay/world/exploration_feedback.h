#pragma once

#include "native/engine/core/tick_clock.h"

#include <cstdint>
#include <string>

enum class ExplorationFeedbackType : uint8_t {
  None = 0,
  PoiDiscovered = 1,
  PuzzleActivated = 2,
  GateOpened = 3,
  RewardClaimed = 4,
};

struct ExplorationFeedback {
  ExplorationFeedbackType type = ExplorationFeedbackType::None;
  int32_t id = -1;
  std::string title;
  std::string subtitle;
  Tick remainingMs = 0;
};

class ExplorationFeedbackState {
 public:
  void publish(ExplorationFeedbackType type, int32_t id,
               const std::string& title, const std::string& subtitle,
               Tick durationMs);
  void update(Tick dtMs);
  const ExplorationFeedback& snapshot() const { return feedback_; }

 private:
  ExplorationFeedback feedback_;
};
