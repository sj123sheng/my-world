# M3-2 Task 4 报告：Surface 生命周期、动画选择和 Mesh 回退

## 结果

- 新增纯数据 `RenderAnimation`、`ActorRenderState`、`ChooseAnimation`、
  `ResolveClip` 与 `ModelKind`，优先级为死亡 > 攻击 > 受击 > 移动 > 待机。
- `Surface::setModelAsset` 线程安全地保存玩家、敌人与 Boss 三份字节并标脏；资源只在
  EGL context current 的 `init3DResources` 或下一次 `draw3DPhase` 中消费一次。
- 资源替换先销毁旧模型；Surface 销毁时先销毁三份 `SkinnedModel`，再销毁静态 Mesh 和
  shader；context 重建时已有字节重新标脏。
- 3D 角色统一经过 `drawActor`：仅当模型 ready 且调色板有效时启用蒙皮，否则继续绘制
  M3-1 静态 Mesh；地面、2D 阶段、Camera3D 与 HUD 契约未改。
- 角色阶段启用深度测试与背面剔除，结束后恢复关闭状态；M3-1 双面地面先于剔除阶段绘制。
- `Loop::updateFixed` 只读投影玩家 HP、`currentAction`、受击事件的 200ms 计时、移动状态，
  以及敌人/Boss 存活状态，不向 gameplay、Camera3D 或 HUD 写回。
- CMake 已接入 `skinned_model.cpp`。

## TDD 记录

按三个独立接口增量执行 RED → GREEN：

1. 先新增动画优先级与 clip 回退测试，首次编译因缺少
   `native/engine/render/render_animation.h` 失败。
2. 再新增运行时模型降级契约测试，编译因缺少 `SkinnedModel` 类型失败；最小实现后验证
   非真实加载路径明确返回 false、`ready()` 保持 false 且给出诊断。
3. 最后新增 late asset 存储测试，编译因 `Surface` 缺少 `setModelAsset`、三份字节和脏标记
   失败；最小实现后通过。

上述 RED 均由缺失的目标接口触发，不是测试拼写或环境错误。macOS 默认 `c++` 无法定位
标准库头，因此 GREEN 与回归使用与既有 Task1/3 一致的 `xcrun clang++` + SDK sysroot。

## 验证

以下命令均在 `/Users/xiling/Documents/project/game/my-world/.worktrees/codex-m3-2-gltf`
执行并退出 0：

- `test_render_animation`
- `test_skinned_model`
- `test_mesh`
- `test_shader_3d`
- `test_camera`
- `test_camera3d`
- `node tests/test_bridge_contract.mjs`
- `loop.cpp` 的 macOS SDK / C++17 宿主对象编译
- HarmonyOS Hvigor `assembleHap`，输出 `BUILD SUCCESSFUL in 14 s 103 ms`
- `git diff --check`

宿主 `loop.cpp` 编译保留两个非阻塞告警：`tick` 参数未使用、非 OHOS 分支下
`update3DCamera` 未使用；没有编译错误。Hvigor 保留既有 N-API 声明验证和未配置签名告警，
Native CMake/Ninja、ArkTS 编译与 HAP 打包均成功。

## 明确接口缺口与降级边界

Task1 当前只有 glTF 输入校验、关键帧采样和调色板计算的纯数据函数；Task3 只有骨骼顶点
布局与 shader uniform。两者尚未提供以下真实运行时能力：

- cgltf 数据到顶点、索引、节点、skin、clip 和纹理的解析/所有权；
- `SkinnedVertex` VBO/IBO/纹理上传与属性 3/4 绑定；
- 按 clip 采样节点 TRS、形成调色板并绘制的 GPU `SkinnedModel`。

因此本 Task 只补了最小、诚实的 `SkinnedModel` 生命周期接口：
`tryInitialize` 明确返回 false 并报告 `runtime loader is unavailable`，不会把 GLB 误报为已解析；
Surface 会稳定使用静态 Mesh 回退。真实 GLB 解析和骨骼播放仍被上述接口缺口阻塞，不能作为
本提交的已完成功能宣称。

## 审查修复（surface_destroy current-context 门控）

### TDD 记录

先在 `tests/test_render_animation.cpp` 写入三项宿主可执行状态测试：

- late-dirty 资产只消费一次；
- 资产替换和清空都产生一次可消费的 pending 状态；
- 销毁计划只有在 `eglMakeCurrent` 成功时才包含
  `SkinnedModels -> StaticMeshes -> Shader3D -> Program2D` 的 GL 删除顺序；失败时
  GL 删除序列为空，但仍要求清除 CPU GL 句柄跟踪并销毁 EGL 资源。

首次按 macOS SDK + libc++ 命令编译，因缺少
`native/engine/render/render_lifecycle.h` 失败，确认 RED。随后最小新增
`PendingModelAsset` 与 `PlanSurfaceDestroy`，并让 `Surface` 的 pending 字节路径使用它；
GREEN 后全部断言通过。

### 生命周期策略

`surface_destroy` 现在保存 `eglMakeCurrent` 的返回值并通过 `PlanSurfaceDestroy` 门控：

- 成功：在 current context 中按 `SkinnedModel -> Mesh -> Shader3D -> 2D Program`
  释放 GL 对象，随后解绑并销毁 surface/context。
- 失败：绝不调用 `destroy3DResources`、`glDeleteProgram` 或任何其他 GL 删除；
  `abandonGpuResources` 只将 CPU 侧的对象句柄/状态置为无效。随后仍调用
  `eglDestroyContext`，由 EGL 释放该 context 拥有的实际驱动对象。这不是把泄漏标为成功：
  日志明确记录“skipping GL deletes and relying on eglDestroyContext”，且 CPU 资产字节
  会在完整 Surface 销毁时清空。

真实 runtime loader 尚未存在；其未来资源句柄必须遵守
`SkinnedModel::abandonGpuResources` 的无 GL 调用契约，才能安全走失败路径。

### 审查修复验证

以下命令在本 worktree 执行并退出 0：

- `test_render_animation`（含新增生命周期状态测试）
- `test_skinned_model`、`test_mesh`、`test_shader_3d`、`test_camera`、`test_camera3d`
  与 `node tests/test_bridge_contract.mjs`
- macOS C++17 `loop.cpp` 对象编译（仅保留既有 `tick`、`update3DCamera` 未使用告警）
- HarmonyOS Hvigor `assembleHap --analyze=normal --parallel --incremental --daemon`，
  输出 `BUILD SUCCESSFUL in 6 s 690 ms`；仅有既有未配置签名告警
- `git diff --check`

额外的独立 OHOS `surface.cpp` 语法命令未包含工程 CMake 提供的 GLM include，因 SDK
sysroot 找不到 `glm/vec3.hpp` 失败；真实 HAP Native CMake/Ninja 编译已成功，故该命令不
作为源码失败结论。
