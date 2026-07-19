# 阶段 6 HUD、表现与优化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在阶段 5 首领与关卡系统上交付表现事件驱动 VFX、敌人/首领灰盒渲染、移动 HUD、占位音频、性能降级和真机稳定性门槛。

**Architecture:** `VfxSystem` 消费 `PresentationEvent` 维护短时效果计时器；`PerformanceGuard` 按 FPS 滑动窗口输出降级级别；`AudioBridge` 按事件类型分发占位音效；渲染层只读消费 `VfxSnapshot` 和降级级别；HUD 升级为正式条状布局并保留可切换调试覆盖。所有表现层只消费事件和快照，不反向修改战斗状态。

**Tech Stack:** C++17、HarmonyOS N-API、ArkTS、OpenGL ES 3、Node.js 合约测试、DevEco Hvigor、HDC 真机验证

## Global Constraints

- 不引入正式美术资源、角色模型、动画骨骼、NavMesh、区域流式加载、存档进度、联网或 Mod 系统。
- 不新增敌人、首领、技能或关卡环节。
- VFX、音频和 HUD 只消费事件和快照，不反向修改战斗状态。
- 渲染线程不执行战斗逻辑或 AI 决策。
- 音频失败不得阻塞渲染线程或战斗循环。
- 性能降级只减少表现层数据量，不改变战斗结果或命中判定。
- HUD 快照频率固定在 10–20 Hz，不逐事件跨 N-API 更新。
- 灰盒 VFX 只用 GLES3 基本几何和颜色混合，不新增 Shader Program。
- 不修改、暂存或提交用户现有的 `build-profile.json5`。
- 每个提交必须包含 `Prompt: 好的 进入阶段6`。

---

## File Structure

- `native/engine/presentation/vfx_system.h`：定义 `VfxSnapshot`、`VfxSystem`。
- `native/engine/presentation/vfx_system.cpp`：实现效果计时器消费和衰减。
- `native/engine/presentation/performance_guard.h`：定义 `PerformanceGuard`。
- `native/engine/presentation/performance_guard.cpp`：实现 FPS 滑动窗口和降级级别。
- `native/platform/harmony/audio_bridge.h`：定义 `AudioBridge`。
- `native/platform/harmony/audio_bridge.cpp`：实现占位音频分发和静默降级。
- `native/engine/core/game_snapshot.h`：增加 `perfLevel`、`vfxFlags`、`cameraShakeX/Y`、`bossHpRatio`、`bossCastRatio`、`debugHud` 字段。
- `native/engine/core/loop.h`：增加 `VfxSystem`、`PerformanceGuard`、`AudioBridge` 成员和 `toggleDebugHud()` 方法。
- `native/engine/core/loop.cpp`：在 `updateFixed` 中接入 VFX 消费、音频分发和性能采样；在 `tickOnce` 中同步快照和渲染扩展字段。
- `native/engine/render/surface.h`：增加 `VfxSnapshot`、`EncounterSnapshot`、`TargetSelection` 只读字段。
- `native/engine/render/surface.cpp`：增加敌人、首领、VFX 叠加、镜头震动和软锁定绘制。
- `entry/src/main/cpp/native_bridge.cpp`：增加 `NativeToggleDebugHud` 和新快照字段。
- `entry/src/main/cpp/types/libnative_game/Index.d.ts`：扩展声明。
- `entry/src/main/ets/napi/Bridge.ets`：增加 `toggleDebugHud` 导出和新快照字段。
- `entry/src/main/ets/ui/Hud.ets`：升级为正式条状 HUD 加可切换调试覆盖。
- `entry/src/main/ets/ui/CombatControls.ets`：增加 `调试` 按钮。
- `entry/src/main/ets/pages/GamePage.ets`：增加新字段轮询和 `debugHud` 状态。
- `entry/src/main/cpp/CMakeLists.txt`：增加新源文件。
- `tests/test_vfx_system.cpp`：覆盖各事件类型触发、衰减和重复刷新。
- `tests/test_performance_guard.cpp`：覆盖 FPS 滑动窗口和降级边界。
- `tests/test_bridge_contract.mjs`：覆盖新增方法和快照字段。

### Task 1: VFX 系统

