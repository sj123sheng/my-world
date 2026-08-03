---
kind: dependency_management
name: HarmonyOS 项目依赖管理（oh-package + hvigor）
category: dependency_management
scope:
    - '**'
source_files:
    - oh-package.json5
    - entry/oh-package.json5
    - entry/oh-package-lock.json5
    - build-profile.json5
    - hvigorfile.ts
    - entry/hvigorfile.ts
---

本项目基于 HarmonyOS 应用开发，使用 ohpm（Open Harmony Package Manager）进行依赖声明与解析，构建系统采用 hvigor。整体依赖管理呈现以下特征：

1. **包声明方式**
   - 根目录 `oh-package.json5` 定义应用元信息（modelVersion 6.1.0、name my-world），dependencies 为空对象，说明应用本身不直接依赖第三方 npm 风格包。
   - 模块级 `entry/oh-package.json5` 通过 `file:` 协议引入本地 C++ 类型绑定包 `libnative_game.so@src/main/cpp/types/libnative_game`，这是 ArkTS 调用 native 层的典型方式。

2. **锁文件与版本锁定**
   - `entry/oh-package-lock.json5` 由 ohpm 自动生成，lockfileVersion=3，specifiers 中仅包含本地 file: 协议的 libnative_game.so 包，registryType 标记为 local。
   - 未启用统一锁文件（enableUnifiedLockfile=false），每个模块独立维护自己的 lock 文件。

3. **构建系统集成**
   - 根目录和 entry 模块分别有独立的 `hvigorfile.ts`，均使用官方插件 `@ohos/hvigor-ohos-plugin` 的 appTasks/hapTasks。
   - `build-profile.json5` 配置了 CMake 路径、ABI 过滤器（arm64-v8a, x86_64）、BiSheng 编译器以及 debug/release 构建模式。

4. **第三方库策略**
   - 项目中没有使用 npm/yarn/pip/go.mod 等常见语言包管理器。
   - C++ 依赖以源码形式内联在 `native/` 目录下（如 glm、cgltf、stb_image.h），属于 vendoring 策略。
   - 无私有仓库或代理配置，所有依赖均为本地文件或内联源码。

5. **约束与约定**
   - ArkTS 层不声明外部依赖，仅通过 file: 协议引用本地 C++ 类型定义。
   - Native 层第三方库直接随源码提交，不通过包管理器安装。
   - 构建过程完全由 hvigor + CMake 驱动，无需额外依赖解析步骤。