# 阶段 3 角色战斗与三源 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在阶段 2 固定循环之上交付可真机操作的四段普攻、闪避体力、三源技能与反应、三源连携和共鸣终结技。

**Architecture:** 独立 Native C++ 战斗域按固定 tick 顺序推进状态，`GameplayEvent` 与 `PresentationEvent` 只做跨系统通知。静态训练假人和固定训练脉冲提供无 AI 的验收环境，ArkTS 六按钮只提交一次性动作。

**Tech Stack:** C++17、FixedPoint、HarmonyOS Native XComponent/N-API、ArkTS、Hvigor。

## Global Constraints

- 保持阶段 2 左手移动、右手相机、双指并发和 Native 单一触控生产者不变。
- 战斗时间使用整数 tick/毫秒；伤害、韧性、体力和共鸣使用 `FixedPoint`。
- 同 tick 输入按 `InputEvent::sequence`，命中按 `(tick, attacker, target, sequence)` 排序。
- 当前 tick 派生事件进入下一批，不递归消费。
- 敌人 AI、首领流程和正式 HUD/VFX/音频不进入本阶段。
- 不修改、暂存或提交用户的 `build-profile.json5`。
- 每项先验证测试失败再实现；提交必须含 `Prompt: 继续阶段3完整范围开发`。

## File Structure

- `native/gameplay/combat/combat_config.h`：强类型配置、分组校验和默认值。
- `native/gameplay/combat/combat_action.h`：动作、状态、拒绝原因和请求类型。
- `native/gameplay/combat/action_state_machine.h/.cpp`：四段普攻、闪避和施法动作。
- `native/gameplay/combat/combat_resources.h/.cpp`：体力、冷却、洞察、共鸣。
- `native/gameplay/combat/damage_resolver.h/.cpp`：生命/韧性、无敌、易伤、破韧和死亡。
- `native/gameplay/combat/source_reaction_system.h/.cpp`：附着、反应和三源历史。
- `native/gameplay/combat/training_target.h/.cpp`：静态训练假人。
- `native/gameplay/combat/training_pulse.h/.cpp`：预警、命中与精准闪避窗口。
- `native/gameplay/combat/combat_controller.h/.cpp`：战斗模块固定顺序编排。
- `native/engine/core/loop.h/.cpp`、`game_snapshot.h`：循环与快照集成。
- `entry/src/main/cpp/native_bridge.cpp`、ArkTS bridge/UI：动作桥接和六按钮。
- `tests/test_*.cpp`、`tests/test_bridge_contract.mjs`：单元、集成与契约测试。

---

### Task 1: 战斗配置、动作与拒绝契约

**Files:**
- Create: `native/gameplay/combat/combat_config.h`
- Create: `native/gameplay/combat/combat_action.h`
- Create: `tests/test_combat_config.cpp`
- Modify: `native/engine/input/player_intent.h`
- Modify: `tests/test_pointer_input.cpp`

**Interfaces:**
- Consumes: `Tick`、`FixedPoint`、六种既有战斗 `InputAction`。
- Produces: `CombatConfig::defaults/validated`、`CombatAction`、`ActionState`、`ActionRejectReason`、`ActionRequest`、`TryMapCombatAction`、`PlayerIntent::actions`。

- [ ] **Step 1: 写失败测试**

```cpp
const CombatConfig defaults = CombatConfig::defaults();
assert(defaults.comboDamage == std::array<FixedPoint,4>{fp(8),fp(10),fp(12),fp(18)});
assert(defaults.dodgeCost == fp(30));
CombatConfig invalid = defaults;
invalid.maxStamina = 0; invalid.dodgeCost = fp(-1);
const CombatConfig safe = invalid.validated();
assert(safe.maxStamina == fp(100) && safe.dodgeCost == fp(30));
CombatAction action{};
assert(TryMapCombatAction(InputAction::Ultimate, action) && action == CombatAction::Ultimate);
assert(!TryMapCombatAction(InputAction::PointerDown, action));
```

- [ ] **Step 2: 运行确认失败**

Run: 用 README 的显式 macOS SDK/libc++ clang 命令编译执行 `test_combat_config.cpp` 和 `test_pointer_input.cpp`。  
Expected: FAIL，提示配置或动作类型不存在。

