# 动态探索路径门碰撞 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让关闭的探索路径门真实阻挡玩家、敌人和首领，并在机关激活、存档恢复后保持一致的空间通行状态。

**Architecture:** 保持 `BuildingCollision` 的静态环境碰撞职责不变，新增独立的 `ExplorationGateCollision` 动态碰撞组件。它从 `ExplorationContent` 的路径门配置生成关闭门的 `BuildingBox`，Loop 和遭遇系统按固定顺序先解算静态建筑、再解算动态路径门；路径门状态唯一来源仍是 `ExplorationContent`。

**Tech Stack:** HarmonyOS Native C++17、ArkTS/ArkUI、N-API、CMake/Hvigor、现有 `BuildingCollision` OBB 碰撞解算、Node.js 契约测试、macOS clang++ 纯逻辑测试。

## Global Constraints

- `assets/world/world.json` 是探索内容单一事实来源，运行时不解析 JSON。
- 路径门状态不得在碰撞层复制持久化状态；只通过 `ExplorationContent::isGateOpen()` 查询。
- 静态建筑碰撞与动态路径门碰撞必须保持确定的解算顺序：静态建筑后动态路径门。
- 玩家、敌人和首领必须使用相同的动态路径门碰撞语义。
- 保持现有 V1–V9 存档兼容，不改变 V9 已有字段顺序。
- 每个行为变更先写失败测试并观察预期失败，再写生产代码。
- 不在本次范围内新增导航网格、门动画资源、联网同步、六区域内容化或首领两阶段机制。

## 文件职责映射

- `assets/world/world.json`：增加路径门碰撞几何配置。
- `config/schema/world.schema.json`：约束碰撞几何字段类型与范围。
- `automation/assets/generate_world_layout.mjs`：校验并生成路径门碰撞字段。
- `native/generated/world_layout.gen.h`：生成的 constexpr 路径门数据，不手工编辑。
- `native/gameplay/world/exploration_gate_collision.h/.cpp`：动态关闭门碰撞集合与 `BuildingBox` 解算适配。
- `native/gameplay/world/exploration_content.h/.cpp`：暴露路径门状态和碰撞配置所需的稳定读取接口。
- `native/engine/core/loop.h/.cpp`：组合玩家碰撞、创建动态门集合、向敌人/首领注入组合解算器。
- `entry/src/main/cpp/CMakeLists.txt`：注册新 Native 源文件。
- `native/engine/core/game_snapshot.h`、`native_bridge.cpp`、`Bridge.ets`、`Index.d.ts`、`GamePage.ets`、`ExplorationHud.ets`：发布当前受阻路径门和关联机关提示。
- `tests/test_exploration_gate_collision.cpp`：动态门碰撞单元测试。
- `tests/test_exploration_content.cpp`、`tests/test_loop_integration.cpp`、`tests/test_encounter_building_collision.cpp`、`tests/test_save_v8.cpp`：状态切换、组合解算、敌人/首领和存档回归。
- `tests/test_bridge_contract.mjs`：跨层字段和 HUD 契约。
- `PROJECT_STATE.md`、`TASKS.md`、`DECISIONS.md`：记录已完成动态门能力和剩余真机验收。

---

### Task 1: 增加路径门碰撞数据字段与生成校验

**Files:** `assets/world/world.json`, `config/schema/world.schema.json`, `automation/assets/generate_world_layout.mjs`, `native/generated/world_layout.gen.h`, `tests/test_world_layout_gen.cpp`。

**Interfaces:** 生成 `WorldTraversalGateDef::halfExtents[2]`, `yaw`, `top`；`WorldLayout::kTraversalGates` 是 `ExplorationContent` 的唯一生成输入。

- [ ] **Step 1: Write the failing test**

在路径门测试循环中断言：

