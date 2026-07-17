# 阶段 4 Task 6 实施报告：三类敌人原型

## 状态

完成实现、本地自审与规定验证。正式提交哈希见任务最终返回。

`git ls-files` 确认 `.superpowers/sdd/task-6-report.md` 是已跟踪的旧报告，因此本任务使用
`.superpowers/sdd/task-6-enemy-ai-report.md`，没有覆盖旧文件。

## 实现内容

- 新增 `enemy_archetypes.h/.cpp`，提供裂爪兽、辉印祭司、蚀甲守卫三套命名默认配置和稳定
  能力 ID。三套配置均通过 `EnemyAiConfig::validated()`，差异由强类型 category、target、
  effect、telegraph、cancel policy，以及范围、权重、时序和冷却表达；核心逻辑不读取标签字符串。
- 裂爪兽使用可在前摇期打断的近战爪击，并复用 Agent 已有的警戒空闲、追击、稳定分离、区域
  返回能力。
- 辉印祭司提供强类型 `Shield` 支援效果和 `WarningYellow` 读条数据；策略只把缺盾、存活、
  区域内、范围内且不是自身的友军视为合法支援目标。黄色只作为表现数据，没有加入 UI 或渲染。
- 通用 Combat event 层定义 `CombatEffectType::Shield` 与 `CombatEffectRequest`；执行器对 Damage
  只产生 `HitRequest`，对 Shield 只产生效果请求，二者共享一次性事务 ID 且严格互斥。Agent
  原样转发效果请求，不执行护盾数值结算。
- 蚀甲守卫提供不可被普通韧性伤害打断的盾击、1200ms 破韧硬直，以及通用方向防御配置。
  `DamageResolver` 只消费 `DamageResolutionContext`，正面 HP 伤害乘 0.5，背面 HP 正常，韧性
  伤害不减；Combat 不依赖 AI 或 `EnemyArchetype`。
- Agent 的正式冷却语义为：每次 update 先按非负 `dtMs` 将既有冷却饱和递减到 0；只有
  `ActionExecutor::start()` 成功接受动作事务时，才按能力快照写入完整冷却。start 失败不设置、
  不重置；区域取消、普通打断和 PoiseBreak 不清除已经起算的冷却；`reset()` 与死亡完整清零。
- `staggerRecoveryMs == 0` 保留既有显式 `releaseStagger()` 语义；值大于 0 时，首次观测到
  staggered 的 tick 使用饱和加法锁存截止。重复 true 不延长；只有外部 staggered 已解除且当前
  tick 到达截止时才自动释放。reset/死亡清除截止。
- `EnemyUpdateInput` 通过可选强类型 `EnemyAgentInterrupt` 接收普通韧性打断，与
  `world.staggered` 的 PoiseBreak 路径分离。普通打断只作用于已有事务，遵守能力阶段与阈值，
  不清除已经开始的冷却；Agent 结果回传恢复破绽状态。
- CMake 登记两个原型文件；没有修改任何 `build-profile.json5`。

## TDD：RED / GREEN

### RED

先新增三个原型测试，仅编译测试时退出码为 1，预期失败为：

```text
fatal error: 'gameplay/ai/enemy_archetypes.h' file not found
```

首次使用 `xcrun --find clang++` 时还暴露本机默认工具链缺失 libc++ 搜索路径；随后固定使用
`/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk` 和其
`usr/include/c++/v1`，得到上述纯净 RED。

### GREEN

实现最小原型、方向减伤、冷却和硬直语义后，三个聚焦测试开始执行。首轮有两个测试夹具错误：

```text
Assertion failed: returning.desiredPosition == input.world.safeReturnPosition
Assertion failed: executor.update(1000, 0, {}).phase == EnemyActionPhase::Windup
```

根因分别是返回阶段仍保留重叠友军导致既有分离偏移，以及默认执行上下文把目标标记为死亡。
只修正夹具（返回前清空友军、显式设置 `targetAlive=true`）后，三个测试全部通过。随后补充
非法 telegraph/负硬直校验、硬直首次截止/不延长/外部条件/reset/死亡和完整冷却生命周期测试，
均保持 GREEN。

### 复审修复 RED / GREEN

