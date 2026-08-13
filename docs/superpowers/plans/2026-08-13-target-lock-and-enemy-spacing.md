# 统一目标锁定与敌人留白 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 增加自动与手动目标锁定、独立锁定按钮和可靠重选，使攻击与表现共享唯一目标，同时让近战、远程和 Boss 与主角保持合理空挡并在群战中稳定环形站位。

**Architecture:** 新增 `TargetLockController` 取代 Loop 直接调用 `SoftTargeting`，由它独占模式、当前 ID、循环顺序、超距/死亡重选和自动锁定活跃窗口。敌人间距由独立 `engagement_spacing` 纯函数给出原型参数、环形槽位与分离向量，DecisionPolicy/TacticalPlanner/Encounter/WildSpawn 只消费结果。

**Tech Stack:** C++17、ArkTS/ArkUI、N-API、现有固定步 Combat/AI、宿主 `clang++` 测试、Hvigor/HarmonyOS API 23。

## Global Constraints

- 单击锁定最近目标，再次单击按距离从近到远循环；约 `500ms` 长按解除且松手不能额外单击。
- 自动模式以距离为首要排序，镜头前方为次级；连招期间保持目标，停止战斗后淡出。
- Boss 和普通敌人统一参与候选，不强制抢锁。
- 攻击结算、投射物、镜头、脚下环、轮廓和血条高亮必须消费同一个目标 ID。
- 近战保留约一个角色身位，远程保持更远距离，Boss 按体型扩大空挡。
- 敌人只移动自己，不得强制推开主角；攻击突进后回到理想位置。
- 每次提交包含项目格式的 `Prompt:` 摘要。

---

## File Structure

- `native/gameplay/targeting/target_lock_controller.h/.cpp`：唯一目标状态机。
- `native/gameplay/targeting/soft_targeting.h/.cpp`：保留纯候选测量能力，不再持模式。
- `native/gameplay/ai/engagement_spacing.h/.cpp`：原型距离、环形槽位与邻居分离纯函数。
- `native/engine/input/input_event.h`、`native/engine/core/loop.h/.cpp`：锁定输入与唯一目标接线。
- `entry/src/main/ets/ui/CombatControls.ets`、Bridge/N-API 声明：独立按钮、单击/长按映射。
- `native/gameplay/ai/decision_policy.cpp`、`tactical_planner.cpp`、`encounter_controller.cpp`、`wild_spawn_system.cpp`、`boss.cpp`：间距消费与攻击后回位。
- `native/engine/render/surface.h/.cpp`：自动/手动锁定环强度。
- `tests/test_target_lock_controller.cpp`、`tests/test_engagement_spacing.cpp`：新增纯逻辑覆盖。
- `tests/test_soft_targeting.cpp`、`tests/test_enemy_decision.cpp`、`tests/test_tactical_planner.cpp`、`tests/test_encounter_controller.cpp`、`tests/test_wild_spawn_system.cpp`、`tests/test_loop_integration.cpp`、`tests/test_bridge_contract.mjs`：集成回归。

### Task 1: TargetLockController 自动模式与稳定目标

**Files:**
- Create: `native/gameplay/targeting/target_lock_controller.h`
- Create: `native/gameplay/targeting/target_lock_controller.cpp`
- Create: `tests/test_target_lock_controller.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`
- Modify: `native/gameplay/targeting/soft_targeting.h`
- Modify: `native/gameplay/targeting/soft_targeting.cpp`
- Modify: `tests/test_soft_targeting.cpp`

**Interfaces:**
- Produces: `enum class TargetLockMode { Automatic, Manual }`。
- Produces: `struct TargetLockCandidate { EntityId id; Vec2 position; bool alive; bool attackable; bool boss; }`。
- Produces: `struct TargetLockResult { optional<EntityId> id; TargetLockMode mode; float distance; float angle; bool showMarker; }`。
- Produces: `TargetLockResult updateAutomatic(Vec2 player, float cameraYaw, const std::vector<TargetLockCandidate>& candidates, bool attackTriggered, bool comboActive, Tick now)`。
- Produces: `void invalidate(EntityId id)`、`void clear()`。

