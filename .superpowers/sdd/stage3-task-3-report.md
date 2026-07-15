# 阶段 3 Task 3 实施报告

## Status

完成：实现体力、闪避无敌窗口、一次性洞察和训练脉冲，并保持既有
`ActionStateMachine::request/update` 接口不变。

## RED

先仅创建 `tests/test_combat_resources.cpp`、`tests/test_training_pulse.cpp`，并扩展
`tests/test_action_state_machine.cpp`，随后执行：

```bash
c++ -std=c++17 tests/test_combat_resources.cpp \
  -o /tmp/my-world-task3-tests/test_combat_resources
c++ -std=c++17 tests/test_training_pulse.cpp \
  -o /tmp/my-world-task3-tests/test_training_pulse
```

关键输出（两个编译命令均退出码 1）：

```text
fatal error: '../native/gameplay/combat/combat_resources.h' file not found
fatal error: '../native/gameplay/combat/training_pulse.h' file not found
resource_compile_exit=1
pulse_compile_exit=1
```

失败原因与预期一致：需求类型尚不存在，而非测试拼写或运行环境错误。

## GREEN 与回归

本机 CommandLineTools 需显式指定 macOS SDK 和 libc++ 头目录。最终命令使用：

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
COMMON=(-std=c++17 -Wall -Wextra -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
```

本任务三个测试以额外 `-Werror` 编译、执行：

```text
PASS test_combat_resources
PASS test_training_pulse
PASS test_action_state_machine
```

指定回归：

```text
PASS test_fixed_step (existing unused-parameter warnings)
```

`test_fixed_step` 退出码 0；其两条 `unused parameter 'dt'` 警告来自既有测试，未修改该文件。
`git diff --check` 退出码 0。

## 实现文件

- `native/gameplay/combat/combat_resources.h`
- `native/gameplay/combat/combat_resources.cpp`
- `native/gameplay/combat/training_pulse.h`
- `native/gameplay/combat/training_pulse.cpp`
- `native/gameplay/combat/action_state_machine.h`
- `native/gameplay/combat/action_state_machine.cpp`
- `tests/test_combat_resources.cpp`
- `tests/test_training_pulse.cpp`
- `tests/test_action_state_machine.cpp`

## 行为覆盖

- 体力初值/上限 100，闪避原子扣除 30，余额不足不改变资源。
- 消耗后延迟 500ms，随后以整数毫秒累计 20/秒恢复，并保留定点余数。
- 闪避持续 300ms，区间 `[0, 200)` 无敌。
- 洞察持续 5000ms 且只能消费一次。
- tick 0 首次预警、tick 800 首次命中，此后每 3000ms 重复。
- 命中点前后闭区间 100ms 判为精准闪避。
- 大 dt 返回跨越区间内最新事件，保留绝对周期相位，同 tick 不重复事件。

## 自审

- 未改变 Task 2 已审查的 `request/update` 签名；仅增加只读的 `stamina()` 与
  `isInvulnerable()` 查询。
- 闪避扣费在改变动作状态之前完成，拒绝路径保持原子性。
- 攻击连段原有大 dt 余量逻辑保持不变。
- 新模块仅依赖纯 C++ 战斗配置，可由 macOS clang++ 独立测试。
- 未触碰用户已有的 `build-profile.json5` 修改，也未纳入暂存范围。

## Concerns

- `test_fixed_step` 在 `-Werror` 下会因既有未使用参数失败；正常警告级别回归通过。
- 初始实现的单值 `PulseEvent` 接口会遗漏跨越区间的中间事件；此项已在下方审查修复中解决。

## 审查阻塞项修复（追加提交）

### RED

先修改测试要求批量事件、资源 reset、闪避不被移动/受伤取消和状态机 reset 恢复体力，
随后编译新测试。

关键输出：

```text
tests/test_training_pulse.cpp:8:18: error: no member named 'size' in 'PulseEvent'
tests/test_training_pulse.cpp:10:29: error: no member named 'empty' in 'PulseEvent'
training_red_exit=1
tests/test_combat_resources.cpp:35:16: error: no member named 'reset' in 'CombatResources'
resources_red_exit=1
```

失败原因分别是旧接口仅返回单事件、资源类型缺少 reset，符合审查项预期。

### GREEN 与回归

使用显式 macOS SDK、libc++ 头目录、C++17、`-Wall -Wextra -Werror` 编译并执行：

```text
GREEN PASS: task3 3/3
```

覆盖：

- `test_training_pulse`
- `test_combat_resources`
- `test_action_state_machine`

回归以相同工具链执行；`test_fixed_step` 因既有未使用参数警告未启用 `-Werror`：

```text
REGRESSION PASS: fixed_step event_order decision_log combat_config; diff-check clean
```

### 修复内容

- `TrainingPulse::advance` 改为 `std::vector<PulseEvent>`，返回 `(lastAdvanceTick, now]`
  内全部事件；首次调用额外包含 tick 0 Warning，事件严格按 tick 排序且不重复。
- 大 dt 直接推进到 6800ms 返回 `0W, 800H, 3000W, 3800H, 6000W, 6800H`。
- 移动/受伤只重置普攻连段，不取消进行中的 Dodging。
- `CombatResources::reset()` 恢复满体力并清空恢复时间线、定点余数和洞察。
- `ActionStateMachine::reset()` 同步调用资源 reset。
- 洞察有效期边界和一次性消费使用独立实例验证。

### 自审与 Concerns 更新

- 批量接口消除了旧实现永久丢弃中间脉冲事件的问题；此前“只返回最新事件”的 concern 已解决。
- 所有新增状态只在 Task 3 文件内，未修改后续模块或既有 `request/update` 签名。
- `build-profile.json5` 仍为用户已有未暂存修改，本任务未触碰。
- 剩余非阻塞 concern：`test_fixed_step` 的既有未使用参数警告仍存在，执行结果为退出码 0。
