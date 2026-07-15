# 阶段 2 输入与自由相机 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在阶段 1 固定循环之上交付确定性的双区多指触控、相机相对移动、自由第三人称相机和软锁定目标选择。

**Architecture:** ArkTS 与 N-API 只转发带指针 ID 的原始触控事件；Native 输入层将其解析为持续移动与逐 tick 镜头增量。固定循环依次更新玩家控制器、第三人称相机和软锁定，并通过线程安全快照向 ArkUI 暴露诊断状态。

**Tech Stack:** C++17、HarmonyOS NDK、ArkTS、N-API、XComponent、CMake、Node.js 契约测试

## Global Constraints

- 只实现阶段 2，不让攻击、闪避或三源输入产生战斗效果。
- 输入、角色控制、相机和软锁定核心必须可脱离 HarmonyOS/GLES 运行纯 C++ 测试。
- 输入事件继续按 `InputEvent::sequence` 顺序消费，固定 tick 是唯一玩法更新时间轴。
- 左右区域控制权在指针按下时确定，移动越区不得换权。
- 释放或取消一根指针不得改变另一根活动指针的状态。
- 保持现有 EGL/GLES3 WindowSurface 初始化和安全销毁顺序不变。
- `compatibleSdkVersion` 与 `targetSdkVersion` 保持 `6.1.0(23)`。

---

## 文件结构

- `native/engine/math/vec2.h`：有限值检查、向量运算、归一化和角度辅助。
- `native/engine/input/player_intent.h`：固定 tick 消费的规范化输入语义。
- `native/engine/input/touch_router.h`：活动指针职责与双区控制权。
- `native/engine/input/virtual_joystick.h`：左区触控到持续移动向量。
- `native/engine/input/camera_gesture.h`：右区触控到逐 tick 镜头增量。
- `native/gameplay/player/player_controller.h/.cpp`：相机相对的确定性角色移动。
- `native/engine/render/camera.h/.cpp`：第三人称轨道相机状态与固定步更新。
- `native/gameplay/targeting/soft_targeting.h/.cpp`：候选目标过滤和稳定评分。
- `native/engine/core/loop.h/.cpp`：编排输入、移动、相机、锁定和快照。
- `native/engine/core/game_snapshot.h`：阶段 2 诊断字段。
- `native/engine/render/surface.h/.cpp`：渲染状态读取新玩家/相机结果，不解释输入。
- `entry/src/main/cpp/native_bridge.cpp`：原始多指转发与快照字段导出。
- `entry/src/main/ets/napi/Bridge.ets`、`entry/src/main/cpp/types/libnative_game/Index.d.ts`、`entry/src/main/ets/pages/GamePage.ets`：桥接契约与调试状态。
- `entry/src/main/cpp/CMakeLists.txt`：注册新增 Native 源文件与目录。
- `tests/test_touch_controls.cpp`、`tests/test_player_controller.cpp`、`tests/test_camera.cpp`、`tests/test_soft_targeting.cpp`、`tests/test_loop_integration.cpp`、`tests/test_bridge_contract.mjs`：阶段 2 测试。

---

### Task 1: 双区多指输入语义

**Files:**
- Create: `native/engine/math/vec2.h`
- Create: `native/engine/input/player_intent.h`
- Create: `native/engine/input/touch_router.h`
- Create: `native/engine/input/virtual_joystick.h`
- Create: `native/engine/input/camera_gesture.h`
- Create: `tests/test_touch_controls.cpp`

**Interfaces:**
- Consumes: `InputEvent { action, pointerId, x, y, sequence }`。
- Produces: `Vec2`、`PlayerIntent`、`TouchRouter::handle(...)`、`VirtualJoystick::move()`、`CameraGesture::consumeDelta()`。

- [ ] **Step 1: 写失败测试，固定多指职责和意图规则**

