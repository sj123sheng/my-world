#include "native/gameplay/world/exploration_feedback.h"

#include <cassert>

int main() {
  ExplorationFeedbackState state;
  assert(state.snapshot().type == ExplorationFeedbackType::None);
  assert(state.snapshot().id == -1);

  state.publish(ExplorationFeedbackType::PuzzleActivated, 71, "湖心机关",
               "路径门已准备开启", 900);
  assert(state.snapshot().type == ExplorationFeedbackType::PuzzleActivated);
  assert(state.snapshot().id == 71);
  assert(state.snapshot().title == "湖心机关");
  assert(state.snapshot().remainingMs == 900);

  state.update(400);
  assert(state.snapshot().remainingMs == 500);
  state.update(500);
  assert(state.snapshot().type == ExplorationFeedbackType::None);
  assert(state.snapshot().remainingMs == 0);

  state.publish(ExplorationFeedbackType::PoiDiscovered, 11, "翠风观景台", "",
               1200);
  state.publish(ExplorationFeedbackType::GateOpened, 81, "湖畔石门", "通路已开启",
               1000);
  assert(state.snapshot().type == ExplorationFeedbackType::GateOpened);
  assert(state.snapshot().id == 81);
  assert(state.snapshot().title == "湖畔石门");
  assert(state.snapshot().subtitle == "通路已开启");
  return 0;
}
