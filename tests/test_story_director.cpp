#include "native/gameplay/flow/story_director.h"

#include <cassert>

int main() {
  // 开场演出：三条字幕，未启动时不活跃。
  StoryDirector director = StoryDirector::opening();
  assert(director.count() == 3);
  assert(!director.active());
  assert(!director.started());
  assert(director.current() == nullptr);

  // 启动后展示第一条。
  director.start(0);
  assert(director.started());
  assert(director.active());
  assert(director.index() == 0);
  assert(director.current() != nullptr);
  assert(!director.current()->speaker.empty());
  assert(!director.current()->text.empty());
  assert(director.current()->cameraHint == 1);  // 第一条拉远镜头。

  // 未到时长不推进。
  director.tick(3400);
  assert(director.index() == 0);

  // 到时长切换下一条。
  director.tick(3500);
  assert(director.index() == 1);
  director.tick(7000);
  assert(director.index() == 2);

  // 最后一条结束后不再活跃，finished 置位。
  director.tick(10500);
  assert(!director.active());
  assert(director.finished());
  assert(director.current() == nullptr);

  // 空序列：start 后直接结束，不崩溃。
  StoryDirector empty;
  empty.start(0);
  assert(!empty.active());
  assert(empty.finished());
  assert(empty.current() == nullptr);

  // 重复 start 重置到第一条。
  director.start(100);
  assert(director.index() == 0);
  assert(director.active());

  // 手动 advance（“继续”按钮）：逐条推进，末条后结束。
  director.advance(101);
  assert(director.index() == 1);
  director.advance(102);
  assert(director.index() == 2);
  director.advance(103);
  assert(!director.active());
  assert(director.finished());
  // 结束后 advance 无副作用。
  director.advance(104);
  assert(!director.active());

  return 0;
}