```cpp
#include "native/engine/input/camera_gesture.h"
#include "native/engine/input/touch_router.h"
#include "native/engine/input/virtual_joystick.h"
#include <cassert>
#include <cmath>

int main() {
  TouchRouter router;
  assert(!router.handle({InputAction::PointerDown, 1, 100, 200, 0}, 0, 800));
  assert(router.handle({InputAction::PointerDown, 1, 100, 200, 1}, 1000, 800));
  assert(router.role(1) == TouchRole::Movement);
  assert(router.handle({InputAction::PointerDown, 2, 800, 200, 2}, 1000, 800));
  assert(router.role(2) == TouchRole::Camera);
  assert(router.handle({InputAction::PointerMove, 1, 900, 200, 3}, 1000, 800));
  assert(router.role(1) == TouchRole::Movement);
  assert(router.handle({InputAction::PointerUp, 2, 820, 210, 4}, 1000, 800));
  assert(router.role(1) == TouchRole::Movement);
  assert(router.role(2) == TouchRole::None);

  VirtualJoystick joystick({0.1f, 100.0f});
  joystick.begin(1, {100, 200});
  joystick.move(1, {105, 200});
  assert(joystick.value() == Vec2{});
  joystick.move(1, {200, 300});
  assert(std::abs(joystick.value().length() - 1.0f) < 0.0001f);
  joystick.end(1);
  assert(joystick.value() == Vec2{});

  CameraGesture camera({0.01f, 0.01f});
  camera.begin(2, {800, 200});
  camera.move(2, {820, 190});
  camera.move(2, {830, 180});
  assert(camera.consumeDelta() == Vec2{0.3f, -0.2f});
  assert(camera.consumeDelta() == Vec2{});
}
```

- [ ] **Step 2: 运行测试并确认因缺少头文件失败**

Run:

```bash
c++ -std=c++17 -I. tests/test_touch_controls.cpp -o /tmp/test_touch_controls
```

Expected: FAIL，提示 `camera_gesture.h`、`touch_router.h` 或 `virtual_joystick.h` 不存在。

- [ ] **Step 3: 实现最小数学与输入类型**

```cpp
// native/engine/math/vec2.h
#pragma once
#include <cmath>

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
  bool operator==(const Vec2& other) const {
    return std::abs(x - other.x) < 0.0001f && std::abs(y - other.y) < 0.0001f;
  }
  Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
  Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
  Vec2 operator*(float scale) const { return {x * scale, y * scale}; }
  float length() const { return std::sqrt(x * x + y * y); }
  bool finite() const { return std::isfinite(x) && std::isfinite(y); }
};

inline Vec2 ClampLength(Vec2 value, float maximum) {
  const float length = value.length();
  return length > maximum && length > 0.0f ? value * (maximum / length) : value;
}
```

```cpp
// native/engine/input/player_intent.h
#pragma once
#include "../math/vec2.h"

struct PlayerIntent {
  Vec2 move;
  Vec2 lookDelta;
  bool softLock = true;
};
```

`TouchRouter::handle(event, width, height)` 必须拒绝非正尺寸和非有限坐标；首次左/右区
指针分别分配 `Movement`/`Camera`，占用区域中的额外指针分配 `Ignored`。`PointerMove`
只接受已登记指针，`PointerUp`/`PointerCancel` 只移除对应登记项。提供 `clear()` 清空状态。

`VirtualJoystick` 必须保存活动 pointer ID 与起点；位移小于 `deadZone * radius` 时返回零，
否则将位移除以 radius 并限制长度为 1。`CameraGesture` 必须保存上次坐标并累积
`delta * sensitivity`，`consumeDelta()` 返回累计值后清零。

- [ ] **Step 4: 运行输入测试**

Run:

```bash
c++ -std=c++17 -I. tests/test_touch_controls.cpp -o /tmp/test_touch_controls
/tmp/test_touch_controls
```

Expected: PASS，进程退出码为 0。

- [ ] **Step 5: 提交输入语义**

```bash
git add native/engine/math/vec2.h native/engine/input/player_intent.h \
  native/engine/input/touch_router.h native/engine/input/virtual_joystick.h \
  native/engine/input/camera_gesture.h tests/test_touch_controls.cpp
git commit -m "feat: 增加双区多指输入语义" \
  -m "实现稳定指针职责、虚拟摇杆与逐tick镜头增量。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 2: 相机相对角色控制器

**Files:**
- Create: `native/gameplay/player/player_controller.h`
- Create: `native/gameplay/player/player_controller.cpp`
- Create: `tests/test_player_controller.cpp`
- Modify: `native/engine/render/surface.h`

**Interfaces:**
- Consumes: `Vec2 move`、`float cameraYaw`、`float dtSeconds`。
- Produces: `PlayerController::update(Player&, Vec2, float, float)`；`Player` 保留 `x/y/angle/moving`，移除触点目标语义。

- [ ] **Step 1: 写失败测试，固定相机相对移动**

```cpp
#include "native/gameplay/player/player_controller.h"
#include <cassert>
#include <cmath>

