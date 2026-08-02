---
kind: build_system
name: HarmonyOS HAP 构建系统与多模块编排
category: build_system
scope:
    - '**'
source_files:
    - build-profile.json5
    - hvigorfile.ts
    - entry/build-profile.json5
    - entry/hvigorfile.ts
    - entry/src/main/cpp/CMakeLists.txt
    - oh-package.json5
    - AppScope/app.json5
    - hvigor/hvigor-config.json5
    - automation/perf/profile_collect.sh
    - automation/hvigor/check_rules.js
---

本项目基于 HarmonyOS (OpenHarmony) 的 HAP 工程结构，使用 hvigor 作为统一构建编排工具，结合 CMake 编译 C++ 游戏引擎，形成 ArkTS + NAPI + C++ 的多层混合构建体系。

**构建系统架构**
- 根级 `hvigorfile.ts` 通过 `@ohos/hvigor-ohos-plugin` 的 `appTasks` 启动应用构建流程，当前未注册自定义插件。
- `build-profile.json5` 定义应用签名、产品配置（default）、构建模式（debug/release）及 native 编译器选项。debug 模式启用 BiSheng 编译器并过滤 arm64-v8a/x86_64 ABI。
- `entry/build-profile.json5` 声明 entry 模块为 stageMode，指向 `entry/src/main/cpp/CMakeLists.txt` 作为外部原生构建入口。
- `entry/hvigorfile.ts` 使用 `hapTasks` 处理 HAP 模块构建。
- `hvigor/hvigor-config.json5` 声明 hvigor 模型版本 6.1.0。

**C++ 引擎构建**
- `entry/src/main/cpp/CMakeLists.txt` 是核心构建脚本：设置 `NATIVE_ROOT=../../../../../native`，显式列出 engine/core、engine/render、gameplay/combat、gameplay/ai、platform/harmony 等所有源文件，生成 `native_game` 共享库。
- 链接 HarmonyOS 系统库：libace_napi.z.so、libace_ndk.z.so、libnative_window.so、libEGL.so、libGLESv3.so、libhilog_ndk.z.so。
- 启用 C++17 标准并通过 `OHOS_PLATFORM` 宏区分平台代码。

**包管理与依赖**
- 根 `oh-package.json5` 声明项目元数据（modelVersion 6.1.0、version 1.0.0），无外部依赖。
- `AppScope/app.json5` 集中管理应用元数据：bundleName=com.ethelandev.myworld、versionCode=1000000、min/target API=23。

**自动化与性能采集**
- `automation/perf/profile_collect.sh`：通过 hdc 连接设备，抓取 hilog 中的 PROFILE 日志，解析 fps/perf_level/environment_draw_calls 等指标，输出 CSV 并进行 FPS 阈值校验（normal≥30，boss≥24）。
- `automation/hvigor/check_rules.js`：占位脚本，用于对比 config/assets 命名与参考作品重叠性（spec §2.4）。
- `automation/assets/` 包含 fetch/validate 环境资产的 Node.js 脚本。

**构建约束与约定**
- 所有 C++ 源码必须位于 `native/` 目录，通过 CMake 相对路径引用。
- ABI 仅支持 arm64-v8a 和 x86_64，其他架构需修改 abiFilters。
- 调试符号仅在 debug 模式启用，release 模式关闭 debuggable。
- 性能测试要求连续 3 秒内 FPS 不低于阈值，否则构建失败。