```cpp
assert(gate.halfExtents[0] > 0.0f && gate.halfExtents[0] < 0.15f);
assert(gate.halfExtents[1] > 0.0f && gate.halfExtents[1] < 0.15f);
assert(std::isfinite(gate.yaw));
assert(gate.top > 0.0f && gate.top < 0.5f);
```

- [ ] **Step 2: Run test to verify it fails**

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative tests/test_world_layout_gen.cpp -o /tmp/test_world_layout_gen_gate_red
```

Expected: FAIL，因为生成门定义还没有这些字段。

- [ ] **Step 3: Write minimal implementation**

为四个 `traversalGates` 条目增加 `halfExtents: [x, z]`、`yaw`、`top`；Schema 增加必填字段和范围；生成器扩展结构体输出并拒绝非有限、非正或超范围几何。运行生成器更新头文件。

- [ ] **Step 4: Run test to verify it passes**

```bash
node automation/assets/generate_world_layout.mjs
/tmp/test_world_layout_gen_gate_red
```

Expected: PASS，第二次生成保持文件字节一致。

- [ ] **Step 5: Commit**

```bash
git add assets/world/world.json config/schema/world.schema.json automation/assets/generate_world_layout.mjs native/generated/world_layout.gen.h tests/test_world_layout_gen.cpp
git commit -m "feat: 增加路径门碰撞几何配置" -m "Prompt: 继续开发动态探索路径门碰撞"
```

### Task 2: 实现动态路径门碰撞组件

**Files:** 创建 `native/gameplay/world/exploration_gate_collision.h/.cpp`、`tests/test_exploration_gate_collision.cpp`；修改 `native/gameplay/world/exploration_content.h/.cpp`。

**Interfaces:** `ExplorationGateCollision::fromContent(const ExplorationContent&)`、`resolve(float&, float&, float radius, float height) const`、`blocks(int32_t gateId) const`、`boxes()`。

- [ ] **Step 1: Write the failing test**

```cpp
ExplorationContent content = ExplorationContent::verticalSlice();
ExplorationGateCollision closed = ExplorationGateCollision::fromContent(content);
assert(closed.blocks(81));
float x = 0.78f;
float y = 0.28f;
assert(closed.resolve(x, y, 0.012f, 0.0f).touching);
content.activatePuzzle(71, MotionState::Swimming);
ExplorationGateCollision open = ExplorationGateCollision::fromContent(content);
assert(!open.blocks(81));
x = 0.78f; y = 0.28f;
assert(!open.resolve(x, y, 0.012f, 0.0f).touching);
```

覆盖高度越过、四门过滤、无效输入安全和机关激活前后状态切换。

- [ ] **Step 2: Run test to verify it fails**

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative tests/test_exploration_gate_collision.cpp native/gameplay/world/exploration_content.cpp native/gameplay/world/exploration_gate_collision.cpp -o /tmp/test_exploration_gate_collision_red
```

Expected: FAIL，因为组件尚不存在。

- [ ] **Step 3: Write minimal implementation**

在 `ExplorationContent` 增加 `gateById(int32_t)` 与 `puzzleById(int32_t)` 只读查询。新组件只把 `!isGateOpen(id)` 的门转换成 `BuildingBox`，保留 gate id 与 box 下标的稳定映射；`resolve` 复用 OBB 推出语义，盒顶以上不阻挡，非法输入返回不触碰。

- [ ] **Step 4: Run test to verify it passes**

