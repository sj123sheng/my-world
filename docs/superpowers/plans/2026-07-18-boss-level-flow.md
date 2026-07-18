# 阶段 5 首领与关卡 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在阶段 4 遭遇系统上交付三阶段首领、线性关卡推进、战斗门、补给点和 5 秒内快速重试。

**Architecture:** `EncounterController` 继续作为固定 tick 战斗编排入口，新增 Boss 状态机只处理首领阶段、机制门控和重试复位，不接管玩家动作或 AI 决策。关卡流程以强类型状态保存当前环节、门状态、重试点和补给次数，并通过 `GameSnapshot` 暴露最小 HUD 数据。

**Tech Stack:** C++17、HarmonyOS N-API、ArkTS、OpenGL ES 3、Node.js 合约测试、DevEco Hvigor、HDC 真机验证

## Global Constraints

- 不引入任务系统、剧情系统、NavMesh、脚本运行时、掉落、存档进度或正式资源。
- 不制作最终 HUD、VFX、音频、性能降级或 8-12 分钟完整表现报告；这些属于阶段 6。
- 阶段阈值 70%、35%、0% 只能单次触发，重复 update 不得重复发事件。
- 首领失败后从首领入口直接重试，不重复前置普通敌人，重试状态必须在 5 秒内可重新开始。
- 训练、兽群、混战、守卫四个阶段 4 遭遇入口不得回归。
- N-API 不暴露可变容器；快照字段使用定长标量。
- 不修改、暂存或提交用户现有的 `build-profile.json5`。
- 每个提交必须包含 `Prompt: 好的 进入阶段5`。

---

## File Structure

- `native/gameplay/entities/boss.h`：替换旧占位，定义 `BossPhase`、`BossMechanic`、`BossSnapshot`、`BossConfig` 和 `BossController`。
- `native/gameplay/entities/boss.cpp`：实现阶段阈值、机制门、节点击破、共鸣检查、失败状态和重试复位。
- `native/gameplay/ai/encounter_controller.h/.cpp`：增加 `Boss` 与 `LevelFlow` 模式，复用现有敌人/训练目标伤害管线。
- `native/engine/core/game_snapshot.h`：增加关卡环节、门状态、补给状态、首领 HP/韧性/阶段/机制字段。
- `native/engine/core/loop.cpp`：将 Encounter 快照同步到 `GameSnapshot`，增加快速重试入口。
- `entry/src/main/cpp/native_bridge.cpp`、`entry/src/main/ets/napi/Bridge.ets`、`entry/src/main/cpp/types/libnative_game/Index.d.ts`：扩展桥接契约。
- `entry/src/main/ets/ui/CombatControls.ets`、`entry/src/main/ets/ui/Hud.ets`、`entry/src/main/ets/pages/GamePage.ets`：增加首领流程、补给和重试的真机入口与调试显示。
- `tests/test_boss_controller.cpp`：覆盖三阶段单次触发、节点门控、终末熔铸和重试复位。
- `tests/test_level_flow.cpp`：覆盖训练到首领入口的门控、补给、失败后不重复前置战斗和快速重试。
- `tests/test_encounter_controller.cpp`：扩展现有入口，确保阶段 4 遭遇和训练语义不回归。
- `tests/test_bridge_contract.mjs`：覆盖新增 N-API/ArkTS 字段和入口。

### Task 1: Boss 配置与三阶段状态机

**Files:**
- Create: `native/gameplay/entities/boss.cpp`
- Replace: `native/gameplay/entities/boss.h`
- Create: `tests/test_boss_controller.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `Tick`、`FixedPoint`、`EntityId`、`CombatAction::Ultimate` 产生的 `lastAbility`。
- Produces: `BossController::start(BossConfig)`、`BossController::applyDamage(FixedPoint hpDamage, FixedPoint poiseDamage, Tick tick)`、`BossController::update(BossFrameInput)`、`BossController::retry(Tick tick)`、`BossController::snapshot()`。

- [ ] **Step 1: 写入失败测试**

```cpp
void testPhaseThresholdsTriggerOnce() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  boss.applyDamage(fp(300), fp(20), 100);
  assert(boss.snapshot().phase == BossPhase::RadianceLockdown);
  boss.applyDamage(fp(1), fp(0), 116);
  assert(boss.snapshot().phase == BossPhase::CurrentStorm);
  assert(boss.snapshot().transitionCount == 1);
  boss.update({132, 16, false, false, 0});
  assert(boss.snapshot().transitionCount == 1);
  boss.applyDamage(fp(350), fp(0), 148);
  assert(boss.snapshot().phase == BossPhase::CorruptionCollapse);
  assert(boss.snapshot().transitionCount == 2);
}
```

- [ ] **Step 2: 编译确认 RED**

Run:
```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CXX="$(xcrun --find clang++)"
"$CXX" -std=c++17 -isysroot "$SDKROOT" -I. -Inative tests/test_boss_controller.cpp \
  native/gameplay/entities/boss.cpp -o /tmp/test_boss_controller