- [ ] **Step 3: 最小实现**

```cpp
enum class CombatAction : uint8_t { Attack, Dodge, Radiance, Current, Corruption, Ultimate };
enum class ActionState : uint8_t { Idle, Attack1, Attack2, Attack3, Attack4, Dodging, CastingSource, CastingUltimate };
enum class ActionRejectReason : uint8_t { None, NoTarget, TargetDead, Cooldown, InsufficientStamina, InsufficientResonance, ActionLocked, InvalidAction };
struct ActionRequest { CombatAction action; uint64_t sequence; };
```

`CombatConfig` 写入规格中的全部数值，按连击、体力闪避、源技能、反应共鸣、训练环境五组校验并整组回退。`PlayerIntent` 增加 `std::vector<ActionRequest> actions`。

- [ ] **Step 4: 运行确认通过**

Run: 重复 Step 2，并运行既有 `test_pointer_input`。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/combat_config.h native/gameplay/combat/combat_action.h native/engine/input/player_intent.h tests/test_combat_config.cpp tests/test_pointer_input.cpp
git commit -m "feat: 定义阶段3战斗配置契约" -m "增加强类型动作、拒绝原因和安全配置回退。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 2: 四段普攻状态机

**Files:**
- Create: `native/gameplay/combat/action_state_machine.h`
- Create: `native/gameplay/combat/action_state_machine.cpp`
- Create: `tests/test_action_state_machine.cpp`

**Interfaces:**
- Consumes: `CombatConfig`、`ActionRequest`、tick、移动/受击和目标上下文。
- Produces: `ActionDecision request(...)`、`std::optional<HitRequest> update(...)`、`resetCombo()`、`reset()`。

- [ ] **Step 1: 写失败测试**

```cpp
ActionStateMachine machine(CombatConfig::defaults());
assert(machine.request({CombatAction::Attack,1}, targetContext()).accepted);
assert(advanceUntilHit(machine).baseDamage == fp(8));
assert(chainAndHit(machine, 2).baseDamage == fp(10));
assert(chainAndHit(machine, 3).baseDamage == fp(12));
assert(chainAndHit(machine, 4).baseDamage == fp(18));
assert(comboResetsOnMove(machine));
assert(comboResetsOnDamageTaken(machine));
assert(comboResetsAfter(machine, 481));
```

同时断言动作锁定时同 tick 后续攻击返回 `ActionLocked` 且不改变段数。

- [ ] **Step 2: 运行确认失败**

Run: 编译执行 `test_action_state_machine.cpp`。  
Expected: FAIL，提示状态机不存在。

- [ ] **Step 3: 最小实现**

`ActionDecision` 含 `accepted/reason`；`HitRequest` 含 attacker、target、ability、可选 source、生命/韧性伤害、tick、sequence。每段只在固定命中点生成一次请求，命中后开放 `480ms` 衔接窗口；移动非零、实际受击或超时调用 `resetCombo()`。

- [ ] **Step 4: 运行确认通过**

Run: 执行 `test_action_state_machine`、`test_event_order`、`test_decision_log`。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/action_state_machine.* tests/test_action_state_machine.cpp
git commit -m "feat: 实现四段普攻状态机" -m "固定命中点、衔接窗口及三种重置条件。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 3: 体力、闪避与训练脉冲

**Files:**
- Create: `native/gameplay/combat/combat_resources.h`
- Create: `native/gameplay/combat/combat_resources.cpp`
- Create: `native/gameplay/combat/training_pulse.h`
- Create: `native/gameplay/combat/training_pulse.cpp`
- Create: `tests/test_combat_resources.cpp`
- Create: `tests/test_training_pulse.cpp`
- Modify: `native/gameplay/combat/action_state_machine.h`
- Modify: `native/gameplay/combat/action_state_machine.cpp`

**Interfaces:**
- Consumes: 配置、tick、闪避请求和脉冲命中点。
- Produces: `CombatResources::{spendStamina,advance,grantInsight,consumeInsight}`、`TrainingPulse::{advance,classifyDodge}`、状态机无敌查询。