**Files:**
- Create: `native/engine/presentation/vfx_system.h`
- Create: `native/engine/presentation/vfx_system.cpp`
- Create: `tests/test_vfx_system.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `PresentationEvent`、`PresentationEventType`、`Tick`、`FixedPoint`。
- Produces: `VfxSnapshot`、`VfxSystem::consume()`、`VfxSystem::update()`、`VfxSystem::snapshot()`。

- [x] **Step 1: 写入失败测试**

```cpp
void testHitFlashTriggersAndDecays() {
  VfxSystem vfx;
  PresentationEvent e{};
  e.type = PresentationEventType::HitFlash;
  e.intensity = fp(50);
  e.tick = 100;
  vfx.consume({{}, {e}});
  assert(vfx.snapshot().hitFlashMs > 0);
  vfx.update(116, 16);
  assert(vfx.snapshot().hitFlashMs == 0);
}

void testDodgeFlashTriggers() {
  VfxSystem vfx;
  PresentationEvent e{};
  e.type = PresentationEventType::DodgeFlash;
  e.tick = 50;
  vfx.consume({{}, {e}});
  assert(vfx.snapshot().dodgeFlashMs > 0);
}

void testCameraShakeDecays() {
  VfxSystem vfx;
  PresentationEvent e{};
  e.type = PresentationEventType::CameraShake;
  e.intensity = fp(30);
  e.tick = 0;
  vfx.consume({{}, {e}});
  assert(vfx.snapshot().cameraShakeX != 0 || vfx.snapshot().cameraShakeY != 0);
  for (int i = 0; i < 20; i++) vfx.update(16 * (i + 1), 16);
  assert(vfx.snapshot().cameraShakeX == 0 && vfx.snapshot().cameraShakeY == 0);
}

void testRepeatEventRefreshesNotStacks() {
  VfxSystem vfx;
  PresentationEvent e{};
  e.type = PresentationEventType::HitFlash;
  e.intensity = fp(50);
  e.tick = 100;
  vfx.consume({{}, {e}});
  Tick firstMs = vfx.snapshot().hitFlashMs;
  vfx.consume({{}, {e}});
  assert(vfx.snapshot().hitFlashMs == firstMs);
}
```

- [x] **Step 2: 编译确认 RED**

Run:
```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CXX="$(xcrun --find clang++)"
"$CXX" -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative tests/test_vfx_system.cpp native/engine/presentation/vfx_system.cpp -o /tmp/test_vfx_system
```
Expected: `VfxSystem` 或 `vfx_system.cpp` 不存在导致失败。

- [x] **Step 3: 实现 VFX 系统**

`VfxSnapshot` 包含 `hitFlashMs`、`dodgeFlashMs`、`poiseBreakMs`、`resonanceBurstMs`、`phaseTransitionMs`、`castBarBrokenMs`（均为 `Tick`）、`cameraShakeX/Y`（`float`）和 `vfxFlags`（`int32_t` 位掩码）。`consume()` 按事件类型设置对应计时器，重复事件刷新不叠加。`update()` 每 tick 按固定步长衰减所有计时器，归零后清除。镜头震动使用衰减偏移。

- [x] **Step 4: 运行测试确认 GREEN**

Run: `/tmp/test_vfx_system`
Expected: exit code 0。

- [x] **Step 5: 提交**

```bash
git add native/engine/presentation/vfx_system.h native/engine/presentation/vfx_system.cpp tests/test_vfx_system.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 增加表现事件驱动 VFX 系统" \
  -m "消费 PresentationEvent 维护命中闪光、闪避辉光、破韧爆发、共鸣爆发、阶段切换和镜头震动计时器。" \
  -m "Prompt: 好的 进入阶段6"
```

### Task 2: 性能降级守护

**Files:**
- Create: `native/engine/presentation/performance_guard.h`
- Create: `native/engine/presentation/performance_guard.cpp`
- Create: `tests/test_performance_guard.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `Tick`、`int64_t dtMs`、`float fps`。
- Produces: `PerformanceGuard::sample()`、`PerformanceGuard::level()`。

- [x] **Step 1: 写入失败测试**

```cpp
void testLevelBoundaries() {
  PerformanceGuard guard;
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 60.0f);
  assert(guard.level() == 0);
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 45.0f);
  assert(guard.level() == 1);
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 35.0f);
  assert(guard.level() == 2);
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 25.0f);
  assert(guard.level() == 3);
}

void testRecoveryUpgrades() {
  PerformanceGuard guard;
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 25.0f);
  assert(guard.level() == 3);
  for (int i = 0; i < 120; i++) guard.sample(16 * i, 16, 60.0f);
  assert(guard.level() == 0);
}
```