- [ ] **Step 1: 写自动锁定 RED 测试**

测试防止“按角度优先而跳过更近敌人”“连招中抖动换目标”“无攻击时永远显示环”。至少断言：距离 `0.3` 但角度较大的目标优于距离 `0.5` 正前目标；相同距离时前方优先；连招活跃保持 preferred；目标死亡重选；攻击停止 800ms 后 `showMarker=false`。

- [ ] **Step 2: 编译确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-lock-auto-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_target_lock_controller.cpp \
  native/gameplay/targeting/target_lock_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/lock"
```

Expected: FAIL，控制器不存在。

- [ ] **Step 3: 最小实现自动模式**

把 SoftTargeting 的测量提取为 `MeasureTarget(player, cameraYaw, candidate)`；控制器过滤无效候选后按 `(distance, angle, id)` 排序。只有 `attackTriggered`、`comboActive` 或 800ms 活跃窗口内显示 marker；连招内如果当前候选仍存活且在维持距离内，保持当前 ID。

- [ ] **Step 4: 运行 GREEN**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-lock-auto-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_target_lock_controller.cpp \
  native/gameplay/targeting/target_lock_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/lock"
"$TEST_BIN_DIR/lock"
```

Expected: 退出 `0`。

- [ ] **Step 5: 提交 Task 1**

```bash
git add native/gameplay/targeting/target_lock_controller.* \
  native/gameplay/targeting/soft_targeting.* tests/test_target_lock_controller.cpp \
  tests/test_soft_targeting.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 统一自动目标锁定" \
  -m "按距离优先选敌并在连招中稳定目标，停止战斗后自动淡出锁定表现。" \
  -m "Prompt: 根据距离自动锁定攻击敌人"
```

### Task 2: 手动循环、解除和目标失效重选

**Files:**
- Modify: `native/gameplay/targeting/target_lock_controller.h`
- Modify: `native/gameplay/targeting/target_lock_controller.cpp`
- Modify: `tests/test_target_lock_controller.cpp`

**Interfaces:**
- Produces: `TargetLockResult cycleManual(Vec2 player, float cameraYaw, const std::vector<TargetLockCandidate>& candidates, Tick now)`。
- Produces: `TargetLockResult releaseManual(...same context...)`，切回 Automatic。
- Produces: `TargetLockResult refresh(...candidates...)`，处理死亡、超距与卸载。

- [ ] **Step 1: 添加手动模式 RED 用例**

以距离 `0.2/0.4/0.6` 三个候选和一个 Boss `0.5`：首次 cycle 选 `0.2`，后续顺序 `0.4→Boss 0.5→0.6→0.2`；输入数组反序不改变顺序。当前目标死亡/超距时选择下一个最近；所有候选无效时模式恢复 Automatic 且 ID 空。

- [ ] **Step 2: 运行确认 RED**

运行 Task 1 的编译命令，Expected: FAIL，手动 API 或行为缺失。

- [ ] **Step 3: 实现手动循环快照**

每次点击都从当前有效候选按 `(distance,id)` 重新排序；若当前 ID 存在，选其后一项并环绕，否则选第一项。手动维持距离单独配置为自动获取距离的 `1.5` 倍。`releaseManual` 清除手动 ID 并立即按自动规则刷新，但不伪造攻击活跃窗口。

- [ ] **Step 4: 运行 GREEN 并做 mutation check**

运行目标控制器测试；临时将排序改为 angle-first，确认距离用例失败后恢复。Expected: 最终退出 `0`。

- [ ] **Step 5: 提交 Task 2**

```bash
git add native/gameplay/targeting/target_lock_controller.* tests/test_target_lock_controller.cpp
git commit -m "feat: 增加手动循环锁定" \
  -m "支持按距离循环敌人与Boss，长距失效自动重选且无候选时回到自动模式。" \
  -m "Prompt: 手动切换敌人进行锁定攻击"
```

