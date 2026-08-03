---
kind: configuration_system
name: 游戏配置系统：JSON 数据 + C++ 结构体验证
category: configuration_system
scope:
    - '**'
source_files:
    - native/gameplay/combat/combat_config.h
    - native/gameplay/entities/boss.h
    - config/dev/characters.json
    - config/dev/enemies.json
    - config/dev/relics.json
    - config/dev/resonances.json
    - config/schema/resonance.schema.json
    - tests/test_config_schema.cpp
---

本仓库的配置系统采用「静态 JSON 数据文件 + C++ 结构体默认值与校验」的双层设计，分为运行时数值配置（combat_config.h）与可编辑游戏内容配置（config/dev/*.json）两部分。

**1. 运行时数值配置（C++ 内嵌）**
- 核心定义位于 `native/gameplay/combat/combat_config.h` 中的 `CombatConfig` 结构体，包含连击伤害、耐力、闪避、源技能冷却/伤害、共鸣反应、训练目标等全部可调参数，所有字段均提供合理的默认值。
- 通过 `validated()` 方法对传入配置进行逐项校验，不合法的值会回退到 `defaults()` 提供的安全默认值，确保崩溃防护。
- Boss 相关配置在 `native/gameplay/entities/boss.h` 的 `BossConfig` 中，同样提供 `karounDefaults()` 默认工厂。
- UI 层（ArkTS）不直接持有这些数值，而是通过 native 快照下发（如 `CombatConfig.sourceCooldownMs`、`dodgeCost`），实现 UI 与逻辑解耦。

**2. 可编辑游戏内容配置（JSON 文件）**
- 位置：`config/dev/` 目录下存放角色、敌人、遗物、共鸣表等可热更数据：
  - `characters.json`：角色基础属性（id、hp、poise）
  - `enemies.json`：敌人列表（id、hp、resist 抗性）
  - `relics.json`：遗物效果（id、modifiesAbility、mult 倍率）
  - `resonances.json`：元素共鸣反应表（a+b→result）
- 这些 JSON 文件由测试 `tests/test_config_schema.cpp` 验证存在性（MVP 阶段仅检查文件可读，注释标明发布期将接入 JSON Schema 校验库）。
- 针对共鸣表的 schema 定义在 `config/schema/resonance.schema.json`，规定了 pairs 数组及 a/b/result 字段的类型约束。

**3. 架构约定**
- 数值平衡类配置以 C++ 结构体内嵌默认值为主，保证编译期可用性与运行期健壮性；
- 策划可编辑的游戏内容以 JSON 文件形式存放在 `config/dev/`，便于版本化管理与热更新；
- 配置校验遵循「失败回退到默认值」原则，避免启动崩溃；
- UI 层通过 snapshot 机制消费 native 侧配置，不直接解析 JSON。