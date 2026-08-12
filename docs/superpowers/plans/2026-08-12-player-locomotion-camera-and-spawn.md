# 出生安全区、主角移动动画与自由环绕视角 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除出生台地的追击型侦察敌，让主角以真实速度平滑切换待机、走路和跑步，并在停稳后允许相机 360° 环绕查看主角正面。

**Architecture:** `assets/world/world.json` 继续作为刷怪布局事实来源；`PlayerController` 暴露只读基础速度，Loop 用平滑后的真实速度发布归一化移动比例；每个 `SkinnedAnimationState` 独立维护走跑步态和实际 clip 混合状态；软锁定保留选敌与战斗路由，但不再覆盖停步主角朝向。

**Tech Stack:** HarmonyOS Native C++17、现有 GLM/蒙皮动画管线、Node.js 世界布局生成器、macOS clang++ 宿主测试、Hvigor/CMake。

## Global Constraints

- 只删除 `sz_spawn_scout`；训练假人和其他区域刷怪点必须保留。
- 不修改敌人全局 AI、感知、攻击、仇恨、脱战或重生规则。
- 移动中继续使用相机相对方向；完全停稳后保持最后移动朝向。
- 软锁定继续驱动选敌、攻击路由、目标表现和探索/战斗镜头距离。
- 走跑迟滞阈值固定为：Run 降到 `< 0.30` 才进入 Walk，Walk 升到 `> 0.40` 才进入 Run，首次移动以 `0.35` 为分界。
- 移动 clip 互切混合 `0.15s`；进入主动动作 `0.12s`；主动动作恢复 `0.20s`；进入死亡 `0.25s`。
- 主角缺少 `walk` 时兼容 `Walking_B`，两者都缺少时回退 `run`；不得新增动画资产。
- 不改变角色移速、疾跑消耗、碰撞、战斗数值或存档格式。
- 所有生产改动必须先有能观察到预期失败的测试；提交信息必须包含变更类型、简述和 `Prompt:` 行。

## File Structure

- `assets/world/world.json`：删除出生侦察敌的唯一源配置。
- `native/generated/world_layout.gen.h`：由生成器同步世界布局，不手工维护。
- `tests/test_world_layout_gen.cpp`：锁定出生安全区和其他区域刷怪仍存在。
- `tests/test_wild_spawn_system.cpp`：从真实生成布局验证出生分块没有敌人。
- `native/gameplay/player/player_controller.h`：提供 `float speed() const` 只读接口。
- `native/engine/core/loop.cpp`：发布真实速度比例，并移除停步软锁定朝向覆盖。
- `native/engine/render/render_animation.h`：定义每实例步态决策、迟滞阈值、clip 回退与转场时长。
- `native/engine/render/skinned_model.h/.cpp`：每实体保存步态，并按实际 clip 变化交叉混合。
- `tests/test_player_controller.cpp`、`tests/test_render_animation.cpp`、`tests/test_skinned_model.cpp`、`tests/test_loop_integration.cpp`：分别覆盖速度接口、步态纯函数、蒙皮中间姿态和 Loop 数据流/相机行为。
- `PROJECT_STATE.md`、`TASKS.md`：实现验证后记录已完成体验修复和仍需真机验收项；`DECISIONS.md` 已在设计提交中记录长期决策，无需重复追加。

---

### Task 1: 删除出生侦察敌并锁定出生安全区

**Files:**
- Modify: `tests/test_world_layout_gen.cpp`
- Modify: `tests/test_wild_spawn_system.cpp`
- Modify: `assets/world/world.json`
- Generate: `native/generated/world_layout.gen.h`

**Interfaces:**
- Consumes: `WorldLayout::kSpawnZones`、`WorldLayout::kSpawnZoneCount`、`WildSpawnSystem(std::vector<WorldSpawnZoneDef>)`。
- Produces: 不含 `sz_spawn_scout` 的生成布局；出生分块激活后安全半径内野外敌人为零。

- [ ] **Step 1: 在布局测试中写出生安全区失败断言**

在 `tests/test_world_layout_gen.cpp` 的刷怪区检查旁加入：