- [x] **Step 2: 编译确认 RED**

Expected: `PerformanceGuard` 不存在导致失败。

- [x] **Step 3: 实现性能降级守护**

`PerformanceGuard` 维护 2 秒滑动窗口的 FPS 采样，计算平均帧率后按 55/40/30 边界输出 0-3 降级级别。降级只升不降需持续 2 秒恢复窗口，避免单帧抖动导致频繁切换。

- [x] **Step 4: 运行测试确认 GREEN**

Run: `/tmp/test_performance_guard`
Expected: exit code 0。

- [x] **Step 5: 提交**

```bash
git add native/engine/presentation/performance_guard.h native/engine/presentation/performance_guard.cpp tests/test_performance_guard.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 增加 FPS 性能降级守护" \
  -m "按 2 秒滑动窗口计算帧率，输出 0-3 降级级别。" \
  -m "Prompt: 好的 进入阶段6"
```

### Task 3: 快照、桥接与渲染扩展

**Files:**
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/ui/CombatControls.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `tests/test_bridge_contract.mjs`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Produces: `toggleDebugHud()`、快照字段 `perfLevel`、`vfxFlags`、`cameraShakeX/Y`、`bossHpRatio`、`bossCastRatio`、`debugHud`。
- Produces: `Surface` 扩展字段 `vfx`、`encounter`、`currentTarget`。
- Produces: 渲染层敌人、首领、VFX 叠加、镜头震动和软锁定绘制。

- [x] **Step 1: 写入失败合约测试**

`tests/test_bridge_contract.mjs` 断言 Bridge 声明 `toggleDebugHud` 返回 void，Index.d.ts 声明该方法，native bridge 导出 `NativeToggleDebugHud` 并委托 Loop；新增快照字段在 Bridge、Index.d.ts、native bridge 和 GamePage 中一致；CombatControls 包含 `调试` 按钮并调用 `toggleDebugHud()`。

- [x] **Step 2: 运行 Node 测试确认 RED**

Run: `node tests/test_bridge_contract.mjs`
Expected: 缺少新增方法或字段。

- [x] **Step 3: 实现快照、桥接与渲染扩展**

`GameSnapshot` 增加 `perfLevel`、`vfxFlags`、`cameraShakeX/Y`、`bossHpRatio`、`bossCastRatio`、`debugHud` 字段。`Loop` 增加 `VfxSystem`、`PerformanceGuard` 成员，在 `updateFixed` 中消费表现事件和采样 FPS，在 `tickOnce` 中同步快照和写入 `Surface` 扩展字段。`Surface` 增加 `vfx`、`encounter`、`currentTarget` 只读字段。`surface_draw` 增加敌人、首领、VFX 叠加、镜头震动和软锁定绘制。`native_bridge.cpp` 增加 `NativeToggleDebugHud` 和新快照字段。ArkTS 侧增加 `toggleDebugHud` 导出、`调试` 按钮和新字段轮询。

- [x] **Step 4: 运行 Node 测试确认 GREEN**

Run: `node tests/test_bridge_contract.mjs`
Expected: exit code 0。

- [x] **Step 5: 提交**

```bash
git add native/engine/core/game_snapshot.h native/engine/core/loop.h native/engine/core/loop.cpp native/engine/render/surface.h native/engine/render/surface.cpp entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/ui/CombatControls.ets entry/src/main/ets/pages/GamePage.ets tests/test_bridge_contract.mjs entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 接入 VFX 性能降级与渲染扩展" \
  -m "扩展快照、桥接和渲染层，增加敌人首领灰盒和 VFX 叠加。" \
  -m "Prompt: 好的 进入阶段6"
```

### Task 4: 移动 HUD

**Files:**
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces: 正式条状 HUD 布局和可切换调试覆盖。

- [x] **Step 1: 写入失败合约测试**

`tests/test_bridge_contract.mjs` 断言 Hud 包含生命条 `Progress`、首领条 `Progress`、技能冷却遮罩和调试覆盖切换逻辑；GamePage 传递 `debugHud` 状态。

- [x] **Step 2: 运行 Node 测试确认 RED**

