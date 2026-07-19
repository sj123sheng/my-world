# 阶段 6 HUD、表现与优化设计

**日期：** 2026-07-19
**状态：** 待确认
**上游依赖：** 阶段 5 自动化、生产构建和真机入口验收全部通过
**范围：** 表现事件驱动 VFX、占位音频、移动 HUD、性能降级梯度和真机稳定性门槛

## 1. 目标与边界

阶段 6 在阶段 5 首领与关卡系统上交付表现层和性能护栏，使 8–12 分钟完整战斗流程在
HarmonyOS 真机上可运行、可区分反馈、可稳定维持。本阶段不扩展玩法规则、敌人种类或首领
阶段数量，只把已有的战斗事件、首领阶段和关卡流程以灰盒 VFX、占位音频和正式 HUD 呈现出来，
并加入性能降级保护。

本阶段交付：

- 表现事件驱动的灰盒 VFX：命中闪光、精准闪避辉光、破韧爆发、共鸣爆发、首领阶段切换
  和镜头震动。
- 移动 HUD：生命/韧性/体力条、首领生命条与阶段标记、技能冷却遮罩、关卡门/补给状态、
  非战斗淡出和可切换调试覆盖。
- 敌人和首领灰盒渲染：软锁定候选、首领阶段配色和机制读条。
- 占位音频：动作、命中、源能力、首领机制和环境五层，接入 `OHAudioRenderer`，设备不支持
  时静默降级。
- 性能降级：FPS 监控和粒子/拖尾/调试频率自适应梯度，不得降级命中判定、敌方预警、首领
  读条和三源状态表现。
- 真机稳定性门槛：8–12 分钟完整流程连续通关、60 FPS 目标、45 FPS 压力场景、30 FPS 最低
  兼容、10 分钟持续无崩溃或内存增长、冷启动/前后台/锁屏/Surface 重建稳定。

明确不属于阶段 6：

- 正式美术资源、角色模型、动画骨骼或骨骼绑定。
- NavMesh、区域流式加载、存档进度、联网或 Mod 系统。
- 新敌人、新首领、新技能或新关卡环节。
- 剧情系统、任务系统、掉落或成长闭环。
- 最终 UI 美术和品牌设计。

## 2. 架构原则

表现层遵循"事件驱动＋只读消费"原则。固定 tick 的单向数据流为：

```text
Loop::updateFixed
  -> EncounterController::update (产生 GameplayEvent + PresentationEvent)
  -> VfxSystem::consume (消费 PresentationEvent，更新效果状态)
  -> PerformanceGuard::sample (采样 FPS 和负载，输出降级级别)
  -> AudioBridge::dispatch (按事件类型分发占位音效)
  -> surface_draw (读取 VFX 状态和降级级别，提交渲染)
  -> SnapshotStore::publish (10–20 Hz HUD 快照)
```

必须保持以下边界：

- VFX、音频和 HUD 只消费事件和快照，不反向修改战斗状态。
- 渲染线程不执行战斗逻辑或 AI 决策。
- 音频失败不得阻塞渲染线程或战斗循环。
- 性能降级只减少表现层数据量，不改变战斗结果或命中判定。
- HUD 快照频率固定在 10–20 Hz，不逐事件跨 N-API 更新。
- 调试覆盖可由真机按钮或 N-API 入口切换，不影响正式 HUD 布局。

## 3. 核心组件

### 3.1 `VfxSystem`

`VfxSystem` 消费每 tick 的 `PresentationEvent` 批次，维护短时表现效果状态。效果状态只包含
定长标量和定容数组，不持有动态分配或外部句柄。

`VfxSnapshot` 至少包含：

- 命中闪光计时器和强度（按 `HitFlash` 事件）。
- 精准闪避辉光计时器（按 `DodgeFlash` 事件）。
- 破韧爆发计时器（按 `PoiseBreakBurst` 事件）。
- 共鸣爆发计时器（按 `ResonanceBurst` 事件）。
- 首领阶段切换闪光计时器（按 `PhaseTransition` 事件）。
- 镜头震动偏移和衰减计时器（按 `CameraShake` 事件）。
- 读条碎裂计时器（按 `CastBarBroken` 事件）。

所有计时器以 tick 为单位，每 tick 按固定步长衰减，归零后效果消失。`VfxSystem::update()`
在 `Loop::updateFixed` 的战斗结算后调用，`VfxSystem::snapshot()` 供渲染层只读消费。

