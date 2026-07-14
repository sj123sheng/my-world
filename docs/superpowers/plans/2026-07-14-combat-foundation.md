# 战斗基础循环与事件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前与渲染表面耦合的演示循环升级为可测试的线程安全输入、固定时间步、类型化事件和 HUD 快照基础，为自由相机与战斗系统提供稳定接口。

**Architecture:** 将与平台无关的时间步、输入、事件和快照放入纯 C++ 小模块，使 macOS `clang++` 可直接测试。HarmonyOS `Loop` 只编排这些模块与 `Surface`，不改变已经真机验证的 XComponent、NativeWindow、EGL 与 GLES3 生命周期。

**Tech Stack:** C++17、HarmonyOS NDK、ArkTS/ArkUI、N-API、XComponent、NativeWindow、EGL、OpenGL ES 3、独立 `assert` 测试程序。

## Global Constraints

- 仅建设阶段 1 基础能力，不实现自由相机、技能、敌人或首领。
- 所有纯逻辑模块不得依赖 HarmonyOS 头文件，必须可用 macOS `clang++` 测试。
- 固定逻辑步长为 16 毫秒；单帧最多补算 4 步，超额累计时间丢弃并计数。
- 输入生产者和渲染线程消费者可以并发访问，不允许数据竞争。
- GameplayEvent 与 PresentationEvent 分离；HUD 只读取快照，不直接读取可变游戏状态。
- 保持已验证的 `OnSurfaceCreated → surface_init → Loop::start` 和停止后销毁顺序。
- 不修改 EGL/GLES3 初始化策略，不恢复 NativeWindow CPU 软件降级路径。
- Commit 必须符合仓库纪律，包含类型、简述和 `Prompt:`。

---

## 文件结构

| 文件 | 职责 |
|---|---|
| `native/engine/input/input_event.h` | 平台无关输入动作与数据结构 |
| `native/engine/input/input_queue.h` | 有容量限制的线程安全输入队列 |
| `native/engine/core/fixed_step.h` | 固定时间步累计与补算上限 |
| `native/engine/core/event_queue.h` | Gameplay/Presentation 类型化事件队列 |
| `native/engine/core/game_snapshot.h` | Native 游戏状态到 HUD 的不可变快照 |
| `native/engine/core/snapshot_store.h` | 快照发布与读取同步 |
| `native/engine/core/loop.h/.cpp` | 编排输入、固定步更新、渲染与快照发布 |
| `entry/src/main/cpp/native_bridge.cpp` | 将触屏事件规范化并将快照导出到 N-API |
| `entry/src/main/cpp/types/libnative_game/Index.d.ts` | N-API 类型声明 |
| `entry/src/main/ets/napi/Bridge.ets` | ArkTS 快照和输入类型 |
| `entry/src/main/cpp/CMakeLists.txt` | 注册新增头文件/源文件 |
| `tests/test_input_queue.cpp` | 输入并发安全和容量测试 |
| `tests/test_fixed_step.cpp` | 固定时间步测试 |
| `tests/test_event_queue.cpp` | 事件分流和顺序测试 |
| `tests/test_snapshot_store.cpp` | 快照发布/读取测试 |

### Task 1: 线程安全输入队列

**Files:**
- Create: `native/engine/input/input_event.h`
- Modify: `native/engine/input/input_queue.h`
- Create: `tests/test_input_queue.cpp`

**Interfaces:**
- Consumes: XComponent 触摸类型、屏幕坐标和指针 ID。
- Produces: `InputEvent`、`InputAction`、`InputQueue::push(const InputEvent&)`、`InputQueue::pop(InputEvent&)`、`InputQueue::droppedCount()`。

- [ ] **Step 1: 编写失败测试**

创建 `tests/test_input_queue.cpp`：

