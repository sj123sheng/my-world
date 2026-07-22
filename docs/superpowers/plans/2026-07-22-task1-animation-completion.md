# M4 Task 1 Animation Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成玩家闪避与四类技能动画、敌人和首领动作动画，并在 Pura 70 Pro 模拟器达到 M4 Task 1 的全部验收项。

**Architecture:** 战斗和 AI 状态机继续作为动作事实的唯一来源，循环层把精确动作投影为渲染动画意图，`SkinnedModel` 只负责解析和播放 clip。专用 clip 缺失时通过集中式候选表回退，不改变 EGL/GLES/XComponent 生命周期，也不在渲染层伪造战斗位移。

**Tech Stack:** C++17、HarmonyOS NDK、GLES3、ArkTS/ArkUI、N-API、glm、cgltf、Hvigor、HDC

## Global Constraints

- 不新增或重制美术资源。
- 不修改战斗数值、冷却和资源消耗规则。
- 不引入新的图形 API 或软件渲染路径。
- 不扩展到 M4 Task 2 的场景氛围和视觉特效。
- 所有生产代码必须先有能够正确失败的测试。
- 真机或模拟器验收必须使用当前构建的签名 HAP、HDC 日志和截图。
- 提交消息必须包含变更类型、50 字以内简述和 `Prompt:` 信息。

---

### Task 1: 扩展精确动画意图与 clip 回退

**Files:**
- Modify: `native/engine/render/render_animation.h`
- Modify: `tests/test_render_animation.cpp`

**Interfaces:**
- Produces: `RenderAnimation::{Dodge,Radiance,Current,Corruption,Ultimate}`
- Produces: `ActorRenderState::action`，类型为 `RenderAnimation`
- Produces: `ResolveClip(const std::vector<std::string>&, RenderAnimation)` 的多级回退行为

- [ ] **Step 1: 写入失败测试**

在 `tests/test_render_animation.cpp` 增加：

```cpp
void testDedicatedActionClipNames() {
  assert(std::string(RenderAnimationName(RenderAnimation::Dodge)) ==
         "Running_Strafe_Right");
  assert(std::string(RenderAnimationName(RenderAnimation::Radiance)) ==
         "Spellcast_Raise");
  assert(std::string(RenderAnimationName(RenderAnimation::Current)) ==
         "Spellcast_Shoot");
  assert(std::string(RenderAnimationName(RenderAnimation::Corruption)) ==
         "Spellcasting");
  assert(std::string(RenderAnimationName(RenderAnimation::Ultimate)) ==
         "Spellcast_Long");
}

void testDedicatedActionClipFallbacks() {
  assert(ResolveClip({"idle", "run"}, RenderAnimation::Dodge) == "run");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Radiance) == "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Current) == "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Corruption) == "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Ultimate) == "attack");
}

void testExplicitActionPriority() {
  ActorRenderState actor;
  actor.action = RenderAnimation::Dodge;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Dodge);
  actor.alive = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);
}
```

并在 `main()` 调用三个测试。

- [ ] **Step 2: 验证 RED**

使用计划末尾的聚焦测试编译命令运行 `test_render_animation`。

Expected: 编译失败，提示 `Dodge/Radiance/Current/Corruption/Ultimate` 或 `ActorRenderState::action` 不存在。

- [ ] **Step 3: 实现最小动画模型**

在 `render_animation.h`：

```cpp
enum class RenderAnimation {
  Idle, Run, Attack, Dodge, Radiance, Current, Corruption, Ultimate, Hit, Death,
};

struct ActorRenderState {
  bool alive = true;
  RenderAnimation action = RenderAnimation::Idle;
  bool hit = false;
  bool moving = false;
};
```

`ChooseAnimation` 按 `Death > action != Idle > Hit > Run > Idle` 返回。`RenderAnimationName` 使用设计文档中的首选名称。`ResolveClip` 为 Dodge 尝试 `{Running_Strafe_Right, run, idle}`，为四类技能尝试 `{专用名, attack, idle}`，其余动画尝试 `{标准名, idle}`，最后才返回第一个 clip。

- [ ] **Step 4: 验证 GREEN**

