---
kind: logging_system
name: 基于 HarmonyOS HiLog 的原生日志系统
category: logging_system
scope:
    - '**'
source_files:
    - entry/src/main/cpp/native_bridge.cpp
    - native/engine/core/loop.cpp
    - native/engine/render/shader_3d.cpp
    - native/engine/render/surface.cpp
---

本项目在 C++ 游戏引擎与 ArkTS-NAPI 桥接层中，统一使用 HarmonyOS 平台的 HiLog 框架进行日志输出，未引入第三方日志库或自定义日志抽象层。

**使用的框架与头文件**
- 所有日志通过 `#include <hilog/log.h>` 引入
- 调用 `OH_LOG_Print(LOG_APP, level, tag, format, ...)` 直接输出
- 仅在 `#ifdef OHOS_PLATFORM` 条件下编译，非平台侧（如测试/跨平台编译）通过宏替换为空操作

**日志级别与标签约定**
- 主要使用两个级别：`LOG_INFO` 用于常规流程、性能统计和调试信息；`LOG_ERROR` 用于错误路径和初始化失败
- 每个模块定义独立的 LOGI/LOGE 宏，并指定不同的 TAG：
  - `native_bridge.cpp`：TAG = "Ethelan"，记录 XComponent 生命周期、输入事件、NAPI 导出注册等
  - `loop.cpp`：TAG = "Ethelan"，记录循环启动/停止、FPS 统计、tick 频率等
  - `shader_3d.cpp`：TAG = "Ethelan3D"，记录着色器编译/链接状态、GL 资源创建等
  - `surface.cpp`：TAG = "Ethelan"，记录渲染表面相关日志
- 未使用 DEBUG/WARN/FATAL 等其他 HiLog 级别，也未实现动态日志级别控制

**结构化字段与输出格式**
- 采用 printf 风格的格式化字符串，使用 `%{public}d`、`%{public}.1f`、`%{public}s` 等 HiLog 安全格式符
- 关键性能指标以结构化方式输出，例如 FPS 日志包含 fps、perf_level、environment_ready、draw_calls、triangles、texture_tier、encounter_mode 等字段
- 输入事件日志包含 type、id、x、y 等坐标信息
- 错误日志通常附带简短的错误原因描述

**条件编译与可移植性**
- 所有日志调用被包裹在 `#ifdef OHOS_PLATFORM` 中，确保在非 HarmonyOS 平台（如单元测试、CI 环境）下零开销
- 非平台侧的 LOGI 宏定义为 `((void)0)`，完全消除运行时开销
- 这种设计使得同一套代码可以在开发机、测试环境和目标设备上复用

**日志策略与使用模式**
- 关键路径日志：XComponent 生命周期回调（OnSurfaceCreated/Changed/Destroyed）、Loop 启动/停止、着色器编译失败
- 性能监控日志：每秒输出一次 FPS、性能等级、渲染统计（draw calls、triangles、texture tier）
- 调试辅助日志：每 60 帧输出一次 tickOnce 统计，便于观察运行状态
- 错误处理日志：surface_init 失败、OH_NativeXComponent_GetTouchEvent 失败、GL 资源创建失败等
- 未发现统一的日志收集、异步写入、文件落盘或远程上报机制，日志直接输出到系统 HiLog 缓冲区