```cpp
bool hasSpawnScout = false;
bool hasWestPack = false;
for (const auto& zone : WL::kSpawnZones) {
  hasSpawnScout = hasSpawnScout || zone.zoneId == "sz_spawn_scout";
  hasWestPack = hasWestPack || zone.zoneId == "sz_west_pack";
}
assert(!hasSpawnScout);
assert(hasWestPack);
assert(WL::kSpawnZoneCount == 7);
```

该测试会在旧生成布局仍含出生侦察敌时失败；`hasWestPack` 防止误删全部野外敌人。

- [ ] **Step 2: 编译执行布局测试，确认 RED**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-spawn-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_world_layout_gen.cpp \
  -o "$TEST_BIN_DIR/world_layout"
"$TEST_BIN_DIR/world_layout"
```

Expected: 进程因 `assert(!hasSpawnScout)` 失败而非零退出。

- [ ] **Step 3: 在真实刷怪系统测试中增加出生分块行为断言**

在 `tests/test_wild_spawn_system.cpp` 加入并从 `main()` 调用：

```cpp
void testGeneratedLayoutKeepsSpawnPlateauSafe() {
  std::vector<WorldLayout::WorldSpawnZoneDef> zones(
      WorldLayout::kSpawnZones.begin(), WorldLayout::kSpawnZones.end());
  WildSpawnSystem wild(zones);
  const std::vector<int32_t> spawnChunk{4};
  const Vec2 player{0.50f, 0.12f};
  wild.update(makeInput(kStepMs, player, &spawnChunk));

  for (const WildEnemySnapshot& enemy : wild.snapshot()) {
    assert((enemy.position - player).length() >= 0.15f);
  }
  assert(wild.snapshot().empty());
}
```

这里使用世界 8×8 网格的出生坐标 `(0.50, 0.12)`，对应 chunk id `4`。旧布局会生成 `sz_spawn_scout`，因此测试失败。

- [ ] **Step 4: 编译执行刷怪测试，确认 RED**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-wild-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_wild_spawn_system.cpp \
  native/gameplay/ai/wild_spawn_system.cpp \
  native/gameplay/ai/enemy_agent.cpp \
  native/gameplay/ai/enemy_archetypes.cpp \
  native/gameplay/ai/perception_system.cpp \
  native/gameplay/ai/decision_policy.cpp \
  native/gameplay/ai/tactical_planner.cpp \
  native/gameplay/ai/action_executor.cpp \
  native/gameplay/ai/combat_region.cpp \
  native/gameplay/combat/training_target.cpp \
  -o "$TEST_BIN_DIR/wild_spawn"
"$TEST_BIN_DIR/wild_spawn"
```

Expected: 新增出生安全区断言失败，不是编译或链接错误。

- [ ] **Step 5: 删除 JSON 中唯一的出生侦察区并重新生成**

从 `assets/world/world.json` 的 `spawnZones` 删除整个对象：

```json
{
  "zoneId": "sz_spawn_scout",
  "districtId": "spawn_plateau",
  "archetype": "RiftClaw",
  "count": 1,
  "positions": [[0.58, 0.08]],
  "patrolCenter": [0.58, 0.08],
  "aggroGroup": "spawn_scout",
  "respawnMs": 30000
}
```

然后只用生成器更新头文件：

```bash
node automation/assets/generate_world_layout.mjs
```

确认生成器退出 `0`，`native/generated/world_layout.gen.h` 的 `kSpawnZoneCount` 为 `7`，且首个正常区域仍为 `sz_west_pack`。

- [ ] **Step 6: 运行 GREEN 和生成器幂等检查**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-spawn-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_world_layout_gen.cpp \
  -o "$TEST_BIN_DIR/world_layout"
"$TEST_BIN_DIR/world_layout"
cp native/generated/world_layout.gen.h "$TEST_BIN_DIR/world_layout.before.h"
node automation/assets/generate_world_layout.mjs
cmp "$TEST_BIN_DIR/world_layout.before.h" native/generated/world_layout.gen.h
```

继续重建并运行真实刷怪测试：

```bash
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_wild_spawn_system.cpp \
  native/gameplay/ai/wild_spawn_system.cpp \
  native/gameplay/ai/enemy_agent.cpp \
  native/gameplay/ai/enemy_archetypes.cpp \
  native/gameplay/ai/perception_system.cpp \
  native/gameplay/ai/decision_policy.cpp \
  native/gameplay/ai/tactical_planner.cpp \
  native/gameplay/ai/action_executor.cpp \
  native/gameplay/ai/combat_region.cpp \
  native/gameplay/combat/training_target.cpp \
  -o "$TEST_BIN_DIR/wild_spawn"
