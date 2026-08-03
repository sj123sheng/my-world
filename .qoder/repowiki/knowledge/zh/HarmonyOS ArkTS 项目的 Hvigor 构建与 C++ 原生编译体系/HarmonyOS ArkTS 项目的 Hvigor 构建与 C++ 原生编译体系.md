---
kind: build_system
name: HarmonyOS ArkTS 项目的 Hvigor 构建与 C++ 原生编译体系
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
    - entry/oh-package.json5
    - AppScope/app.json5
    - hvigor/hvigor-config.json5
    - automation/assets/fetch_environment_assets.mjs
    - automation/perf/profile_collect.sh
    - automation/hvigor/check_rules.js
---

本项目基于 HarmonyOS 的 DevEco Studio 生态，使用 Hvigor 作为统一构建编排器，结合 CMake 编译 C++ 游戏引擎，形成 ArkTS UI + Native 引擎的双层架构。构建系统围绕以下核心要素组织：

**1. 构建工具链与入口**
- 根级 `hvigorfile.ts` 引入 `@ohos/hvigor-ohos-plugin` 的 `appTasks`，定义应用级任务；模块级 `entry/hvigorfile.ts` 使用 `hapTasks` 定义 HAP 模块任务。
- 项目通过 `hvigor/hvigor-config.json5` 声明模型版本 `6.1.0`，依赖为空，表明未引入第三方 Hvigor 插件。
- 文档中多次出现 `hvigorw assembleHap`、`ohpm run build` 等命令，说明开发时通过 hvigorw 包装脚本或 ohpm 执行构建。

**2. 应用与模块配置**
- 根 `build-profile.json5` 定义 app 产品 `default`，指定 `compatibleSdkVersion`/`targetSdkVersion` 为 `6.1.0(23)`，runtimeOS 为 `HarmonyOS`，nativeCompiler 设为 `BiSheng`（华为自研编译器）。
- 定义 `debug` 与 `release` 两种 buildMode，其中 debug 模式启用 `debuggable: true`，并配置 externalNativeOptions 指向 `./entry/src/main/cpp/CMakeLists.txt`，abiFilters 限定 `arm64-v8a` 和 `x86_64`。
- 模块 `entry/build-profile.json5` 声明 `apiType: stageMode`，同样配置 externalNativeOptions，targets 仅含 `default`。
- `AppScope/app.json5` 定义 bundleName `com.ethelandev.myworld`、versionCode `1000000`、versionName `1.0.0`、minAPIVersion/targetAPIVersion 均为 23。

**3. C++ 原生库编译**
- `entry/src/main/cpp/CMakeLists.txt` 是核心构建脚本，设置 `cmake_minimum_required(VERSION 3.5)`，项目名称 `native_game`。
- 通过 `NATIVE_ROOT` 变量引用项目根目录下的 `native/` 文件夹，include_directories 覆盖 engine/core、engine/input、engine/render、engine/presentation、engine/resource、gameplay/*、platform/harmony 等子模块。
- 使用 `add_library(native_game SHARED ...)` 将所有 .cpp/.h 源文件聚合为一个共享库，显式列出所有源文件（共约 90+ 个），包括 bridge、engine core、render、presentation、resource、gameplay combat/AI/entities/player/targeting/growth、platform harmony 等。
- 设置 C++ 标准为 `cxx_std_17`，定义宏 `OHOS_PLATFORM`，链接 libace_napi.z.so、libace_ndk.z.so、libnative_window.so、libnative_buffer.so、libEGL.so、libGLESv3.so、libhilog_ndk.z.so 等 HarmonyOS 系统库。

**4. 资源管线与自动化脚本**
- `automation/assets/fetch_environment_assets.mjs` 是一个完整的 Node.js 资源处理流水线：从 Poly Haven API 下载 glTF/GLB 资产，解析 GLB 二进制格式，按 layout.json 分区生成四个环境区域（outerRing、centerRift、backdrop、decoration），烘焙节点变换、合并材质、嵌入纹理层级（2K/1K）、验证外部 URI 已被内嵌，最终输出到 `entry/src/main/resources/rawfile/environment/` 并生成 `assets/environment/manifest.json` 记录来源与校验信息。
- `automation/perf/profile_collect.sh` 是基于 HDC 的性能采集脚本：通过 hilog 抓取 PROFILE 日志，解析 fps、perf_level、draw_calls、triangles 等指标，输出 CSV 并进行 FPS 阈值检查（normal 阶段 ≥30fps，boss 阶段 ≥24fps）。
- `automation/hvigor/check_rules.js` 是原创性检查脚本桩，用于对比 config/ 与 assets/ 命名与参考作品是否重叠。

**5. 依赖管理**
- 根 `oh-package.json5` 声明项目名 `my-world`、版本 `1.0.0`，无 dependencies。
- `entry/oh-package.json5` 声明对本地 `.so` 文件的依赖：`"libnative_game.so": "file:./src/main/cpp/types/libnative_game"`，通过 N-API 类型定义桥接 ArkTS 与 C++。

**6. 构建产物与输出**
- 构建产物位于 `entry/build/` 目录（空目录，实际产物由 hvigor 生成）。
- 中间缓存位于 `.hvigor/cache/`、`.hvigor/dependencyMap/`、`.hvigor/outputs/`、`.hvigor/report/`。
- 测试代码集中在 `tests/` 目录，包含大量 C++ 单元测试（test_*.cpp）和 Node.js 合约测试（test_bridge_contract.mjs、test_environment_assets.mjs）。

**约定与约束**
- 所有 C++ 源文件必须显式在 CMakeLists.txt 中注册，不允许自动发现。
- ABI 限制为 arm64-v8a 与 x86_64，不支持其他架构。
- 资源管线要求所有外部 URI 必须在导出前被内嵌，禁止运行时网络访问。
- 性能采集脚本强制要求 PROFILE 日志中存在特定键值对，否则视为采集失败。
- 版本管理采用 AppScope 中的 versionCode（数值型）与 versionName（字符串型）双轨制。