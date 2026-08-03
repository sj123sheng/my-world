# 人物缩放、视角灵敏度与按钮输入优先级 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将所有角色模型缩小为当前约 `1/3`，将右半屏视角灵敏度降至 `0.0035`，并使 ArkTS 按钮优先于底层 XComponent 处理触摸。

**Architecture:** 视觉尺寸仍集中在 `AssetProfile`，视角参数仍集中在 `CameraGestureConfig`。`CombatControls` 只在真实按钮节点上阻断命中，容器空白区保持透明，不改变 XComponent 作为移动/相机唯一生产输入源的约定。

**Tech Stack:** C++17、HarmonyOS ArkTS、OH_NativeXComponent、Node.js 契约测试、Hvigor/HAP、HDC 真机验收。

## Global Constraints

- Player、Enemy、Boss 的新 scale 分别是 `0.025/3`、`0.022/3`、`0.045/3`。
- `sensitivityX` 和 `sensitivityY` 都是 `0.0035f`，不新增加速曲线或设置页。
- 按钮节点使用 `HitTestMode.Block`；`CombatControls` 全屏根容器使用 `None`，按钮簇空白区和冷却环使用 `Transparent`。
- 不改变玩家移速、逻辑碰撞、攻击距离、软锁定、摄像机距离或已确认的坐标约定。
- 保留当前脏工作树的既有改动，特别是 `build-profile.json5`；不重置、覆盖或自动暂存。
- 因多个测试文件已包含用户未提交改动，实施阶段不自动创建代码提交；验证后由用户决定是否提交。

---

### Task 1: 将角色资源档案缩小为原尺寸的 `1/3`

**Files:**
- Modify: `tests/test_asset_profile.cpp:12-21`
- Modify: `native/engine/render/asset_profile.cpp:3-16`

**Interfaces:**
- Consumes: `AssetProfile AssetProfile::forModel(ModelKind kind)`
- Produces: Player `scale=0.025f/3.0f`，Enemy `scale=0.022f/3.0f`，Boss `scale=0.045f/3.0f`

- [ ] **Step 1: 先写精确缩放的失败测试**

在 `testProfilesProduceUsableActorTransforms()` 中把只检查正数的断言替换为：

```cpp
assert(nearlyEqual(player.scale, 0.025f / 3.0f));
assert(nearlyEqual(enemy.scale, 0.022f / 3.0f));
assert(nearlyEqual(boss.scale, 0.045f / 3.0f));
assert(boss.scale > player.scale);
assert(!nearlyEqual(boss.yawOffsetRadians, 0.0f));
```

- [ ] **Step 2: 运行测试并确认因旧 scale 失败**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-asset-profile.XXXXXX)
CXX_STDLIB=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
clang++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
  tests/test_asset_profile.cpp native/engine/render/asset_profile.cpp \
  -o "$TEST_BIN_DIR/test_asset_profile"
"$TEST_BIN_DIR/test_asset_profile"
```

Expected: 程序在 Player 的精确 scale 断言处失败，因为实际值仍是 `0.025f`。

- [ ] **Step 3: 实现最小 scale 变更**

将 `AssetProfile::forModel` 的三个 scale 写成：

```cpp
case ModelKind::Player:
  return {0.025f / 3.0f, 0.0f, {0.16f, 0.24f, 0.27f},
          {0.31f, 0.84f, 0.75f}, 0.75f, 0};
case ModelKind::Enemy:
  return {0.022f / 3.0f, 0.0f, {0.24f, 0.20f, 0.25f},
          {0.45f, 0.30f, 0.48f}, 0.35f, 1};
case ModelKind::Boss:
  return {0.045f / 3.0f, 3.14159265f, {0.18f, 0.16f, 0.22f},
          {0.72f, 0.39f, 0.66f}, 0.65f, 3};
```

- [ ] **Step 4: 重跑资源档案测试**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-asset-profile.XXXXXX)
CXX_STDLIB=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
clang++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
  tests/test_asset_profile.cpp native/engine/render/asset_profile.cpp \
  -o "$TEST_BIN_DIR/test_asset_profile"
"$TEST_BIN_DIR/test_asset_profile"
```

Expected: 退出码 `0`。

- [ ] **Step 5: 检查任务边界**

Run:

```bash
git diff -- native/engine/render/asset_profile.cpp tests/test_asset_profile.cpp
```

Expected: 只有三个 scale 值和对应精确断言改变；不更改 tint、outline、yaw offset 或 core mount。

---

### Task 2: 降低默认视角灵敏度

**Files:**
- Modify: `tests/test_touch_controls.cpp:86-110`
- Modify: `native/engine/input/camera_gesture.h:5-8`

**Interfaces:**
- Consumes: `CameraGesture::begin` / `move` / `consumeDelta`
- Produces: `CameraGestureConfig{}` 的 X/Y 灵敏度都为 `0.0035f`

- [ ] **Step 1: 先写默认灵敏度的失败测试**

在现有 CameraGesture 测试前加入：