int main() {
  Player player;
  PlayerController controller({2.0f, 8.0f});
  controller.update(player, {0, 1}, 0.0f, 0.5f);
  assert(std::abs(player.x - 0.5f) < 0.0001f);
  assert(std::abs(player.y - 1.0f) < 0.0001f);
  player = {};
  controller.update(player, {0, 1}, 1.5707963f, 0.25f);
  assert(player.x > 0.99f);
  assert(std::abs(player.y - 0.5f) < 0.0001f);
  controller.update(player, {}, 1.5707963f, 0.25f);
  assert(!player.moving);
}
```

- [ ] **Step 2: 编译并确认缺少控制器失败**

Run: `c++ -std=c++17 -I. tests/test_player_controller.cpp native/gameplay/player/player_controller.cpp -o /tmp/test_player_controller`

Expected: FAIL，提示 `player_controller.h` 或实现不存在。

- [ ] **Step 3: 实现固定步移动**

```cpp
struct PlayerControllerConfig { float speed = 0.5f; float turnSpeed = 8.0f; };

class PlayerController {
 public:
  explicit PlayerController(PlayerControllerConfig config = {}) : config_(config) {}
  void update(Player& player, Vec2 move, float cameraYaw, float dtSeconds) const;
 private:
  PlayerControllerConfig config_;
};
```

实现必须先将输入限制为单位长度，再以相机前向 `(-sin(yaw), cos(yaw))` 和右向
`(cos(yaw), sin(yaw))` 合成世界方向。位置限制在 `[0,1]`；无输入或无效 `dt/yaw`
时只设置 `moving=false`。角色角度使用 `turnSpeed * dt` 限制单 tick 转角。

- [ ] **Step 4: 运行角色控制器测试**

Run:

```bash
c++ -std=c++17 -I. tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o /tmp/test_player_controller
/tmp/test_player_controller
```

Expected: PASS。

- [ ] **Step 5: 提交角色控制器**

```bash
git add native/gameplay/player/player_controller.h native/gameplay/player/player_controller.cpp \
  native/engine/render/surface.h tests/test_player_controller.cpp
git commit -m "feat: 增加相机相对角色移动" \
  -m "使用固定步意图驱动灰盒角色位置与朝向。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 3: 自由第三人称相机

**Files:**
- Modify: `native/engine/render/camera.h`
- Modify: `native/engine/render/camera.cpp`
- Create: `tests/test_camera.cpp`

**Interfaces:**
- Consumes: `Vec2 target`、`Vec2 lookDelta`、`float dtSeconds`。
- Produces: `ThirdPersonCamera::update(...)`、`yaw()`、`pitch()`、`distance()`、`target()`、`position()`。

- [ ] **Step 1: 写失败测试，固定角度限制、跟随与异常恢复**

```cpp
#include "native/engine/render/camera.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
  ThirdPersonCamera camera;
  camera.update({0.5f, 0.5f}, {1.0f, 100.0f}, 0.016f);
  assert(camera.yaw() == 1.0f);
  assert(camera.pitch() == camera.config().maxPitch);
  camera.setDistance(100.0f);
  assert(camera.distance() == camera.config().maxDistance);
  camera.update({1.0f, 1.0f}, {}, 0.1f);
  assert(camera.target().x > 0.5f && camera.target().x < 1.0f);
  camera.update({0.5f, 0.5f}, {std::numeric_limits<float>::infinity(), 0}, 0.016f);
  assert(std::isfinite(camera.yaw()));
  assert(camera.pitch() == camera.config().defaultPitch);
}
```

- [ ] **Step 2: 编译并确认旧 `Camera` 接口不匹配**

Run: `c++ -std=c++17 -I. tests/test_camera.cpp native/engine/render/camera.cpp -o /tmp/test_camera`

Expected: FAIL，提示 `ThirdPersonCamera` 未定义。

