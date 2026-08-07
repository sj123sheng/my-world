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
    // ---- 开放世界 NPC 对话（Phase 4，dialogId 100-105）----
    // 100 台地引路人（出生台地，无任务）。
    instance.dialogs_.push_back({100, {
      {"台地引路人", "静默断层之后，很少有旅人走到启明台地来。"},
      {"台地引路人", "顺着脉光走，锚点会为你亮起。别在夜里靠近裂隙。"},
    }});
    // 101 低地巡林员（翠风低地，发布支线 201）。
    instance.dialogs_.push_back({101, {
      {"低地巡林员", "又是裂隙爪狼！它们最近把低地的牧道全都堵住了。"},
      {"巡脉者", "需要我帮忙清理吗？"},
      {"低地巡林员", "求之不得！击退三头爪狼后回来找我，我教你辨认安全的巡林路线。"},
    }, 201});
    // 102 湖畔渔夫（辉光湖畔，无任务）。
    instance.dialogs_.push_back({102, {
      {"湖畔渔夫", "湖底的辉光一晚比一晚亮，鱼群都不肯浮上来了。"},
      {"湖畔渔夫", "老人们说，那是脉网在重新苏醒的征兆。"},
    }});
    // 103 回廊信使（中枢回廊，发布支线 202）。
    instance.dialogs_.push_back({103, {
      {"回廊信使", "嘘——回廊里的守卫被断层扰动，已经不认识通行印信了。"},
      {"回廊信使", "我手里的急件必须送到北境。替我扫清两名守卫，我就能趁乱穿过去。"},
    }, 202});
    // 104 荒原勘探者（灰烬荒原，无任务）。
    instance.dialogs_.push_back({104, {
      {"荒原勘探者", "灰烬底下埋着旧纪元的锻炉，铁块还带着余温。"},
      {"荒原勘探者", "再往深处就有裂隙领主出没，我这把老骨头可不敢奉陪。"},
    }});
    // 105 圣所守望者（圣所高地，发布支线 203）。
    instance.dialogs_.push_back({105, {
      {"圣所守望者", "能登上阶地的人，都是被脉网选中的行者。"},
      {"圣所守望者", "圣所试炼从未关闭：击败四头被污染的造物，再回来向我复命。"},
      {"巡脉者", "试炼的尽头有什么？"},
      {"圣所守望者", "去完成它，你自然会看见。"},
    }, 203});
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