```
Expected: `BossController` 或 `boss.cpp` 不存在导致失败。

- [ ] **Step 3: 实现最小 Boss 状态机**

`BossConfig::karounDefaults()` 使用 1000 HP、300 韧性、70%/35% 阈值。`applyDamage()` 只扣血和韧性；`update()` 处理阶段阈值、二阶段节点门、三阶段终末熔铸读条和失败状态。`retry()` 清空阶段触发标记、机制计时、节点、失败和死亡，并恢复默认 HP/韧性。

- [ ] **Step 4: 运行测试确认 GREEN**

Run: `/tmp/test_boss_controller`
Expected: exit code 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/entities/boss.h native/gameplay/entities/boss.cpp tests/test_boss_controller.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 增加卡洛恩首领状态机" \
  -m "实现三阶段阈值、机制门控和重试复位。" \
  -m "Prompt: 好的 进入阶段5"
```

### Task 2: 关卡流程、战斗门与补给

**Files:**
- Modify: `native/gameplay/ai/encounter_controller.h`
- Modify: `native/gameplay/ai/encounter_controller.cpp`
- Create: `tests/test_level_flow.cpp`
- Modify: `tests/test_encounter_controller.cpp`

**Interfaces:**
- Consumes: `EncounterMode::Beast/Mixed/Guard` 和 `BossController`。
- Produces: `EncounterMode::LevelFlow`、`EncounterMode::Boss`、`LevelStage`、`GateState`、`SupplyState`、`EncounterController::advanceLevel()`、`EncounterController::useSupply()`、`EncounterController::retryBoss()`。

- [ ] **Step 1: 写入失败测试**

```cpp
void testLevelDoorsOpenOnlyAfterVictory() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::LevelFlow));
  assert(encounter.snapshot().levelStage == LevelStage::Training);
  assert(encounter.snapshot().gateState == GateState::Closed);
  assert(!encounter.advanceLevel());
  defeatCurrentWave(encounter);
  assert(encounter.snapshot().gateState == GateState::Open);
  assert(encounter.advanceLevel());
  assert(encounter.snapshot().levelStage == LevelStage::RiftClawFight);
}
```

- [ ] **Step 2: 编译确认 RED**

Run: compile `tests/test_level_flow.cpp` with existing AI/combat/entity sources。
Expected: 新枚举或方法不存在导致失败。

- [ ] **Step 3: 实现最小关卡编排**

流程固定为 `Training -> RiftClawFight -> PriestMixedFight -> GuardElite -> Supply -> Boss`。当前环节胜利后门打开；`advanceLevel()` 只在门打开时推进；`useSupply()` 在补给环节恢复玩家资源并只消费一次；进入 Boss 后记录重试点。

- [ ] **Step 4: 运行测试确认 GREEN，并复跑 `tests/test_encounter_controller.cpp`**

Expected: 新关卡测试通过，阶段 4 模式仍通过。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/ai/encounter_controller.h native/gameplay/ai/encounter_controller.cpp tests/test_level_flow.cpp tests/test_encounter_controller.cpp
git commit -m "feat: 增加关卡流程与战斗门" \
  -m "串联训练、普通敌人、精英、补给和首领入口。" \
  -m "Prompt: 好的 进入阶段5"
