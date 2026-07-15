# 阶段 3 Task 6 实施报告

## 结果

已实现 `CombatController`，并接入固定步循环、稳定训练假人、战斗快照与
Surface 只读渲染状态。战斗帧严格按资源推进、动作、训练脉冲、直接伤害、
派生反应、共鸣提交、快照发布的顺序处理。源技能和终结技在
`DamageResolver` 返回后，使用对应 `transactionId` 调用 `confirmHit`。

派生反应只追加到当前 `CombatEventBatch`，不会重新进入控制器事件处理。

## RED

1. 新增 `tests/test_combat_controller.cpp` 后首次编译失败：
   `native/gameplay/combat/combat_controller.h file not found`。
2. 新增停止清理断言后，`test_loop_integration` 失败于
   `combatLoop.snapshot().comboSegment == 0`，证明停止路径尚未发布重置后的
   战斗快照。

## GREEN

- 第一个 16 ms 固定步只进入普攻第一段，训练假人 HP 保持 300。
- 累计推进到 160 ms 时，通过 `DamageResolver` 将训练假人 HP 从 300 降到
  292。
- 输入按 `sequence` 稳定排序；战斗输入不进入 `TouchRole`。
- 移动会重置连段；无效目标不会接受攻击。
- 训练脉冲命中、闪避无敌和精准闪避洞察由控制器统一编排。
- 两种源技能验证直接伤害、一次派生反应和最终共鸣值，派生事件不递归。
- `stop()`、渲染失效及重新启动前都会清空战斗状态并恢复训练假人。
- 训练假人 ID 固定为 1001，位置固定为 `(0.5, 0.8)`；场景 props 仅渲染，
  不参与战斗软锁定。集成测试覆盖 prop 与训练假人重合时仍选中稳定 ID。

## 测试

使用 macOS SDK、`clang++`、C++17 独立编译执行：

- `test_combat_controller`：PASS
- `test_loop_integration`：PASS
- `test_loop_lifecycle`：PASS
- `test_touch_controls`：PASS
- `test_player_controller`：PASS
- `test_camera`：PASS
- `test_soft_targeting`：PASS
- `test_combat_config`：PASS
- `test_action_state_machine`：PASS
- `test_combat_resources`：PASS
- `test_training_pulse`：PASS
- `test_damage_resolver`：PASS
- `test_source_reaction_system`：PASS
- `test_source_abilities`：PASS
- `test_resonance_window`：PASS

生产构建命令：

```sh
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw \
  assembleHap --mode module -p product=default -p buildMode=debug --no-daemon
```

结果：`BuildNativeWithCmake`、`BuildNativeWithNinja`、`PackageHap` 全部完成，
进程退出码为 0。

`git diff --check`：PASS。

## 文件

- 新增 `native/gameplay/combat/combat_controller.h`
- 新增 `native/gameplay/combat/combat_controller.cpp`
- 新增 `tests/test_combat_controller.cpp`
- 修改 `native/engine/core/loop.h`
- 修改 `native/engine/core/loop.cpp`
- 修改 `native/engine/core/game_snapshot.h`
- 修改 `native/engine/render/surface.h`
- 修改 `native/engine/render/surface.cpp`
- 修改 `tests/test_loop_integration.cpp`
- 修改 `entry/src/main/cpp/CMakeLists.txt`

用户原有 `build-profile.json5` 修改未编辑、未暂存。

## Concerns

- `CombatEventBatch` 是逐固定帧瞬时批次；当前渲染只读取快照，不消费事件。
  后续表现层接入时应在同一帧复制批次，不能持有控制器内部引用。
- 场景 props 不再作为软锁定候选，这是为保证训练假人 ID/位置稳定而做的
  明确战斗域约束；已有回归测试固定该行为。