"$TEST_BIN_DIR/wild_spawn"
```

Expected: 两个测试均退出 `0`，`cmp` 无输出。

- [ ] **Step 7: 提交出生安全区增量**

```bash
git add assets/world/world.json native/generated/world_layout.gen.h \
  tests/test_world_layout_gen.cpp tests/test_wild_spawn_system.cpp
git commit -m "fix: 移除出生台地追击侦察敌" \
  -m "保留训练假人与其他区域刷怪，并增加出生安全区回归测试。" \
  -m "Prompt: 移除主角身边持续贴身跟随的敌人"
```

### Task 2: 用平滑后的真实速度发布移动比例

**Files:**
- Modify: `native/gameplay/player/player_controller.h`
- Modify: `tests/test_player_controller.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `tests/test_loop_integration.cpp`

**Interfaces:**
- Consumes: `Player::velocity`、本帧传给 `PlayerController::update` 的 `speedScale`。
- Produces: `float PlayerController::speed() const`；`ActorRenderState.moveRatio` 为 `clamp(|velocity| / (speed() * speedScale), 0, 1)`，非法分母返回 `0`。

- [ ] **Step 1: 为基础速度只读接口写失败测试**

在 `tests/test_player_controller.cpp` 加入：

```cpp
PlayerController configured({0.42f, 8.0f, 16.0f, 18.0f});
assert(close(configured.speed(), 0.42f));
PlayerController negativeSpeed({-2.0f, 8.0f, 16.0f, 18.0f});
assert(close(negativeSpeed.speed(), 0.0f));
PlayerController invalidSpeed(
    {std::numeric_limits<float>::infinity(), 8.0f, 16.0f, 18.0f});
assert(close(invalidSpeed.speed(), 0.0f));
```

接口返回与控制器实际使用相同的非负基础速度，避免 Loop 复制 `0.3f`。

- [ ] **Step 2: 编译确认 `speed()` 尚不存在**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-speed-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o "$TEST_BIN_DIR/player"
```

Expected: 编译失败，提示 `PlayerController` 没有 `speed` 成员。

- [ ] **Step 3: 添加最小只读接口并统一控制器速度钳制**

在 `PlayerController` 公有区增加：

```cpp
float speed() const {
  return std::isfinite(config_.speed) ? std::max(0.0f, config_.speed) : 0.0f;
}
```

并让 `update()` 的有效速度使用同一接口：

```cpp
const float effectiveSpeed =
    speed() * std::max(0.0f,
        std::isfinite(speedScale) ? speedScale : 1.0f);
```

在头文件补 `<algorithm>`、`<cmath>`；测试文件补 `<limits>`。不得改变默认速度值和加减速曲线。

- [ ] **Step 4: 运行控制器测试确认 GREEN**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-speed-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o "$TEST_BIN_DIR/player"
"$TEST_BIN_DIR/player"
```

Expected: 退出 `0`。

- [ ] **Step 5: 为 Loop 真实速度比例写失败集成测试**

在 `tests/test_loop_integration.cpp` 建立隔离 Loop：

```cpp
Loop locomotionLoop;
isolateWildSpawns(locomotionLoop);
locomotionLoop.intent.move = {0.0f, 1.0f};
locomotionLoop.updateFixed(1, 16);
const float firstRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
assert(firstRatio > 0.0f && firstRatio < 0.5f);

for (Tick tick = 2; tick <= 40; ++tick) {
  locomotionLoop.updateFixed(tick, 16);
}
const float settledRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
assert(settledRatio > 0.95f && settledRatio <= 1.0f);

locomotionLoop.intent.move = {};
locomotionLoop.updateFixed(41, 16);
const float releaseRatio = locomotionLoop.surface.player3dAnimation.moveRatio;
assert(releaseRatio > 0.0f && releaseRatio < settledRatio);
for (Tick tick = 42; locomotionLoop.surface.player.moving && tick < 100;
     ++tick) {
  locomotionLoop.updateFixed(tick, 16);
}
assert(locomotionLoop.surface.player3dAnimation.moveRatio == 0.0f);
```