- [ ] **Step 1: 写失败测试**

```cpp
CombatResources r(CombatConfig::defaults());
assert(r.spendStamina(fp(30),0) && r.stamina() == fp(70));
r.advance(499); assert(r.stamina() == fp(70));
r.advance(999); assert(r.stamina() == fp(80));
TrainingPulse pulse(CombatConfig::defaults());
assert(pulse.advance(800).kind == PulseEventKind::Hit);
assert(pulse.classifyDodge(700) == DodgeGrade::Precise);
assert(pulse.classifyDodge(599) == DodgeGrade::Normal);
```

增加闪避耗 `30`、总长 `300ms`、前 `200ms` 无敌、体力不足原子拒绝、精准闪避授予洞察断言。
后续真机调优将精准窗口调整为脉冲命中前 `100–500ms`，洞察延长至 `15 秒`；详见
`docs/superpowers/plans/2026-07-16-precision-dodge-window.md`。

- [ ] **Step 2: 运行确认失败**

Run: 编译执行 `test_combat_resources` 和 `test_training_pulse`。  
Expected: FAIL，提示类型不存在。

- [ ] **Step 3: 最小实现**

体力按整数毫秒恢复并限制在 `[0,100]`，消耗后延迟 `500ms`；脉冲周期 `3000ms`、预警 `800ms`，命中点前后闭区间 `100ms` 为精准。状态机接受闪避前先检查并扣除体力。

- [ ] **Step 4: 运行确认通过**

Run: 执行本任务测试、`test_fixed_step` 和 `test_action_state_machine`。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/combat_resources.* native/gameplay/combat/training_pulse.* native/gameplay/combat/action_state_machine.* tests/test_combat_resources.cpp tests/test_training_pulse.cpp tests/test_action_state_machine.cpp
git commit -m "feat: 实现体力闪避与训练脉冲" -m "增加无敌帧、精准闪避和一次性源流洞察。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 4: 训练假人、伤害与三源反应

**Files:**
- Create: `native/gameplay/combat/training_target.h`
- Create: `native/gameplay/combat/training_target.cpp`
- Create: `native/gameplay/combat/damage_resolver.h`
- Create: `native/gameplay/combat/damage_resolver.cpp`
- Create: `native/gameplay/combat/source_reaction_system.h`
- Create: `native/gameplay/combat/source_reaction_system.cpp`
- Create: `tests/test_damage_resolver.cpp`
- Create: `tests/test_source_reaction_system.cpp`
- Modify: `native/gameplay/combat/source_aura.h`
- Modify: `native/gameplay/combat/source_aura.cpp`
- Modify: `native/gameplay/combat/resonance.h`
- Modify: `native/gameplay/combat/resonance.cpp`

**Interfaces:**
- Consumes: `HitRequest`、配置、训练目标和事件类型。
- Produces: `DamageResolver::resolve`、`SourceReactionSystem::apply`、`TrainingTarget::{advance,reset}`。

- [ ] **Step 1: 写失败测试**

```cpp
TrainingTarget target = TrainingTarget::defaults();
DamageResolver resolver(CombatConfig::defaults());
assert(resolver.resolve(target,basicHit(fp(8),fp(4))).hpDamage == fp(8));
SourceReactionSystem sources(CombatConfig::defaults());
sources.apply(target,SourceType::Radiance,fp(1),0,1);
const ReactionOutcome out = sources.apply(target,SourceType::Current,fp(1),10,2);
assert(out.type == ResonanceType::Refraction && out.hpDamage == fp(12));
assert(target.weakUntil() == 3010);
```

覆盖凝滞 `4 秒/+50%` 韧伤、崩解 `30` 韧伤及破韧追加 `20` 生命伤害、同源刷新、过期、顺序无关、单次最多一次反应、死亡 `2 秒` 复位。

- [ ] **Step 2: 运行确认失败**

Run: 编译执行两个新测试。  
Expected: FAIL，提示模块不存在或旧共鸣默认分支错误。

- [ ] **Step 3: 最小实现**