```

### Task 3: 首领战接入遭遇与快速重试

**Files:**
- Modify: `native/gameplay/ai/encounter_controller.h`
- Modify: `native/gameplay/ai/encounter_controller.cpp`
- Modify: `tests/test_level_flow.cpp`
- Modify: `tests/test_boss_controller.cpp`

**Interfaces:**
- Consumes: `BossController::snapshot()` 和 `CombatController::updateEnemy()`。
- Produces: 首领候选目标、Boss 失败状态、Boss 胜利状态、`retryBoss()` 在 5 秒内复位。

- [ ] **Step 1: 写入失败测试**

```cpp
void testBossRetryDoesNotResetPreviousStages() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  enterBossFromLevelFlow(encounter);
  forcePlayerDeath(encounter);
  assert(encounter.snapshot().state == EncounterState::Defeat);
  assert(encounter.retryBoss());
  assert(encounter.snapshot().levelStage == LevelStage::Boss);
  assert(encounter.snapshot().boss.hp == fp(1000));
  assert(encounter.snapshot().gateState == GateState::Closed);
}
```

- [ ] **Step 2: 编译确认 RED**

Expected: Defeat、boss snapshot 或 retry 行为不存在。

- [ ] **Step 3: 接入首领战斗**

Boss 使用实体 ID `3001`，作为软锁定候选；玩家攻击通过现有 `TrainingTarget` 伤害管线落到 Boss HP/韧性；Boss 命中玩家复用 `applyEnemyHit()`；玩家死亡进入 `Defeat`，Boss 死亡进入 `Victory`。

- [ ] **Step 4: 运行 Boss 与 LevelFlow 测试确认 GREEN**

Expected: exit code 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/ai/encounter_controller.h native/gameplay/ai/encounter_controller.cpp tests/test_level_flow.cpp tests/test_boss_controller.cpp
git commit -m "feat: 接入首领战与快速重试" \
  -m "复用战斗结算并确保失败后从首领入口恢复。" \
  -m "Prompt: 好的 进入阶段5"
```

### Task 4: 快照、桥接与 ArkTS 调试入口

**Files:**
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/ui/CombatControls.ets`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces: `startEncounter(4)` 进入完整流程、`startEncounter(5)` 直接进入 Boss、`advanceLevel()`、`useSupply()`、`retryBoss()`。
- Produces snapshot fields: `levelStage`、`gateState`、`supplyState`、`bossHp`、`bossPoise`、`bossPhase`、`bossMechanic`、`bossCastMs`。

- [ ] **Step 1: 写入失败合约测试**

`tests/test_bridge_contract.mjs` 断言 ArkTS 声明包含新增方法、返回值为 boolean，Native snapshot 包含新增字段，UI 文本包含 `流程`、`首领`、`推进`、`补给`、`重试`。

- [ ] **Step 2: 运行 Node 测试确认 RED**

Run: `node tests/test_bridge_contract.mjs`
Expected: 缺少新增方法或字段。

- [ ] **Step 3: 实现桥接与 UI**

Bridge 只转发 `Loop` 方法；ArkTS 页面维护新增字段并在 HUD 显示。按钮文本保持短标签，避免阶段 6 前做复杂 HUD。

- [ ] **Step 4: 运行 Node 测试确认 GREEN**

Run: `node tests/test_bridge_contract.mjs`
Expected: exit code 0。

- [ ] **Step 5: 提交**

```bash
git add native/engine/core/game_snapshot.h native/engine/core/loop.h native/engine/core/loop.cpp entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/ui/CombatControls.ets entry/src/main/ets/ui/Hud.ets entry/src/main/ets/pages/GamePage.ets tests/test_bridge_contract.mjs
git commit -m "feat: 暴露阶段5流程调试入口" \
  -m "增加首领、推进、补给和重试桥接及 HUD 字段。" \
  -m "Prompt: 好的 进入阶段5"
```

### Task 5: 文档、批量验证与真机验收

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md`
- Modify: `docs/superpowers/plans/2026-07-18-boss-level-flow.md`

**Interfaces:**
- Produces: 阶段 5 自动化、构建、真机验证记录。

- [ ] **Step 1: 运行自动化测试**

Run focused tests first: `test_boss_controller`、`test_level_flow`、`test_encounter_controller`、`node tests/test_bridge_contract.mjs`。随后运行本地可编译 C++ 批量测试，记录 skipped 的 HarmonyOS/platform 测试。

- [ ] **Step 2: 运行格式与构建验证**

Run: `git diff --check`。需要 HAP 时临时复制本机签名配置，构建后恢复 `build-profile.json5`，不得提交签名内容。

- [ ] **Step 3: 真机安装与入口验收**

使用 HDC 安装 signed HAP，启动应用，dump layout 确认 `流程`、`首领`、`推进`、`补给`、`重试` 存在；切换 Boss 后 HUD 显示 Boss 阶段和 HP；触发重试后应用进程保持运行。

- [ ] **Step 4: 更新验收文档**

记录测试矩阵、HAP SHA-256、设备 ID、布局与截图路径。

- [ ] **Step 5: 提交**

```bash
git add README.md docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md docs/superpowers/plans/2026-07-18-boss-level-flow.md
git commit -m "docs: 完成阶段5首领验收" \
  -m "记录自动化、构建和真机流程入口验证结果。" \
  -m "Prompt: 好的 进入阶段5"
```