运行聚焦测试，Expected: exit 0。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/render_animation.h tests/test_render_animation.cpp
git commit -m "feat: 扩展角色动作动画意图" \
  -m "增加闪避和四类技能 clip 映射及安全回退。" \
  -m "Prompt: 完成 M4 Task 1 动画响应目标"
```

### Task 2: 发布玩家精确战斗动作

**Files:**
- Modify: `native/gameplay/combat/action_state_machine.h`
- Modify: `native/gameplay/combat/combat_controller.h`
- Modify: `native/gameplay/combat/combat_controller.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `tests/test_action_state_machine.cpp`
- Modify: `tests/test_render_animation.cpp`

**Interfaces:**
- Produces: `CombatAction ActionStateMachine::activeAction() const`
- Produces: `CombatSnapshot::activeCombatAction`
- Produces: `RenderAnimation PlayerRenderAnimation(uint8_t, CombatAction)`

- [ ] **Step 1: 写入状态机失败测试**

在 `tests/test_action_state_machine.cpp` 的现有合法上下文中依次请求 Dodge、Radiance、Current、Corruption、Ultimate，并断言动作接受后 `activeAction()` 返回请求动作；每个场景使用新的状态机或 `reset()`，避免冷却和资源串场。

- [ ] **Step 2: 验证 RED**

编译运行 `test_action_state_machine`。

Expected: 编译失败，提示 `activeAction` 不存在。

- [ ] **Step 3: 发布活动 CombatAction**

在 `ActionStateMachine` 增加：

```cpp
CombatAction activeAction() const { return activeAction_; }
```

在 `CombatSnapshot` 增加：

```cpp
CombatAction activeCombatAction = CombatAction::Attack;
```

并在 `CombatController::refreshSnapshot` 赋值：

```cpp
snapshot_.activeCombatAction = actions_.activeAction();
```

- [ ] **Step 4: 验证状态机 GREEN**

运行 `test_action_state_machine`，Expected: exit 0。

- [ ] **Step 5: 写入玩家投影失败测试**

将动作投影提取为可测试的头文件内纯函数或 `loop.cpp` 对应的小型模块，并在 `test_render_animation.cpp` 断言：Attack1–4 → Attack、Dodging → Dodge、三种 CastingSource → 对应技能、CastingUltimate → Ultimate、Idle → Idle。

- [ ] **Step 6: 验证投影 RED**

运行 `test_render_animation`，Expected: 新增映射断言失败或函数尚不存在。

- [ ] **Step 7: 实现玩家投影并接入 Loop**

用 `combatSnapshot.currentAction` 和 `activeCombatAction` 设置 `surface.player3dAnimation.action`，保留 `alive/hit/moving`。删除旧 `isAttackingAction` 和 `attacking` 布尔映射。

- [ ] **Step 8: 验证玩家链路 GREEN**

运行 `test_action_state_machine` 与 `test_render_animation`，Expected: 均 exit 0。

- [ ] **Step 9: 提交**

```bash
git add native/gameplay/combat/action_state_machine.h \
  native/gameplay/combat/combat_controller.h \
  native/gameplay/combat/combat_controller.cpp \
  native/engine/core/loop.cpp \
  tests/test_action_state_machine.cpp tests/test_render_animation.cpp
git commit -m "feat: 映射玩家精确技能动画" \
  -m "从战斗状态机发布活动动作并驱动闪避及来源技能 clip。" \
  -m "Prompt: 完成 M4 Task 1 动画响应目标"
```

### Task 3: 投影敌人移动、攻击与受击动画