```cpp
CameraGesture defaultCamera(CameraGestureConfig{});
defaultCamera.begin(20, {800.0f, 200.0f});
defaultCamera.move(20, {900.0f, 150.0f});
const Vec2 defaultDelta = defaultCamera.consumeDelta();
assert(std::abs(defaultDelta.x - 0.35f) < 0.0001f);
assert(std::abs(defaultDelta.y + 0.175f) < 0.0001f);
```

- [ ] **Step 2: 运行测试并确认因旧灵敏度失败**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-touch-controls.XXXXXX)
CXX_STDLIB=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
clang++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
  tests/test_touch_controls.cpp -o "$TEST_BIN_DIR/test_touch_controls"
"$TEST_BIN_DIR/test_touch_controls"
```

Expected: `defaultDelta.x` 实际为 `1.0f` 而不是 `0.35f`，断言失败。

- [ ] **Step 3: 实现默认灵敏度变更**

```cpp
struct CameraGestureConfig {
  float sensitivityX = 0.0035f;
  float sensitivityY = 0.0035f;
};
```

保留现有显式配置 `CameraGesture({0.01f, 0.01f})` 和非法数值测试，以确认构造参数仍可覆盖默认值。

- [ ] **Step 4: 重跑触摸控制测试**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-touch-controls.XXXXXX)
CXX_STDLIB=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
clang++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
  tests/test_touch_controls.cpp -o "$TEST_BIN_DIR/test_touch_controls"
"$TEST_BIN_DIR/test_touch_controls"
```

Expected: 退出码 `0`。

- [ ] **Step 5: 检查任务边界**

Run:

```bash
git diff -- native/engine/input/camera_gesture.h tests/test_touch_controls.cpp
```

Expected: 只改默认灵敏度与新的默认行为测试；不改 `move()` 的坐标符号、pointer 归属或增量消费。

---

### Task 3: 让 ArkTS 按钮优先消费触摸

**Files:**
- Modify: `tests/test_bridge_contract.mjs:30-35`
- Modify: `entry/src/main/ets/ui/CombatControls.ets:51-244`
- Modify: `entry/src/main/ets/pages/GamePage.ets:277-286`

**Interfaces:**
- Consumes: ArkUI `Button(...).hitTestBehavior(HitTestMode.Block)`
- Produces: 六个战斗按钮、调试入口和十个调试面板按钮均阻断该 pointer 下传；根节点不参与命中，非按钮区仍下传

- [ ] **Step 1: 先写按钮阻断命中的失败契约测试**

在 `buttonActions` 测试后加入：

```js
const blockingButtons = [
  '普攻', '闪避', '辉印', '脉流', '蚀质', '终结', '☰',
  '训练', '兽群', '混战', '守卫', '流程', '首领', '推进', '补给', '重试', '调试'
];
for (const label of blockingButtons) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*` +
      `\\.hitTestBehavior\\(HitTestMode\\.Block\\)(?:(?!Button\\().)*\\.onClick`, 's'),
    `${label} must block its pointer before invoking the action`);
}
assert.match(controls,
  /\.hitTestBehavior\(HitTestMode\.None\)\s*\n\s*}\s*\n}/,
  'CombatControls root must skip itself while preserving child button hit testing');
assert.doesNotMatch(page,
  /CombatControls\(\{[\s\S]*?ultimateWindowMs:\s*this\.ultimateWindowMs\s*\}\)\s*\.hitTestBehavior\(HitTestMode\.Transparent\)/,
  'GamePage must not override button-level blocking with an outer transparent hit-test mode');
```

- [ ] **Step 2: 运行契约测试并确认因按钮未 Block 失败**

Run:

```bash
node tests/test_bridge_contract.mjs
```

Expected: 首个战斗按钮报 `must block its pointer before invoking the action`。

- [ ] **Step 3: 在每个交互按钮上实现局部 Block**

在每个 `Button` 的 `.onClick(...)` 之前加入：

```ts
.hitTestBehavior(HitTestMode.Block)
```

具体覆盖：`终结`、`蚀质`、`脉流`、`辉印`、`闪避`、`普攻`、`☰`、`训练`、`兽群`、`混战`、`守卫`、`流程`、`首领`、`推进`、`补给`、`重试`、`调试`。

冷却环和按钮簇 Stack 继续使用：

```ts
.hitTestBehavior(HitTestMode.Transparent)
```

将 `CombatControls` 根 Stack 设为 `HitTestMode.None`，并删除 `GamePage` 挂载
`CombatControls` 后额外的 `.hitTestBehavior(HitTestMode.Transparent)`。

- [ ] **Step 4: 重跑桥接契约测试**

Run:

```bash
node tests/test_bridge_contract.mjs
```

Expected: 退出码 `0`。

- [ ] **Step 5: 构建 ArkTS 以确认 HitTestMode 使用合法**

Run:

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p product=default -p module=entry@default assembleHap \
  --analyze=normal --parallel --incremental
```

Expected: `BUILD SUCCESSFUL`。

---

### Task 4: 更新长期输入契约并完成全量验证

**Files:**
- Modify: `DECISIONS.md`
- Verify: `entry/build/default/outputs/default/entry-default-signed.hap`

**Interfaces:**
- Consumes: Tasks 1–3 的 scale、灵敏度和 UI 命中策略
- Produces: 可重现的自动化与真机验收证据

