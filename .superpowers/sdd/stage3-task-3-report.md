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
- 单值 `PulseEvent` 接口无法一次返回跨越区间的全部事件；当前确定性规则是返回最新边界事件，
  绝对周期相位不会因大 dt 漂移。
