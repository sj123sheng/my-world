# 玩家移动与相机最终修复报告

日期：2026-08-13

## Finding 修复映射

1. `native/engine/render/surface.cpp:896` 不再以无实例 gait 的 `ResolveClip`
   重算日志 clip。`SkinnedModel::resolvedClipName(const SkinnedAnimationState&)`
   只读取该模型、该资源版本、该实例当前实际播放的 clip；`drawActor` 在既有
   `update` 后消费它。渲染、播放和 gait 决策均未改变。
2. `tests/test_wild_spawn_system.cpp` 的布局注释由“8 区”校正为当前“7 区”。
3. `tests/test_loop_integration.cpp` 的隔离注释改为通用表述：排除野外刷怪对
   时序场景的干扰，不再提已删除的出生侦察敌。
4. `tests/test_render_animation.cpp` 增加 `walk` 与 `Walking_B` 同时存在时
   `walk` 优先的显式断言。
5. `tests/test_skinned_model.cpp` 增加 run→walk 实际 clip 混合测试：首帧保持
   既有 run 姿态，中间帧介于既有 run 姿态与目标 walk 姿态之间，可杀死硬切
   mutation。

## TDD 证据

- 先在 `testResolvedClipNameFollowsInstanceGaitHysteresis` 写入对
  `resolvedClipName` 的实际 clip 断言。实现前运行：
  `clang++ ... tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp`
  以退出码 1 失败，错误为 `no member named 'resolvedClipName' in 'SkinnedModel'`。
- 随后实现最小只读接口与日志消费点；完整 host 库链接的
  `test_skinned_model` 退出码 0。测试在 Walk 迟滞区的 0.35 输入断言仍为
  `Walking_B`，证明接口观察的是实例已解析结果而非无状态重算。

## 验证命令与真实结果

- 用宿主静态库（排除 `render/surface.cpp`、`core/loop.cpp` 及平台文件）重建后：
  `test_render_animation`：exit 0。
- 同一静态库：`test_skinned_model`：exit 0。
- 同一静态库：`test_wild_spawn_system`：exit 0，输出 `all passed`。
- 同一静态库加 `native/engine/core/loop.cpp`：`test_loop_integration`：exit 0。
- `git diff --check`：exit 0、无输出。

首次直接编译 `test_skinned_model` 漏链 `texture.cpp`，报 `stbi_*` 未定义；
首次按旧聚焦命令链接 `test_render_animation` 漏链 world/collision 实现。两者均为
测试命令依赖不完整，不是本修复代码错误；最终统一改用重建的完整 host 静态库验证。

## 文件与自审

- 生产：`native/engine/render/skinned_model.h`、
  `native/engine/render/skinned_model.cpp`、`native/engine/render/surface.cpp`。
- 测试与注释：四个指定测试文件；未改动 gameplay 或播放逻辑。
- 自审确认：新接口验证 owner、资源 revision、clip 下标与 ready 状态，失配返回空串；
  日志去重仍由原 `AnimationLogState` 负责。

## 顾虑

未执行 HarmonyOS HAP/真机图形验收；本次接口是仅在既有 `update` 后读取状态，
并已由 host-safe 测试覆盖其实际 clip 语义。设备日志输出仍应在下一次真机验收时抽查。
