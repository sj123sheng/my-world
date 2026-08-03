---
kind: logging_system
name: 日志系统 — 基于 HarmonyOS HILOG 的轻量结构化日志
category: logging_system
scope:
    - '**'
source_files:
    - entry/src/main/cpp/native_bridge.cpp
    - native/engine/core/loop.cpp
    - native/engine/render/shader_3d.cpp
    - native/engine/render/surface.cpp
---

本工程的日志系统采用 HarmonyOS 平台原生 HILOG（`hilog/log.h`）作为唯一输出通道，通过 C++ 宏封装 `OH_LOG_Print` 调用，按模块划分 Tag 与日志级别，未引入第三方日志框架或集中式 Logger 单例。

**系统与工具**
- 依赖头文件 `<hilog/log.h>`，使用 `OH_LOG_Print(LOG_APP, LOG_*, 0xFF00, "Tag", ...)` 直接输出。
- 所有日志均被 `#ifdef OHOS_PLATFORM` 保护：在非平台编译路径下，`LOGI`/`LOGE` 宏展开为空操作，保证跨平台可编译。

**关键文件与位置**
- `entry/src/main/cpp/native_bridge.cpp`：N-API 桥接层，定义全局 `LOGI`/`LOGE`（Tag="Ethelan"），记录 XComponent 生命周期、surface 初始化、触摸事件等。
- `native/engine/core/loop.cpp`：游戏主循环，定义 `LOGI`（Tag="Ethelan"），记录启动跳过、FPS 采样、每帧 tick 计数等。
- `native/engine/render/shader_3d.cpp`：3D 着色器模块，定义 `LOGI_3D`/`LOGE_3D`（Tag="Ethelan3D"），记录 shader 编译/链接失败及 uniform 位置缓存。
- `native/engine/render/surface.cpp`：渲染管线，定义 `LOGI`/`LOGE`（Tag="Ethelan"），记录 GL_VERSION、shader 编译回退、环境批次加载状态等。
- `native/gameplay/combat/decision_log.h`：纯数据结构的决策日志（非 HILOG），用于战斗回放比对，不属于运行时输出日志。

**架构与约定**
- 每个源文件独立 `#define LOGI(...)` / `LOGE(...)` 宏，参数固定为 `OH_LOG_Print(LOG_APP, LOG_INFO/LOG_ERROR, 0xFF00, "Tag", __VA_ARGS__)`。
- Tag 命名遵循“模块前缀”约定：通用模块用 `"Ethelan"`，3D 渲染子模块用 `"Ethelan3D"`，便于在设备日志中区分来源。
- 日志级别仅使用 `LOG_INFO` 与 `LOG_ERROR` 两级；调试信息以 INFO 形式输出，错误路径统一走 ERROR。
- 无集中式初始化：各模块按需 include `<hilog/log.h>` 并定义宏，无需全局 logger 启动。
- 结构化字段通过格式化字符串中的 `%{public}s/%{public}d/%{public}.1f` 等占位符传递，未使用键值对结构体。

**约束与规则**
- 所有日志调用必须包裹在 `#ifdef OHOS_PLATFORM` 内，确保非 HarmonyOS 构建不产生任何日志开销。
- 禁止直接使用 `printf`/`std::cout` 输出调试信息，统一通过 HILOG 宏输出以保证平台一致性。
- 日志 Tag 不得随意拼接长串，应保持在 1~2 个词以内，便于日志过滤与聚合。
- 性能敏感路径（如每帧 FPS 统计）仅在每秒阈值触发时输出，避免高频日志影响帧率。