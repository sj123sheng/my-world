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