```cpp
#include "../native/engine/input/input_queue.h"
#include <cassert>
#include <thread>

int main() {
  InputQueue queue(2);
  assert(queue.push({InputAction::PointerDown, 7, 10.0f, 20.0f, 1}));
  assert(queue.push({InputAction::PointerMove, 7, 11.0f, 21.0f, 2}));
  assert(!queue.push({InputAction::PointerUp, 7, 12.0f, 22.0f, 3}));
  assert(queue.droppedCount() == 1);

  InputEvent out{};
  assert(queue.pop(out));
  assert(out.action == InputAction::PointerDown && out.pointerId == 7 && out.sequence == 1);
  assert(queue.pop(out));
  assert(out.action == InputAction::PointerMove && out.sequence == 2);
  assert(!queue.pop(out));

  InputQueue concurrent(256);
  std::thread producer([&]() {
    for (uint64_t i = 0; i < 200; ++i) {
      assert(concurrent.push({InputAction::PointerMove, 1, float(i), 0.0f, i}));
    }
  });
  producer.join();
  uint64_t expected = 0;
  while (concurrent.pop(out)) assert(out.sequence == expected++);
  assert(expected == 200);
  return 0;
}
```

- [ ] **Step 2: 运行测试并确认按预期失败**

Run:

```bash
clang++ -std=c++17 -pthread tests/test_input_queue.cpp -I. -o /tmp/test_input_queue
```

Expected: FAIL，提示 `InputAction`、带容量构造函数或 `push` 返回值不存在。

- [ ] **Step 3: 实现最小输入类型**

创建 `native/engine/input/input_event.h`：

```cpp
#pragma once
#include <cstdint>

enum class InputAction : uint8_t {
  PointerDown,
  PointerMove,
  PointerUp,
  PointerCancel,
  Attack,
  Dodge,
  Radiance,
  Current,
  Corruption,
  Ultimate,
};

struct InputEvent {
  InputAction action = InputAction::PointerCancel;
  int32_t pointerId = -1;
  float x = 0.0f;
  float y = 0.0f;
  uint64_t sequence = 0;
};
```

将 `native/engine/input/input_queue.h` 替换为：

```cpp
#pragma once
#include "input_event.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>

class InputQueue {
 public:
  explicit InputQueue(size_t capacity = 256) : capacity_(capacity) {}

  bool push(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= capacity_) {
      ++dropped_;
      return false;
    }
    queue_.push(event);
    return true;
  }

  bool pop(InputEvent& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop();
    return true;
  }

  uint64_t droppedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
  }

 private:
  const size_t capacity_;
  mutable std::mutex mutex_;
  std::queue<InputEvent> queue_;
  uint64_t dropped_ = 0;
};
```

- [ ] **Step 4: 运行测试并确认通过**

Run:

```bash
clang++ -std=c++17 -pthread tests/test_input_queue.cpp -I. -o /tmp/test_input_queue && /tmp/test_input_queue
```

Expected: exit 0，无输出。

- [ ] **Step 5: 提交**

```bash
git add native/engine/input/input_event.h native/engine/input/input_queue.h tests/test_input_queue.cpp
git commit -m "refactor: 建立线程安全输入队列" \
  -m "增加动作语义、指针标识、顺序号和容量保护。" \
  -m "Prompt: 构建战斗垂直切片阶段1输入基础"
```

### Task 2: 固定时间步调度器

**Files:**
- Create: `native/engine/core/fixed_step.h`
- Create: `tests/test_fixed_step.cpp`

**Interfaces:**
- Consumes: 每次渲染循环测得的毫秒增量。
- Produces: `FixedStep::advance(int64_t elapsedMs, Fn update)`、`FixedStep::tick()`、`FixedStep::droppedFrames()`。

- [ ] **Step 1: 编写失败测试**

创建 `tests/test_fixed_step.cpp`：

```cpp
#include "../native/engine/core/fixed_step.h"
#include <cassert>
#include <vector>

int main() {
  FixedStep step(16, 4);
  std::vector<Tick> ticks;
  assert(step.advance(15, [&](Tick tick, int64_t dt) { ticks.push_back(tick); }) == 0);
  assert(step.advance(1, [&](Tick tick, int64_t dt) {
    assert(dt == 16);
    ticks.push_back(tick);
  }) == 1);
  assert(ticks.size() == 1 && ticks[0] == 1);

  assert(step.advance(80, [&](Tick tick, int64_t dt) { ticks.push_back(tick); }) == 4);
  assert(step.tick() == 5);
  assert(step.droppedFrames() == 1);
  return 0;
}
```

