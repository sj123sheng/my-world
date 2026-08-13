# 战斗血条与扣血动效 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 A「均衡清晰」方向美化普通敌人和 Boss 血条、延迟扣血与伤害数字，限制同屏信息密度并保证不同屏幕比例下一屏清楚可读。

**Architecture:** 把血条延迟层、候选筛选、世界锚点、距离缩放和伤害数字合并拆为纯逻辑模块，Loop 发布有限且完整的渲染状态；GLES 负责普通敌人世界空间 billboard，ArkUI 负责顶部 Boss 安全区布局。普通敌人不再重复显示顶部 `TargetFrame`。

**Tech Stack:** C++17、OpenGL ES 3、ArkTS/ArkUI、N-API snapshot、宿主 `clang++` 测试、Node Bridge 契约、Hvigor/HarmonyOS API 23。

## Global Constraints

- 采用 A「均衡清晰」：深色半透明底、红橙实际血量、浅金延迟扣血、金色锁定细边。
- 普通敌人只显示锁定、最近受击和攻击范围内高威胁的少量目标；过远、镜头后方、无效或被遮挡目标淡出/不显示。
- 普通敌人只使用头顶血条，不再重复顶部 `TargetFrame`；Boss 继续使用顶部专属条。
- Boss 条包含名称、阶段、实际血量、延迟层和吟唱条，并避开系统安全区与任务/地图入口。
- 普通伤害暖白、暴击金色略大、元素伤害保留元素色；连续小额同类伤害合并或错峰，大额/暴击/反应独立。
- 血条和伤害数字都有对象/同屏上限，不能无界分配。
- 每次提交包含项目格式的 `Prompt:` 摘要。

---

## File Structure

- `native/engine/presentation/health_bar_presentation.h/.cpp`：延迟扣血状态、血条候选排序、头顶锚点与距离缩放纯函数。
- `native/engine/presentation/damage_numbers.h/.cpp`：带目标 ID 的合并窗口、优先级容量淘汰、pop/rise/fade。
- `native/engine/core/loop.h/.cpp`：发布筛选后血条与伤害数字。
- `native/engine/render/surface.h/.cpp`：普通敌人血条层次、锁定边框、投影过滤和屏幕钳制。
- `entry/src/main/ets/ui/BossHealthBar.ets`：独立 Boss 安全区组件。
- `entry/src/main/ets/ui/Hud.ets`、`TargetFrame.ets`、`GamePage.ets`：移除重复普通目标框并接入 Boss 组件。
- `tests/test_health_bar_presentation.cpp`、`tests/test_damage_numbers.cpp`、`tests/test_loop_integration.cpp`、`tests/test_bridge_contract.mjs`：逻辑与接线回归。

### Task 1: 延迟扣血层纯状态机

**Files:**
- Create: `native/engine/presentation/health_bar_presentation.h`
- Create: `native/engine/presentation/health_bar_presentation.cpp`
- Create: `tests/test_health_bar_presentation.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Produces: `struct HpTrailState { float displayed; Tick holdRemainingMs; }`。
- Produces: `HpTrailState UpdateHpTrail(HpTrailState state, float actualRatio, int64_t dtMs)`。
- Constants: hold `180ms`，chase `2.2 ratio/second`，输入/输出钳制 `[0,1]`。

- [ ] **Step 1: 写 RED 状态机测试**

断言从 `1.0→0.6` 时 actual 立即是 `0.6`，trail 在 180ms 内仍 `1.0`，之后单调下降且不低于 `0.6`；连续第二次受击重新开始 hold；回血时 trail 立即贴合新 actual；非法比例和负 dt 安全钳制。

- [ ] **Step 2: 编译确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-hp-trail-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_health_bar_presentation.cpp \
  native/engine/presentation/health_bar_presentation.cpp -o "$TEST_BIN_DIR/hp"
```

Expected: FAIL，模块不存在。

- [ ] **Step 3: 实现最小状态机**

检测 `actual < displayed` 时进入/续接 hold；hold 结束后按 `2.2 * dt` 收缩；回血或复活直接贴合。使用 `std::clamp`，dt 用安全转换，不在 Loop 重复定时逻辑。

- [ ] **Step 4: 运行 GREEN**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-hp-trail-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_health_bar_presentation.cpp \
  native/engine/presentation/health_bar_presentation.cpp -o "$TEST_BIN_DIR/hp"
