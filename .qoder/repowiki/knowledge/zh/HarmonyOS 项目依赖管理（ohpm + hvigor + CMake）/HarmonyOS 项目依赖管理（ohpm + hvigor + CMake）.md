---
kind: dependency_management
name: HarmonyOS 项目依赖管理（ohpm + hvigor + CMake）
category: dependency_management
scope:
    - '**'
source_files:
    - oh-package.json5
    - entry/oh-package.json5
    - entry/oh-package-lock.json5
    - entry/src/main/cpp/types/libnative_game/oh-package.json5
    - hvigorfile.ts
    - build-profile.json5
---

本项目基于 HarmonyOS 应用工程，采用 ohpm（OpenHarmony Package Manager）作为包管理器，配合 hvigor 构建系统与 CMake 管理原生依赖。具体实践如下：

1. **包声明与版本管理**
   - 根目录 `oh-package.json5` 定义应用元数据，当前无第三方依赖。
   - `entry/oh-package.json5` 声明对本地 NAPI 模块 `libnative_game.so` 的依赖，通过 `file:./src/main/cpp/types/libnative_game` 指向本地路径。
   - `entry/src/main/cpp/types/libnative_game/oh-package.json5` 将 C++ NAPI 模块包装为 ohpm 可识别的类型包，暴露 `Index.d.ts` 供 ArkTS 调用。

2. **锁定文件与安装**
   - `entry/oh-package-lock.json5` 记录依赖解析结果，lockfileVersion 为 3，registryType 标记为 local，表明依赖来自本地文件系统而非远程仓库。
   - 根级 `oh_modules/` 目录为空，说明依赖尚未安装或按需懒加载。

3. **构建系统编排**
   - 根 `hvigorfile.ts` 引入 `@ohos/hvigor-ohos-plugin`，使用默认 appTasks 系统任务，未注册自定义插件。
   - `build-profile.json5` 统一配置签名、产品变体（debug/release）、SDK 版本（6.1.0(23)）、原生编译器（BiSheng）及 ABI 过滤（arm64-v8a, x86_64），并通过 `externalNativeOptions.path` 指定 CMakeLists.txt 路径。

4. **C/C++ 依赖策略**
   - 第三方库以源码形式直接纳入仓库：`native/third_party/cgltf/cgltf.h` 和 `native/engine/core/math/glm/` 整个 GLM 数学库均内联在工程中，未通过外部包管理器引入。
   - 图像加载使用 `stb_image.h` 单头文件方式集成。

5. **约束与约定**
   - 所有 ohm 依赖必须通过 `oh-package.json5` 声明，禁止在代码中硬编码路径引用。
   - 本地 NAPI 模块需同时提供 `.so` 二进制与对应的 `oh-package.json5` + `*.d.ts` 类型声明。
   - 构建产物由 hvigor 统一编排，开发者不应直接调用 CMake 或 ohpm CLI 绕过构建流程。