旧实现第一帧直接发布摇杆幅度 `1.0`，因此 `firstRatio < 0.5` 会失败。

- [ ] **Step 6: 编译/运行 Loop 测试确认 RED**

从当前源码直接重建宿主安全生产源并链接 Loop 测试，不复用旧二进制：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-loop-speed-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
```

Expected: 失败于 `firstRatio < 0.5f`，不是编译或链接错误。

- [ ] **Step 7: 在 Loop 以真实速度和本帧倍率发布比例**

在 `Loop::updateFixed` 中把疾跑倍率存为同一个局部值并同时传给控制器和动画归一化：

```cpp
const float playerSpeedScale =
    motionState.sprinting
        ? explorationMotion.config().sprintSpeedMultiplier
        : 1.0f;
playerController.update(surface.player, intent.move, camera.yaw(),
                        dtSeconds, playerSpeedScale, turnSpeedScale);
```

在发布 `moveRatio` 处改为：

```cpp
const float maximumPlayerSpeed = playerController.speed() * playerSpeedScale;
const float actualPlayerSpeed = surface.player.velocity.length();
surface.player3dAnimation.moveRatio =
    std::isfinite(actualPlayerSpeed) &&
            std::isfinite(maximumPlayerSpeed) && maximumPlayerSpeed > 0.0f
        ? std::clamp(actualPlayerSpeed / maximumPlayerSpeed, 0.0f, 1.0f)
        : 0.0f;
```

这里不修改 `surface.player3dAnimation.moving` 和 `locomotionRateScale = 0.65f`。

- [ ] **Step 8: 运行 GREEN 与相机相对移动回归**

先从当前源码重建 Loop 测试：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-loop-speed-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
```

Expected: `test_loop_integration` 退出 `0`。再运行控制器与相机相对移动测试：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-speed-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
clang++ "${COMMON[@]}" tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o "$TEST_BIN_DIR/player"
"$TEST_BIN_DIR/player"
clang++ "${COMMON[@]}" tests/test_camera_render_transform.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp \
  -o "$TEST_BIN_DIR/camera_render"
"$TEST_BIN_DIR/camera_render"
```

Expected: 全部退出 `0`；既有 yaw=`π/2` 的屏幕前向移动断言继续通过。

- [ ] **Step 9: 提交真实速度数据流**

```bash
git add native/gameplay/player/player_controller.h \
  native/gameplay/player/player_controller.cpp native/engine/core/loop.cpp \
  tests/test_player_controller.cpp tests/test_loop_integration.cpp
git commit -m "fix: 使用真实速度驱动主角移动动画" \
  -m "按控制器平滑速度发布移动比例，保留既有移速与相机相对操控。" \
  -m "Prompt: 主角正常走路姿势平滑切换到走路动作"
```

### Task 3: 增加走跑迟滞与实际 clip 交叉混合

**Files:**
- Modify: `native/engine/render/render_animation.h`
- Modify: `native/engine/render/skinned_model.h`
- Modify: `native/engine/render/skinned_model.cpp`
- Modify: `tests/test_render_animation.cpp`
- Modify: `tests/test_skinned_model.cpp`

**Interfaces:**
- Consumes: Task 2 发布的真实 `moveRatio`。
- Produces: `enum class LocomotionGait { Unknown, Walk, Run }`；`LocomotionGait ChooseLocomotionGait(LocomotionGait previous, float moveRatio)`；每实例 gait；实际 clip 变化时正确混合。

- [ ] **Step 1: 写步态迟滞纯函数失败测试**

在 `tests/test_render_animation.cpp` 加入并从 `main()` 调用：

```cpp
void testLocomotionGaitUsesHysteresis() {
  assert(IsLoopingClip("Walking_B"));
  assert(ChooseLocomotionGait(LocomotionGait::Unknown, 0.20f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Unknown, 0.35f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.39f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.40f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.41f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.31f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.30f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.29f) ==
         LocomotionGait::Walk);
}
```

再把 clip 测试改为显式 gait：

```cpp
assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                   LocomotionGait::Walk) == "Walking_B");
assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                   LocomotionGait::Run) == "run");
```

- [ ] **Step 2: 编译确认 gait API 不存在**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-gait-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))
clang++ "${COMMON[@]}" \
  tests/test_render_animation.cpp native/engine/render/skinned_model.cpp \
  native/engine/render/asset_profile.cpp native/engine/render/environment.cpp \
  native/engine/render/texture.cpp "${GAMEPLAY_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/render_animation"
```