- [ ] **Step 2: 运行测试并确认按预期失败**

Run:

```bash
clang++ -std=c++17 tests/test_fixed_step.cpp -I. -o /tmp/test_fixed_step
```

Expected: FAIL，提示 `fixed_step.h` 不存在。

- [ ] **Step 3: 实现固定时间步**

创建 `native/engine/core/fixed_step.h`：

```cpp
#pragma once
#include "tick_clock.h"
#include <algorithm>
#include <cstdint>

class FixedStep {
 public:
  FixedStep(int64_t stepMs, int maxSteps)
      : stepMs_(stepMs), maxSteps_(maxSteps) {}

  template <typename Fn>
  int advance(int64_t elapsedMs, Fn&& update) {
    accumulatorMs_ += std::max<int64_t>(0, elapsedMs);
    int available = static_cast<int>(accumulatorMs_ / stepMs_);
    int count = std::min(available, maxSteps_);
    for (int i = 0; i < count; ++i) {
      ++tick_;
      update(tick_, stepMs_);
      accumulatorMs_ -= stepMs_;
    }
    if (available > maxSteps_) {
      droppedFrames_ += static_cast<uint64_t>(available - maxSteps_);
      accumulatorMs_ %= stepMs_;
    }
    return count;
  }

  Tick tick() const { return tick_; }
  uint64_t droppedFrames() const { return droppedFrames_; }

 private:
  int64_t stepMs_;
  int maxSteps_;
  int64_t accumulatorMs_ = 0;
  Tick tick_ = 0;
  uint64_t droppedFrames_ = 0;
};
```

- [ ] **Step 4: 运行测试并确认通过**

Run:

```bash
clang++ -std=c++17 tests/test_fixed_step.cpp -I. -o /tmp/test_fixed_step && /tmp/test_fixed_step
```

Expected: exit 0，无输出。

- [ ] **Step 5: 提交**

```bash
git add native/engine/core/fixed_step.h tests/test_fixed_step.cpp
git commit -m "feat: 增加固定时间步调度器" \
  -m "固定16毫秒逻辑步并限制单帧最大补算次数。" \
  -m "Prompt: 构建战斗垂直切片阶段1固定循环"
```

### Task 3: 类型化游戏与表现事件

**Files:**
- Create: `native/engine/core/event_queue.h`
- Modify: `native/gameplay/combat/event.h`
- Create: `tests/test_event_queue.cpp`

**Interfaces:**
- Consumes: 玩法系统产生的实体 ID、tick、事件类型和数值。
- Produces: `GameplayEvent`、`PresentationEvent`、`EventQueue<T>::push`、`EventQueue<T>::drain`。

- [ ] **Step 1: 编写失败测试**

创建 `tests/test_event_queue.cpp`：

```cpp
#include "../native/engine/core/event_queue.h"
#include "../native/gameplay/combat/event.h"
#include <cassert>

int main() {
  EventQueue<GameplayEvent> gameplay(2);
  assert(gameplay.push({5, 2, 8, GameplayEventType::Hit, fp(10), 1}));
  assert(gameplay.push({5, 2, 8, GameplayEventType::PoiseBreak, fp(2), 2}));
  assert(!gameplay.push({5, 2, 8, GameplayEventType::Death, 0, 3}));
  auto events = gameplay.drain();
  assert(events.size() == 2);
  assert(events[0].sequence == 1 && events[1].sequence == 2);
  assert(gameplay.drain().empty());

  EventQueue<PresentationEvent> presentation;
  assert(presentation.push({5, 2, 8, PresentationEventType::HitFlash, fp(1), 1}));
  assert(presentation.drain()[0].type == PresentationEventType::HitFlash);
  return 0;
}
```