把 `resolveResonance` 改成 `std::optional<ResonanceType>`，同源返回空；反应消费组合旧附着后再写当前新附着。目标破韧 `3000ms` 内生命承伤 `+25%`，结束恢复韧性 `100`；死亡 `2000ms` 后完整复位。

- [ ] **Step 4: 运行确认通过**

Run: 执行新测试、`test_source_aura`、`test_resonance`、`test_event_order`。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/training_target.* native/gameplay/combat/damage_resolver.* native/gameplay/combat/source_reaction_system.* native/gameplay/combat/source_aura.* native/gameplay/combat/resonance.* tests/test_damage_resolver.cpp tests/test_source_reaction_system.cpp tests/test_source_aura.cpp tests/test_resonance.cpp
git commit -m "feat: 实现训练假人与三源反应" -m "确定性结算折光、凝滞、崩解、破韧和复位。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 5: 三源技能、共鸣与终结技

**Files:**
- Create: `tests/test_source_abilities.cpp`
- Create: `tests/test_resonance_window.cpp`
- Modify: `native/gameplay/combat/action_state_machine.h`
- Modify: `native/gameplay/combat/action_state_machine.cpp`
- Modify: `native/gameplay/combat/combat_resources.h`
- Modify: `native/gameplay/combat/combat_resources.cpp`
- Modify: `native/gameplay/combat/source_reaction_system.h`
- Modify: `native/gameplay/combat/source_reaction_system.cpp`

**Interfaces:**
- Consumes: 三个源动作、有效目标、冷却、洞察和反应结果。
- Produces: 三源/终结 `HitRequest`、`addResonance`、`recordDistinctSource`、`canUltimate`、`spendUltimate`。

- [ ] **Step 1: 写失败测试**

```cpp
assert(cast(CombatAction::Radiance).damage == fp(20));
resources.grantInsight(0);
assert(cast(CombatAction::Current).damage == fp(24));
assert(!resources.hasInsight());
recordSource(Radiance,0); recordSource(Current,3000); recordSource(Corruption,7000);
assert(resources.resonance() == fp(100));
assert(cast(CombatAction::Ultimate).damage == fp(60));
assert(resources.resonance() == 0);
```

覆盖 `3/4/5 秒` 冷却、源技能 `+10` 共鸣、反应额外 `+20`、超过 `8 秒` 不充满、终结不足拒绝、`5 秒` 窗口过期保留满能量。

- [ ] **Step 2: 运行确认失败**

Run: 编译执行两个新测试。  
Expected: FAIL，提示冷却或三源历史接口不存在。

- [ ] **Step 3: 最小实现**

三源生成 `(20,6)/(16,8)/(12,18)` 命中并启动 `3000/4000/5000ms` 冷却。洞察在成功命中时把生命伤害和附着乘 `1.5` 后消费。三种不同源最大最小命中时间差不超过 `8000ms` 时共鸣置 `100` 并打开 `5000ms` 窗口；终结生成 `(60,40)` 并清空共鸣。

- [ ] **Step 4: 运行确认通过**

Run: 执行 Tasks 1–5 全部新增战斗测试。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/action_state_machine.* native/gameplay/combat/combat_resources.* native/gameplay/combat/source_reaction_system.* tests/test_source_abilities.cpp tests/test_resonance_window.cpp
git commit -m "feat: 实现三源技能与共鸣终结" -m "增加冷却、洞察、三源窗口和终结消费。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 6: 战斗控制器与固定循环集成