"$TEST_BIN_DIR/hp"
```

Expected: 退出 `0`。

- [ ] **Step 5: 提交 Task 1**

```bash
git add native/engine/presentation/health_bar_presentation.* \
  tests/test_health_bar_presentation.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 提取血条延迟扣血状态" \
  -m "实际血量立即减少，浅金延迟层停留后平滑追赶并正确处理连续受击与回血。" \
  -m "Prompt: 美化敌人和Boss扣血动效"
```

### Task 2: 普通敌人血条筛选、锚点与缩放

**Files:**
- Modify: `native/engine/presentation/health_bar_presentation.h`
- Modify: `native/engine/presentation/health_bar_presentation.cpp`
- Modify: `tests/test_health_bar_presentation.cpp`

**Interfaces:**
- Produces: `struct EnemyHpBarCandidate { EntityId id; float distance; float threat; bool locked; bool recentlyHit; bool alive; bool inFront; bool occluded; int archetype; }`。
- Produces: `std::vector<EntityId> SelectEnemyHpBars(const std::vector<EnemyHpBarCandidate>& candidates, size_t capacity = 5)`。
- Produces: `float EnemyHpAnchorHeight(int archetype, float modelScale)`。
- Produces: `float EnemyHpDistanceScale(float distance)`，钳制 `0.72..1.0`。

- [ ] **Step 1: 添加筛选 RED 测试**

用 8 个字面量候选断言：locked 永远第一；recentlyHit 第二；其余按 threat 降序、distance 升序、ID；最多 5；dead/behind/occluded/tooFar 被过滤。locked 即使 threat 低也保留。锚点随 Bruiser/Elite/Boss-like scale 增大；距离缩放单调但不低于 `0.72`。

- [ ] **Step 2: 运行确认 RED**

运行 Task 1 测试命令，Expected: FAIL，新接口不存在。

- [ ] **Step 3: 实现排序与纯视觉曲线**

排序键：`!locked, !recentlyHit, -threat, distance, id`。最大显示距离使用现有软锁定维持距离，避免另造不一致世界尺度。`EnemyHpAnchorHeight` 复用 `EnemyArchetypeScale` 的体型分档但只返回高度，不引用 Surface。

- [ ] **Step 4: 运行 GREEN**

Expected: `test_health_bar_presentation` 退出 `0`。

- [ ] **Step 5: 提交 Task 2**

```bash
git add native/engine/presentation/health_bar_presentation.* \
  tests/test_health_bar_presentation.cpp
git commit -m "feat: 筛选清晰敌人血条" \
  -m "优先锁定和最近受击目标，限制同屏数量并按体型与距离调整头顶位置。" \
  -m "Prompt: 一屏内更清楚地查看敌人血条"
```

### Task 3: 伤害数字目标合并与优先级容量

**Files:**
- Modify: `native/engine/presentation/damage_numbers.h`
- Modify: `native/engine/presentation/damage_numbers.cpp`
- Modify: `tests/test_damage_numbers.cpp`

**Interfaces:**
- Produces in `DamageNumber`: `EntityId targetId`、`bool critical`、`bool reaction`。
- Produces: `spawn(EntityId targetId, Vec2 position, float value, DamageNumberKind kind, bool critical = false, bool reaction = false)`。
- Constants: small-hit merge window `120ms`、capacity `24`。

- [ ] **Step 1: 添加合并和淘汰 RED 测试**

同目标、同 kind、两个普通小额伤害在 120ms 内合并为一个总值；不同目标/元素不合并。Heavy、critical、reaction 永远独立。容量满时新 critical 应淘汰最旧普通小额而不是另一个 critical；全部高优先项时淘汰最旧。

- [ ] **Step 2: 运行确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-damage-number-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_damage_numbers.cpp \
  native/engine/presentation/damage_numbers.cpp -o "$TEST_BIN_DIR/damage"
"$TEST_BIN_DIR/damage"
```

Expected: FAIL，现有 spawn 无 targetId 且所有命中独立。

- [ ] **Step 3: 实现合并与优先级淘汰**

只合并 Normal/元素普通、非 critical/reaction、elapsed <=120ms 的同 target+kind 条目；更新 value、origin、elapsed=0 并保留 sequence。容量淘汰先找最旧低优先级，再退化最旧。保留现有确定性 drift、rise、alpha 和 pop 曲线。

- [ ] **Step 4: 运行 GREEN 与旧曲线回归**