### Task 3: 锁定按钮、500ms 长按和 N-API 输入

**Files:**
- Modify: `native/engine/input/input_event.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/ui/CombatControls.ets`
- Modify: `tests/test_input_queue.cpp`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces InputAction: `CycleTarget`、`ReleaseTargetLock` appended after existing values。
- Produces Bridge actions: `pushAction(11)` = cycle，`pushAction(12)` = release。
- Produces ArkTS state: `lockPressedAtMs: number`、`lockLongPressFired: boolean`。

- [ ] **Step 1: 先扩展 Bridge 行为测试**

`test_bridge_contract.mjs` 断言 native action 范围变为 `0..12` 且映射末尾为 `SwitchCharacter, CycleTarget, ReleaseTargetLock`。断言 `CombatControls` 有 `Button('锁定')`，Down 记录时间，约 500ms 触发 `pushAction(12)`，Up 在未长按时只触发 `pushAction(11)`，长按后 Up 不再 cycle。

- [ ] **Step 2: 运行 Node RED**

```bash
node tests/test_bridge_contract.mjs
```

Expected: FAIL，缺少锁定按钮和动作映射。

- [ ] **Step 3: 实现 Native 动作映射**

枚举只追加，不改已有 0–10 数值。`NativePushAction` 接受 `0..12`；Loop `processInput` 把两个动作设为单帧队列标志，下一固定步交给 `TargetLockController`。

- [ ] **Step 4: 实现 ArkTS 长按且避免松手双触发**

优先使用 ArkUI `LongPressGesture({ repeat: false, duration: 500 })` 与 `onAction` 设置 fired 并发 release；若当前 API 23 组件组合不允许 Button gesture，则在 Touch Down 启动 `setTimeout(500)`，Up/Cancel 清 timer。无论实现哪种，Up 必须检查 `lockLongPressFired`；Cancel 不发送 cycle。

- [ ] **Step 5: 运行 GREEN 与 HAP 类型检查**

```bash
node tests/test_bridge_contract.mjs
```

Expected: Node 退出 `0`。随后运行增量 HAP 构建；Expected SDK 完整时 `BUILD SUCCESSFUL`，否则记录真实 SDK 错误。

- [ ] **Step 6: 提交 Task 3**

```bash
git add native/engine/input/input_event.h native/engine/core/loop.cpp \
  entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts \
  entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/ui/CombatControls.ets \
  tests/test_input_queue.cpp tests/test_bridge_contract.mjs
git commit -m "feat: 增加锁定操作按钮" \
  -m "单击循环目标，500ms长按解除锁定，并避免长按松手额外切换。" \
  -m "Prompt: 新增自动和手动切换敌人锁定功能"
```

### Task 4: Loop 唯一目标接线与表现一致性

**Files:**
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `tests/test_loop_integration.cpp`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: `TargetLockController` from Tasks 1–3。
- Produces snapshot: `targetLockMode` (`0 Automatic, 1 Manual`) and existing `targetId` from the same result。
- Produces render marker: `TargetMarkerRenderState::manual`、`visibility`。

- [ ] **Step 1: 添加 Loop RED 测试证明唯一 ID**

集成测试创建普通敌人、野怪和 Boss 候选，先自动攻击再手动循环；每一步断言以下值相同：`snapshot.targetId`、`surface.targetMarker3d.targetId`、`combat external binding id`、`EncounterController` 接收的 selected id、投射物 releaseTarget。目标死亡后下一固定步全部同步切换，不能出现一帧分叉。

- [ ] **Step 2: 运行 Loop RED**

使用 Plan 1 的 Loop 编译脚本，Expected: FAIL，Loop 仍直接 `softTargeting.select` 且没有模式。

- [ ] **Step 3: 替换 Loop 的 currentTarget/SoftTargeting 状态**