- [ ] **Step 3: 将占位 `Camera` 替换为第三人称轨道相机**

```cpp
struct ThirdPersonCameraConfig {
  float defaultYaw = 0.0f;
  float defaultPitch = 0.45f;
  float minPitch = -0.2f;
  float maxPitch = 1.1f;
  float defaultDistance = 0.35f;
  float minDistance = 0.2f;
  float maxDistance = 0.6f;
  float followSharpness = 10.0f;
};

class ThirdPersonCamera {
 public:
  explicit ThirdPersonCamera(ThirdPersonCameraConfig config = {});
  void update(Vec2 desiredTarget, Vec2 lookDelta, float dtSeconds);
  void setDistance(float distance);
  void reset();
  float yaw() const;
  float pitch() const;
  float distance() const;
  Vec2 target() const;
  Vec2 position() const;
  const ThirdPersonCameraConfig& config() const;
};
```

跟随系数使用 `1 - exp(-followSharpness * dt)`；任一输入或内部状态非有限时调用
`reset()`，恢复全部默认值。`position()` 从目标点、yaw、pitch 和 distance 计算二维灰盒
投影，本阶段不修改 GLES shader 为三维矩阵。

- [ ] **Step 4: 运行相机测试**

Run:

```bash
c++ -std=c++17 -I. tests/test_camera.cpp native/engine/render/camera.cpp -o /tmp/test_camera
/tmp/test_camera
```

Expected: PASS。

- [ ] **Step 5: 提交自由相机**

```bash
git add native/engine/render/camera.h native/engine/render/camera.cpp tests/test_camera.cpp
git commit -m "feat: 实现固定步自由相机" \
  -m "增加轨道角度限制、平滑跟随与异常状态恢复。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 4: 稳定软锁定目标选择

**Files:**
- Create: `native/gameplay/targeting/soft_targeting.h`
- Create: `native/gameplay/targeting/soft_targeting.cpp`
- Create: `tests/test_soft_targeting.cpp`

**Interfaces:**
- Consumes: 玩家位置、相机 yaw、`std::vector<TargetCandidate>`。
- Produces: `std::optional<TargetSelection> SoftTargeting::select(...) const`。

- [ ] **Step 1: 写失败测试，固定过滤、评分与 ID 决胜**

```cpp
#include "native/gameplay/targeting/soft_targeting.h"
#include <cassert>
#include <limits>

int main() {
  SoftTargeting targeting({1.0f, 1.0471976f});
  std::vector<TargetCandidate> candidates{{3, {0.5f, 0.8f}}, {2, {0.5f, 0.8f}}};
  auto selected = targeting.select({0.5f, 0.5f}, 0.0f, candidates);
  assert(selected && selected->id == 2);
  candidates.push_back({1, {0.5f, -0.5f}});
  assert(targeting.select({0.5f, 0.5f}, 0.0f, candidates)->id == 2);
  candidates = {{4, {std::numeric_limits<float>::infinity(), 0}}};
  assert(!targeting.select({0.5f, 0.5f}, 0.0f, candidates));
}
```

- [ ] **Step 2: 编译并确认缺少软锁定模块失败**

Run: `c++ -std=c++17 -I. tests/test_soft_targeting.cpp native/gameplay/targeting/soft_targeting.cpp -o /tmp/test_soft_targeting`

Expected: FAIL，提示目标模块不存在。

- [ ] **Step 3: 实现确定性目标选择**

```cpp
struct TargetCandidate { int32_t id = 0; Vec2 position; };
struct TargetSelection { int32_t id = 0; float distance = 0; float angle = 0; Vec2 direction; };
struct SoftTargetingConfig { float maxDistance = 0.75f; float maxAngle = 1.0471976f; };

class SoftTargeting {
 public:
  explicit SoftTargeting(SoftTargetingConfig config = {}) : config_(config) {}
  std::optional<TargetSelection> select(Vec2 player, float cameraYaw,
      const std::vector<TargetCandidate>& candidates) const;
 private:
  SoftTargetingConfig config_;
};
```

过滤 `id <= 0`、非有限位置、零距离、超距和超角候选；比较键严格使用
`(angle, distance, id)`，不得依赖容器迭代顺序。相机水平前向与 Task 2 保持一致。

- [ ] **Step 4: 运行软锁定测试**

Run:

```bash
c++ -std=c++17 -I. tests/test_soft_targeting.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o /tmp/test_soft_targeting
/tmp/test_soft_targeting
```

Expected: PASS。

- [ ] **Step 5: 提交软锁定模块**

```bash
git add native/gameplay/targeting/soft_targeting.h native/gameplay/targeting/soft_targeting.cpp \
  tests/test_soft_targeting.cpp
