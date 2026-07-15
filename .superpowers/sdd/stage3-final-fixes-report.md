# 阶段 3 最终整分支审查修复报告

## Status

四项 Important 与六项 Minor 已修复。用户本地 `build-profile.json5` 未修改、未暂存；
构建前后 SHA-256 均为 `b1dfc54516dff5408c530d16745175c956df9415d3bb1ce6bd39ee6b06788390`。

## RED / GREEN

先新增 `tests/test_stage3_final_fixes.cpp`，用与全量相同的 macOS SDK、libc++、C++17
命令编译。RED 退出码 1，关键错误为 `CombatSnapshot` 缺少 `playerPoise`、
`currentAction`、三技能冷却、附着与 `pulsePhase`，`GameplayEvent::sequence` 的
`uint64_t` 值截断为 `uint32_t`。这证明测试命中了缺失行为而非环境错误。

GREEN 聚焦命令编译并运行 `test_stage3_final_fixes`、`test_combat_controller`、
`test_loop_integration`，均退出码 0。中间回归发现两项真实问题并保留修复：Node 契约要求
N-API 字段显式且按序导出；大 dt 内先处理 tick 800 脉冲会丢失 tick 160 攻击命中。
修复后重新从空输出目录执行完整验证。

## Important 修复

1. 普通脉冲实际扣血时同时调用真实 `ActionStateMachine::resetCombo()`。控制器测试覆盖首击命中、
   480ms 内受脉冲、下一击 `ability=1` 且伤害 8。
2. 第十次脉冲把 HP 扣至零后，同 tick 重置玩家 HP/韧性、动作、体力、洞察、冷却、共鸣、
   脉冲和训练目标，发布 `EncounterReset`，并 `break` 停止该批后续脉冲；已生成但尚未结算的
   同帧攻击也不再污染复位状态。
3. 删除 `GameSnapshot::playerHp`，`hp/poise` 直接来自 `CombatSnapshot`。补齐当前动作、连击窗、
   三冷却、终结窗、破韧、三附着、当前反应、侵蚀与 pulse phase，并贯通 N-API、类型声明、
   Bridge、GamePage、Hud。C++ 行为测试覆盖实际状态变化，Node 覆盖字段顺序契约。
4. `HitEvent`、`GameplayEvent`、`PresentationEvent` 的 sequence 统一为 `uint64_t`；控制器每批和
   Loop 多 fixed-step 聚合后按 `(tick, source, target, sequence)` `stable_sort`。测试覆盖大 dt
   的 tick 160 攻击/tick 800 脉冲、反应事件稳定顺序及跨 `UINT32_MAX`。

## Minor 修复

- 体力恢复乘加使用 `__int128` 并在到达上限时饱和。
- 训练目标、反应和附着期限使用饱和 deadline。
- TrainingPulse 游标饱和且在无法前进时终止，避免 Tick 最大值附近重复/死循环。
- Loop 的 `combatTimeMs_` 改为 atomic，getter 消除 data race。
- Corruption 增加随附着存在的侵蚀状态、`AuraApplied` 事件和快照；消费、过期、reset 清除。
- README、路线图、Task 8 仅追加 31/31 自动化和新构建证据，九项真机出口仍未勾选。

## Fresh 验证

- C++：31/31 编译并运行通过。
- Node：1/1 通过。
- `git diff --check`：通过，无输出。
- Hvigor：`BUILD SUCCESSFUL in 12 s 518 ms`。
- signed HAP：4,081,727 bytes；SHA-256
  `55114bc7196b2a2efbac08092f3d7eb7122e74ab67cfba66819e68941541f60d`。

## 文件与自审

实现涉及 combat controller/state/resources/events/target/pulse/reaction、Loop/GameSnapshot、
N-API/ArkTS/HUD、聚焦测试与阶段 3 证据文档。自审确认事件排序键与设计一致，复位不会继续
结算同批脉冲或同帧预生成攻击，HP/韧性没有第二份 GameSnapshot 真相。

## Concerns

九项真机验收仍缺设备证据，保持未完成；自动化与 signed HAP 构建不替代真机操作验收。

## 最终复审追加修复

### RED

- `test_training_pulse.cpp` 首次编译退出码 1：`TrainingPulse` 没有 `resetAt`。
- 边界测试要求恢复延迟为 0 且 `now=Tick::max` 时不溢出；侵蚀测试要求 Corruption aura
  被反应消费后独立状态仍持续至 deadline。

### GREEN

- `TrainingPulse::resetAt(now)` 建立绝对 epoch，复位 tick 不回溯发布 Warning；快照 phase
  表达预警，首次 Hit 为 `now+800ms`，之后每 3000ms。27800ms 复位后 27801/28599ms
  HP 仍为 100，28600ms 首次降为 90，旧 EncounterReset 不重复。
- 体力 eligible/elapsed 差值使用 `__int128`。四项聚焦测试以 UBSan 和
  `-fno-sanitize-recover=all` 运行通过，无未定义行为。
- 侵蚀仅由 `corrosionUntil_` 驱动；反应消费 aura 后仍持续，deadline 到达及 reset 清除。

### Fresh 证据

- C++ 31/31、Node 1/1、`git diff --check` clean。
- Hvigor：`BUILD SUCCESSFUL in 8 s 803 ms`。
- signed HAP：4,081,855 bytes；SHA-256
  `e2358664e37799365188f042c714fcdde9939f0ac14c4fcec2bfa5a3ac7f7f07`。
- `build-profile.json5` 前后 SHA-256 仍为
  `b1dfc54516dff5408c530d16745175c956df9415d3bb1ce6bd39ee6b06788390`。
- 九项真机出口继续保持未勾选。

## Epoch 精准闪避最终修复

RED：`resetAt(27800)` 后 `classifyDodge(27800)` 被旧绝对时间轴误判为 Precise，测试以
断言退出码 134 失败。

GREEN：`classifyDodge` 先拒绝 `tick < epochTick_`，再用 `__int128` 计算距
`epoch+800+3000n` 最近命中的安全距离。边界结果为 27800/28499/28701 Normal，
28500/28700 Precise。控制器测试确认复位后 27800 Dodge 不授予洞察，28500 Dodge 授予洞察。
两项聚焦测试在 UBSan 与 `-fno-sanitize-recover=all` 下通过。

Fresh：C++ 31/31、Node 1/1、diff-check clean；Hvigor
`BUILD SUCCESSFUL in 7 s 475 ms`；signed HAP 4,081,455 bytes，SHA-256
`8f91fe68540c33a99483fd76d5b3fef1f23a78daf8724a7e4665377c26c6db7f`。
`build-profile.json5` 前后 SHA-256 保持
`b1dfc54516dff5408c530d16745175c956df9415d3bb1ce6bd39ee6b06788390`；真机九项未勾选。