Expected: 编译失败，提示 `LocomotionGait` / `ChooseLocomotionGait` 未定义。

- [ ] **Step 3: 实现步态纯函数和显式 gait clip 解析**

在 `render_animation.h` 补 `<cmath>` 并加入：

```cpp
enum class LocomotionGait { Unknown, Walk, Run };

inline LocomotionGait ChooseLocomotionGait(LocomotionGait previous,
                                           float moveRatio) {
  const float ratio = std::isfinite(moveRatio)
                          ? std::clamp(moveRatio, 0.0f, 1.0f)
                          : 0.0f;
  if (previous == LocomotionGait::Walk) {
    return ratio > 0.40f ? LocomotionGait::Run : LocomotionGait::Walk;
  }
  if (previous == LocomotionGait::Run) {
    return ratio < 0.30f ? LocomotionGait::Walk : LocomotionGait::Run;
  }
  return ratio < 0.35f ? LocomotionGait::Walk : LocomotionGait::Run;
}
```

同时把兼容行走 clip 加入循环列表，避免长时间轻推后停在尾帧：

```cpp
return name == "idle" || name == "run" || name == "Walking_B" ||
       /* 保留其余既有循环 clip */;
```

给 `ResolveClip` 末尾新增默认参数：

```cpp
LocomotionGait gait = LocomotionGait::Unknown
```

当 `animation == Run` 时，若 gait 为 Unknown 先用 `ChooseLocomotionGait(Unknown, moveRatio)`；Walk 候选顺序必须是主角语义 `walk` 优先、旧资产 `Walking_B` 次之、`run` 回退：

```cpp
if (animation == RenderAnimation::Run &&
    resolvedGait == LocomotionGait::Walk) {
  candidates.insert(candidates.begin(), "Walking_B");
  candidates.insert(candidates.begin(), "walk");
}
```

保留现有调用方默认行为和所有非移动 clip 回退。

- [ ] **Step 4: 运行步态纯函数 GREEN**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-gait-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))
clang++ "${COMMON[@]}" \
  tests/test_render_animation.cpp native/engine/render/skinned_model.cpp \
  native/engine/render/asset_profile.cpp native/engine/render/environment.cpp \
  native/engine/render/texture.cpp "${GAMEPLAY_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/render_animation"
"$TEST_BIN_DIR/render_animation"
```

Expected: 退出 `0`。

- [ ] **Step 5: 写实际 clip 变化混合失败测试**

现有 `makeWalkVariantGlb()` 的 `Walking_B` 与 `run` 共用 0→2 平移曲线，但切换时两者的播放时间不同，足以区分“保留上一姿态混合”和“目标 clip 从第 0 帧硬切”。在 `tests/test_skinned_model.cpp` 加入并从 `main()` 调用：

```cpp
void testWalkRunTransitionBlendsActualClips() {
  SkinnedModel model;
  assert(model.tryInitialize(gltf_fixture::makeWalkVariantGlb(), "gait.glb"));

  SkinnedAnimationState animation;
  ActorRenderState actor;
  actor.moving = true;
  actor.moveRatio = 0.20f;
  model.update(animation, actor, 0.15f);  // idle -> walk 完成
  const SkinPalette walkBefore = model.update(animation, actor, 0.05f);

  actor.moveRatio = 0.80f;
  const SkinPalette blendStart = model.update(animation, actor, 0.0f);
  assert(close(blendStart.matrices[0][3].x,
               walkBefore.matrices[0][3].x));

  const SkinPalette blendMiddle = model.update(animation, actor, 0.075f);
  assert(blendMiddle.matrices[0][3].x > 3.18f);
  assert(blendMiddle.matrices[0][3].x < walkBefore.matrices[0][3].x + 0.10f);
}
```

这个测试捕获旧实现的根因：`desiredClip` 已变化，但前后 `requestedAnimation` 都是 Run，混合时长为零。

- [ ] **Step 6: 编译执行蒙皮测试，确认 RED**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-blend-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
clang++ "${COMMON[@]}" \
  tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp \
  -o "$TEST_BIN_DIR/skinned_model"
"$TEST_BIN_DIR/skinned_model"
```

