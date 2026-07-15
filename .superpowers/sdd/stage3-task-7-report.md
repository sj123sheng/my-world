# 阶段 3 Task 7 实施报告：N-API、六按钮与诊断 HUD

## 结论

- 新增严格的 `pushAction(type)` N-API：仅接受一个有限整数 `0..5`，非法输入抛
  `TypeError`，六种动作统一通过 `Loop::enqueueInput` 获取 sequence。
- `Native XComponent` 仍是唯一指针触控生产者；`GamePage` 未新增 `.onTouch`，也不调用
  保留的 `pushInput`。
- 新增右下角六按钮和阶段 3 临时诊断 HUD。按钮区使用 `Alignment.BottomEnd`，与左下移动区
  分离，未做阶段 6 视觉精修。
- Native、`Index.d.ts`、`Bridge.ets`、`GamePage` 和 `Hud` 的新增字段名称与类型一致。
- Hvigor 完成 Native/ArkTS 编译、HAP 打包及签名，生成 signed HAP。

验证日期：2026-07-16。基线提交：`aacb06e`。

## TDD 记录

先只扩展 `tests/test_bridge_contract.mjs` 并运行：

```bash
node tests/test_bridge_contract.mjs
```

RED：退出码 `1`，首个预期失败为 `CombatControls missing 普攻`。

实现动作桥接、六按钮及快照透传后再次运行同一命令，GREEN：退出码 `0`。契约同时检查：

- 六个中文按钮及 `pushAction(0..5)`；
- 参数数量、number 类型、有限值、整数和 `0..5` 边界；
- 六个 `InputAction` 显式映射以及 `g_loop.enqueueInput(action, -1, 0, 0)`；
- GamePage 不含 `.onTouch` 与 `pushInput`；
- Native/声明/Bridge 快照字段顺序一致，GamePage 完整初始化并轮询赋值。

## 完整快照补齐

基线的 `GameSnapshot` 尚缺 `invulnerable`、`insightMs`、`pulseWarningMs` 和
`lastRejectReason`。为避免 N-API 伪造默认值，本任务最小扩展战斗状态链：

- `ActionStateMachine` 暴露剩余洞察时间；
- `TrainingPulse` 暴露下次警告剩余时间；
- `CombatController` 记录无敌、洞察、脉冲和最近动作拒绝原因；
- `Loop::ApplyCombatSnapshot` 统一写入 `GameSnapshot`。

定点数体力、共鸣、假人生命和韧性在 N-API 边界转换为 ArkTS `number`。

## C++ 回归

使用 macOS SDK clang、C++17、`-I. -Inative` 编译并执行：

```text
PASS test_combat_controller
PASS test_loop_integration
PASS test_training_pulse
```

三项编译与运行最终退出码均为 `0`。首次编译命令漏列 `resonance.cpp` 与
`source_aura.cpp`，链接失败；补齐既有依赖后从空输出目录重跑通过，该失败属于验证命令依赖
清单问题，不是实现编译错误。

## Signed HAP

命令：

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal --parallel --incremental --no-daemon
```

结果：Native CMake/Ninja、ArkTS、打包和 `SignHap` 均完成，`BUILD SUCCESSFUL`，退出码
`0`。签名产物：

```text
entry/build/default/outputs/default/entry-default-signed.hap
```

## 自审与剩余疑虑

- `build-profile.json5` 是用户已有修改，本任务未触碰、未暂存。
- 未进行真机点击、双指移动/相机并行操作或 HUD 可读性验证；右下布局与左下移动区的隔离已由
  代码结构保证，但仍需设备尺寸上的人工确认。
- 诊断 HUD 直接展示数值和拒绝原因枚举，符合阶段 3 临时诊断范围，未加入阶段 6 文案映射和
  视觉精修。

## 审查修复（2026-07-16）

审查发现并修复两处边界问题：

1. `CombatController` 原先对每次动作决策都写入 `lastRejectReason_`，导致同帧先拒绝、后成功
   时成功决策的 `None` 覆盖最近拒绝。现仅在 `!decision.accepted` 时更新，生命周期
   `reset()` 才清为 `None`。新增测试覆盖同帧非法动作后成功普攻仍保留
   `InvalidAction`，并验证 reset 清零。
2. `TrainingPulse::warningRemainingMs` 原先通过
   `(elapsed / period + 1) * period` 构造下一个绝对 Tick，接近 `Tick` 最大值时发生有符号
   溢出。现只用 `elapsed % period` 计算剩余量，结果保持在 `[1, period]`。新增
   `Tick::max` 与 `Tick::max - 1` 测试，并使用 signed-integer-overflow sanitizer 验证。

Node 契约同步加强：

- 每个 `Button(label)` 必须在下一个按钮出现前调用对应的 `pushAction(0..5)`，交换按钮编号
  会失败；
- Native `kActions` 整体顺序必须是 `Attack, Dodge, Radiance, Current, Corruption,
  Ultimate`，逐项存在但顺序错误也会失败。

RED 证据：

```text
Assertion failed: rejectThenAccept.snapshot().lastRejectReason ==
ActionRejectReason::InvalidAction
controller_exit=134

training_pulse.cpp:40:62: runtime error: signed integer overflow:
3074457345618259 * 3000 cannot be represented in type 'Tick'
sanitizer_exit=134
```

最终从空目录编译并运行：

```text
node tests/test_bridge_contract.mjs                         PASS
test_combat_controller                                     PASS
test_training_pulse + signed-integer-overflow sanitizer    PASS
test_loop_integration                                      PASS
git diff --check                                           PASS
```

Hvigor 使用前述相同 `DEVECO_SDK_HOME` 和 `assembleHap` 命令重新执行；Native Ninja、
ArkTS、PackageHap、SignHap 均完成：

```text
BUILD SUCCESSFUL in 8 s 804 ms
FINAL PASS: node=1 controller=1 pulse+ubsan=1 loop=1 signed_hap=1 diff_check=1
```

本次审查修复未修改 `build-profile.json5`，未执行 Task 8 真机验证。
