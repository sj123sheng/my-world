# 阶段 3 Task 4 实施报告

## Status

完成：实现训练假人、伤害结算与三源反应，保持既有 `HitRequest`、`CombatConfig` 和事件枚举，
未重构已审查的 Tasks 1–3。

## RED

先仅新增 `tests/test_damage_resolver.cpp`、`tests/test_source_reaction_system.cpp`，并扩展
`test_source_aura`、`test_resonance`，随后以 C++17、`-Wall -Wextra -Werror` 编译两个新测试。

关键输出：

```text
fatal error: '../native/gameplay/combat/damage_resolver.h' file not found
damage_red_exit=1
fatal error: '../native/gameplay/combat/source_reaction_system.h' file not found
reaction_red_exit=1
```

失败原因与预期一致：Task 4 模块尚不存在，不是测试语法或环境错误。

## GREEN 与回归

本机工具链使用 `xcrun --sdk macosx --show-sdk-path` 提供 SDK，并以 C++17、
`-Wall -Wextra -Werror` 编译执行。

Task 4 新测试及 brief 指定回归：

```text
PASS task4_and_required_regressions=5/5
```

覆盖 `test_damage_resolver`、`test_source_reaction_system`、`test_source_aura`、
`test_resonance`、`test_event_order`。

额外战斗相关回归：

```text
PASS related_combat_regressions=4/4
```

覆盖 `test_combat_config`、`test_combat_resources`、`test_training_pulse`、
`test_action_state_machine`。`git diff --check` 退出码 0。

## 实现文件

- `native/gameplay/combat/training_target.h/.cpp`
- `native/gameplay/combat/damage_resolver.h/.cpp`
- `native/gameplay/combat/source_reaction_system.h/.cpp`
- `native/gameplay/combat/source_aura.h/.cpp`
- `native/gameplay/combat/resonance.h/.cpp`
- `tests/test_damage_resolver.cpp`
- `tests/test_source_reaction_system.cpp`
- `tests/test_source_aura.cpp`
- `tests/test_resonance.cpp`

## 行为覆盖

- 基础生命/韧性伤害确定性结算。
- 折光固定造成 12 生命伤害，随后施加 3000ms 易伤。
- 凝滞持续 4000ms，韧性伤害乘以 1.5。
- 崩解造成 30 韧伤；该次伤害恰好破韧时固定追加 20 生命伤害。
- 三种两两组合顺序无关，同源只刷新；到期 tick 不再参与反应。
- 反应先消费旧附着，再重新应用当前命中源；一次 apply 最多产生一次反应。
- 破韧到期 tick 恢复 100 韧性，破韧期间生命承伤乘以 1.25。
- 死亡后 2000ms 的绝对 tick 完整复位；大 dt 可直接跨越到期边界。
- 折光易伤和破韧恢复使用独立绝对 tick，折光到期不会错误恢复韧性。

## 自审

- `resolveResonance` 返回 `std::optional<ResonanceType>`；同源明确返回空，不保留旧默认分支。
- 所有持续时间存储为绝对 tick，边界统一使用 `now >= deadline` 结束。
- 反应结算只遍历到第一个有效组合，并在写入当前源前清除旧组合，避免单次多反应。
- 崩解破韧追加伤害为固定 20，不会被同一次刚产生的 1.25 破韧易伤再次放大。
- 使用 `__int128` 作为定点乘法中间值，避免常规伤害乘法溢出。
- 未修改或暂存用户已有的 `build-profile.json5` 变更。

## Concerns

- Task 4 brief 未要求把新增 `.cpp` 注册到生产 CMake；本提交严格未扩大到 CMake，后续集成
  Task 应负责注册。
- 反应事件向量/表现事件未在 brief 的产出接口或断言中定义；当前 outcome 只提供结算结果，
  后续循环集成若需要事件队列，应在对应任务中补充契约。

## 审查阻塞项修复

### 根因与 RED

初版将 `SourceAuraContainer` 放在 `SourceReactionSystem`，系统没有 target key，导致同一个
系统实例对任意目标读写同一附着容器；目标死亡复位也无法访问并清理该状态。先新增双目标
隔离、死亡复活清理和真实双候选测试，再编译，退出码 `1`：

```text
error: no member named 'sourceAuras' in 'TrainingTarget'
review_red_exit=1
```

该失败证明旧目标类型不拥有附着，符合审查指出的根因。

### 修复内容

- `SourceAuraContainer` 改由每个 `TrainingTarget` 独立持有；反应系统不再保存全局附着。
- `TrainingTarget::advance(now)` 仅衰减自身附着，`reset()` 调用 `clear()`，死亡 2000ms
  复位后不会保留旧附着。
- `SourceReactionSystem::apply()` 只读取和修改传入目标的容器。
- 多候选固定按 `Radiance`、`Current`、`Corruption` 枚举顺序选择首个有效组合，不依赖
  容器插入/迭代顺序；选中后只结算一次，消费旧组合并重新施加当前源。
- 测试用逆序预置 `Corruption`、`Radiance`，再命中 `Current`，固定选择折光，只造成
  12 生命伤害、0 韧伤，最终仅保留 `Current`。

### GREEN 与回归

首次修复后定向执行：

```text
review_green_exit=0
```

提交前以 C++17、`-Wall -Wextra -Werror` 重新编译运行两个 Task 4 测试、
`test_source_aura`、`test_resonance`、`test_event_order` 及四个战斗相关回归，结果：

```text
REVIEW FINAL VERIFY PASS: 9/9 tests; diff checks clean
```

`build-profile.json5` 仍为用户已有未暂存修改；本次未触碰 CMake、AI 或 HUD。