运行测试，Expected: 合并/容量与现有 rise/alpha/pop 全部通过。

- [ ] **Step 5: 提交 Task 3**

```bash
git add native/engine/presentation/damage_numbers.* tests/test_damage_numbers.cpp
git commit -m "feat: 优化连续伤害飘字" \
  -m "合并同目标连续小额伤害，并优先保留暴击、大额和元素反应数字。" \
  -m "Prompt: 美化被攻击后的扣血数字动效"
```

### Task 4: Loop 发布有限血条与完整伤害语义

**Files:**
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/render/surface.h`
- Modify: `tests/test_loop_integration.cpp`
- Modify: `tests/test_combat_vfx.cpp`

**Interfaces:**
- Consumes: `UpdateHpTrail`、`SelectEnemyHpBars`、TargetLockResult from Plan 2。
- Produces in `EnemyHpBarRenderState`: `id`、`height`、`distanceScale`、`locked`、`recentlyHit`、`ratio`、`trailRatio`、`element`。
- Produces in `DamageNumberRenderState`: critical/reaction flags and target id。

- [ ] **Step 1: 添加发布侧 RED 测试**

Loop 构造 7 个敌人，锁定一个、命中一个、不同 threat；断言 surface 只发布 5 个且优先顺序符合纯函数。每个 bar 的 id、locked、height、scale 与实际实体对应。一次命中后 ratio 立即变，trail 在 hold 内不变。伤害事件包含 target ID，并把 heavy/reaction 正确传给系统。

- [ ] **Step 2: 运行 Loop RED**

Expected: FAIL，现有 Loop 发布所有存活敌人且 bar 没有 ID/优先字段。

- [ ] **Step 3: 替换 Loop 内嵌延迟条逻辑**

删除 `EnemyHpTrailState` 私有重复实现，改为 `unordered_map<EntityId, HpTrailState>`。先构造候选并筛选 ID，再只发布选中条目。目标死亡或区块卸载时清理状态；Boss 不进入普通敌人 bar 列表。

- [ ] **Step 4: 接入伤害 target 与高优先语义**

Combat Event 的 Damage/Resonance/PoiseBreak 分支调用新 spawn 签名。普通 combo 终结段或超过普通伤害阈值标 Heavy；暴击若现有战斗没有独立数值事件，则仅为接口预留 false，不伪造暴击判定；元素反应明确 reaction=true。

- [ ] **Step 5: 运行 GREEN**

运行 `test_health_bar_presentation`、`test_damage_numbers`、`test_loop_integration`、`test_combat_vfx`。Expected: 全部退出 `0`。

- [ ] **Step 6: 提交 Task 4**

```bash
git add native/engine/core/loop.* native/engine/render/surface.h \
  tests/test_loop_integration.cpp tests/test_combat_vfx.cpp
git commit -m "refactor: 发布有限战斗信息" \
  -m "Loop只发布优先血条并统一延迟扣血，伤害数字携带目标与反应语义。" \
  -m "Prompt: 控制同屏血条和扣血信息密度"
```

### Task 5: GLES 普通敌人血条 A 方案绘制

**Files:**
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/render/visual_tokens.h`
- Modify: `tests/test_visual_tokens.cpp`
- Modify: `tests/test_camera_render_transform.cpp`

**Interfaces:**
- Consumes: enriched `EnemyHpBarRenderState` from Task 4。
- Produces: `EnemyHpBarVisualStyle EnemyHpBarStyleFor(bool locked, int element)`。
- Produces: `bool ProjectCombatBillboard(..., glm::vec2& ndc)`，镜头后方 false，NDC 安全钳制。

- [ ] **Step 1: 写样式和投影 RED 测试**

断言深色背景、红橙 fill、浅金 trail；locked 有金色细边但 width scale 不变；元素只改变细框混色。投影测试断言镜头后方 false、边缘锚点钳制到 NDC `[-0.94,0.94]`、距离缩放不低于 0.72。

- [ ] **Step 2: 运行确认 RED**

编译 `test_visual_tokens` 和 `test_camera_render_transform`，Expected: 新接口缺失。

- [ ] **Step 3: 实现四层绘制顺序**

每条按：元素/金色细边（可选）→深色底→浅金 trail→红橙 actual fill。使用 bar.height 和 distanceScale 定位，不再固定 `ground+0.085`。启用 blend、depth test 但关闭 depth write；被场景遮挡的判定使用当前 depth 或发布侧 visibility 结果，不能让墙后血条透视。