```bash
/tmp/test_exploration_gate_collision_red
```

Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add native/gameplay/world/exploration_content.h native/gameplay/world/exploration_content.cpp native/gameplay/world/exploration_gate_collision.h native/gameplay/world/exploration_gate_collision.cpp tests/test_exploration_gate_collision.cpp
git commit -m "feat: 实现动态路径门碰撞组件" -m "Prompt: 继续开发动态探索路径门碰撞"
```

### Task 3: 接入 Loop 玩家移动和动态门状态刷新

**Files:** 修改 `native/engine/core/loop.h/.cpp`, `entry/src/main/cpp/CMakeLists.txt`, `tests/test_exploration_loop_contract.cpp`, `tests/test_loop_integration.cpp`。

**Interfaces:** `Loop::explorationGateCollision`；`Loop::resolvePlayerWorldCollision(Vec2&, float)`；固定顺序为静态建筑后动态门，机关激活后下一固定步使用新状态。

- [ ] **Step 1: Write the failing test**

增加关闭 81 门时玩家探针被推出、机关激活后刷新可通过的行为断言，并锁定 Loop 源码中静态 `buildingCollision.resolve` 位于动态门 `resolve` 之前。

- [ ] **Step 2: Run test to verify it fails**

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative tests/test_loop_integration.cpp native/engine/core/loop.cpp -o /tmp/test_loop_integration_gate_red
```

Expected: FAIL，因为 Loop 没有动态门集合和组合 helper。

- [ ] **Step 3: Write minimal implementation**

增加动态门组件，玩家移动后先调用静态碰撞，再调用动态门碰撞；探索交互完成后刷新动态门集合。不要把门盒写入静态 `BuildingCollision`。

- [ ] **Step 4: Run test to verify it passes**

运行 Loop 聚焦测试，Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add native/engine/core/loop.h native/engine/core/loop.cpp entry/src/main/cpp/CMakeLists.txt tests/test_exploration_loop_contract.cpp tests/test_loop_integration.cpp
git commit -m "feat: 接入玩家路径门动态碰撞" -m "Prompt: 继续开发动态探索路径门碰撞"
```

### Task 4: 接入敌人与首领的组合碰撞

**Files:** 修改 `native/engine/core/loop.cpp`, `tests/test_encounter_building_collision.cpp`, `tests/test_bridge_contract.mjs`。

**Interfaces:** 为 `EncounterController::update` 注入的 resolver 先执行 `buildingCollision.resolve`，再执行动态门 resolver；动态门状态从同一 `ExplorationContent` 读取。

- [ ] **Step 1: Write the failing test**

扩展遭遇碰撞测试，使关闭门位于敌人/首领追击路径上，断言两个实体均不能进入门盒；增加调用计数，确认动态门 resolver 被调用。

- [ ] **Step 2: Run test to verify it fails**

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
clang++ -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative \
  tests/test_encounter_building_collision.cpp \
  native/gameplay/ai/encounter_controller.cpp native/gameplay/ai/combat_region.cpp \
  native/gameplay/ai/enemy_agent.cpp native/gameplay/ai/enemy_archetypes.cpp \
  native/gameplay/combat/combat_controller.cpp native/gameplay/combat/combat_resources.cpp \
  native/gameplay/combat/action_state_machine.cpp native/gameplay/combat/training_target.cpp \
  native/gameplay/combat/damage_resolver.cpp native/gameplay/combat/source_aura.cpp \
  native/gameplay/combat/source_reaction_system.cpp native/gameplay/combat/resonance.cpp \
  native/gameplay/combat/training_pulse.cpp native/gameplay/entities/boss.cpp \
  native/gameplay/player/character.cpp native/gameplay/growth/character_growth.cpp \
  native/gameplay/growth/weapon_system.cpp native/gameplay/inventory/inventory.cpp \
  native/gameplay/world/exploration_content.cpp native/gameplay/world/exploration_gate_collision.cpp \
  -o /tmp/test_encounter_gate_red
```

Expected: FAIL，因为现有 resolver 只有静态建筑碰撞。

- [ ] **Step 3: Write minimal implementation**

抽取 Loop 的组合位置 resolver，捕获 `ExplorationGateCollision`；每个敌人和首领位置先过静态建筑，再过关闭门碰撞。遇到已开启门时动态 resolver 不改变位置。

- [ ] **Step 4: Run test to verify it passes**

