---
kind: configuration_system
name: HarmonyOS 游戏项目配置系统
category: configuration_system
scope:
    - '**'
source_files:
    - AppScope/app.json5
    - build-profile.json5
    - hvigor/hvigor-config.json5
    - entry/src/main/module.json5
    - config/dev/characters.json
    - config/schema/resonance.schema.json
    - assets/manifest.json
    - assets/environment/layout.json
    - native/gameplay/combat/combat_config.h
    - native/gameplay/ai/enemy_ai_config.h
---

该 HarmonyOS 游戏项目采用分层配置系统，将应用元数据、构建配置、游戏数值配置和资源清单分离管理：

**应用与构建配置层**
- `AppScope/app.json5`：HarmonyOS 应用级元数据（bundleName、versionCode、minAPIVersion 等），使用 JSON5 格式便于注释
- `build-profile.json5`：统一编排所有子模块的构建配置，定义 products（default）、buildModeSet（debug/release）、nativeCompiler（BiSheng）及 ABI 过滤（arm64-v8a, x86_64）
- `hvigor/hvigor-config.json5`：Hvigor 构建工具版本与依赖声明
- `entry/src/main/module.json5`：entry 模块的 ArkUI 页面、权限、设备类型等声明

**游戏数值配置层**
- `config/dev/`：开发环境的游戏数值配置，包含 characters.json、enemies.json、relics.json、resonances.json 等 JSON 文件
- `config/schema/resonance.schema.json`：JSON Schema 用于验证 resonances.json 的数据结构
- C++ 头文件中的硬编码默认值：`CombatConfig`、`EnemyAiConfig`、`ThirdPersonCameraConfig` 等结构体提供 validated() 方法，在运行时验证并回退到 defaults()

**资源清单层**
- `assets/manifest.json`：资源版本管理与条目清单（id、size、hash、deps）
- `assets/environment/layout.json`：场景布局配置，定义模型实例的 id、region、rotation、scale、translation 等属性

**配置加载与验证模式**
- 数值配置通过 C++ 结构体的 validated() 方法进行运行时校验，不合法时回退到 defaults()
- 资源通过 N-API 桥接从 ArkTS 层传入二进制数据，经 CopyAndCommitModelAssets/CopyAndCommitEnvironmentAssets 提交到渲染层
- 构建期通过 hvigor 统一编排，支持 debug/release 双模式切换

**约束与约定**
- 所有数值配置必须提供 validated() 和 defaults() 方法
- 资源清单需遵循 manifest.json 的 schema（version、entries 数组）
- 环境布局配置要求每个条目包含 id、region、sourceNode 及变换参数
- 构建配置通过 build-profile.json5 统一管理，避免分散在各模块中