Loop 持有 `TargetLockController targetLock` 和 `TargetLockResult currentTarget`。候选收集后统一调用控制器，再将唯一 ID 传给 camera、marker、boss targeted、combat binding、encounter update 和 VFX release target。删除任何下游再次调用 `select` 的代码。

自动 marker visibility 用控制器活跃窗口；手动 marker 始终显示。TargetMarker 渲染手动模式的 alpha 基线 `0.92`、自动活跃 `0.72` 并按 fade 值衰减，尺寸不夸张放大。

- [ ] **Step 4: 运行 GREEN**

运行 `test_target_lock_controller`、`test_soft_targeting`、`test_loop_integration` 和 `test_bridge_contract.mjs`。Expected: 全部退出 `0`。

- [ ] **Step 5: 提交 Task 4**

```bash
git add native/engine/core/loop.* native/engine/core/game_snapshot.h \
  native/engine/render/surface.* entry/src/main/ets/napi/Bridge.ets \
  entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts \
  tests/test_loop_integration.cpp tests/test_bridge_contract.mjs
git commit -m "refactor: 统一锁定目标数据流" \
  -m "攻击、投射物、镜头、锁定环和目标快照统一消费TargetLockController结果。" \
  -m "Prompt: 锁定图标和实际攻击始终指向同一敌人"
```

### Task 5: 原型交战距离与环形槽位纯函数

**Files:**
- Create: `native/gameplay/ai/engagement_spacing.h`
- Create: `native/gameplay/ai/engagement_spacing.cpp`
- Create: `tests/test_engagement_spacing.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Produces: `struct EngagementRange { float minimum; float ideal; float attack; float maxPursuit; }`。
- Produces: `EngagementRange EngagementRangeFor(EnemyArchetype archetype, float bodyRadius, bool boss)`。
- Produces: `Vec2 EngagementSlotPosition(EntityId id, Vec2 player, float idealRadius, const std::vector<EntityId>& participants)`。
- Produces: `Vec2 SeparationOffset(EntityId self, Vec2 selfPosition, const std::vector<EngagementNeighbor>& neighbors, float minimumSpacing)`。

- [ ] **Step 1: 写距离和槽位 RED 测试**

用字面量范围断言：近战 minimum 至少 `0.08`、远程 ideal 大于近战、Boss minimum 大于普通近战；所有 `minimum < ideal <= attack < maxPursuit`。四名敌人的槽位角度均匀、ID 顺序稳定，输入参与者反序不改变各 ID 位置；重叠邻居产生有限非零分离向量，主角位置不被修改。

- [ ] **Step 2: 运行确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-spacing-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_engagement_spacing.cpp \
  native/gameplay/ai/engagement_spacing.cpp -o "$TEST_BIN_DIR/spacing"
```

Expected: FAIL，模块不存在。

- [ ] **Step 3: 实现稳定环形分配与退化回退**

原型参数集中一处：近战 minimum `0.08`、ideal `0.14`；远程 minimum `0.16`、ideal `0.30`；Boss minimum 为 `max(0.14, bodyRadius*1.5)`。实际 attack/maxPursuit 从现有 ability/region 上限适配，不缩短攻击有效性。

槽位将参与 ID 排序后取 index，基准角由最小 ID 的稳定哈希决定，角间距 `2π/N`。分离向量在完全重叠时用两 ID 哈希方向，强度钳制，禁止 NaN。

- [ ] **Step 4: 运行 GREEN**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-spacing-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_engagement_spacing.cpp \
  native/gameplay/ai/engagement_spacing.cpp -o "$TEST_BIN_DIR/spacing"
"$TEST_BIN_DIR/spacing"
```

Expected: 退出 `0`。

- [ ] **Step 5: 提交 Task 5**

```bash
git add native/gameplay/ai/engagement_spacing.* tests/test_engagement_spacing.cpp \
  entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 定义敌人交战留白" \
  -m "集中近战、远程和Boss距离并提供稳定环形槽位与敌人分离向量。" \
  -m "Prompt: 敌人不能离主角太近并留出空挡"