锁定小标识复用 target ring 的小菱形 mesh 或 hp quad 组合，位于血条左侧；不要放大整条。

- [ ] **Step 4: 运行 GREEN 与渲染资源回归**

运行 `test_visual_tokens`、`test_camera_render_transform`、`test_shader_3d`、`test_mesh`。Expected: 全部退出 `0`。

- [ ] **Step 5: 提交 Task 5**

```bash
git add native/engine/render/surface.cpp native/engine/render/visual_tokens.h \
  tests/test_visual_tokens.cpp tests/test_camera_render_transform.cpp
git commit -m "feat: 美化普通敌人血条" \
  -m "绘制深色底、红橙血量、浅金延迟层和锁定细边，并按体型距离调整位置。" \
  -m "Prompt: 敌人血条更符合正常审美且位置距离合适"
```

### Task 6: 独立 BossHealthBar 安全区组件

**Files:**
- Create: `entry/src/main/ets/ui/BossHealthBar.ets`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces snapshot: `bossHpTrailRatio`。
- Produces component props: `visible`、`name`、`phase`、`hpRatio`、`trailRatio`、`castRatio`。

- [ ] **Step 1: 先写 Bridge/UI RED 契约**

Node 测试断言 `BossHealthBar.ets` 存在并由 `GamePage` 挂载；组件包含实际 Progress、浅金 trail 层、三阶段标识和可选吟唱条；Hud 不再内嵌旧 Boss Progress。断言 Snapshot 全链路包含 `bossHpTrailRatio`。

- [ ] **Step 2: 运行 Node RED**

```bash
node tests/test_bridge_contract.mjs
```

Expected: FAIL，组件和字段不存在。

- [ ] **Step 3: 发布 Boss 延迟层**

Loop 使用 Task 1 同一 `UpdateHpTrail` 维护 Boss trail，遭遇重置/回血时贴合。Snapshot 和 N-API 全链路发布 ratio，Boss defeated 后组件随 encounter 状态淡出。

- [ ] **Step 4: 实现 A 方案组件和安全区布局**

组件顶部宽度用相对百分比并设置合理 `maxWidth`；阶段名/点位、深色背景、阶段元素色 actual、浅金 trail、吟唱条按列排列。`GamePage` 顶部 Column 先放任务目标再放 Boss 条，使用 `.expandSafeArea` 已有策略并预留状态栏 top margin；地图入口在右侧时 Boss maxWidth 不覆盖其触控区。

- [ ] **Step 5: 运行 GREEN 与 HAP 构建**

```bash
node tests/test_bridge_contract.mjs
```

随后运行 HAP 增量构建。Expected: Node 退出 `0`；SDK 完整时 `BUILD SUCCESSFUL`。

- [ ] **Step 6: 提交 Task 6**

```bash
git add entry/src/main/ets/ui/BossHealthBar.ets entry/src/main/ets/ui/Hud.ets \
  entry/src/main/ets/pages/GamePage.ets native/engine/core/game_snapshot.h \
  native/engine/core/loop.cpp entry/src/main/cpp/native_bridge.cpp \
  entry/src/main/ets/napi/Bridge.ets entry/src/main/cpp/types/libnative_game/Index.d.ts \
  tests/test_bridge_contract.mjs
git commit -m "feat: 重做Boss顶部血条" \
  -m "增加阶段、实际血量、浅金延迟层和吟唱条，并按系统安全区限制布局。" \
  -m "Prompt: Boss血条美化并在一屏内更清楚"
```

### Task 7: 移除普通敌人重复 TargetFrame 与伤害数字视觉收口