**Files:**
- Modify: `native/gameplay/ai/encounter_controller.h`
- Modify: `native/gameplay/ai/encounter_controller.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `tests/test_encounter_controller.cpp`
- Modify: `tests/test_render_animation.cpp`

**Interfaces:**
- Produces: `EncounterEnemySnapshot::{moving,attacking,hit}`
- Consumes: `EnemyUpdateResult::{movement,phase,interrupted}`

- [ ] **Step 1: 写入敌人快照失败测试**

扩展遭遇控制器测试：启动 Beast 场景，推进 AI 帧并断言非零移动结果发布 `moving=true`；推进到 `Windup/Active` 断言 `attacking=true`；制造可中断场景后断言 `hit=true`。同时扩展快照相等测试，证明三个字段参与 `operator==`。

- [ ] **Step 2: 验证 RED**

运行对应遭遇/敌人 AI 聚焦测试。

Expected: 编译失败，提示快照字段不存在。

- [ ] **Step 3: 保存 EnemyUpdateResult 动画事实**

在 `EnemySlot` 增加：

```cpp
bool moving = false;
bool attacking = false;
bool hit = false;
```

处理每次 `EnemyUpdateResult` 时：`movement.length() > 0` 设置 moving；`phase` 为 Windup 或 Active 设置 attacking；`interrupted` 设置 hit。每帧用最新结果覆盖，避免状态永久粘住。

- [ ] **Step 4: 发布并投影敌人动作**

把三个字段复制到 `EncounterEnemySnapshot`，纳入 `operator==`；`publish3DEncounterState` 将其投影为 `ActorRenderState.action = attacking ? Attack : Idle`、`hit` 和 `moving`。

- [ ] **Step 5: 验证 GREEN**

运行敌人 AI、遭遇控制器及 `test_render_animation`，Expected: 均 exit 0。

- [ ] **Step 6: 提交**

```bash
git add native/gameplay/ai/encounter_controller.h \
  native/gameplay/ai/encounter_controller.cpp native/engine/core/loop.cpp \
  tests/test_render_animation.cpp tests/test_encounter_controller.cpp
git commit -m "feat: 接入敌人动作动画状态" \
  -m "从 AI 结果发布移动、攻击和受击动画意图。" \
  -m "Prompt: 完成 M4 Task 1 动画响应目标"
```

### Task 4: 投影首领施法、受击与死亡动画

**Files:**
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `tests/test_render_animation.cpp`

**Interfaces:**
- Produces: `Boss3DRenderState::{previousHp,hitAnimationSeconds}`
- Consumes: `BossSnapshot::{hp,castRemainingMs,defeated}`

- [ ] **Step 1: 写入首领投影失败测试**

为纯函数 `BossRenderAnimation(const BossSnapshot&, FixedPoint previousHp)` 写测试：施法期 → Ultimate，HP 下降且未施法 → Hit，击败 → Death，其余 → Idle；死亡优先于施法和受击。

- [ ] **Step 2: 验证 RED**

运行 `test_render_animation`。

Expected: 编译失败，提示投影函数不存在。

- [ ] **Step 3: 实现首领投影**

在循环层维护前一帧 boss HP 和约 0.2 秒受击显示窗口。`defeated` 设置 `alive=false`；`castRemainingMs > 0` 设置 action=Ultimate；受击窗口设置 hit=true。不得伪造当前控制器不存在的普通攻击。

- [ ] **Step 4: 验证 GREEN**

运行 `test_render_animation` 和现有 boss 测试，Expected: 均 exit 0。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/surface.h native/engine/core/loop.cpp \
  tests/test_render_animation.cpp
git commit -m "feat: 接入首领施法受击动画" \
  -m "从首领快照投影施法、受击和死亡动画意图。" \
  -m "Prompt: 完成 M4 Task 1 动画响应目标"
```

### Task 5: 限流日志、回归构建与设备验收

**Files:**
- Modify: `native/engine/render/skinned_model.h`
- Modify: `native/engine/render/skinned_model.cpp`
- Modify: `native/engine/render/surface.cpp`
- Modify: `README.md`
- Test: `tests/test_skinned_model.cpp`
- Test: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces: 动作变化时的 `entity/action/resolvedClip` HiLog
- Produces: 当前分支签名 HAP 与 Task 1 验收记录

- [ ] **Step 1: 写入实际 clip 可观测性失败测试**

为 `SkinnedAnimationState` 或纯辅助函数增加测试，断言请求动画变化时报告一次解析后的实际 clip，连续相同帧不重复报告。

- [ ] **Step 2: 验证 RED**

运行 `test_skinned_model`，Expected: 新 API 不存在或断言失败。

- [ ] **Step 3: 实现变化日志**

让 `SkinnedModel::update` 或调用方取得实际 resolved clip；只在实体动作/clip 改变时输出 HiLog。删除当前每帧 `drawActor` 和 `player3dAnimation.attacking` 刷屏日志。

- [ ] **Step 4: 运行聚焦与契约测试**

