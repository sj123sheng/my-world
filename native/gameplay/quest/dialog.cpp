#include "native/gameplay/quest/dialog.h"

const DialogLibrary& DialogLibrary::defaults() {
  static DialogLibrary library = [] {
    DialogLibrary instance;
    // 对话 1：引路灵开场与任务交接（主线 Q1）。
    instance.dialogs_.push_back({1, {
      {"引路灵", "巡脉者，你终于醒了。静默断层正在侵蚀这片遗迹。"},
      {"引路灵", "先去共鸣祭坛吧，那里的脉网还残留着回响。"},
      {"巡脉者", "共鸣祭坛？我该怎么做？"},
      {"引路灵", "跟随微光前行，抵达后触碰祭坛锚点即可。路上小心守卫。"},
    }});
    // 对话 2：祭坛抵达后的指引（主线 Q2 完成后触发由 UI 选用）。
    instance.dialogs_.push_back({2, {
      {"引路灵", "很好，祭坛的脉网已经重新连接。"},
      {"引路灵", "但遗迹守卫被断层扰动了，清除它们才能继续深入。"},
    }});
    // 对话 3：宝箱与采集指引。
    instance.dialogs_.push_back({3, {
      {"引路灵", "守卫退散了。遗迹深处留有前人的馈赠。"},
      {"引路灵", "打开宝箱，再采一朵脉流花，三源之力便会为你引路。"},
    }});
    return instance;
  }();
  return library;
}

const DialogDef* DialogLibrary::find(int32_t dialogId) const {
  for (const DialogDef& dialog : dialogs_) {
    if (dialog.id == dialogId) return &dialog;
  }
  return nullptr;
}

void DialogSession::start(const DialogDef* def) {
  def_ = def;
  index_ = 0;
  if (def_ == nullptr || def_->lines.empty()) {
    def_ = nullptr;
  }
}

bool DialogSession::active() const {
  return def_ != nullptr && index_ >= 0 &&
         index_ < static_cast<int32_t>(def_->lines.size());
}

bool DialogSession::advance() {
  if (!active()) return false;
  index_ += 1;
  if (index_ >= static_cast<int32_t>(def_->lines.size())) {
    def_ = nullptr;
    index_ = 0;
    return false;
  }
  return true;
}

const DialogLine* DialogSession::current() const {
  return active() ? &def_->lines[index_] : nullptr;
}

int32_t DialogSession::lineCount() const {
  return def_ == nullptr ? 0 : static_cast<int32_t>(def_->lines.size());
}