**Files:**
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Delete: `entry/src/main/ets/ui/TargetFrame.ets`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/render/digit_atlas.h`
- Modify: `tests/test_digit_atlas.cpp`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: DamageNumberRenderState from Task 4。
- Produces: normal warm-white、heavy/critical gold、elemental existing colors；critical scale multiplier `1.15`。

- [ ] **Step 1: 写 RED 契约与颜色测试**

Node 断言 GamePage 不再 import/mount `TargetFrame`。C++ 断言 normal tint 为暖白（RGB 都高且 R>=G>=B）、heavy/critical 为金色、元素色保持 `AuraColorFor` 映射；critical 只放大 1.15，不改变 lifetime。

- [ ] **Step 2: 运行确认 RED**

运行 `node tests/test_bridge_contract.mjs` 和 `test_digit_atlas`，Expected: TargetFrame 仍存在或样式接口缺失。

- [ ] **Step 3: 删除重复普通顶部框**

移除 GamePage import 和组件调用，删除文件；快照中的 targetId/archetype/hpRatio 若仍被调试 HUD 或锁定控制器使用则保留，不因 UI 删除破坏数据链。

- [ ] **Step 4: 调整数字 billboard**

从实际受击点上方开始，使用 existing pop/rise/drift/alpha；normal 暖白，Heavy/critical 金色，元素色保持；reaction 可增加小型元素描边但不得双倍创建数字。投影镜头后方时跳过，屏幕边缘钳制到安全 NDC。

- [ ] **Step 5: 运行 GREEN**

运行 `test_damage_numbers`、`test_digit_atlas`、`test_camera_render_transform`、Node 契约。Expected: 全部退出 `0`。

- [ ] **Step 6: 提交 Task 7**

```bash
git add -A entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/ui/TargetFrame.ets \
  native/engine/render/surface.cpp native/engine/render/digit_atlas.h \
  tests/test_digit_atlas.cpp tests/test_bridge_contract.mjs
git commit -m "style: 收口伤害数字与目标信息" \
  -m "移除普通敌人重复顶部框，统一暖白、金色和元素伤害数字的弹出与边界表现。" \
  -m "Prompt: 扣血动效美观且同屏信息不拥挤"
```

### Task 8: 全面验证、三比例视觉清单与项目记忆

**Files:**
- Modify: `PROJECT_STATE.md`
- Modify: `DECISIONS.md`
- Modify: `TASKS.md`
- Modify: `docs/environment_vertical_slice_validation.md`

- [ ] **Step 1: 运行聚焦测试**

运行 `test_health_bar_presentation`、`test_damage_numbers`、`test_digit_atlas`、`test_visual_tokens`、`test_camera_render_transform`、`test_combat_vfx`、`test_loop_integration`、`node tests/test_bridge_contract.mjs` 和 `git diff --check`。

Expected: 全部退出 `0`，diff check 无输出。

- [ ] **Step 2: 运行完整宿主与 HAP 构建**

沿用 Plan 1 的完整宿主重建脚本，然后执行 HAP assemble 命令。Expected: 宿主零失败；SDK 完整时 `BUILD SUCCESSFUL`。

- [ ] **Step 3: 真机或模拟器视觉验收**

在可用设备按 16:9、20:9 和近 4:3 三种窗口/设备比例采集：单敌锁定、五敌群战、Boss 普通阶段、Boss 吟唱、连续小额伤害、暴击/大额、元素反应。逐项确认：

- 敌人血条不贴头、不飘过高，锁定细边清楚。
- 同屏不超过 5 条普通敌人血条，遮挡/镜头后方不透出。
- Boss 条不与任务、地图入口、刘海和系统栏重叠。
- 延迟层颜色和追赶速度能看清单次扣血。
- 连续数字不糊成一团，大额/反应仍突出。
- 平均 FPS 与 1% low 不低于项目门槛。

若无设备，只把这些项写入 `TASKS.md` 未完成列表，不声称视觉验收通过。

- [ ] **Step 4: 更新项目记忆**

`DECISIONS.md` 记录同屏 5 条、筛选顺序、180ms hold/2.2 ratio/s、普通 TargetFrame 移除和 Boss 独立组件。`PROJECT_STATE.md` 记录自动化实际覆盖；`TASKS.md` 记录尚未执行的屏幕比例与设备性能项。

- [ ] **Step 5: 提交 Task 8**

```bash
git add PROJECT_STATE.md DECISIONS.md TASKS.md docs/environment_vertical_slice_validation.md
git commit -m "docs: 记录战斗UI验证" \
  -m "记录血条筛选、延迟扣血、伤害合并与三种屏幕比例的真机验收边界。" \
  -m "Prompt: 敌人Boss血条和扣血动效美化验收"
```

## Plan 3 Completion Gate

- 血条延迟层、筛选、锚点、缩放、数字合并和容量均有纯逻辑测试。
- 普通敌人不再显示重复顶部 TargetFrame。
- BossHealthBar snapshot/ArkTS/HAP 全链路通过，或明确 SDK 阻塞。
- 聚焦测试、完整宿主测试和 Node 契约零失败。
- 设备视觉/性能证据存在，或 `TASKS.md` 明确标为未完成，绝不以自动化替代。
