---
kind: error_handling
name: ArkTS/原生桥接错误处理策略
category: error_handling
scope:
    - '**'
source_files:
    - entry/src/main/ets/pages/GamePage.ets
    - entry/src/main/cpp/native_bridge.cpp
    - entry/src/main/ets/EntryAbility.ets
    - native/engine/render/skinned_model.h
    - native/engine/render/static_model.h
---

本仓库的错误处理采用分层策略：ArkTS UI 层使用 try/catch + console.error 进行降级与日志记录，C++ NAPI 桥接层通过 napi_throw_type_error 抛出类型错误并返回布尔值表示成功/失败，核心引擎逻辑则依赖返回值和状态字段而非异常。具体模式如下：

**ArkTS 层（UI 与页面）**
- GamePage.ets 中所有异步资源加载（模型 GLB、环境资产）均包裹在 try/catch 块内，捕获后通过 console.error 输出带 '[Ethelan]' 前缀的日志，并回退到程序化网格或默认材质。
- EntryAbility.ets 的 onWindowStageCreate 回调中，页面加载失败时检查 err.code 并记录错误。
- 每帧 pullSnapshot 调用被 try/catch 包裹但 catch 体为空，属于静默吞掉异常的防御性写法。
- 未定义统一的错误类型或错误码枚举，错误信息以字符串形式直接输出。

**NAPI 桥接层（native_bridge.cpp）**
- 所有从 ArkTS 调用的 C++ 函数都进行严格的参数校验，使用 ThrowInputTypeError 辅助函数统一抛出 napi 类型错误，消息格式为 'functionName expects/requires ...'。
- 资源设置函数（NativeSetModelAssets、NativeSetEnvironmentAssets）不抛异常，而是返回布尔值表示是否成功提交，由上层根据返回值决定是否记录错误并降级。
- 输入事件推送函数（pushInput、pushAction、startEncounter 等）对参数类型、范围进行严格验证，不符合条件时立即返回 napi 类型错误。
- 使用 OH_LOG_Print 宏（LOGI/LOGE）记录关键生命周期和错误路径。

**原生引擎层（C++ 核心）**
- 引擎内部不使用 C++ 异常（未见 throw/try/catch），而是通过 bool 返回值、lastError() 字符串（如 skinned_model.h、static_model.h 中的 lastError_ 成员）以及状态字段传递错误信息。
- 渲染管线初始化失败时通过 InvalidateSurfaceSnapshot 重置表面状态并发布 rendererStopped 信号，由 UI 层显示不支持 GLES 的提示。
- 使用 #error 编译期断言（GLM platform.h）确保编译器兼容性。

**约定与约束**
- ArkTS 侧：所有跨进程/跨语言调用必须用 try/catch 包裹，错误通过 console.error 记录并以降级策略继续运行。
- NAPI 侧：参数校验失败一律抛 napi 类型错误；业务失败通过布尔返回值表达，禁止在桥接层抛出 C++ 异常。
- 引擎侧：避免使用 C++ 异常，通过返回值和状态字段传播错误；关键路径使用 LOGE 记录。
- 日志前缀统一使用 '[Ethelan]' 便于过滤。