Expected: 失败于 walk→run 的首帧/中间帧姿态断言，证明旧代码硬切。

- [ ] **Step 7: 在每实例状态保存 gait，并按实际 clip 分类混合**

在 `SkinnedAnimationState` 增加：

```cpp
LocomotionGait locomotionGait = LocomotionGait::Unknown;
```

`reset()` 将它重置为 Unknown。`SkinnedModel::update` 在解析移动 clip 前更新实例 gait：

```cpp
if (requestedAnimation == RenderAnimation::Run) {
  animation.locomotionGait =
      ChooseLocomotionGait(animation.locomotionGait, actor.moveRatio);
} else if (requestedAnimation == RenderAnimation::Idle) {
  animation.locomotionGait = LocomotionGait::Unknown;
}
```

把 gait 传给 `ResolveClip`。新增四参数混合函数，以实际 clip 变化补足同一
`RenderAnimation::Run` 内部 walk/run 切换，同时保持主动动作缺失时的既有回退时长：

```cpp
inline float AnimationBlendSeconds(RenderAnimation previousAnimation,
                                   RenderAnimation requestedAnimation,
                                   const std::string& previousClip,
                                   const std::string& requestedClip) {
  if (previousClip == requestedClip) return 0.0f;
  if (previousAnimation == RenderAnimation::Run &&
      requestedAnimation == RenderAnimation::Run) {
    return 0.15f;
  }
  return AnimationBlendSeconds(previousAnimation, requestedAnimation);
}
```

`SkinnedModel::update` 在覆盖 `currentClip` 前读取当前 clip 名，并调用四参数版本。实际 clip 不变时继续不进入切换分支，因此不会重置时间或混合。

- [ ] **Step 8: 运行动画 GREEN 与现有回退回归**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-animation-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))
clang++ "${COMMON[@]}" \
  tests/test_render_animation.cpp native/engine/render/skinned_model.cpp \
  native/engine/render/asset_profile.cpp native/engine/render/environment.cpp \
  native/engine/render/texture.cpp "${GAMEPLAY_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/render_animation"
"$TEST_BIN_DIR/render_animation"
clang++ "${COMMON[@]}" tests/test_skinned_model.cpp \
  native/engine/render/skinned_model.cpp -o "$TEST_BIN_DIR/skinned_model"
"$TEST_BIN_DIR/skinned_model"
```

Expected: `test_render_animation`、`test_skinned_model` 都退出 `0`；既有受击/死亡/闪避/施法/旧 `Walking_B` 回退测试保持通过。

- [ ] **Step 9: 提交步态与蒙皮混合增量**

```bash
git add native/engine/render/render_animation.h \
  native/engine/render/skinned_model.h native/engine/render/skinned_model.cpp \
  tests/test_render_animation.cpp tests/test_skinned_model.cpp
git commit -m "fix: 平滑混合主角走跑动画" \
  -m "增加每实例步态迟滞，并让实际 walk/run clip 变化进入交叉混合。" \
  -m "Prompt: 主角正常走路姿势平滑切换到走路动作"
```

### Task 4: 停步保持朝向并完成全量验证

**Files:**
- Modify: `tests/test_loop_integration.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `PROJECT_STATE.md`
- Modify: `TASKS.md`

**Interfaces:**
- Consumes: 现有 `SoftTargeting`、`ThirdPersonCamera`、Task 2/3 的移动状态。
- Produces: 停稳后相机 yaw 可变化而 `Player::angle` 不变；目标仍保持锁定。

- [ ] **Step 1: 写停步软锁定自由环绕失败测试**

在 `tests/test_loop_integration.cpp` 的 `targetingLoop` 场景中，先建立锁定并人为设置一个不朝向假人的最后移动角，然后只输入右侧相机拖动：