git commit -m "feat: 增加稳定软锁定选择" \
  -m "按夹角、距离和实体ID确定性筛选候选目标。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 5: 固定循环集成输入、移动、相机与锁定

**Files:**
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `tests/test_loop_integration.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1–4 的公开类型。
- Produces: `Loop::resetInput()`；快照字段 `moveX/moveY/cameraYaw/cameraPitch/targetDist`；固定循环完整链路。

- [ ] **Step 1: 扩展失败测试，固定集成链路与生命周期清理**

在 `tests/test_loop_integration.cpp` 中设置 `loop.surface.width=1000`、`height=800`，依次
入队左区 down/move 与右区 down/move，调用 `processInput()` 和 `updateFixed(1,16)`，断言：

```cpp
assert(loop.intent.move.length() > 0.0f);
assert(loop.camera.yaw() != 0.0f);
assert(loop.surface.player.moving);
loop.resetInput();
assert(loop.intent.move == Vec2{});
assert(loop.intent.lookDelta == Vec2{});
assert(loop.touchRouter.activeCount() == 0);
```

同时将 `GameSnapshot` 默认值测试扩展为：

```cpp
assert(initial.moveX == 0.0f && initial.moveY == 0.0f);
assert(initial.cameraYaw == 0.0f);
assert(initial.targetDist == 0.0f);
```

- [ ] **Step 2: 编译并确认新接口失败**

Run:

```bash
c++ -std=c++17 -I. tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp native/engine/render/camera.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp \
  -o /tmp/test_loop_integration
```

Expected: FAIL，提示 `intent`、`camera`、`resetInput` 或新快照字段不存在。

- [ ] **Step 3: 将 Loop 改为模块编排**

`Loop` 新增：

```cpp
TouchRouter touchRouter;
VirtualJoystick joystick;
CameraGesture cameraGesture;
PlayerIntent intent;
PlayerController playerController;
ThirdPersonCamera camera;
SoftTargeting softTargeting;
void resetInput();
```

`processInput()` 只按队列顺序路由事件并更新 joystick/cameraGesture；完成后写入持续
`intent.move` 并把 `consumeDelta()` 累积到 `intent.lookDelta`。`updateFixed()` 的固定顺序是：

1. 复制并清空当前 `lookDelta`。
2. `playerController.update(surface.player, intent.move, camera.yaw(), dt)`。
3. `camera.update({player.x, player.y}, lookDelta, dt)`。
4. 从 `surface.props` 构造 ID 从 1 开始的灰盒候选并执行 `softTargeting.select()`。
5. 保存当前目标结果供快照使用。

`stop()`、Surface 失效路径和显式取消调用 `resetInput()`。删除旧的触点目标移动逻辑及
`Player::targetX/targetY`；`surface.cpp` 删除旧目标环绘制，保持玩家和场景绘制路径可用。

快照追加：

```cpp
float moveX = 0.0f;
float moveY = 0.0f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;
float targetDist = 0.0f;
```

- [ ] **Step 4: 注册新增源码并运行集成测试**

在 `CMakeLists.txt` 增加 `engine/math`、`gameplay/targeting` include 目录以及
`player_controller.cpp`、`soft_targeting.cpp` 源文件。

Run:

```bash
c++ -std=c++17 -I. tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp native/engine/render/camera.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp \
  -o /tmp/test_loop_integration
/tmp/test_loop_integration
```

Expected: PASS。若 macOS 缺少 HarmonyOS `hilog`/GLES 头，给 `loop.cpp` 增加现有测试可用的
平台条件编译，不得用测试桩替换玩法逻辑。

- [ ] **Step 5: 提交循环集成**

```bash
git add native/engine/core/loop.h native/engine/core/loop.cpp \
  native/engine/core/game_snapshot.h native/engine/render/surface.h \
  native/engine/render/surface.cpp tests/test_loop_integration.cpp \
  entry/src/main/cpp/CMakeLists.txt