- [ ] **Step 1: 记录按钮与 XComponent 的长期边界**

在 `DECISIONS.md` 的“XComponent 是游戏触摸的唯一生产输入源”中追加：

```markdown
- ArkTS 可交互按钮必须在按钮节点使用 `HitTestMode.Block`，阻断该 pointer
  进入 XComponent；全屏控制根节点使用 `None`，按钮簇空白容器使用
  `Transparent`，使非按钮区仍可控制视角。
```

- [ ] **Step 2: 运行全部相关自动化测试**

Run:

```bash
set -e
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-gameplay-ux.XXXXXX)
CXX_STDLIB=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
COMMON_FLAGS=(-std=c++17 -isystem "$CXX_STDLIB" -I. -Inative -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))
clang++ "${COMMON_FLAGS[@]}" tests/test_asset_profile.cpp \
  native/engine/render/asset_profile.cpp -o "$TEST_BIN_DIR/asset_profile"
"$TEST_BIN_DIR/asset_profile"
clang++ "${COMMON_FLAGS[@]}" tests/test_touch_controls.cpp -o "$TEST_BIN_DIR/touch"
"$TEST_BIN_DIR/touch"
clang++ "${COMMON_FLAGS[@]}" tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o "$TEST_BIN_DIR/player"
"$TEST_BIN_DIR/player"
clang++ "${COMMON_FLAGS[@]}" tests/test_camera.cpp \
  native/engine/render/camera.cpp -o "$TEST_BIN_DIR/camera"
"$TEST_BIN_DIR/camera"
clang++ "${COMMON_FLAGS[@]}" tests/test_camera3d.cpp \
  native/engine/render/camera3d.cpp -o "$TEST_BIN_DIR/camera3d"
"$TEST_BIN_DIR/camera3d"
clang++ "${COMMON_FLAGS[@]}" tests/test_camera_render_transform.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/camera_render"
"$TEST_BIN_DIR/camera_render"
clang++ "${COMMON_FLAGS[@]}" tests/test_soft_targeting.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/targeting"
"$TEST_BIN_DIR/targeting"
clang++ "${COMMON_FLAGS[@]}" tests/test_render_animation.cpp \
  native/engine/render/skinned_model.cpp native/engine/render/asset_profile.cpp \
  native/engine/render/environment.cpp native/engine/render/texture.cpp \
  "${GAMEPLAY_SOURCES[@]}" -o "$TEST_BIN_DIR/render_animation"
"$TEST_BIN_DIR/render_animation"
node tests/test_bridge_contract.mjs
git diff --check
```

Expected: 所有可执行文件和 Node 契约测试均退出 `0`，`git diff --check` 无输出。

- [ ] **Step 3: 重新构建签名 HAP**

Run:

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p product=default -p module=entry@default assembleHap \
  --analyze=normal --parallel --incremental
```

Expected: `BUILD SUCCESSFUL`，产物为 `entry/build/default/outputs/default/entry-default-signed.hap`。

- [ ] **Step 4: 安装并启动真机产物**

Run:

```bash
HDC_BIN=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc
"$HDC_BIN" list targets
"$HDC_BIN" install -r entry/build/default/outputs/default/entry-default-signed.hap
"$HDC_BIN" shell aa force-stop com.ethelandev.myworld
"$HDC_BIN" shell aa start -a EntryAbility -b com.ethelandev.myworld
```

Expected: 设备 `2MN0224C12000754` 在线，安装和启动成功。

- [ ] **Step 5: 真机验收模型尺寸与视角灵敏度**

1. 打开调试 HUD，在默认 yaw 下截图，确认主角与训练假人体型约为修改前的 `1/3`。
2. 切换普通敌人和 Boss 遭遇并截图，确认它们同比缩小且 Boss 仍大于主角。
3. 在右半屏执行 `80px` 水平拖动，确认 HUD yaw 的目标变化约为 `0.28rad`，不再是旧值约 `0.8rad`。
4. 执行相同距离纵向拖动，确认 pitch 增量同样约为旧值的 `35%`。

- [ ] **Step 6: 真机验收按钮优先级和多指操作**

1. 记录调试 HUD 的 `cameraYaw`、`moveX/moveY` 和 `inputEventCount`。
2. 依次点击普攻、闪避、辉印、脉流、蚀质、终结：动作/冷却正常触发，`cameraYaw` 不改变，且 XComponent 不新增该按键 pointer 事件。
3. 开始一次左侧慢速长滑，在滑动未结束时点击普攻：HUD 仍显示非零 `moveX/moveY`，普攻动作触发，`cameraYaw` 不偏转。
4. 在右下按键之间的空白处拖动：`cameraYaw` 正常改变，证明未屏蔽整个按键簇。
5. 真机验收完成后关闭调试 HUD，重启应用留在干净启动界面。

- [ ] **Step 7: 最终工作树审计**

Run:

```bash
git status --short
git diff --stat
git diff --check
```

Expected: 仅包含本计划修改、先前已确认的移动/朝向改动及用户原有 `build-profile.json5` 改动；无未预期文件，无空白错误。