**Files:**
- Create: `native/gameplay/combat/combat_controller.h`
- Create: `native/gameplay/combat/combat_controller.cpp`
- Create: `tests/test_combat_controller.cpp`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `tests/test_loop_integration.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `PlayerIntent::actions`、移动、软锁定目标和固定 tick。
- Produces: `CombatController::update(const CombatFrameInput&)`、`CombatController::snapshot()` 和事件批次。

- [ ] **Step 1: 写失败测试**

```cpp
CombatController combat(CombatConfig::defaults());
combat.enqueue({CombatAction::Attack,1});
combat.update({0,16,false,1});
assert(combat.snapshot().comboSegment == 1);
combat.update({160,144,false,1});
assert(combat.snapshot().targetHp == 292);
Loop loop; loop.surface.width=1000; loop.surface.height=800;
assert(loop.enqueueInput(InputAction::Attack,-1,0,0));
loop.processInput(); loop.updateFixed(1,16);
assert(loop.snapshot().comboSegment == 1);
```

覆盖 sequence 顺序、移动重置、脉冲受击/精准闪避、停止重启清理和目标失效拒绝。

- [ ] **Step 2: 运行确认失败**

Run: 编译执行 `test_combat_controller` 和 `test_loop_integration`。  
Expected: FAIL，提示控制器或快照字段不存在。

- [ ] **Step 3: 最小集成**

`processInput()` 把六种战斗动作追加 `{action,sequence}`，不走 TouchRole；固定更新移交动作后清空。软锁定候选保留稳定训练假人 ID，并把位置/存活交给控制器。`stop()`、渲染失效和重启调用 `combat.reset()`。渲染只读取快照，不解释战斗规则。CMake 登记所有新增 `.cpp`。

- [ ] **Step 4: 运行确认通过**

Run: 执行本任务测试及 `test_loop_lifecycle`、`test_touch_controls`、`test_player_controller`、`test_camera`、`test_soft_targeting`。  
Expected: 全部退出码 0。

- [ ] **Step 5: 提交**

```bash
git add native/gameplay/combat/combat_controller.* native/engine/core/loop.* native/engine/core/game_snapshot.h native/engine/render/surface.* entry/src/main/cpp/CMakeLists.txt tests/test_combat_controller.cpp tests/test_loop_integration.cpp
git commit -m "refactor: 集成阶段3固定步战斗域" -m "确定编排动作、脉冲、伤害、反应和快照。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 7: N-API、六按钮与诊断 HUD

**Files:**
- Create: `entry/src/main/ets/ui/CombatControls.ets`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: `Loop::enqueueInput`、完整 `GameSnapshot`。
- Produces: `pushAction(type: number): void`、`CombatControls`、阶段 3 ArkTS 快照字段。

- [ ] **Step 1: 写契约失败测试**

```js
for (const label of ['普攻','闪避','辉印','脉流','蚀质','终结']) assert.match(controls,new RegExp(label));
assert.match(bridge,/export const pushAction/);
assert.doesNotMatch(gamePage,/\.onTouch\(/);
for (const field of ['stamina','comboSegment','invulnerable','insightMs','resonance','targetHp','targetPoise','pulseWarningMs','lastRejectReason']) assert.match(bridge,new RegExp(field));
```

Run: `node tests/test_bridge_contract.mjs`  
Expected: FAIL，提示按钮、动作桥接或字段缺失。

- [ ] **Step 2: 实现严格动作桥接**

`NativePushAction` 只接受一个整数 `0..5`，依次映射六动作；缺参、非整数、非有限、越界抛 `TypeError`。调用 `g_loop.enqueueInput(action,-1,0,0)` 沿用 sequence 临界区。页面不调用保留的 `pushInput`。

- [ ] **Step 3: 实现按钮和展示**

`CombatControls` 在右下用 `Column/Row` 排列六个文字 `Button`，每个 `onClick` 只调用 `pushAction(0..5)`。`GamePage` 拉取阶段 3 字段；`Hud` 显示体力、连击、洞察、共鸣、假人生命/韧性和脉冲预警。按钮不得覆盖左下移动区。

- [ ] **Step 4: 运行契约和 HAP 构建**

Run: `node tests/test_bridge_contract.mjs`，再以 `DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk` 执行 Hvigor `assembleHap`。  
Expected: Node 退出码 0；ArkTS、Native、打包和签名成功。

- [ ] **Step 5: 提交**

```bash
git add entry/src/main/ets/ui/CombatControls.ets entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/ui/Hud.ets tests/test_bridge_contract.mjs
git commit -m "feat: 接入阶段3真机战斗按钮" -m "增加严格动作桥接、六按钮和战斗诊断快照。" -m "Prompt: 继续阶段3完整范围开发"
```

