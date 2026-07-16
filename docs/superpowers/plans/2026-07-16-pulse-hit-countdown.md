# 脉冲命中倒计时对齐 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 HUD 的“脉冲”数值与精准闪避共同表示距下一命中点的剩余时间，使屏幕上的 `100–500ms` 与实际精准窗口完全一致。

**Architecture:** `TrainingPulse` 作为唯一时间语义来源，新增/改名为 `hitRemainingMs(Tick)`；`CombatController` 快照及 Native、NAPI、ArkTS 全链路统一使用 `pulseHitRemainingMs`。HUD 只在闭区间 `[100ms, 500ms]` 显示红色，精准判定仍复用 `preciseDodgeHitTick`。

**Tech Stack:** C++17、HarmonyOS N-API、ArkTS、Node.js 合约测试、DevEco hvigor、HDC 真机验证

## Global Constraints

- 精准闪避窗口保持命中前闭区间 `[100ms, 500ms]`。
- 洞察持续时间保持 `15000ms`。
- 普通闪避的 `300ms` 时长、`200ms` 无敌和 `30` 体力消耗不变。
- 不修改、暂存或提交用户的 `build-profile.json5`。

---

### Task 1: 统一 Native 命中倒计时

**Files:**
- Modify: `native/gameplay/combat/training_pulse.h`
- Modify: `native/gameplay/combat/training_pulse.cpp`
- Modify: `native/gameplay/combat/combat_controller.h`
- Modify: `native/gameplay/combat/combat_controller.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.cpp`
- Test: `tests/test_training_pulse.cpp`
- Test: `tests/test_combat_controller.cpp`

**Interfaces:**
- Consumes: `TrainingPulse::preciseDodgeHitTick(Tick) -> std::optional<Tick>`。
- Produces: `TrainingPulse::hitRemainingMs(Tick) -> Tick` 与 `pulseHitRemainingMs` 快照字段。

- [ ] **Step 1: 写入失败的命中倒计时边界测试**

在 `tests/test_training_pulse.cpp` 断言首次命中位于 `800ms`：

```cpp
assert(pulse.hitRemainingMs(299) == 501);
assert(pulse.hitRemainingMs(300) == 500);
assert(pulse.hitRemainingMs(700) == 100);
assert(pulse.hitRemainingMs(701) == 99);
assert(pulse.hitRemainingMs(800) == 3000);
```

并在 `tests/test_combat_controller.cpp` 断言快照在 `300ms` 时导出 `500`。

- [ ] **Step 2: 运行聚焦测试并确认失败**

使用现有 C++ 测试构建命令编译并运行 `test_training_pulse`、`test_combat_controller`。

Expected: 编译失败，提示缺少 `hitRemainingMs` 或 `pulseHitRemainingMs`。

- [ ] **Step 3: 实现唯一的下一命中倒计时**

让 `hitRemainingMs` 使用与 `preciseDodgeHitTick` 相同的 epoch、首次命中 `800ms` 和周期 `3000ms` 计算下一命中；命中 tick 本身返回下一周期的 `3000ms`。删除 `warningRemainingMs`，并将 Native 快照字段全部重命名为 `pulseHitRemainingMs`。

- [ ] **Step 4: 运行聚焦测试并确认通过**

Expected: `test_training_pulse` 与 `test_combat_controller` 均退出 `0`。

- [ ] **Step 5: 提交 Native 语义修正**

```bash
git add native/gameplay/combat/training_pulse.h native/gameplay/combat/training_pulse.cpp \
  native/gameplay/combat/combat_controller.h native/gameplay/combat/combat_controller.cpp \
  native/engine/core/game_snapshot.h native/engine/core/loop.cpp \
  tests/test_training_pulse.cpp tests/test_combat_controller.cpp
git commit -m "fix: 对齐脉冲命中倒计时" \
  -m "Prompt: 修正 HUD 2700ms 才触发精准闪避的问题"
```

### Task 2: 对齐桥接契约与 HUD 提示

**Files:**
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `README.md`
- Test: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: Native `GameSnapshot::pulseHitRemainingMs`。
- Produces: ArkTS `Snapshot.pulseHitRemainingMs` 和 HUD 闭区间红色提示。

- [ ] **Step 1: 写入失败的跨层命名与颜色契约测试**

将 `tests/test_bridge_contract.mjs` 的必需字段改为 `pulseHitRemainingMs`，断言生产源码不再出现 `pulseWarningMs`，并断言 HUD 同时检查 `>= 100` 与 `<= 500`。

- [ ] **Step 2: 运行 Node 合约测试并确认失败**

Run: `node tests/test_bridge_contract.mjs`

Expected: FAIL，指出 Bridge/HUD 尚未提供 `pulseHitRemainingMs`。

- [ ] **Step 3: 重命名全链路字段并修正颜色边界**

Native N-API 属性、类型声明、Bridge、GamePage 和 Hud 全部改为 `pulseHitRemainingMs`。HUD 文本继续显示“脉冲”，颜色表达式使用：

```typescript
this.pulseHitRemainingMs >= 100 && this.pulseHitRemainingMs <= 500
  ? Color.Red : Color.Gray
```

README 明确屏幕数值现在是距下一命中时间。

- [ ] **Step 4: 运行合约测试并确认通过**

Run: `node tests/test_bridge_contract.mjs`

Expected: PASS，退出 `0`。

- [ ] **Step 5: 运行完整测试、构建和真机验收**

运行完整 C++ `31/31`、Node `1/1`、`git diff --check`；使用 hvigor 构建签名 HAP，安装到设备 `2MN0224C12000754`。观察 HUD 在 `501ms` 为灰色、`500–100ms` 为红色，窗口内单次闪避显示接近 `洞察 15000ms`，且该次脉冲不扣血。

- [ ] **Step 6: 提交桥接和 HUD 修正**

```bash
git add entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts \
  entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/pages/GamePage.ets \
  entry/src/main/ets/ui/Hud.ets README.md tests/test_bridge_contract.mjs
git commit -m "fix: 修正精准闪避 HUD 倒计时" \
  -m "Prompt: 让脉冲 100–500ms 显示与精准判定一致"
```
