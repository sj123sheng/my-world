---
kind: configuration_system
name: HarmonyOS 工程配置系统（JSON5 + C++ 结构体验证）
category: configuration_system
scope:
    - '**'
source_files:
    - AppScope/app.json5
    - entry/src/main/module.json5
    - build-profile.json5
    - oh-package.json5
    - config/dev/characters.json
    - config/dev/enemies.json
    - config/dev/relics.json
    - config/dev/resonances.json
    - config/schema/resonance.schema.json
    - native/gameplay/combat/combat_config.h
    - native/gameplay/ai/enemy_ai_config.h
    - native/engine/input/camera_gesture.h
    - native/engine/input/virtual_joystick.h
    - native/engine/render/camera.h
    - tests/test_config_schema.cpp
---

本工程的配置系统由两层组成：上层是 HarmonyOS 应用与模块的 JSON5 声明式配置，下层是 C++ 游戏核心中通过结构体默认值与验证函数承载的运行时参数。两者共同构成“静态声明 + 运行时校验”的配置体系。

**1. 应用与模块级配置（JSON5）**
- `AppScope/app.json5`：定义 bundleName、vendor、versionCode/versionName、icon、label、min/target APIVersion 等应用元数据。
- `entry/src/main/module.json5`：声明 entry 模块的 type、mainElement、deviceTypes、pages、abilities、requestPermissions 等 ArkUI 模块配置。
- `build-profile.json5`：顶层构建配置，包含 signingConfigs、products（签名、SDK 版本、ABI 过滤 arm64-v8a/x86_64）、buildModeSet（debug/release 模式及 nativeCompiler BiSheng）。
- `oh-package.json5`：项目包描述，modelVersion、name、version、author、license 等。
- `.hvigor/hvigor-config.json5`：Hvigor 构建工具配置。

这些文件全部采用 JSON5 格式，支持注释与尾逗号，便于开发者维护。

**2. 游戏内容配置（JSON + Schema）**
- `config/dev/` 目录存放开发期数值配置：`characters.json`、`enemies.json`、`relics.json`、`resonances.json`，均为扁平 JSON 对象/数组，直接映射到 C++ 结构体字段。
- `config/schema/resonance.schema.json`：使用 JSON Schema 定义 resonances 数据的结构约束（如 pairs 数组中 a/b/result 字段类型与必填项），为发布期数据校验提供模板。
- `tests/test_config_schema.cpp`：当前 MVP 阶段仅做文件存在性断言，注释明确“发布期接 JSON Schema 校验库”，表明未来会接入完整 schema 验证。

**3. C++ 运行时配置（结构体 + validated()）**
游戏核心不使用外部配置文件解析器，而是将平衡性参数以 C++ 结构体形式内联定义，并通过 `validated()` 方法在构造后做范围检查与回退：
- `native/gameplay/combat/combat_config.h`：`CombatConfig` 包含连击伤害、耐力、闪避、源技能冷却/伤害、共鸣、训练目标等数十个字段，`defaults()` 提供默认值，`validated()` 逐项校验并回退到默认值。
- `native/gameplay/ai/enemy_ai_config.h`：`EnemyAiConfig` / `CombatRegionConfig` 定义敌人 AI 能力、区域、冷却等，`validated()` 返回 `std::optional`，非法时返回空。
- `native/engine/input/camera_gesture.h`、`native/engine/input/virtual_joystick.h`、`native/engine/render/camera.h`：输入与渲染相关参数（灵敏度、死区、半径、视角角度/距离范围）均以 `*Config` 结构体 + 构造函数内 `std::isfinite` 校验的方式处理。

**4. 资源清单与环境配置**
- `assets/manifest.json`、`assets/environment/layout.json`、`assets/environment/manifest.json`：资源索引与关卡布局。
- `automation/assets/fetch_environment_assets.mjs`、`automation/assets/validate_environment_assets.mjs`：Node.js 脚本用于拉取与校验环境资源。

**约定与约束**
- 所有数值型配置必须非负或满足业务区间，否则 `validated()` 会回退到 `defaults()` 或返回 `std::nullopt`，保证运行时安全。
- JSON5 用于构建与应用声明式配置，JSON + JSON Schema 用于游戏内容数据，C++ 结构体作为最终运行时载体。
- 测试覆盖 `test_combat_config.cpp`、`test_enemy_ai_config.cpp`、`test_config_schema.cpp` 等，确保配置合法性。
- 未使用环境变量、`.env` 文件或运行时动态加载机制；配置在编译/启动时即确定。