```cpp
targetingLoop.surface.player.angle = 1.5707963f;
targetingLoop.surface.player.velocity = {};
targetingLoop.surface.player.moving = false;
const float facingBeforeOrbit = targetingLoop.surface.player.angle;
const float yawBeforeOrbit = targetingLoop.camera.yaw();

assert(targetingLoop.enqueueInput(InputAction::PointerDown, 90,
                                  700.0f, 400.0f));
assert(targetingLoop.enqueueInput(InputAction::PointerMove, 90,
                                  780.0f, 400.0f));
targetingLoop.tickOnce(16);
assert(targetingLoop.camera.yaw() != yawBeforeOrbit);
assert(std::abs(targetingLoop.surface.player.angle - facingBeforeOrbit) <
       0.0001f);
assert(targetingLoop.snapshot().targetId ==
       static_cast<int32_t>(CombatController::kTrainingTargetId));
```

旧 Loop 会在同一帧把停步主角朝向训练假人，第二个 angle 断言失败。

- [ ] **Step 2: 重新编译运行 Loop 测试确认 RED**

用以下命令从当前源码重建并运行：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-loop-orbit-red.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
```

Expected: 仅新增的停步 angle 断言失败；相机 yaw 已变化，目标仍存在。

- [ ] **Step 3: 删除停步软锁定朝向覆盖**

从 `Loop::updateFixed` 删除完整代码块：

```cpp
if (!surface.player.moving && currentTarget.has_value() &&
    currentTarget->direction.length() > 0.0f) {
  // 目标角、remainder、maxTurn 与 player.angle 写入
}
```

保留 `currentTarget` 的选择、`camera.setExploration`、目标标记、攻击绑定和释放目标逻辑。不要添加相机 yaw 到 `player.angle` 的任何替代同步。

- [ ] **Step 4: 运行 Loop、相机和软锁定 GREEN**

先从当前源码重建并运行 Loop 测试：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-loop-orbit-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
```

Expected: `test_loop_integration` 退出 `0`。然后运行相机与软锁定测试：

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-orbit-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
clang++ "${COMMON[@]}" tests/test_camera.cpp \
  native/engine/render/camera.cpp -o "$TEST_BIN_DIR/camera"
"$TEST_BIN_DIR/camera"
clang++ "${COMMON[@]}" tests/test_camera3d.cpp \
  native/engine/render/camera3d.cpp -o "$TEST_BIN_DIR/camera3d"
"$TEST_BIN_DIR/camera3d"
clang++ "${COMMON[@]}" tests/test_soft_targeting.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/targeting"
"$TEST_BIN_DIR/targeting"
```

Expected: 全部退出 `0`。

- [ ] **Step 5: 更新长期项目状态与真机待办**

在 `PROJECT_STATE.md` 的“已完成”追加一条，准确记录：出生侦察敌已从数据源移除、动画由真实速度和迟滞驱动、停步自由环绕已通过宿主测试。只有 HAP 构建成功时才写“构建通过”，只有设备实际操作验证后才写“真机通过”。

在 `TASKS.md`：

- 若尚未完成设备验证，新增未完成项：冷启动出生安全区、轻推/重推/松手动画、临界阈值不抖、停步环绕一周可见正面的真机验收。
- 若本轮已完成真机验证，则以 `[x]` 记录同一验收及设备/包证据，不保留重复待办。

- [ ] **Step 6: 运行全部相关自动化测试**

Run:

```bash
set -e
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-locomotion-final.XXXXXX)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))
SDKROOT="$(xcrun --show-sdk-path)"
COMMON_FLAGS=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)

clang++ "${COMMON_FLAGS[@]}" tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o "$TEST_BIN_DIR/player"
"$TEST_BIN_DIR/player"

clang++ "${COMMON_FLAGS[@]}" tests/test_render_animation.cpp \
  native/engine/render/skinned_model.cpp native/engine/render/asset_profile.cpp \
  native/engine/render/environment.cpp native/engine/render/texture.cpp \
  "${GAMEPLAY_SOURCES[@]}" -o "$TEST_BIN_DIR/render_animation"
"$TEST_BIN_DIR/render_animation"

clang++ "${COMMON_FLAGS[@]}" tests/test_skinned_model.cpp \
  native/engine/render/skinned_model.cpp -o "$TEST_BIN_DIR/skinned_model"
"$TEST_BIN_DIR/skinned_model"

clang++ "${COMMON_FLAGS[@]}" tests/test_camera.cpp \
  native/engine/render/camera.cpp -o "$TEST_BIN_DIR/camera"
"$TEST_BIN_DIR/camera"

clang++ "${COMMON_FLAGS[@]}" tests/test_camera3d.cpp \
  native/engine/render/camera3d.cpp -o "$TEST_BIN_DIR/camera3d"
