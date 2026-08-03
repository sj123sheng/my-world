---
kind: build_system
name: HarmonyOS HAP + CMake 混合构建系统
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
    - automation/hvigor/check_rules.js
---

本项目采用 HarmonyOS 官方构建体系（Hvigor + AppGallery Connect），以 ArkUI (ETS) 作为 UI 层，通过 N-API 桥接原生 C++ 游戏引擎，形成「ArkUI + N-API + CMake」的混合构建模式。

**构建工具链与入口**
- 根目录 `hvigorfile.ts` 引入 `@ohos/hvigor-ohos-plugin` 的 `appTasks`，模块级 `entry/hvigorfile.ts` 使用 `hapTasks`，由 Hvigor 统一编排编译、打包流程。
- 应用包信息定义在 `AppScope/app.json5`（bundleName、versionCode/Name、min/target APIVersion）。
- 依赖管理使用 ohpm：根 `oh-package.json5` 声明模型版本 6.1.0，无外部依赖；模块依赖通过 `entry/oh-package.json5` 管理。

**C++ 引擎构建**
- CMake 清单位于 `entry/src/main/cpp/CMakeLists.txt`，生成共享库 `native_game`，C++ 标准固定为 C++17，并定义 `OHOS_PLATFORM` 宏。
- include 路径直接指向项目根下的 `native/` 目录，按 engine/core、engine/render、gameplay/*、platform/harmony 等子模块组织源文件。
- 链接的 HarmonyOS 系统库包括 libace_napi.z.so、libEGL.so、libGLESv3.so、libnative_window.so 等，构成 XComponent/NativeWindow/EGL+GLES3 渲染链路。

**产物与 ABI**
- 通过 `build-profile.json5` 和 `entry/build-profile.json5` 的 `abiFilters` 限定 arm64-v8a 与 x86_64 两种架构。
- 构建模式分为 debug（debuggable=true，含签名配置）与 release（debuggable=false）两档。
- 签名配置写在根 `build-profile.json5` 的 signingConfigs 中，包含证书路径、别名、密码等。

**自动化与检查**
- `automation/hvigor/check_rules.js` 提供命名原创性扫描规则（对比 config/ 与 assets/ 名称）。
- `automation/perf/profile_collect.sh` 用于性能数据采集。
- `.hvigor/` 缓存目录存放构建中间产物与报告。

**约定与约束**
- 所有 C++ 源码必须放在 `native/` 下，并通过 CMakeLists.txt 显式注册到 native_game 目标。
- 平台相关代码通过 `platform/harmony/` 隔离，由 OHOS_PLATFORM 宏区分编译分支。
- 资源文件通过 `assets/` 与 `entry/src/main/resources/rawfile/` 两级管理，前者供自动化脚本校验，后者随 HAP 打包。