运行 `test_render_animation`、`test_skinned_model`、敌人 AI/遭遇测试、boss 测试及：

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node tests/test_bridge_contract.mjs
git diff --check
```

Expected: 全部 exit 0，`git diff --check` 无输出。

- [ ] **Step 5: 构建签名 HAP**

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal --parallel \
--incremental --daemon
```

Expected: `BUILD SUCCESSFUL`，并生成 `entry/build/default/outputs/default/entry-default-signed.hap`。

- [ ] **Step 6: 安装并核对当前二进制**

使用 `/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc -t 127.0.0.1:5555` 先执行覆盖安装；若返回签名不一致，则卸载 `com.ethelandev.myworld` 后安装当前 signed HAP。随后清空 HiLog 并启动 `com.ethelandev.myworld/EntryAbility`，记录 HAP SHA-256、设备版本、ABI 和应用 PID。

- [ ] **Step 7: 验收玩家全部动作**

在训练场景逐项触发普攻、闪避、辉印、脉流、蚀质和终结。只有动作被战斗状态机接受时才采信日志；确认实际 clip 分别为 `attack`、`Running_Strafe_Right`、`Spellcast_Raise`、`Spellcast_Shoot`、`Spellcasting`、`Spellcast_Long`。闪避前后采集玩家位置或截图，确认位移。

- [ ] **Step 8: 验收移动、敌人与首领**

拖动摇杆确认 `run → idle` 和 yaw 变化；启动 Beast/Mixed/Boss 场景，确认敌人移动和攻击不再始终为 idle，受击/死亡动作可见，首领施法出现 `Spellcast_Long`。采集截图和动作变化日志。

- [ ] **Step 9: 扫描图形稳定性**

确认应用进程存活，扫描 `SIGSEGV|cppcrash|glGetString.*invalid|RequestBuffer.*fail|EGL.*error`。模拟器 DGLES 吞吐统计若仍存在，必须与应用 PID、交换缓冲成功及画面证据一起分类，不得直接当作根因或忽略。

- [ ] **Step 10: 更新 README 并提交**

记录 Task 1 聚焦测试、HAP SHA、设备信息、逐项动画 clip 和已知限制。

```bash
git add native/engine/render/skinned_model.h native/engine/render/skinned_model.cpp \
  native/engine/render/surface.cpp tests/test_skinned_model.cpp README.md
git commit -m "test: 完成 Task 1 设备验收" \
  -m "记录角色、敌人和首领动画链路的模拟器验证证据。" \
  -m "Prompt: 完成 M4 Task 1 动画响应目标"
```

## 聚焦测试编译命令

`test_render_animation` 需要显式 macOS SDK、libc++ 和依赖源码：

```bash
SDKROOT_PATH=$(xcrun --sdk macosx --show-sdk-path)
CLANGXX_PATH=$(xcrun --find clang++)
"$CLANGXX_PATH" -std=c++17 -isysroot "$SDKROOT_PATH" \
  -I"$SDKROOT_PATH/usr/include/c++/v1" -I. -Inative -Inative/engine/math \
  tests/test_render_animation.cpp \
  native/engine/render/skinned_model.cpp native/engine/render/texture.cpp \
  native/gameplay/ai/action_executor.cpp native/gameplay/ai/combat_region.cpp \
  native/gameplay/ai/decision_policy.cpp native/gameplay/ai/encounter_controller.cpp \
  native/gameplay/ai/enemy_agent.cpp native/gameplay/ai/enemy_archetypes.cpp \
  native/gameplay/ai/perception_system.cpp native/gameplay/ai/tactical_planner.cpp \
  native/gameplay/combat/action_state_machine.cpp native/gameplay/combat/combat_controller.cpp \
  native/gameplay/combat/combat_resources.cpp native/gameplay/combat/damage_resolver.cpp \
  native/gameplay/combat/resonance.cpp native/gameplay/combat/source_aura.cpp \
  native/gameplay/combat/source_reaction_system.cpp native/gameplay/combat/training_pulse.cpp \
  native/gameplay/combat/training_target.cpp native/gameplay/entities/boss.cpp \
  -o /tmp/test_render_animation_task1 && /tmp/test_render_animation_task1
```
