#pragma once

#include "native/engine/core/tick_clock.h"

#include <cstdint>
#include <string>
#include <vector>

// 剧情演出导演（阶段二验收补齐）：把 demo_director 的固定演出泛化为
// 数据驱动的 StoryCue 序列——每条提示包含说话人、字幕文本、时长与
// 镜头提示（0=默认 1=拉远 2=特写），由 Loop 按时间自动推进，
// 供开场与主线关键节点复用。全程确定性可测试。
struct StoryCue {
  std::string speaker;
  std::string text;
  Tick durationMs = 3000;
  int32_t cameraHint = 0;
};

class StoryDirector {
 public:
  // 开场演出：世界观引入三连字幕。
  static StoryDirector opening();

  explicit StoryDirector(std::vector<StoryCue> cues = {});

  // 启动播放（重复调用重置到第一条）。
  void start(Tick now);
  // 按时间推进；超过当前提示时长后切换下一条。
  void tick(Tick now);
  // 手动推进（“继续”按钮）：跳到下一条；最后一条时立即结束。
  void advance(Tick now) {
    if (!active()) return;
    index_ += 1;
    cueStartedAt_ = now;
    if (index_ >= static_cast<int32_t>(cues_.size())) {
      active_ = false;
    }
  }

  bool active() const { return active_ && index_ < static_cast<int32_t>(cues_.size()); }
  const StoryCue* current() const {
    return active() ? &cues_[static_cast<size_t>(index_)] : nullptr;
  }
  int32_t index() const { return index_; }
  int32_t count() const { return static_cast<int32_t>(cues_.size()); }
  bool finished() const { return started_ && !active(); }
  bool started() const { return started_; }

 private:
  std::vector<StoryCue> cues_;
  int32_t index_ = 0;
  Tick cueStartedAt_ = 0;
  bool active_ = false;
  bool started_ = false;
};
