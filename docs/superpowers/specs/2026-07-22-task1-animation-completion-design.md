# M4 Task 1 动画响应补全设计

## 目标

补全 M4 Task 1 尚未达到的验收项：闪避动画与位移、四类技能的独立动画、敌人和首领的动作动画；保留已经验证通过的普攻、移动朝向以及 `run/idle` 切换。

## 现状与根因

当前渲染动画只区分 `idle`、`run`、`attack`、`hit` 和 `death`。`Dodging` 没有对应渲染意图，来源技能和终结技被压缩为 `attacking` 布尔值，敌人及首领快照也只向渲染层投影存活状态。因此：

- 闪避期间仍播放 `idle`；
- 技能无法选择各自的 clip；
- 敌人和首领存活时基本只播放 `idle`。

三个 GLB 资源均包含可复用的原始动画：`Running_Strafe_Right`、`Spellcast_Raise`、`Spellcast_Shoot`、`Spellcasting` 和 `Spellcast_Long`，无需新增美术资源。

## 动画映射

玩家动作采用以下确定映射：

| 游戏动作 | 渲染动画 | 首选 clip | 后备 clip |
|---|---|---|---|
| 静止 | Idle | `idle` | 第一个可用 clip |
| 移动 | Run | `run` | `idle` |
| 普攻连段 | Attack | `attack` | `idle` |
| 闪避 | Dodge | `Running_Strafe_Right` | `run`、`idle` |
| 辉印 | Radiance | `Spellcast_Raise` | `attack`、`idle` |
| 脉流 | Current | `Spellcast_Shoot` | `attack`、`idle` |
| 蚀质 | Corruption | `Spellcasting` | `attack`、`idle` |
| 终结 | Ultimate | `Spellcast_Long` | `attack`、`idle` |
| 受击 | Hit | `hit` | `idle` |
| 死亡 | Death | `death` | `idle` |

动画优先级为：`Death > 主动动作 > Hit > Run > Idle`。主动动作必须保存明确类型，不能再仅由 `attacking` 布尔值表达。

## 状态与数据流

### 玩家

`CombatSnapshot.currentAction` 是玩家动作的权威来源。循环层将其映射为明确的渲染动画意图：

- `Attack1` 至 `Attack4` 映射为 `Attack`；
- `Dodging` 映射为 `Dodge`；
- `CastingSource` 结合当前来源动作映射为 `Radiance`、`Current` 或 `Corruption`；
- `CastingUltimate` 映射为 `Ultimate`。

在 `ActionStateMachine` 增加只读 `activeAction()`，并由 `CombatSnapshot.activeCombatAction` 在动作存续期发布当前 `CombatAction`。`CastingSource` 必须结合该字段区分三种来源技能；渲染层不得从按钮事件猜测动作。

闪避位移继续使用已有战斗状态机和玩家控制逻辑，不在渲染层伪造位移。

### 敌人

`EncounterEnemySnapshot` 增加 `moving`、`attacking` 和 `hit`。`EnemySlot` 保存最近一次 `EnemyUpdateResult`：非零 `movement` 映射为移动，`Windup` 或 `Active` 阶段映射为攻击，`interrupted` 映射为受击；随后由 `publish3DEncounterState` 投影到 `ActorRenderState`。

敌人移动播放 `run`，攻击播放 `attack`，受击播放 `hit`，死亡播放 `death`。朝向继续由 `facing` 计算 yaw。

### 首领

`Boss3DRenderState` 从现有首领快照派生动作：`castRemainingMs > 0` 时映射为 `Ultimate`，生命值相邻快照下降时启动短暂 `Hit` 意图，`defeated` 映射为 `Death`，其他存活状态为 `Idle`。当前首领控制器没有普通攻击状态，因此本任务不伪造普通攻击；机制施法使用 `Spellcast_Long`，受击和死亡分别使用 `hit`、`death`。

## Clip 解析与容错

clip 解析由单一函数负责，并按映射表依次尝试首选与后备名称。缺少专用 clip 时安全回退到通用动作或 `idle`；资源没有任何动画时保持现有空结果。解析失败只影响动画选择，不得改变 EGL、GLES、XComponent 或 NativeWindow 生命周期，也不得加入软件渲染降级。

设备日志记录：实体类型、动作意图、最终解析的实际 clip、移动/攻击状态。日志应在动作或 clip 发生变化时输出，避免每帧刷屏。

## 测试策略

采用 TDD，先增加失败测试，再实现最小代码：

1. `RenderAnimation` 对闪避和四类技能返回正确首选 clip；
2. 专用 clip 缺失时按规定顺序回退；
3. 动画优先级满足死亡、主动动作、受击、移动、静止顺序；
4. 每个 `ActionState` 精确映射到渲染动画；
5. 敌人和首领快照能投影移动、攻击、受击与死亡意图；
6. 现有玩家、敌人和首领 yaw 测试保持通过；
7. 全部相关宿主测试、Node 桥接契约、HAP 构建和 `git diff --check` 通过。

## 设备验收

在 Pura 70 Pro 模拟器安装本次构建的签名 HAP，核对安装成功后逐项验证：

1. 普攻出现 `attack`；
2. 闪避出现 `Running_Strafe_Right` 且玩家位置变化；
3. 辉印、脉流、蚀质和终结分别出现各自首选 clip；
4. 摇杆移动时出现 `run`，释放后恢复 `idle`，模型朝向跟随移动方向；
5. 启动敌群和首领场景，确认移动、攻击、受击及死亡动作不再始终为 `idle`；
6. 验证期间无 `SIGSEGV`、`cppcrash`、EGL 错误或渲染进程退出。

若技能因资源、目标或冷却条件被拒绝，应先建立满足条件的训练场景，再验证成功动作；被拒绝的输入不能作为动画失败或成功证据。

## 范围边界

- 不新增或重制美术资源；
- 不修改战斗数值、冷却和资源消耗规则；
- 不引入新的图形 API 或软件渲染路径；
- 不扩展到 Task 2 的场景氛围和视觉特效工作。