Run: `node tests/test_bridge_contract.mjs`
Expected: 缺少 `Progress` 或调试覆盖逻辑。

- [x] **Step 3: 实现移动 HUD**

Hud 升级为正式条状布局：左上生命/韧性/体力条，顶部中央首领条和阶段标记，右下技能按钮带冷却遮罩，底部中央关卡状态。调试覆盖由 `debugHud` prop 控制显示，不遮挡正式 HUD。

- [x] **Step 4: 运行 Node 测试确认 GREEN**

Run: `node tests/test_bridge_contract.mjs`
Expected: exit code 0。

- [x] **Step 5: 提交**

```bash
git add entry/src/main/ets/ui/Hud.ets entry/src/main/ets/pages/GamePage.ets tests/test_bridge_contract.mjs
git commit -m "feat: 升级移动 HUD 为正式条状布局" \
  -m "增加生命条、首领条、冷却遮罩和可切换调试覆盖。" \
  -m "Prompt: 好的 进入阶段6"
```

### Task 5: 占位音频

**Files:**
- Create: `native/platform/harmony/audio_bridge.h`
- Create: `native/platform/harmony/audio_bridge.cpp`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `GameplayEvent`、`PresentationEvent`、`EncounterState`。
- Produces: `AudioBridge::dispatch()`、`AudioBridge::start()`、`AudioBridge::stop()`。

- [x] **Step 1: 写入失败测试**

创建 `tests/test_audio_bridge.cpp`，断言 `AudioBridge::dispatch()` 按事件类型分类不崩溃，设备不支持时静默降级为空操作。

- [x] **Step 2: 编译确认 RED**

Expected: `AudioBridge` 不存在导致失败。

- [x] **Step 3: 实现占位音频**

`AudioBridge` 接入 `OHAudioRenderer`，按 `GameplayEventType` 和 `PresentationEventType` 分发五层占位音效。设备不支持时静默降级。`Loop::updateFixed` 在战斗结算后调用 `dispatch()`。

- [x] **Step 4: 运行测试确认 GREEN**

Run: `/tmp/test_audio_bridge`
Expected: exit code 0。

- [x] **Step 5: 提交**

```bash
git add native/platform/harmony/audio_bridge.h native/platform/harmony/audio_bridge.cpp native/engine/core/loop.h native/engine/core/loop.cpp entry/src/main/cpp/CMakeLists.txt tests/test_audio_bridge.cpp
git commit -m "feat: 接入占位音频分发" \
  -m "按事件类型分发五层占位音效，设备不支持时静默降级。" \
  -m "Prompt: 好的 进入阶段6"
```

### Task 6: 文档、批量验证与真机验收

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md`
- Modify: `docs/superpowers/plans/2026-07-19-hud-presentation-optimization.md`

**Interfaces:**
- Produces: 阶段 6 自动化、构建、真机验证记录。

- [x] **Step 1: 运行自动化测试**

Run focused tests first: `test_vfx_system`、`test_performance_guard`、`test_audio_bridge`、`node tests/test_bridge_contract.mjs`。随后运行本地可编译 C++ 批量测试，记录 skipped 的 HarmonyOS/platform 测试。

- [x] **Step 2: 运行格式与构建验证**

Run: `git diff --check`。需要 HAP 时临时复制本机签名配置，构建后恢复 `build-profile.json5`，不得提交签名内容。

- [x] **Step 3: 真机完整流程与稳定性验收**

使用 HDC 安装 signed HAP，启动应用，执行 8–12 分钟完整流程（训练 → 普通敌人 → 精英 → 补给 → 首领），确认 `perfLevel` 随帧率变化；10 分钟持续战斗无崩溃；冷启动/前后台/锁屏/Surface 重建后应用保持运行；精准闪避、打断、破韧、共鸣爆发和首领阶段切换可视觉区分。

- [x] **Step 4: 更新验收文档**

记录测试矩阵、HAP SHA-256、设备 ID、帧率数据、稳定性结果和截图路径。

- [x] **Step 5: 提交**

```bash
git add README.md docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md docs/superpowers/plans/2026-07-19-hud-presentation-optimization.md
git commit -m "docs: 完成阶段6 HUD表现与优化验收" \
  -m "记录自动化、构建、真机完整流程和稳定性验证结果。" \
  -m "Prompt: 好的 进入阶段6"
```
