# 阶段 3 Task 5 实施报告

## Status

完成：实现三源技能、成功命中时的冷却/洞察消费、普通与反应共鸣积累、三源连携窗口和
终结技消费。未引入 Task 6 控制器，未修改 build-profile、CMake 或 UI。

## RED

先只新增 `tests/test_source_abilities.cpp` 与 `tests/test_resonance_window.cpp`，随后使用显式
macOS SDK、C++17、`-Wall -Wextra -Werror` 分别编译。两个命令均退出码 1：

```text
tests/test_source_abilities.cpp:27:19: error: no member named 'resonance' in
'ActionStateMachine'
tests/test_source_abilities.cpp:57:11: error: no member named 'grantInsight' in
'ActionStateMachine'
tests/test_source_abilities.cpp:77:20: error: no member named 'sourceAmount' in 'HitRequest'
source_red_exit=1

tests/test_resonance_window.cpp:9:13: error: no member named 'addResonance' in
'CombatResources'
tests/test_resonance_window.cpp:19:14: error: no member named 'recordDistinctSource' in
'CombatResources'
tests/test_resonance_window.cpp:23:21: error: no member named 'canUltimate' in
'CombatResources'
resonance_red_exit=1
```

失败原因与预期一致：冷却、共鸣、三源历史和附着倍率接口尚不存在，而非测试拼写或运行
环境错误。

## GREEN 与回归

测试统一使用：

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
COMMON=(-std=c++17 -Wall -Wextra -Werror -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
```

两个新增测试首次 GREEN：

```text
source_green_exit=0
resonance_green_exit=0
```

增加真实双源反应 `resonanceGain == 20` 断言后再次执行：

```text
reaction_green_exit=0
```

Tasks 1–5 全部新增战斗测试以相同严格编译参数执行：

```text
PASS test_combat_config
PASS test_event_order
PASS test_decision_log
PASS test_action_state_machine
PASS test_combat_resources
PASS test_training_pulse
PASS test_damage_resolver
PASS test_source_reaction_system
PASS test_source_aura
PASS test_resonance
PASS test_source_abilities
PASS test_resonance_window
PASS task1_to_task5=12/12
```

`git diff --check` 退出码 0。

## 行为覆盖

- 辉印、脉流、蚀质成功命中分别生成 `(20,6)`、`(16,8)`、`(12,18)`，并从命中 tick
  启动 `3000/4000/5000ms` 冷却。
- 洞察在请求拒绝、命中前和目标失效时均不消费；仅成功源命中消费一次，把生命伤害和
  `sourceAmount` 附着倍率乘以 `1.5`，不放大韧性伤害。
- 每个成功源命中增加 10 共鸣；双源反应结果携带 20 共鸣增量；资源增加接口饱和至 100。
- 三个不同源的成功命中 tick 最大值减最小值 `<=8000ms` 时直接充满共鸣，和已有普通积累
  不冲突；过期历史被清理后可用新源重新组成窗口。
- 三源连携开启 `[触发 tick, 触发 tick+5000)` 增益；到期 tick 清除增益但保留满共鸣，
  `canUltimate()` 仍只由满共鸣决定。
- 终结不足时原子拒绝；成功命中生成 `(60,40)` 并清空共鸣；目标在命中前失效则不消费。
- 大 dt 的技能命中、洞察判定、冷却和三源历史均使用计算出的成功命中 tick，而不是帧末 tick。

## 自审

- `HitRequest::sourceAmount` 追加在结构体末尾，保留 Tasks 1–4 的聚合初始化字段顺序。
- `SourceReactionSystem::apply` 原签名不变，只在 `ReactionOutcome` 增加资源增量，避免让反应
  系统依赖状态机或提前引入控制器。
- 共鸣加法使用差值饱和判断，避免大输入发生有符号溢出。
- 所有拒绝路径在改变冷却、洞察、共鸣前返回；命中失败只清当前动作。
- 用户已有 `build-profile.json5` 修改保持未暂存且未改动；未修改 CMake/UI。

## Concerns

- brief 未定义 5 秒“窗口增益”的具体数值效果，本任务只保存并暴露
  `ultimateWindowActive(now)`；Task 6 可编排表现或快照，但不得把它误作终结施放资格。
- 新 `.cpp` 未注册到生产 CMake，符合本 Task 的禁止范围；由后续集成任务统一处理。

## Important 审查修复：两阶段命中提交

### 根因

初版在 `ActionStateMachine::update()` 生成 `HitRequest` 时立即消费洞察、启动冷却、增加共鸣、
记录三源或清空终结能量；但 `DamageResolver::resolve()` 遇到已经死亡的目标会返回全零结果。
这使“命中请求已生成”被错误等同于“目标实际受到有效命中”。

### RED

先扩展两个 Task 5 测试，通过真实 `DamageResolver` 覆盖活目标、请求后死亡、错误 sequence、
重复确认和终结空结果，再以相同 C++17 严格参数编译：

```text
tests/test_source_abilities.cpp:37:18: error: no member named 'confirmHit' in
'ActionStateMachine'
tests/test_source_abilities.cpp:71:19: error: no member named 'confirmHit' in
'ActionStateMachine'
source_review_red=1

tests/test_resonance_window.cpp:65:20: error: no member named 'insightAvailableAt' in
'CombatResources'
tests/test_resonance_window.cpp:97:18: error: no member named 'confirmHit' in
'ActionStateMachine'
resonance_review_red=1
```

失败原因与审查项一致：状态机缺少命中确认边界，资源也不能只读判断指定命中 tick 的洞察。

### GREEN 与语义

`update()` 现在只生成预测后的 `HitRequest`，并保存一个最小 pending transaction：
`sequence/action/source/hitTick/insightApplied`。资源只在
`confirmHit(sequence, landed=true)` 时提交：

- 源技能提交洞察消费、冷却、`+10` 共鸣和三源历史。
- 终结技提交清空 100 共鸣。
- `landed=false` 清 pending 且不提交；重复确认不提交。
- sequence 不匹配返回 false 并保留正确 pending；pending 存在时新动作返回 `ActionLocked`，
  防止错误回执或新动作覆盖待确认事务。

两个定向测试首次 GREEN：

```text
source_review_green=0
resonance_review_green=0
```

### Tick 安全修复

- 洞察、冷却、体力恢复延迟和终结窗口 deadline 改用饱和加法。
- 三源窗口差值使用 `__int128`，避免 `Tick` 极值相减溢出。
- 三源历史显式忽略倒序 tick，并测试倒序后不能错误充满。
- 覆盖接近 `numeric_limits<Tick>::max()` 的冷却、洞察和终结窗口边界；饱和 deadline
  在最大 tick 到期，共鸣仍保留 100。

### 审查修复回归

Tasks 1–5 全部战斗测试与 damage resolver 相关测试执行结果：

```text
PASS test_combat_config
PASS test_event_order
PASS test_decision_log
PASS test_action_state_machine
PASS test_combat_resources
PASS test_training_pulse
PASS test_damage_resolver
PASS test_source_reaction_system
PASS test_source_aura
PASS test_resonance
PASS test_source_abilities
PASS test_resonance_window
PASS task1_to_task5_review=12/12
PASS damage_resolver_related=3/3
PASS git_diff_check
```

`build-profile.json5` 仍是用户原有未暂存修改；未修改 Task 6、CMake 或 UI。
