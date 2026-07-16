# 精准闪避窗口调整 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将训练脉冲精准闪避改为命中前 `100–500ms` 单向窗口，并把洞察持续时间延长至 `15000ms`。

**Architecture:** 保留 `TrainingPulse` 的 epoch 时间轴，由它判定当前闪避对应的下一次命中及剩余时间；`CombatController` 记录精准闪避规避的命中 tick，处理脉冲事件时跳过且只跳过该次伤害。资源层继续统一管理洞察到期时间。

**Tech Stack:** C++17、assert 独立测试、HarmonyOS Hvigor/HAP、HDC 真机验证。

## Global Constraints

- 精准窗口为命中前闭区间 `[100ms, 500ms]`，命中后不得判定精准。
- 精准闪避直接规避对应的单次训练脉冲伤害。
- 洞察默认持续 `15000ms`；普通闪避的 `300ms` 时长、`200ms` 无敌和 `30` 体力消耗不变。
- 不修改、暂存或提交用户现有的 `build-profile.json5` 修改。

---

### Task 1: 精准闪避时间语义

**Files:**
- Modify: `native/gameplay/combat/combat_config.h`
- Modify: `native/gameplay/combat/training_pulse.h`
- Modify: `native/gameplay/combat/training_pulse.cpp`
- Test: `tests/test_training_pulse.cpp`
- Test: `tests/test_combat_config.cpp`

**Interfaces:**
- Consumes: `CombatConfig::trainingPulsePeriodMs`、`trainingPulseWarningMs` 和 pulse epoch。
- Produces: `TrainingPulse::preciseDodgeHitTick(Tick) -> std::optional<Tick>`；仅在下一命中剩余 `100–500ms` 时返回该命中 tick。

- [ ] **Step 1: 写入失败边界测试**

断言首轮及 reset epoch 下 `501ms` 为普通、`500ms` 和 `100ms` 为精准、`99ms` 与命中后为普通，并验证返回的命中 tick。

- [ ] **Step 2: 运行测试确认失败**

Run: 使用仓库现有 C++17 测试编译方式编译并执行 `tests/test_training_pulse.cpp` 和 `tests/test_combat_config.cpp`。  
Expected: 旧的对称 `±100ms` 逻辑或缺失接口导致 FAIL。

- [ ] **Step 3: 实现最小单向窗口**

将配置改为 `preciseDodgeWindowMinMs = 100`、`preciseDodgeWindowMaxMs = 500`，校验 `0 <= min <= max`；根据 epoch、周期和严格的下一命中计算剩余时间，命中后只允许匹配下一周期。

- [ ] **Step 4: 运行聚焦测试确认通过**

Run: 重编译并执行上述两个测试。  
Expected: 两个测试均退出码 `0`。

### Task 2: 单次脉冲规避与 15 秒洞察

**Files:**
- Modify: `native/gameplay/combat/combat_controller.h`
- Modify: `native/gameplay/combat/combat_controller.cpp`
- Modify: `native/gameplay/combat/combat_resources.cpp`
- Test: `tests/test_combat_controller.cpp`
- Test: `tests/test_combat_resources.cpp`

**Interfaces:**
- Consumes: `TrainingPulse::preciseDodgeHitTick` 返回的命中 tick。
- Produces: `CombatController::preciseDodgedPulseTick_`，只在完全匹配的脉冲命中事件上消费；洞察在授予 tick 后 `14999ms` 可用，`15000ms` 到期。

- [ ] **Step 1: 写入失败集成测试**

覆盖剩余 `500ms` 点击后在命中时玩家不掉血并获得接近 `15000ms` 洞察、普通无敌已经结束，以及洞察 `14999/15000ms` 到期边界。

- [ ] **Step 2: 运行测试确认失败**

Run: 编译并执行 `tests/test_combat_controller.cpp` 与 `tests/test_combat_resources.cpp`。  
Expected: 旧实现会在 `500ms` 提前闪避时掉血，或洞察在 `5000ms` 到期。

- [ ] **Step 3: 实现单次规避状态**

精准闪避被接受时保存对应命中 tick 并授予洞察；处理同 tick 的 Hit 时先消费该标记并发出 Dodge 表现，其他命中仍按普通无敌或伤害处理；reset 清空标记。

- [ ] **Step 4: 运行聚焦和完整测试**

Run: 执行聚焦测试，再编译运行全部 `tests/test_*.cpp` 与 Node 桥接契约。  
Expected: 聚焦测试通过，完整测试保持 `31/31` C++ 和 `1/1` Node 通过。

### Task 3: 文档、生产构建与真机验收

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-15-character-combat-three-sources.md`

**Interfaces:**
- Consumes: 通过自动化测试的精准闪避实现和已配置签名。
- Produces: 新 signed HAP、真机 HUD 中接近 `洞察 15000ms` 的验收证据。

- [ ] **Step 1: 同步操作与验收说明**

明确观察脉冲倒计时，在剩余 `100–500ms` 内只点击一次“闪避”；成功时 HUD 立即显示接近 `洞察 15000ms`。

- [ ] **Step 2: 构建 signed HAP**

Run: `DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk node /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js --mode module -p module=entry@default -p product=default -p requiredDeviceType=phone assembleHap --analyze=normal --parallel --incremental --daemon`  
Expected: `BUILD SUCCESSFUL` 且生成 `entry-default-signed.hap`。

- [ ] **Step 3: 安装并启动真机包**

Run: 使用 `/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc` 向设备 `2MN0224C12000754` 安装 HAP，并启动 `com.ethelandev.myworld/EntryAbility`。  
Expected: 安装、启动成功，无崩溃。

- [ ] **Step 4: 执行精准闪避验收**

观察 HUD 脉冲剩余时间，在 `100–500ms` 内只点一次闪避；采集 HUD 截图或 dump。  
Expected: 玩家不受该次脉冲伤害，HUD 立即显示接近 `洞察 15000ms`。

- [ ] **Step 5: 最终一致性检查**

Run: `git diff --check`、完整自动化测试，并检查 `git status --short`。  
Expected: 无空白错误、测试全通过，`build-profile.json5` 仍是唯一不属于本任务的用户修改。