git commit -m "refactor: 集成输入相机固定循环" \
  -m "按固定顺序更新角色、相机、软锁定与诊断快照。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 6: 多指 ArkTS 桥接与快照契约

**Files:**
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: XComponent `TouchEvent.touches`/`changedTouches` 与 Task 5 快照。
- Produces: 每根变化指针一条 `pushInput`；ArkTS `Snapshot` 与 Native 对象字段一致。

> 2026-07-15 真机修正：带 `libraryname` 的 Native XComponent 在目标真机没有触发 ArkTS
> `.onTouch`。当前生产链路已经改为 Native `DispatchTouchEvent` 逐点采集；本任务以下
> ArkTS 转发步骤仅保留为历史实施记录，不再描述当前架构。

- [ ] **Step 1: 扩展契约测试并确认失败**

在 `tests/test_bridge_contract.mjs` 中将快照字段检查扩展为：

```js
for (const field of ['tick', 'moveX', 'moveY', 'cameraYaw', 'cameraPitch',
  'targetDist', 'targetId', 'bossPhase']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`));
  assert.match(nativeBridge, new RegExp(`"${field}"`));
}
assert.match(page, /changedTouches/,
  'GamePage must forward every changed pointer');
assert.match(page, /pointerId:\s*touch\.id/,
  'GamePage must preserve pointer ids');
```

Run: `node tests/test_bridge_contract.mjs`

Expected: FAIL，提示阶段 2 字段或多指转发缺失。

- [ ] **Step 2: 实现多指事件转发**

`GamePage.ets` 的 `.onTouch` 遍历 `event.changedTouches`；若当前 SDK 类型未暴露该字段，
使用 `event.touches` 并对 UP/CANCEL 保留事件携带的变化指针。每次调用必须传：

```ts
pushInput({
  type: event.type as number,
  pointerId: touch.id,
  x: touch.windowX,
  y: touch.windowY
});
```

不得继续固定读取 `touches[0]`，也不得省略 `pointerId`。

- [ ] **Step 3: 同步快照类型、初始化、轮询和 Native 导出**

在 ArkTS `Snapshot`、`.d.ts`、GamePage 初始对象和 State 中增加：

```ts
moveX: number;
moveY: number;
cameraYaw: number;
cameraPitch: number;
```

`native_bridge.cpp` 的 `targetDist` 改从 `snapshot.targetDist` 读取，并导出四个新字段。
GamePage 每次轮询同步所有字段。HUD 暂不增加正式控件，只允许在现有调试信息中显示。

- [ ] **Step 4: 运行桥接契约测试**

Run: `node tests/test_bridge_contract.mjs`

Expected: PASS。

- [ ] **Step 5: 提交桥接契约**

```bash
git add entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/napi/Bridge.ets \
  entry/src/main/cpp/types/libnative_game/Index.d.ts entry/src/main/cpp/native_bridge.cpp \
  tests/test_bridge_contract.mjs
git commit -m "feat: 接入多指输入与相机快照" \
  -m "完整转发指针ID并同步阶段2调试字段。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

