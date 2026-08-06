#include "native/gameplay/flow/story_director.h"

StoryDirector StoryDirector::opening() {
  return StoryDirector({
      {"???", "艾瑟兰——源质涌动的大地，三源共鸣塑造了这里的万物。", 3500, 1},
      {"???", "脉络断流，雾谷滋生蚀影。巡脉者，你被唤醒，正是为了此刻。", 3500, 0},
      {"引路灵", "跟我来。先到前方的共鸣祭坛，点亮你的第一座锚点。", 3500, 0},
  });
}

StoryDirector::StoryDirector(std::vector<StoryCue> cues)
    : cues_(std::move(cues)) {}

void StoryDirector::start(Tick now) {
  index_ = 0;
  cueStartedAt_ = now;
  started_ = true;
  active_ = !cues_.empty();
}

void StoryDirector::tick(Tick now) {
  if (!active()) return;
  const StoryCue& cue = cues_[static_cast<size_t>(index_)];
  if (now - cueStartedAt_ >= cue.durationMs) {
    index_ += 1;
    cueStartedAt_ = now;
    if (index_ >= static_cast<int32_t>(cues_.size())) {
      active_ = false;
    }
  }
}