复审首先指出 Shield 被错误走入 HitRequest。先补执行器与 Agent 时间线测试，编译 RED 为：

```text
no member named 'effectAmount' in 'EnemyAbility'
no member named 'effect' in 'EnemyExecutionResult'
use of undeclared identifier 'CombatEffectType'
```

加入通用效果请求并分流后 GREEN。随后配置矩阵测试在旧实现上退出 134：

```text
Assertion failed: (!damageSupport.validated().has_value())
```

收紧为 `Support + LowestShieldAlly + Shield + positive amount` 后 GREEN，同时保持 Attack 的既有
非 Shield 效果合法。最后新增 Agent 普通打断测试，编译 RED 为缺少 `EnemyAgentInterrupt`、输入
字段和结果标记；接入执行器普通打断后，3 个原型、执行器、Agent、配置共 6/6 聚焦测试通过。

## 验证

编译使用 C++17、显式 macOS 26.5 SDK/libc++、`-I. -Inative`：

- 聚焦严格测试：三个原型测试以及 `test_enemy_action_executor`、`test_enemy_agent`、
  `test_enemy_ai_config` 使用 `-Wall -Wextra -Werror`，6/6 通过且无警告。
- 聚焦 UBSan：同一组 6 个测试使用
  `-fsanitize=undefined -fno-sanitize-recover=undefined`，6/6 通过且无 sanitizer 报告。
- 完整 C++：先编译 25 个可测试生产单元，再逐个链接执行 `tests/test_*.cpp`，41/41 通过。
- Node：`node tests/test_bridge_contract.mjs` 退出 0。
- CMake：在全新临时目录执行配置与生成，退出 0；只有既有 CMake 3.5 兼容弃用警告。
- `git diff --check`：退出 0、无输出。
- 边界检查：`build-profile.json5` 与 `entry/build-profile.json5` 均无 diff；
  `damage_resolver.h/.cpp` 不包含 AI、原型或 `EnemyArchetype`；CMake 已登记两个新增原型文件。

全量生产对象只出现仓库既有的 `loop.cpp`、`relic.cpp`、`character.cpp` 未使用参数警告；本任务
聚焦文件在 `-Werror` 下无警告。

## 自审

- 默认能力配置均经过合法性测试；telegraph 和新增 Shield effect 的无效枚举值会被整体拒绝。
- 能力兼容矩阵拒绝 Support+Damage、Attack+Shield、零数值 Shield 和带效果数值的 Damage；
  执行器入口也会拒绝绕过配置的非法效果载荷。
- 冷却不读取墙钟；同一 update 的顺序明确，成功 start 后的新冷却不会在建立当帧被提前递减。
  长动作中冷却先归零导致的 start 失败不会重置冷却，动作结束后下一帧可确定性重试。
- 区域取消、破韧、显式 release、reset 和死亡路径都有直接测试；守卫硬直的重复 true、提前解除、
  截止仍 true、截止后解除均有直接断言。
- 方向防御对无效倍率、非有限几何、零长度朝向采取无减伤安全回退；只影响 HP，不阻塞正面破韧。
- 没有大型 `switch(EnemyArchetype)`、标签字符串分支、随机数或无序遍历；没有新增遭遇、多目标
  快照或 UI 逻辑。
- Shield 完整 Agent 时间线覆盖合法友军、前摇、单次效果、互斥 hit、同 tick 不重复、恢复和完成；
  普通打断覆盖阈值下、阈值命中、恢复破绽、冷却保持及 Active 期拒绝。

## 顾虑与后续边界

- Shield 已形成强类型效果请求，但本任务不执行护盾数值结算；后续战斗效果层需要消费该请求并
  应用到目标护盾，不能把它重新映射成伤害命中。
- 方向减伤通过显式 `DamageResolutionContext` 传入；后续遭遇接线需要提供攻击者/守卫位置与守卫
  朝向。本任务没有提前扩展世界快照。
- 本机默认 macOS SDK/libc++ 搜索路径不完整，验证固定使用同机完整的 MacOSX26.5 SDK；这是环境
  问题，不是仓库变更。

## 独立审查

首轮独立复审发现效果事务、普通打断输入、配置矩阵与精确断言缺口，本报告记录的第二提交完成
修复；新提交后的统一复审仍由控制层执行。