### Task 7: 阶段 2 全量验证与文档收口

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md`
- Modify: `docs/superpowers/plans/2026-07-15-input-camera.md`
- Write: `.superpowers/sdd/task-7-report.md`

**Interfaces:**
- Consumes: 前六个任务的最终实现。
- Produces: 可复现测试结果、HAP 构建结果和阶段 2 完成记录。

- [x] **Step 1: 运行阶段 2 纯 C++ 测试**

Run:

```bash
c++ -std=c++17 -I. tests/test_touch_controls.cpp -o /tmp/test_touch_controls && /tmp/test_touch_controls
c++ -std=c++17 -I. tests/test_player_controller.cpp native/gameplay/player/player_controller.cpp -o /tmp/test_player_controller && /tmp/test_player_controller
c++ -std=c++17 -I. tests/test_camera.cpp native/engine/render/camera.cpp -o /tmp/test_camera && /tmp/test_camera
c++ -std=c++17 -I. tests/test_soft_targeting.cpp native/gameplay/targeting/soft_targeting.cpp -o /tmp/test_soft_targeting && /tmp/test_soft_targeting
```

Expected: 四个测试均退出码 0。

Result (2026-07-15): PASS。除上述四项外，按仓库当前内容将
`tests/test_loop_integration.cpp` 及全部既有 `tests/test_*.cpp` 一并纳入，最终 19/19 通过。
本机使用显式 macOS SDK clang、libc++ 头目录和 `-I. -Inative`；逐项命令见 Task 7 报告。

- [x] **Step 2: 运行阶段 1 与桥接回归测试**

Run:

```bash
set -e
clang++ -std=c++17 tests/test_config_schema.cpp -I. -o /tmp/test_config_schema
clang++ -std=c++17 tests/test_decision_log.cpp -I. -o /tmp/test_decision_log
clang++ -std=c++17 tests/test_event_order.cpp -I. -o /tmp/test_event_order
clang++ -std=c++17 tests/test_event_queue.cpp -I. -o /tmp/test_event_queue
clang++ -std=c++17 tests/test_fence_wait.cpp native/platform/harmony/fence_wait.cpp -I. -o /tmp/test_fence_wait
clang++ -std=c++17 tests/test_fixed_step.cpp -I. -o /tmp/test_fixed_step
clang++ -std=c++17 -pthread tests/test_input_queue.cpp -I. -o /tmp/test_input_queue
clang++ -std=c++17 tests/test_loop_lifecycle.cpp -I. -o /tmp/test_loop_lifecycle
clang++ -std=c++17 tests/test_pointer_input.cpp -I. -o /tmp/test_pointer_input
clang++ -std=c++17 tests/test_resonance.cpp native/gameplay/combat/resonance.cpp -I. -o /tmp/test_resonance
clang++ -std=c++17 tests/test_resource_manifest.cpp native/engine/resource/loader.cpp -I. -o /tmp/test_resource_manifest
clang++ -std=c++17 tests/test_save_atomic.cpp native/engine/resource/save.cpp -I. -o /tmp/test_save_atomic
clang++ -std=c++17 -pthread tests/test_snapshot_store.cpp -I. -o /tmp/test_snapshot_store
clang++ -std=c++17 tests/test_source_aura.cpp native/gameplay/combat/source_aura.cpp -I. -o /tmp/test_source_aura
for test in /tmp/test_config_schema /tmp/test_decision_log /tmp/test_event_order \
  /tmp/test_event_queue /tmp/test_fence_wait /tmp/test_fixed_step /tmp/test_input_queue \
  /tmp/test_loop_lifecycle /tmp/test_pointer_input /tmp/test_resonance \
  /tmp/test_resource_manifest /tmp/test_save_atomic /tmp/test_snapshot_store \
  /tmp/test_source_aura; do "$test"; done
node tests/test_bridge_contract.mjs
```

Expected: 全部退出码 0。不得因阶段 2 删除或跳过阶段 1 测试。

Result (2026-07-15): PASS。阶段 1/桥接回归未删除或跳过；C++ 总计 19/19，Node 契约
1/1，全部退出码 0。

- [x] **Step 3: 构建 HarmonyOS HAP**

Run:

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal --parallel --incremental --daemon
```

Expected: `BUILD SUCCESSFUL`。若本机 SDK/签名环境阻塞，记录完整错误与已通过的纯 C++
范围，不得将环境阻塞描述为代码通过。

Result (2026-07-15): PASS。Hvigor 输出 `BUILD SUCCESSFUL in 7 s 610 ms`，产物为
`entry/build/default/outputs/default/entry-default-unsigned.hap`。本机未配置签名，构建跳过
`hos_hap` 签名；因此 HAP 组装通过，但签名安装与真机运行不在此结果内。另以 OpenHarmony
toolchain 独立完成 OHOS arm64-v8a `native_game` 全量编译和链接。

- [x] **Step 4: 更新阶段状态与真机清单**

README 当前能力增加“双区多指输入、相机相对移动、自由相机与确定性软锁定”；路线图在
阶段 2 增加完成提交与自动化结果。保留以下真机待验项，未实际执行不得勾选：

```text
[ ] 左手持续移动时右手可同时环绕和俯仰
[ ] 任一指针释放不影响另一侧控制
[ ] 相机无跳变、翻转或非有限状态
[ ] 目标越界后软锁定自动清除
```