```

### Task 6: DecisionPolicy 与 TacticalPlanner 进入理想位置

**Files:**
- Modify: `native/gameplay/ai/enemy_ai_types.h`
- Modify: `native/gameplay/ai/perception_system.cpp`
- Modify: `native/gameplay/ai/decision_policy.cpp`
- Modify: `native/gameplay/ai/tactical_planner.cpp`
- Modify: `tests/test_enemy_decision.cpp`
- Modify: `tests/test_tactical_planner.cpp`

**Interfaces:**
- Consumes: `EngagementRange` and slot/separation from Task 5。
- Produces in `PerceptionSnapshot`: `Vec2 engagementSlot`、`Vec2 separationOffset`、`EngagementRange engagementRange`。
- Produces policy: distance below minimum => Retreat；within ideal band and ability range => Attack；above band => Chase。

- [ ] **Step 1: 写 RED 行为用例**

近战距离 `0.04` 必须 Retreat 而非 Attack；`0.14` 可 Attack；远程 `0.10` Retreat、`0.30` Attack。TacticalPlanner 的 Chase 目标必须是 `engagementSlot + separationOffset` 而不是 playerPosition；Retreat 远离主角但不超出 combat region。

- [ ] **Step 2: 运行确认 RED**

编译运行 `test_enemy_decision` 和 `test_tactical_planner`，Expected: 当前近战 `<=0.25` 直接 Attack，测试失败。

- [ ] **Step 3: 接入 EngagementRange**

移除 `kMeleeAttackDistance/kPriestRetreatDistance` 重复常量。DecisionPolicy 只基于 snapshot 统一参数做分支。TacticalPlanner Chase 使用 slot，Retreat 使用 away + separation 并投影回 `CombatRegion`；Attack plan 仍以玩家为 ability target，不把槽位误作伤害目标。

- [ ] **Step 4: 运行 GREEN**

Expected: 两个测试退出 `0`，原支持技能与区域返回用例不回归。

- [ ] **Step 5: 提交 Task 6**

```bash
git add native/gameplay/ai/enemy_ai_types.h native/gameplay/ai/perception_system.cpp \
  native/gameplay/ai/decision_policy.cpp native/gameplay/ai/tactical_planner.cpp \
  tests/test_enemy_decision.cpp tests/test_tactical_planner.cpp
git commit -m "refactor: 按理想距离规划敌人移动" \
  -m "过近后撤、过远追向环形槽位，并叠加敌人分离而不推动主角。" \
  -m "Prompt: 近战远程和Boss保持合理距离"
