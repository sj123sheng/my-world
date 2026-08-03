---
kind: error_handling
name: 跨层错误处理：N-API 类型校验 + ArkUI try/catch 降级策略
category: error_handling
scope:
    - '**'
source_files:
    - entry/src/main/cpp/native_bridge.cpp
    - entry/src/main/ets/pages/GamePage.ets
    - native/engine/core/loop.h
    - native/engine/core/loop.cpp
    - entry/src/main/ets/EntryAbility.ets
---

该工程采用「C++ N-API 桥接层严格参数校验 + ArkUI 层 try/catch 资源加载降级」的分层错误处理模式，核心思路是：在 C++ 侧对 JS 调用进行强类型与范围校验并抛出明确类型错误，在 ArkUI 侧捕获异常后回退到程序默认状态，保证渲染管线不因单点失败而崩溃。

**1. 系统/框架**
- C++ 侧使用 HarmonyOS N-API（`napi_throw_type_error`、`napi_get_*` 返回值检查）进行参数合法性校验。
- ArkUI 侧使用 `try/catch` 包裹异步资源加载与定时器中的快照拉取，配合 `console.error` 输出日志。
- 原生层通过 `OH_LOG_Print(LOG_APP, LOG_ERROR, ...)` 统一记录错误日志。

**2. 关键文件与位置**
- `entry/src/main/cpp/native_bridge.cpp`：所有 N-API 导出函数均实现参数校验，失败时通过 `ThrowInputTypeError` 或 `napi_throw_type_error` 抛出类型错误，返回 `nullptr` 或布尔值表示失败。
- `entry/src/main/ets/pages/GamePage.ets`：资源加载与快照循环使用多层 try/catch，失败时打印错误并回退到程序化模型/环境。
- `native/engine/core/loop.h/.cpp`：引擎主循环通过 `LifecycleState` 同步包装操作，避免生命周期不一致导致的未定义行为；`publishRendererStopped()` 将渲染器不可用状态安全传播给上层。
- `entry/src/main/ets/EntryAbility.ets`：页面加载失败时通过 `console.error` 记录错误。

**3. 架构与约定**
- **N-API 参数校验模式**：每个入口函数先检查参数个数与类型，再逐项验证数值范围（如 `TryConvertInt32`、`TryMapPointerAction`），不合法则调用 `ThrowInputTypeError` 或 `napi_throw_type_error` 并返回空/假值。这种设计确保 JS 侧不会因非法输入导致 C++ 崩溃。
- **ArkUI 降级策略**：`loadModelAssets` 中分别对模型与环境资产加载使用独立 try/catch，任一失败仅记录错误并继续使用 fallback 网格/程序化环境，不影响其他资源加载。
- **渲染器停止传播**：当 EGL/GLES 初始化失败时，`OnSurfaceCreated/Changed` 调用 `InvalidateSurfaceSnapshot` → `surface_destroy` → `publishRendererStopped`，最终通过 `GameSnapshot.rendererReady = false` 通知 UI 显示错误提示。
- **暂停/恢复一致性**：`Loop::setPaused` 清空输入队列与摇杆状态，防止恢复后消费陈旧事件；`stop()` 中 join 线程、重置所有子系统状态，保证安全退出。

**4. 约定与约束**
- N-API 回调函数必须对 `argc`、参数类型、数值范围进行全面校验，失败时使用 `napi_throw_type_error` 而非直接崩溃。
- ArkUI 资源加载必须使用 try/catch 包裹，失败时记录 `console.error` 并回退到默认状态，不得中断页面生命周期。
- 引擎内部状态变更需通过 `withLifecycle` 同步包装，避免多线程竞态。
- 渲染器不可用时必须调用 `publishRendererStopped`，确保 UI 能感知并显示错误信息。
- 所有日志使用 `LOGI`/`LOGE` 宏（HarmonyOS 平台）或空宏（非平台），保持跨平台一致性。