- [ ] **Step 2: 运行测试并确认按预期失败**

Run:

```bash
clang++ -std=c++17 tests/test_event_queue.cpp -I. -o /tmp/test_event_queue
```

Expected: FAIL，提示 `event_queue.h` 或类型化事件不存在。

- [ ] **Step 3: 实现通用事件队列**

创建 `native/engine/core/event_queue.h`：

```cpp
#pragma once
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

template <typename T>
class EventQueue {
 public:
  explicit EventQueue(size_t capacity = 1024) : capacity_(capacity) {}

  bool push(const T& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.size() >= capacity_) return false;
    events_.push_back(event);
    return true;
  }

  std::vector<T> drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<T> result;
    result.swap(events_);
    return result;
  }

 private:
  size_t capacity_;
  std::mutex mutex_;
  std::vector<T> events_;
};
```

- [ ] **Step 4: 将占位整数事件替换为类型化事件**

在 `native/gameplay/combat/event.h` 中保留现有类型，并新增：

```cpp
enum class GameplayEventType : uint8_t {
  Hit,
  Damage,
  Dodge,
  Interrupt,
  PoiseBreak,
  AuraApplied,
  Resonance,
  PhaseChanged,
  Death,
  EncounterReset,
};

enum class PresentationEventType : uint8_t {
  HitFlash,
  CameraShake,
  DodgeFlash,
  CastBarBroken,
  PoiseBreakBurst,
  ResonanceBurst,
  PhaseTransition,
};

struct GameplayEvent {
  Tick tick = 0;
  EntityId source = 0;
  EntityId target = 0;
  GameplayEventType type = GameplayEventType::Hit;
  FixedPoint value = 0;
  uint32_t sequence = 0;
  bool operator==(const GameplayEvent& other) const {
    return tick == other.tick && source == other.source && target == other.target &&
           type == other.type && value == other.value && sequence == other.sequence;
  }
};

struct PresentationEvent {
  Tick tick = 0;
  EntityId source = 0;
  EntityId target = 0;
  PresentationEventType type = PresentationEventType::HitFlash;
  FixedPoint intensity = 0;
  uint32_t sequence = 0;
  bool operator==(const PresentationEvent& other) const {
    return tick == other.tick && source == other.source && target == other.target &&
           type == other.type && intensity == other.intensity && sequence == other.sequence;
  }
};
```

同时将 `CombatResult` 的两个成员改为：

```cpp
std::vector<GameplayEvent> gameplayEvents;
std::vector<PresentationEvent> presentationEvents;
```

- [ ] **Step 5: 运行新增和既有战斗测试**

Run:

```bash
clang++ -std=c++17 tests/test_event_queue.cpp -I. -o /tmp/test_event_queue && /tmp/test_event_queue
clang++ -std=c++17 tests/test_decision_log.cpp -I. -o /tmp/test_decision_log && /tmp/test_decision_log
clang++ -std=c++17 tests/test_event_order.cpp -I. -o /tmp/test_event_order && /tmp/test_event_order
```

Expected: 三个程序均 exit 0，无输出。

- [ ] **Step 6: 提交**

```bash
git add native/engine/core/event_queue.h native/gameplay/combat/event.h tests/test_event_queue.cpp tests/test_decision_log.cpp
git commit -m "refactor: 分离玩法与表现事件" \
  -m "用类型化事件替换CombatResult中的整数占位。" \
  -m "Prompt: 构建战斗垂直切片阶段1事件基础"
```

### Task 4: 线程安全 HUD 快照

**Files:**
- Create: `native/engine/core/game_snapshot.h`
- Create: `native/engine/core/snapshot_store.h`
- Create: `tests/test_snapshot_store.cpp`

**Interfaces:**
- Consumes: 游戏线程每次逻辑更新后的只读状态。
- Produces: `GameSnapshot`、`SnapshotStore::publish(const GameSnapshot&)`、`SnapshotStore::read()`。

- [ ] **Step 1: 编写失败测试**