"$TEST_BIN_DIR/camera3d"

clang++ "${COMMON_FLAGS[@]}" tests/test_soft_targeting.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/targeting"
"$TEST_BIN_DIR/targeting"

clang++ "${COMMON_FLAGS[@]}" tests/test_world_layout_gen.cpp \
  -o "$TEST_BIN_DIR/world_layout"
"$TEST_BIN_DIR/world_layout"

node automation/assets/generate_world_layout.mjs
node tests/test_bridge_contract.mjs
git diff --check
```

再执行下面的确定命令重建并运行 `test_wild_spawn_system` 与 `test_loop_integration`：

```bash
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_wild_spawn_system.cpp \
  "${HOST_SOURCES[@]}" -o "$TEST_BIN_DIR/wild_spawn"
"$TEST_BIN_DIR/wild_spawn"
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
```

Expected: 所有命令退出 `0`，`git diff --check` 无输出。

- [ ] **Step 7: 运行完整宿主测试集**

从当前源码重建宿主静态库和全部非平台测试，不直接执行修改前留下的 `_audit_build/bin`：

```bash
set -e
SDKROOT="$(xcrun --show-sdk-path)"
FULL_TEST_DIR=$(mktemp -d /tmp/myworld-full-host.XXXXXX)
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math)
HOST_OBJECTS=()
while IFS= read -r source; do
  object="$FULL_TEST_DIR/$(echo "$source" | tr '/' '_').o"
  clang++ "${COMMON[@]}" -c "$source" -o "$object"
  HOST_OBJECTS+=("$object")
done < <(find native -name '*.cpp' \
  ! -path '*render/surface.cpp' \
  ! -path '*core/loop.cpp' \
  ! -path '*harmony/fence_wait.cpp' \
  ! -path '*harmony/lifecycle.cpp' | sort)
ar rcs "$FULL_TEST_DIR/libnative_host.a" "${HOST_OBJECTS[@]}"

for test_source in tests/test_*.cpp; do
  test_name=$(basename "$test_source" .cpp)
  case "$test_name" in
    test_fence_wait|test_loop_integration|test_loop_lifecycle) continue ;;
  esac
  clang++ "${COMMON[@]}" "$test_source" \
    "$FULL_TEST_DIR/libnative_host.a" -o "$FULL_TEST_DIR/$test_name"
  "$FULL_TEST_DIR/$test_name"
done

clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "$FULL_TEST_DIR/libnative_host.a" \
  -o "$FULL_TEST_DIR/test_loop_integration"
"$FULL_TEST_DIR/test_loop_integration"
```

Expected: 零失败。`test_fence_wait` 和 `test_loop_lifecycle` 是显式平台依赖，不计入宿主测试；若其他测试不能编译，命令立即退出并暴露首个错误，不把未执行项报告为通过。

- [ ] **Step 8: 尝试 HAP 构建并记录设备验收边界**

Run:

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p product=default -p module=entry@default assembleHap \
  --analyze=normal --parallel --incremental
```

Expected if SDK complete: `BUILD SUCCESSFUL`。若失败，保留首个真实错误并在交付中明确 HAP/设备验收待完成。

若连接设备，依设计规格逐项手工验证：出生区无追击敌；轻推走、重推跑、松手回 Idle 连续；临界区不抖；停稳环绕一周看见正面；锁定目标仍能攻击；移动中相机相对方向不变。

- [ ] **Step 9: 提交停步视角、项目状态与验收结果**

```bash
git add native/engine/core/loop.cpp tests/test_loop_integration.cpp \
  PROJECT_STATE.md TASKS.md
git commit -m "fix: 支持停步自由环绕查看主角" \
  -m "停步保持最后朝向，保留软锁定选敌与战斗镜头，并记录验证状态。" \
  -m "Prompt: 优化视角以查看主角正面"
```

- [ ] **Step 10: 最终提交范围复核**

Run:

```bash
git status --short
git log --oneline -5
git diff HEAD~4..HEAD --check
git diff HEAD~4..HEAD --stat
```

Expected: 工作树无未提交的本任务文件；四个实现提交均含 `Prompt:`；变更范围只覆盖计划列出的数据、玩家控制、动画、Loop、测试和项目记忆文件。