- [x] **Step 5: 检查差异并提交收口**

Run:

```bash
git diff --check
git status --short
```

Expected: 无空白错误，仅显示本任务文档变更。

```bash
git add README.md docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md \
  docs/superpowers/plans/2026-07-15-input-camera.md
git commit -m "docs: 完成阶段2输入相机验收" \
  -m "记录自动化验证结果与剩余真机检查项。" \
  -m "Prompt: 继续战斗垂直切片阶段2开发"
```

## 计划自检

- 规格覆盖：双区多指、摇杆、相机相对移动、自由相机、软锁定、快照、生命周期清理、
  自动化和真机出口均有对应任务。
- 范围控制：攻击类动作只保留枚举，不进入任何阶段 2 更新路径。
- 类型一致：所有模块统一使用 `Vec2`；软锁定统一返回 `std::optional<TargetSelection>`；
  快照字段统一为 `moveX/moveY/cameraYaw/cameraPitch/targetDist/targetId`。
- 依赖顺序：Task 1 输入语义 → Task 2 控制器 → Task 3 相机 → Task 4 锁定 → Task 5
  循环集成 → Task 6 桥接 → Task 7 验收，每个任务均有独立测试和提交边界。

## 最终审查修复（2026-07-15）

- [x] 新增纯 C++ `CameraRenderState` 世界到视图变换。默认 target `(0.5, 0.5)`、默认
  yaw/pitch/distance 保持原灰盒画面不变；yaw 旋转、pitch 纵向投影与 distance 缩放均可见。
- [x] `Loop::updateFixed()` 将 `ThirdPersonCamera::renderState()` 传给 `Surface`；GL 与软件
  路径的网格、props、particles、player 均读取相同变换，不扩展三维 shader。
- [x] 触控生产者收敛为 ArkTS `changedTouches` → N-API；Native XComponent
  `DispatchTouchEvent` 显式置空，删除 `GetTouchEvent` 与 Native 触控入队路径。
- [x] `Loop::enqueueInput()` 用独立 mutex 在线性化临界区内完成 sequence 分配和
  `InputQueue::push()`，多生产者回归验证出队 sequence 严格单调连续。
- [x] `ThirdPersonCameraConfig` 的 min/max distance 任一非有限、零、负或倒置时，距离配置
  整体回退内建安全默认。
- [x] 新增独立渲染变换测试及 Loop 集成断言；最终矩阵为 C++ 20/20、Node 1/1，
  OHOS arm64 `native_game` 完整链接和 unsigned HAP 组装通过。
- [x] 最终复审统一 yaw 坐标约定：控制器 `right=(cos,-sin)`、`forward=(sin,cos)`，
  渲染逆变换为 `viewX=cos*dx-sin*dy`、`viewY=sin*dx+cos*dy`；角色视图朝向由世界
  朝向向量经同一变换后 `atan2` 得到。
- [x] 玩家和粒子定义为屏幕空间 billboard：位置仍应用 target/yaw/pitch/distance，尺寸只随
  distance 缩放；通过显式 NDC x/y 半径保证 GL 与软件路径在非方形视口中均为像素圆形。
- [x] 跨模块测试验证多组任意 yaw 下 controller move 往返视图方向不变，以及
  SoftTargeting 正前方候选稳定落在视图前轴。

## 真机输入生产者修复（2026-07-15）

- [x] 契约测试要求 `GamePage` 不含 `.onTouch` 或 `pushInput` 生产路径。
- [x] Native 回调注册 `OnDispatchTouchEvent` 并调用
  `OH_NativeXComponent_GetTouchEvent`。
- [x] `numPoints` 限制在 SDK 触点数组容量内，并逐点按各自 `type/id/x/y` 映射、入队；
  零点事件回退顶层 `type/id/x/y`，未知动作不入队。
- [x] `Bridge.ets` 的 `pushInput` API 保留为测试或未来外部输入入口，但页面不再调用，
  生产环境只有 Native XComponent 一个触控生产者。
- [ ] signed HAP 安装后，以单指和双指注入复验 HUD `moveX/moveY`、玩家位置、相机角度
  与画面变化；自动化及构建结果不能替代此设备出口。