运行 `test_encounter_building_collision` 和相关 AI/首领测试，Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add native/engine/core/loop.cpp tests/test_encounter_building_collision.cpp tests/test_bridge_contract.mjs
git commit -m "feat: 阻挡敌人与首领穿过路径门" -m "Prompt: 继续开发动态探索路径门碰撞"
```

### Task 5: 接入 HUD 门状态、存档回归和项目记忆

**Files:** 修改 `native/engine/core/game_snapshot.h`, `native/engine/core/loop.cpp`, `entry/src/main/cpp/native_bridge.cpp`, `entry/src/main/cpp/types/libnative_game/Index.d.ts`, `entry/src/main/ets/napi/Bridge.ets`, `entry/src/main/ets/pages/GamePage.ets`, `entry/src/main/ets/ui/ExplorationHud.ets`, `tests/test_save_v8.cpp`, `tests/test_bridge_contract.mjs`, `PROJECT_STATE.md`, `TASKS.md`, `DECISIONS.md`。

**Interfaces:** 新增 `explorationBlockedGateId`、`explorationBlockedGateLabel`、`explorationBlockedByPuzzleLabel` 快照字段；ArkTS 只消费聚合字段。

- [ ] **Step 1: Write the failing test**

扩展 Node 契约，要求上述字段存在于 `GameSnapshot`、Native bridge、`.d.ts`、Bridge、GamePage 和 HUD；存档测试要求加载状态不丢失。

- [ ] **Step 2: Run test to verify it fails**

```bash
node tests/test_bridge_contract.mjs
```

Expected: FAIL，新增字段尚不存在。

- [ ] **Step 3: Write minimal implementation**

Loop 在当前探索目标为关闭路径门时发布门 ID、门名称和关联机关名称；桥接和 ArkTS 逐层增加字段；HUD 仅在门关闭且玩家接近时显示阻挡提示，开启后回退普通目标提示。

- [ ] **Step 4: Run test to verify it passes**

运行 Node 契约、V9 存档和 HUD 相关纯逻辑测试，Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add native entry tests/test_save_v8.cpp PROJECT_STATE.md TASKS.md DECISIONS.md
git commit -m "feat: 展示路径门阻挡状态" -m "Prompt: 继续开发动态探索路径门碰撞"
```

### Task 6: 全量验证与最终交付

**Files:** 不新增业务文件；必要时只修改本计划涉及的回归测试或项目记忆。

- [ ] **Step 1: 运行路径门和探索聚焦测试**

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CLANG="$(xcrun --find clang++)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
"$CLANG" "${COMMON[@]}" tests/test_exploration_gate_collision.cpp native/gameplay/world/exploration_content.cpp native/gameplay/world/exploration_gate_collision.cpp -o /tmp/final_gate && /tmp/final_gate
"$CLANG" "${COMMON[@]}" tests/test_world_layout_gen.cpp -o /tmp/final_layout && /tmp/final_layout
"$CLANG" "${COMMON[@]}" tests/test_save_v8.cpp native/engine/resource/save.cpp native/gameplay/quest/quest_system.cpp -o /tmp/final_save && /tmp/final_save
node tests/test_bridge_contract.mjs
git diff --check
```

- [ ] **Step 2: Build OHOS Native and HAP**

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw \
  assembleHap --mode module -p module=entry@default -p product=default
```

Expected: `BUILD SUCCESSFUL`，并生成 signed/unsigned HAP。

- [ ] **Step 3: Review changed files and project memory**

确认无运行时 JSON、无第二份门状态、无未提交构建配置变更；在 `PROJECT_STATE.md` 记录实现与未完成真机验收，在 `TASKS.md` 勾选动态门能力，在 `DECISIONS.md` 记录动态碰撞边界。

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-08-07-dynamic-exploration-gate-collision.md native entry assets automation config tests PROJECT_STATE.md TASKS.md DECISIONS.md
git commit -m "feat: 完成动态探索路径门碰撞" -m "Prompt: 继续开发动态探索路径门碰撞"
```