### 3.2 渲染扩展

`surface_draw` 增加以下绘制阶段，均在 GLES3 路径上实现，软件 Buffer 路径不要求覆盖：

- 敌人灰盒：按 `EncounterSnapshot::enemies` 的位置、原型配色和存活状态绘制；死亡敌人
  不绘制。
- 首领灰盒：按 `EncounterSnapshot::boss` 的位置、阶段配色和 HP 比例绘制；`bossCastMs > 0`
  时显示读条进度。
- VFX 叠加：命中闪光、闪避辉光、破韧爆发、共鸣爆发和阶段切换闪光，均以半透明几何叠加
  在对应实体位置。
- 镜头震动：将 `VfxSnapshot` 的震动偏移加到相机渲染状态后提交。
- 软锁定指示：在当前目标位置绘制锁定标记。

渲染扩展通过 `Loop::tickOnce` 在 `surface_draw` 前把 `VfxSnapshot`、`EncounterSnapshot`
和 `currentTarget` 写入 `Surface` 的扩展字段。

### 3.3 移动 HUD

HUD 保留在 ArkTS 层，通过 `pullSnapshot` 轮询。当前纯文本调试 HUD 升级为正式移动 HUD，
同时保留可切换的调试覆盖。

正式 HUD 布局：

- 左上：生命条、韧性条、体力条和源附着标记。
- 顶部中央：首领生命条、阶段标记和读条进度（仅在 Boss 模式下显示）。
- 右下：普攻、闪避、三源技能和终结技按钮，技能冷却用遮罩或灰度表示。
- 底部中央：关卡环节、门状态和补给状态（仅在 LevelFlow 模式下显示）。
- 非战斗状态淡出战斗控件，只保留移动控件。

调试覆盖（可切换）：

- FPS、角色坐标、相机 yaw/pitch、目标距离、动作状态、连击窗口、冷却数值、附着详情、
  反应类型、脉冲相位和遭遇模式/状态。

正式 HUD 不显示调试数值；调试覆盖不遮挡正式 HUD 的关键信息。

### 3.4 占位音频

`AudioBridge` 接入 HarmonyOS `OHAudioRenderer`，按 `GameplayEventType` 和
`PresentationEventType` 分发占位音效。音频分五层：

| 层 | 触发源 | 占位素材 |
|---|---|---|
| 动作 | 玩家普攻、闪避、技能释放 | 短促方波 |
| 命中 | `Hit` / `Damage` 事件 | 低频脉冲 |
| 源能力 | `AuraApplied` / `Resonance` 事件 | 三色正弦音 |
| 首领机制 | `PhaseChanged` / `CastBarBroken` | 低沉警告音 |
| 环境 | 遭遇启动/停止 | 持续低噪 |

`AudioBridge::dispatch()` 在 `Loop::updateFixed` 的战斗结算后调用。设备不支持
`OHAudioRenderer` 时静默降级为空操作，不阻塞渲染或战斗循环。

### 3.5 性能降级

`PerformanceGuard` 在 `Loop::tickOnce` 的 FPS 采样点按滑动窗口计算当前帧率，输出降级级别：

| 级别 | 触发条件 | 降级动作 |
|---|---|---|
| 0 (全量) | FPS >= 55 | 无降级 |
| 1 (轻度) | FPS 40–55 | 减少次要粒子发射率 50%，降低拖尾细分 |
| 2 (中度) | FPS 30–40 | 禁用拖尾，降低粒子上限至 50%，降低调试覆盖频率 |
| 3 (重度) | FPS < 30 | 粒子上限至 20%，禁用非必要 VFX 叠加，只保留命中判定和首领读条 |

不得降级的内容：命中判定、敌方预警、首领读条、三源状态表现和战斗结果。降级级别通过
`GameSnapshot::perfLevel` 暴露给 HUD 和渲染层。

## 4. 数据与接口

### 4.1 `GameSnapshot` 扩展

在阶段 5 快照基础上增加：

```text
int32_t perfLevel = 0;        // 性能降级级别 0–3
int32_t vfxFlags = 0;         // 当前活跃 VFX 位掩码
float cameraShakeX = 0;       // 镜头震动 X 偏移
float cameraShakeY = 0;       // 镜头震动 Y 偏移
float bossHpRatio = 0;        // 首领 HP 比例 0–1
float bossCastRatio = 0;      // 首领读条比例 0–1
```