### Task 8: 全量验证、真机验收与文档收口

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md`
- Modify: `docs/superpowers/plans/2026-07-15-character-combat-three-sources.md`

**Interfaces:**
- Consumes: Tasks 1–7 实现和测试。
- Produces: 自动化结果、signed HAP、真机证据和阶段 3 完成记录。

- [x] **Step 1: 跑全量自动化**

用显式 macOS SDK、SDK 内 `usr/include/c++/v1`、`-I. -Inative` 编译执行每个 `tests/test_*.cpp`，按依赖链接对应 `.cpp`；再运行：

```bash
node tests/test_bridge_contract.mjs
git diff --check
```

Expected: 全部 C++/Node 退出码为 0，diff-check 无输出；不得删除或跳过阶段 1/2 测试。

Result (2026-07-16): PASS。仓库现有 30/30 个 `tests/test_*.cpp` 均使用显式 macOS SDK、
SDK 内 `usr/include/c++/v1`、`-I. -Inative` 逐个编译并运行通过；Node 1/1 通过，
`git diff --check` 无输出。阶段 1/2 测试未删除或跳过。

- [x] **Step 2: 跑生产构建**

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk node /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js --mode module -p module=entry@default -p product=default -p requiredDeviceType=phone assembleHap --analyze=normal --parallel --incremental --daemon
```

Expected: `BUILD SUCCESSFUL`、signed HAP 存在，构建前后 `shasum build-profile.json5` 一致。

Result (2026-07-16): PASS。指定命令返回 `BUILD SUCCESSFUL in 2 s 63 ms`；signed HAP
大小为 3,885,608 bytes，SHA-1 为 `27e50f5df1ab525117ef7c717332eded7120e1b9`。
`build-profile.json5` 构建前后 SHA-1 均为
`a482c4c748ac73c906b8d1b726e0075b000880eb`。

- [ ] **Step 3: 真机逐项验收**

```text
[x] 普攻得到 1→2→3→4，移动、受击、超时重置
[x] 闪避耗尽体力后拒绝，停止消耗后恢复至 100
[x] 普通闪避免疫；在脉冲剩余 `100–500ms` 时单击闪避，精准规避该次脉冲并授予 `15 秒` 洞察
[x] 三源技能分别生效并遵守 3/4/5 秒冷却
[x] 折光、凝滞、崩解的生命/韧性与状态正确
[x] 8 秒内三源充满共鸣，终结技消费 100
[x] 假人破韧、易伤、死亡和 2 秒复位正确
[x] 移动、相机、战斗按钮并发且无输入粘连
[x] 停止重启后无冷却、无敌、附着或脉冲残留
```

Result (2026-07-17): PASS。设备 `2MN0224C12000754` 安装修复版 signed HAP 后，四段普攻
即时截图显示 `连击 4`，假人从 `300/100` 变为 `252/75`；移动和受击后下一击均按第一段
结算。超时后第二击也按第一段造成 `8` 点伤害，并正确显示 `连击 1`。

其余八项通过：体力不足显示 `拒绝 4` 后恢复到 `100.0`；普通闪避保持 `HP 100` 且无洞察，
精准闪避保持 `HP 100` 并显示约 `14104ms` 洞察；技能冷却与 `拒绝 3` 生效；反应编号
`0/1/2` 均有对应生命、韧性和附着变化；三源将共鸣充至 `100.0`，终结后归零；采到假人
`韧性 0.0`、`HP 0.0` 及两秒后的 `300.0/100.0`；三指移动、相机和闪避并发后位置稳定在
`X 0.820/Y 0.793`；停止重启后共鸣、冷却和附着清零。阶段 3 九项真机出口全部通过。

- [x] **Step 4: 更新文档**

README 记录能力、测试数量、signed HAP 和真机结果；路线图只勾选有实际证据的出口，失败项保留未勾选并记录阻塞。

- [x] **Step 5: 检查并提交**

```bash
git diff --check
git status --short
git add README.md docs/superpowers/plans/2026-07-14-combat-vertical-slice-roadmap.md docs/superpowers/plans/2026-07-15-character-combat-three-sources.md
git commit -m "docs: 完成阶段3战斗三源验收" -m "记录自动化、生产构建和真机验证结果。" -m "Prompt: 继续阶段3完整范围开发"
```

提交前确认暂存区不含 `build-profile.json5` 或其他用户已有修改。
