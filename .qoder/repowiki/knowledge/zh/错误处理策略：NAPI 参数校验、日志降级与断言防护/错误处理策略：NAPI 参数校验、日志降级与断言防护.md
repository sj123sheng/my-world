---
kind: error_handling
name: 错误处理策略：NAPI 参数校验、日志降级与断言防护
category: error_handling
scope:
    - '**'
source_files:
    - entry/src/main/cpp/native_bridge.cpp
    - entry/src/main/ets/pages/GamePage.ets
    - native/engine/core/loop.cpp
    - native/engine/core/loop.h
    - native/engine/render/surface.cpp
    - native/engine/core/fixed_step.h
---

本仓库的错误处理采用分层策略，在 ArkTS UI 层、C++ NAPI 桥接层和原生渲染层分别采取不同的容错方式，整体以“快速失败 + 降级运行 + 日志记录”为主，未定义统一的错误类型或异常体系。

1. ArkTS 层（entry/src/main/ets/pages/GamePage.ets）
- 使用 try/catch 包裹资源加载与快照拉取，捕获异常后通过 console.error 输出结构化日志，并回退到程序默认状态（fallback meshes / procedural fallback），保证页面不崩溃。
- 对 nativeSetModelAssets / nativeSetEnvironmentAssets 的布尔返回值进行判断，若返回 false 则记录错误日志并继续使用内置几何体作为后备。
- 定时器轮询 pullSnapshot 时内嵌 try/catch 空块，吞掉快照读取异常，避免 UI 刷新中断。

2. C++ NAPI 桥接层（entry/src/main/cpp/native_bridge.cpp）
- 所有从 ArkTS 调用的 NAPI 入口均对参数数量、类型、取值范围进行严格校验，失败时通过 napi_throw_type_error 抛出 JavaScript 类型错误，并返回 nullptr 终止当前调用。
- 使用统一的 ThrowInputTypeError 辅助函数集中生成错误消息，保持错误信息一致性。
- 平台 API 调用失败（如 OH_NativeXComponent_GetTouchEvent、surface_init、surface_resize）通过 LOGE 记录错误并调用 InvalidateSurfaceSnapshot 安全停止渲染，不会崩溃。
- 资源提交接口（NativeSetModelAssets / NativeSetEnvironmentAssets）返回布尔值表示是否成功提交，由上层决定是否降级。

3. 原生引擎核心（native/engine/core/loop.cpp, loop.h）
- 使用 std::mutex 保护输入队列和战斗事件，确保多线程安全；生命周期操作通过 withLifecycle 包装，防止并发访问导致的状态不一致。
- FixedStep 构造函数中使用 assert 验证 stepMs > 0 和 maxSteps >= 0，开发阶段快速发现配置错误。
- 渲染不可用时通过 publishRendererStopped 发布特殊快照，通知 UI 层显示错误提示，而不是直接崩溃。

4. 渲染层（native/engine/render/surface.cpp）
- OpenGL ES 着色器编译失败时自动回退到 ES 2.0 版本，继续尝试构建渲染管线。
- 模型资源加载失败时记录 lastError 并通过 LOGE 输出，同时保留静态 Mesh 作为后备，确保渲染连续性。
- 软件光栅化路径（softwareDrawFrame）在硬件加速不可用时提供完整的 2D 渲染实现，包括网格、粒子、玩家、敌人等所有元素。
- 所有关键操作失败都通过 LOGE 记录详细错误信息，便于调试和问题定位。

5. 测试层（tests/ 目录）
- 单元测试广泛使用 assert 宏验证预期行为，确保核心逻辑的正确性。
- 契约测试（test_bridge_contract.mjs）验证 ArkTS 与 C++ 之间的数据交换格式。

整体特点：
- 无统一错误类型系统，各层独立处理错误
- 强调降级运行而非崩溃，保证用户体验
- 详细的日志记录便于问题诊断
- 开发期通过 assert 快速发现问题
- NAPI 层严格的参数校验防止非法输入