### 4.2 `Surface` 扩展

`Surface` 增加只读表现字段，由 `Loop::tickOnce` 在 `surface_draw` 前写入：

```text
VfxSnapshot vfx;
EncounterSnapshot encounter;
TargetSelection currentTarget;
```

渲染层只读这些字段，不反向修改。

### 4.3 桥接扩展

N-API 新增：

- `toggleDebugHud()`：切换调试覆盖显示。

`pullSnapshot` 扩展字段：`perfLevel`、`vfxFlags`、`cameraShakeX/Y`、`bossHpRatio`、
`bossCastRatio`。

ArkTS `CombatControls` 新增 `调试` 按钮，调用 `toggleDebugHud()`。

## 5. 测试策略

### 5.1 单元测试（macOS 纯逻辑）

- `VfxSystem`：各事件类型触发对应计时器、计时器按 tick 衰减、归零后效果消失、重复事件
  刷新不叠加。
- `PerformanceGuard`：FPS 滑动窗口正确计算、降级级别边界 55/40/30 单调不回退、恢复后
  升级。
- 快照字段一致性：`GameSnapshot` 新增字段在 Bridge、Index.d.ts 和 native bridge 中顺序
  一致。

### 5.2 合约测试（Node）

- Bridge 声明 `toggleDebugHud` 返回 void。
- 新增快照字段在 Bridge、Index.d.ts、native bridge 和 GamePage 中一致。
- CombatControls 包含 `调试` 按钮并调用 `toggleDebugHud()`。

### 5.3 真机门槛

- 8–12 分钟完整流程：训练 → 普通敌人 → 精英 → 补给 → 首领 → 胜利或失败重试。
- 帧率：目标 60 FPS，压力场景约 45 FPS，最低兼容 30 FPS；`perfLevel` 随帧率变化。
- 持续性：10 分钟持续战斗无崩溃、死锁或明显内存增长。
- 稳定性：冷启动、前后台切换、锁屏恢复和 Surface 重建后应用保持运行。
- 反馈可区分：精准闪避、打断、破韧和共鸣爆发在真机上可视觉区分。
- 首领阶段切换：三阶段切换时有视觉闪光和配色变化。
- 调试覆盖：切换后不遮挡正式 HUD 关键信息。

## 6. 验收出口

| 出口 | 标准 |
|---|---|
| 自动化 | macOS 纯逻辑测试全通过（含新增 VfxSystem/PerformanceGuard 测试），Node 桥接合约通过 |
| 生产构建 | Hvigor `assembleHap` 返回 `BUILD SUCCESSFUL`，signed HAP 可安装 |
| 真机完整流程 | 8–12 分钟流程通关，首领可反复失败和重试 |
| 真机帧率 | 60 FPS 目标，45 FPS 压力，30 FPS 最低，`perfLevel` 随帧率响应 |
| 真机稳定性 | 10 分钟持续无崩溃或内存增长，冷启动/前后台/锁屏/Surface 重建稳定 |
| 真机反馈 | 精准闪避、打断、破韧、共鸣爆发和首领阶段切换可视觉区分 |

## 7. 约束与风险

- 灰盒 VFX 不依赖外部纹理或模型资源，只用 GLES3 基本几何和颜色混合。
- `OHAudioRenderer` 在部分设备上初始化延迟较长，音频首帧可能延迟；采用预初始化和静默
  降级规避。
- 性能降级基于 FPS 滑动窗口，避免单帧抖动导致频繁切换；窗口设为 2 秒。
- 渲染扩展不增加新的 Shader Program，复用现有单色 Shader 加透明度混合。
- `Surface` 扩展字段在 `surface_draw` 前由 Loop 写入，渲染线程不等待锁，只读快照。

## 8. 实施约束

本设计批准并保存后，编写分阶段实施计划。实施计划必须按 VFX 系统 → 渲染扩展 → 移动 HUD →
占位音频 → 性能降级 → 真机验证的顺序拆分，每个 task 包含失败测试、实现和提交三步。不允许
在 VFX 系统未通过单元测试前接入渲染层，也不允许在性能降级未通过单元测试前接入真机验证。