创建 `tests/test_snapshot_store.cpp`：

```cpp
#include "../native/engine/core/snapshot_store.h"
#include <atomic>
#include <cassert>
#include <thread>

int main() {
  SnapshotStore store;
  store.publish({3, 90, 80, 0.25f, 0.75f, 59.5f, true, 4, 2, true});
  GameSnapshot snapshot = store.read();
  assert(snapshot.tick == 3 && snapshot.hp == 90 && snapshot.bossPhase == 2);

  std::atomic<bool> done{false};
  std::thread writer([&]() {
    for (Tick tick = 4; tick <= 100; ++tick) {
      store.publish({tick, 90, 80, 0.25f, 0.75f, 59.5f, true, 4, 2, true});
    }
    done = true;
  });
  Tick last = 0;
  while (!done) {
    Tick current = store.read().tick;
    assert(current >= last);
    last = current;
  }
  writer.join();
  assert(store.read().tick == 100);
  return 0;
}
```

- [ ] **Step 2: 运行测试并确认按预期失败**

Run:

```bash
clang++ -std=c++17 -pthread tests/test_snapshot_store.cpp -I. -o /tmp/test_snapshot_store
```

Expected: FAIL，提示 `snapshot_store.h` 不存在。

- [ ] **Step 3: 定义 HUD 快照**

创建 `native/engine/core/game_snapshot.h`：

```cpp
#pragma once
#include "tick_clock.h"
#include <cstdint>

struct GameSnapshot {
  Tick tick = 0;
  int32_t hp = 100;
  int32_t poise = 100;
  float playerX = 0.5f;
  float playerY = 0.5f;
  float fps = 0.0f;
  bool moving = false;
  int32_t targetId = 0;
  int32_t bossPhase = 0;
  bool rendererReady = false;
};
```

创建 `native/engine/core/snapshot_store.h`：

```cpp
#pragma once
#include "game_snapshot.h"
#include <mutex>

class SnapshotStore {
 public:
  void publish(const GameSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = snapshot;
  }

  GameSnapshot read() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
  }

 private:
  mutable std::mutex mutex_;
  GameSnapshot snapshot_{};
};
```

- [ ] **Step 4: 运行测试并确认通过**

Run:

```bash
clang++ -std=c++17 -pthread tests/test_snapshot_store.cpp -I. -o /tmp/test_snapshot_store && /tmp/test_snapshot_store
```

Expected: exit 0，无输出。

- [ ] **Step 5: 提交**

```bash
git add native/engine/core/game_snapshot.h native/engine/core/snapshot_store.h tests/test_snapshot_store.cpp
git commit -m "feat: 增加线程安全游戏快照" \
  -m "隔离Native可变状态与ArkUI HUD读取。" \
  -m "Prompt: 构建战斗垂直切片阶段1快照基础"
```

### Task 5: 将基础模块接入 Native Loop 与 N-API

**Files:**
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `InputQueue`，Task 2 的 `FixedStep`，Task 4 的 `SnapshotStore`。
- Produces: `Loop::enqueueInput`、`Loop::snapshot`，以及包含 `tick`、`targetId`、`bossPhase` 的 N-API 快照。

- [ ] **Step 1: 先编译现有纯 C++ 回归测试并保存基线**

Run:

```bash
set -e
clang++ -std=c++17 tests/test_source_aura.cpp native/gameplay/combat/source_aura.cpp -I. -o /tmp/test_source_aura
clang++ -std=c++17 tests/test_resonance.cpp native/gameplay/combat/resonance.cpp -I. -o /tmp/test_resonance
clang++ -std=c++17 tests/test_event_order.cpp -I. -o /tmp/test_event_order
clang++ -std=c++17 tests/test_decision_log.cpp -I. -o /tmp/test_decision_log
/tmp/test_source_aura
/tmp/test_resonance
/tmp/test_event_order
/tmp/test_decision_log
```

Expected: 全部 exit 0。若基线失败，先记录既有阻塞，不修改本任务外逻辑。

