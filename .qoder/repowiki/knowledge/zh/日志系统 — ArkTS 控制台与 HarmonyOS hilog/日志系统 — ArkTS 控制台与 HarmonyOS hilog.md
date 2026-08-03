---
kind: logging_system
name: 日志系统 — ArkTS 控制台与 HarmonyOS hilog
category: logging_system
scope:
    - '**'
source_files:
    - entry/src/main/cpp/native_bridge.cpp
    - entry/src/main/ets/pages/GamePage.ets
    - native/engine/render/shader_3d.cpp
    - native/engine/render/surface.cpp
    - tests/test_skinned_model.cpp
---

本仓库的日志输出采用两层结构：ArkTS 层使用框架内置的 `console` API，Native C++ 层通过 HarmonyOS 的 `hilog` 子系统输出。未发现统一的日志框架、结构化日志格式或集中式日志路由配置。

### 1. 使用的系统与工具
- **ArkTS 层**：仅使用 `console.error(...)` 输出错误信息，未引入第三方日志库。
- **Native 层**：通过 `#include <hilog/log.h>` 并使用宏 `LOGI` / `LOGE`（封装 `OH_LOG_Print`）向系统 hilog 输出，标签统一为 `"Ethelan"`。

### 2. 关键文件与位置
- `entry/src/main/cpp/native_bridge.cpp`：定义 `LOGI`/`LOGE` 宏并在 XComponent 生命周期回调、输入事件处理、资源加载等路径中调用。
- `entry/src/main/ets/pages/GamePage.ets`：在模型与环境资源加载失败时通过 `console.error('[Ethelan] ...')` 输出错误。
- `native/engine/render/shader_3d.cpp`、`native/engine/render/surface.cpp`：使用 OpenGL 的 `glGetShaderInfoLog` 获取着色器编译/链接日志（非应用级日志）。
- `tests/test_skinned_model.cpp`：测试代码通过 `std::fprintf(stderr, ...)` 输出诊断信息。

### 3. 架构与约定
- **分层隔离**：ArkTS 侧仅负责 UI 层错误提示；Native 侧负责引擎核心、渲染、平台桥接的日志。
- **标签约定**：所有 hilog 输出均使用固定标签 `"Ethelan"`，便于在设备日志中过滤。
- **级别划分**：仅区分 INFO 与 ERROR 两级；无 DEBUG/WARN/FATAL 等更细粒度级别。
- **结构化字段**：未实现 JSON 或键值对形式的结构化日志；消息为拼接字符串。
- **Sink 路由**：依赖 HarmonyOS hilog 默认 sink（设备日志），未看到自定义 sink、文件落盘或远程上报逻辑。

### 4. 约定与约束
- ArkTS 层错误输出统一以 `[Ethelan]` 前缀开头，便于从大量 console 输出中快速定位。
- Native 层日志通过 `LOGI`/`LOGE` 宏输出，禁止直接调用 `OH_LOG_Print`，保证标签一致。
- 着色器相关日志通过 OpenGL 接口返回，不属于应用日志体系，仅用于渲染调试。
- 测试代码使用 `stderr` 输出，不进入 hilog，避免污染运行时日志。
- 未发现日志级别开关、采样率控制或性能敏感路径的日志裁剪机制。