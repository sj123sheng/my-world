#include "native/gameplay/quest/dialog.h"

#include <cassert>

int main() {
  const DialogLibrary& library = DialogLibrary::defaults();
  assert(library.dialogs().size() >= 3);

  // 查找存在的对话。
  const DialogDef* intro = library.find(1);
  assert(intro != nullptr);
  assert(intro->lines.size() >= 2);
  for (const DialogLine& line : intro->lines) {
    assert(!line.speaker.empty());
    assert(!line.text.empty());
  }

  // 未知对话返回空。
  assert(library.find(999) == nullptr);

  // 会话推进：逐句前进，最后一句 advance 返回 false 并结束。
  DialogSession session;
  assert(!session.active());
  assert(session.current() == nullptr);
  session.start(intro);
  assert(session.active());
  assert(session.index() == 0);
  assert(session.lineCount() == static_cast<int32_t>(intro->lines.size()));
  assert(session.current()->speaker == intro->lines[0].speaker);
  const int32_t total = session.lineCount();
  int32_t advances = 0;
  while (session.advance()) ++advances;
  assert(advances == total - 1);
  assert(!session.active());
  assert(session.current() == nullptr);

  // 空对话或 nullptr 启动后不激活。
  DialogSession invalid;
  invalid.start(nullptr);
  assert(!invalid.active());
  assert(!invalid.advance());
  DialogDef emptyDialog{50, {}};
  invalid.start(&emptyDialog);
  assert(!invalid.active());

  // 重启会话可复用。
  session.start(library.find(2));
  assert(session.active());
  assert(session.index() == 0);
  return 0;
}