```

### Task 7: Encounter、WildSpawn 与 Boss 攻击后回位

**Files:**
- Modify: `native/gameplay/ai/encounter_controller.h`
- Modify: `native/gameplay/ai/encounter_controller.cpp`
- Modify: `native/gameplay/ai/wild_spawn_system.h`
- Modify: `native/gameplay/ai/wild_spawn_system.cpp`
- Modify: `native/gameplay/entities/boss.h`
- Modify: `native/gameplay/entities/boss.cpp`
- Modify: `tests/test_encounter_controller.cpp`
- Modify: `tests/test_wild_spawn_system.cpp`
- Modify: `tests/test_boss_controller.cpp`
- Modify: `tests/test_enemy_combat_integration.cpp`

**Interfaces:**
- Consumes: engagement pure functions and plans from Tasks 5–6。
- Produces: 每帧参与者 ID 集合和邻居位置供槽位/分离。
- Produces: 攻击 Active 可短暂进入 attack range；Recovery 目标重新设为 engagement slot。

- [ ] **Step 1: 写三类集成 RED 测试**

两名近战从同一点更新 120 帧后不重叠且距玩家不小于 minimum；主角主动靠近时 player position 完全不变，敌人后撤。远程保持 ideal band。Boss body radius 放大后不遮住玩家距离。攻击 Active 可接近命中，但 Recovery 完成后回到 ideal 允许误差 `±0.02`。

- [ ] **Step 2: 运行并确认 RED**

编译运行四个相关测试，Expected: 至少贴身/重叠/恢复位置断言失败。

- [ ] **Step 3: 接入槽位参与者与分离**

Encounter 和 WildSpawn 每帧在逻辑快照中收集同一玩家目标下的存活敌人，按 ID 排序。对每个 agent 计算 slot 和 neighbors；Boss 单独使用 boss range。movement resolver 只修改敌人位置，不写玩家位置。

- [ ] **Step 4: 接入攻击突进/回位时序**

Windup 保持站位；Active 允许 action executor 的 Move 效果或最小必要突进；Recovery 将 planner intent 设为 Retreat/Chase 到 slot，直到进入 ideal band。最大追击仍使用现有 spawn region，不因无限地图取消脱战。

- [ ] **Step 5: 运行 GREEN 与 600 帧稳定性重放**

运行相关测试并增加相同输入重放 10 次快照相等断言。Expected: 全部退出 `0`，无 NaN/重叠。

- [ ] **Step 6: 提交 Task 7**

```bash
git add native/gameplay/ai/encounter_controller.* native/gameplay/ai/wild_spawn_system.* \
  native/gameplay/entities/boss.* tests/test_encounter_controller.cpp \
  tests/test_wild_spawn_system.cpp tests/test_boss_controller.cpp \
  tests/test_enemy_combat_integration.cpp
git commit -m "feat: 接入群敌环形站位" \
  -m "敌人和Boss保持空挡、互相分离，攻击突进结束后回到理想交战位置。" \
  -m "Prompt: 群战时敌人不能贴身和重叠"
```

### Task 8: 全面验证与项目记忆

**Files:**
- Modify: `PROJECT_STATE.md`
- Modify: `DECISIONS.md`
- Modify: `TASKS.md`

- [ ] **Step 1: 运行聚焦测试**

运行 `test_target_lock_controller`、`test_soft_targeting`、`test_input_queue`、`test_enemy_decision`、`test_tactical_planner`、`test_engagement_spacing`、`test_encounter_controller`、`test_wild_spawn_system`、`test_boss_controller`、`test_enemy_combat_integration`、`test_loop_integration` 和 `node tests/test_bridge_contract.mjs`。

Expected: 全部退出 `0`。

- [ ] **Step 2: 运行完整宿主、HAP 构建和 git 检查**

沿用 Plan 1 完整宿主重建脚本，之后运行：

```bash
git diff --check
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p product=default -p module=entry@default assembleHap \
  --analyze=normal --parallel --incremental
```

Expected: 宿主零失败、diff check 无输出、SDK 完整时构建成功。

- [ ] **Step 3: 更新项目记忆与真机清单**

记录唯一目标数据流、按钮动作值、500ms 长按、原型距离与环形槽位决策。真机待办包含：单击循环、长按解除、死亡/超距重选、Boss 不抢锁、锁定环和实际攻击一致、近战/远程/Boss 空挡、多敌人不重叠、主角不被推开。

- [ ] **Step 4: 提交 Task 8**

```bash
git add PROJECT_STATE.md DECISIONS.md TASKS.md
git commit -m "docs: 记录锁定与敌人留白验证" \
  -m "记录统一目标控制器、输入映射、站位距离和真机验收边界。" \
  -m "Prompt: 自动手动锁定和敌人距离优化验收"
```

## Plan 2 Completion Gate

- 自动/手动所有目标转换均由控制器测试和 Loop 集成测试覆盖。
- 目标 ID 在结算、VFX、镜头和表现中没有一帧分叉。
- 锁定按钮 HAP 类型检查通过，或明确 SDK 阻塞。
- 600 帧群敌仿真无重叠、NaN 和主角强制位移。
- 完整宿主测试零失败，项目记忆已更新。