- [ ] **Step 2: 更新 Loop 接口**

在 `native/engine/core/loop.h` 中：

- 引入 `fixed_step.h` 和 `snapshot_store.h`。
- 增加 `FixedStep fixedStep{16, 4};`、`SnapshotStore snapshots;`、`std::atomic<uint64_t> inputSequence{0};`。
- 将 `tickOnce()` 改为 `void tickOnce(int64_t elapsedMs);`。
- 增加 `void updateFixed(Tick tick, int64_t dtMs);`。
- 增加：

```cpp
bool enqueueInput(InputAction action, int32_t pointerId, float x, float y) {
  return input.push({action, pointerId, x, y, inputSequence.fetch_add(1)});
}

GameSnapshot snapshot() const { return snapshots.read(); }
```

- [ ] **Step 3: 更新 Loop 编排实现**

在 `Loop::start()` 的线程函数中使用 `steady_clock` 计算实际 `elapsedMs`，限制单次传入 `tickOnce` 的值不超过 250 毫秒。

将 `tickOnce` 实现为：

```cpp
void Loop::tickOnce(int64_t elapsedMs) {
  processInput();
  fixedStep.advance(elapsedMs, [this](Tick tick, int64_t dtMs) {
    updateFixed(tick, dtMs);
  });
  surface_draw(surface);
  surface_swap(surface);
  snapshots.publish({
    fixedStep.tick(),
    100,
    100,
    surface.player.x,
    surface.player.y,
    fps,
    surface.player.moving,
    0,
    0,
    surface.ready,
  });
  // 保留现有 FPS 统计，但不再用固定 0.016f 驱动逻辑。
}

void Loop::updateFixed(Tick, int64_t dtMs) {
  updatePlayer(static_cast<float>(dtMs) / 1000.0f);
}
```

将 `processInput()` 的分支改为 `InputAction` 枚举；本阶段只让 PointerDown/PointerMove 保持现有点按移动行为，PointerUp/PointerCancel 停止移动，其余动作安全忽略。

- [ ] **Step 4: 更新 XComponent 输入映射**

在 `native_bridge.cpp` 增加独立映射函数：

```cpp
static InputAction MapTouchAction(OH_NativeXComponent_TouchEventType type) {
  switch (type) {
    case OH_NATIVEXCOMPONENT_DOWN: return InputAction::PointerDown;
    case OH_NATIVEXCOMPONENT_MOVE: return InputAction::PointerMove;
    case OH_NATIVEXCOMPONENT_UP: return InputAction::PointerUp;
    default: return InputAction::PointerCancel;
  }
}
```

`OnDispatchTouchEvent` 调用：

```cpp
g_loop.enqueueInput(MapTouchAction(touchEvent.type),
                    static_cast<int32_t>(touchEvent.id),
                    touchEvent.x,
                    touchEvent.y);
```

目标 SDK 6.1.0(23) 的 `OH_NativeXComponent_TouchEvent` 公开 `int32_t id` 字段，因此直接使用 `touchEvent.id`；若后续升级 SDK 导致编译失败，先检查该 SDK 的公开头文件再调整映射。

`NativePushInput` 暂将 ArkTS 数值类型映射到四种指针动作，并统一通过 `enqueueInput` 入队，不再直接调用 `input.push`。

- [ ] **Step 5: 从 SnapshotStore 导出 N-API 数据**

`NativePullSnapshot` 首行改为：

```cpp
const GameSnapshot snapshot = g_loop.snapshot();
```

所有字段从 `snapshot` 读取，并增加 `tick`、`targetId`、`bossPhase`。同步更新 `Index.d.ts` 与 `Bridge.ets`：

```typescript
export interface Snapshot {
  tick: number;
  hp: number;
  poise: number;
  x: number;
  y: number;
  fps: number;
  moving: boolean;
  targetDist: number;
  targetId: number;
  bossPhase: number;
  rendererReady: boolean;
}
```

`targetDist` 在阶段 1 保持 `0`，待输入与相机阶段改由目标系统计算。

