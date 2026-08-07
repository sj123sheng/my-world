#include "native/gameplay/world/exploration_feedback.h"

#include <algorithm>

void ExplorationFeedbackState::publish(ExplorationFeedbackType type,
                                        int32_t id,
                                        const std::string& title,
                                        const std::string& subtitle,
                                        Tick durationMs) {
  feedback_.type = type;
  feedback_.id = id;
  feedback_.title = title;
  feedback_.subtitle = subtitle;
  feedback_.remainingMs = durationMs;
}

void ExplorationFeedbackState::update(Tick dtMs) {
  if (feedback_.remainingMs == 0) return;
  feedback_.remainingMs = dtMs >= feedback_.remainingMs
                              ? 0
                              : feedback_.remainingMs - dtMs;
  if (feedback_.remainingMs == 0) {
    feedback_.type = ExplorationFeedbackType::None;
    feedback_.id = -1;
    feedback_.title.clear();
    feedback_.subtitle.clear();
  }
}