- [ ] **Step 6: 更新 CMake 文件清单**

在 `entry/src/main/cpp/CMakeLists.txt` 对应分组中加入：

```cmake
${NATIVE_ROOT}/engine/core/fixed_step.h
${NATIVE_ROOT}/engine/core/game_snapshot.h
${NATIVE_ROOT}/engine/core/snapshot_store.h
${NATIVE_ROOT}/engine/core/event_queue.h
${NATIVE_ROOT}/engine/input/input_event.h
```

- [ ] **Step 7: 运行全部阶段 1 纯 C++ 测试**

Run:

```bash
set -e
clang++ -std=c++17 -pthread tests/test_input_queue.cpp -I. -o /tmp/test_input_queue
clang++ -std=c++17 tests/test_fixed_step.cpp -I. -o /tmp/test_fixed_step
clang++ -std=c++17 tests/test_event_queue.cpp -I. -o /tmp/test_event_queue
clang++ -std=c++17 -pthread tests/test_snapshot_store.cpp -I. -o /tmp/test_snapshot_store
clang++ -std=c++17 tests/test_source_aura.cpp native/gameplay/combat/source_aura.cpp -I. -o /tmp/test_source_aura
clang++ -std=c++17 tests/test_resonance.cpp native/gameplay/combat/resonance.cpp -I. -o /tmp/test_resonance
clang++ -std=c++17 tests/test_event_order.cpp -I. -o /tmp/test_event_order
clang++ -std=c++17 tests/test_decision_log.cpp -I. -o /tmp/test_decision_log
for test in /tmp/test_input_queue /tmp/test_fixed_step /tmp/test_event_queue \
  /tmp/test_snapshot_store /tmp/test_source_aura /tmp/test_resonance \
  /tmp/test_event_order /tmp/test_decision_log; do "$test"; done
```

Expected: 所有程序 exit 0，无断言失败。

- [ ] **Step 8: 在配置好 SDK 的 DevEco Terminal 构建 HAP**

Run:

```bash
hvigorw assembleHap
```

Expected: `BUILD SUCCESSFUL`。若独立 shell 无法发现 SDK，必须在 DevEco Studio 已配置的 Terminal 或 IDE Build Hap(s) 执行，并保存完整构建结果；不得把 SDK 发现失败误报为代码编译失败。

- [ ] **Step 9: 真机生命周期冒烟验证**

在真机执行：

1. 冷启动 5 次，每次等待 60 秒。
2. 前后台切换 5 次。
3. 退出并重新进入页面 5 次。
4. 确认 EGL 初始化、`GL_VERSION`、shader link 和 Surface ready 日志正常。
5. 确认无 `cppcrash`、无销毁后 swap、HUD 的 tick 持续递增。

Expected: 渲染与现有演示行为保持可用，HUD tick 递增，生命周期无崩溃。

- [ ] **Step 10: 提交集成结果**

```bash
git add native/engine/core/loop.h native/engine/core/loop.cpp \
  entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/CMakeLists.txt \
  entry/src/main/cpp/types/libnative_game/Index.d.ts \
  entry/src/main/ets/napi/Bridge.ets
git commit -m "refactor: 接入固定循环与HUD快照" \
  -m "保持GLES3生命周期并统一Native输入和状态出口。" \
  -m "Prompt: 完成战斗垂直切片阶段1基础集成"
```

## 阶段 1 完成检查

- [ ] 四个新增测试均经历 RED → GREEN。
- [ ] 既有三源、事件顺序和回放测试保持通过。
- [ ] 输入队列不存在未同步读写。
- [ ] 逻辑更新由实际经过时间驱动，并限制最大补算。
- [ ] GameplayEvent 与 PresentationEvent 已类型化分离。
- [ ] N-API 只读取 `SnapshotStore`，不直接读取渲染线程可变状态。
- [ ] HAP 构建结果已保存。
- [ ] 真机生命周期冒烟测试通过。
- [ ] Git 工作区只包含计划内改动。
