# UI 组件系统

<cite>
**本文引用的文件**   
- [entry/src/main/ets/ui/Hud.ets](file://entry/src/main/ets/ui/Hud.ets)
- [entry/src/main/ets/ui/CombatControls.ets](file://entry/src/main/ets/ui/CombatControls.ets)
- [entry/src/main/ets/ui/GameStateOverlay.ets](file://entry/src/main/ets/ui/GameStateOverlay.ets)
- [entry/src/main/ets/ui/HitFeedback.ets](file://entry/src/main/ets/ui/HitFeedback.ets)
- [entry/src/main/ets/ui/ComboCounter.ets](file://entry/src/main/ets/ui/ComboCounter.ets)
- [entry/src/main/ets/ui/ActionToast.ets](file://entry/src/main/ets/ui/ActionToast.ets)
- [entry/src/main/ets/pages/GamePage.ets](file://entry/src/main/ets/pages/GamePage.ets)
- [entry/src/main/ets/napi/Bridge.ets](file://entry/src/main/ets/napi/Bridge.ets)
- [native/engine/input/touch_router.h](file://native/engine/input/touch_router.h)
- [native/engine/input/pointer_input.h](file://native/engine/input/pointer_input.h)
- [tests/test_touch_controls.cpp](file://tests/test_touch_controls.cpp)
</cite>

## 更新摘要
**已进行的更改**
- 更新了 CombatControls 组件的渐变背景、阴影效果和边框宽度变化
- 新增了 GameStateOverlay 组件的入场动画和装饰线条效果
- 增强了 HitFeedback 组件的白色闪光效果和红色暗角强度缩放
- 添加了 ComboCounter 组件的动态颜色进度和缩放动画
- 改进了 ActionToast 组件的内边距、边框透明度、圆角和阴影效果
- 更新了所有组件的 HitTestMode.Block 模式改进

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构总览](#架构总览)
5. [详细组件分析](#详细组件分析)
6. [依赖关系分析](#依赖关系分析)
7. [性能考量](#性能考量)
8. [故障排查指南](#故障排查指南)
9. [结论](#结论)
10. [附录：使用与定制指南](#附录使用与定制指南)

## 简介
本技术文档聚焦 my-world 的 UI 组件系统，重点解析 Hud（用户界面显示）、CombatControls（战斗控制）以及新增的 GameStateOverlay、HitFeedback、ComboCounter、ActionToast 等 ArkUI 组件的实现原理、状态管理、事件处理与样式定制。这些组件经过重大视觉优化，包括渐变背景、阴影效果、动画过渡和交互反馈，为游戏提供了沉浸式的用户体验。同时说明它们如何与 NAPI 桥接层交互，完成数据拉取与指令下发；并补充原生输入子系统（触摸路由、虚拟摇杆、相机手势）在移动端多指交互中的角色。

## 项目结构
UI 层由 ArkUI 页面与多个专业组件构成，GamePage 作为入口承载 XComponent（渲染表面）和各个 UI 组件；NAPI Bridge 负责与原生引擎通信；原生输入子系统处理触摸与手势。

```mermaid
graph TB
subgraph "ArkUI 页面"
GP["GamePage"]
HUD["Hud"]
CC["CombatControls"]
GSO["GameStateOverlay"]
HF["HitFeedback"]
CCO["ComboCounter"]
AT["ActionToast"]
end
subgraph "NAPI 桥接"
BR["Bridge.ets"]
end
subgraph "原生引擎"
INP["输入子系统<br/>touch_router / pointer_input"]
NATIVE["Native 游戏循环/渲染"]
end
GP --> HUD
GP --> CC
GP --> GSO
GP --> HF
GP --> CCO
GP --> AT
GP --> BR
CC --> BR
BR --> NATIVE
NATIVE --> INP
```

**图表来源**
- [entry/src/main/ets/pages/GamePage.ets:154-217](file://entry/src/main/ets/pages/GamePage.ets#L154-L217)
- [entry/src/main/ets/ui/Hud.ets:1-217](file://entry/src/main/ets/ui/Hud.ets#L1-L217)
- [entry/src/main/ets/ui/CombatControls.ets:1-301](file://entry/src/main/ets/ui/CombatControls.ets#L1-L301)
- [entry/src/main/ets/ui/GameStateOverlay.ets:1-177](file://entry/src/main/ets/ui/GameStateOverlay.ets#L1-L177)
- [entry/src/main/ets/ui/HitFeedback.ets:1-94](file://entry/src/main/ets/ui/HitFeedback.ets#L1-L94)
- [entry/src/main/ets/ui/ComboCounter.ets:1-67](file://entry/src/main/ets/ui/ComboCounter.ets#L1-L67)
- [entry/src/main/ets/ui/ActionToast.ets:1-74](file://entry/src/main/ets/ui/ActionToast.ets#L1-L74)

## 核心组件
- **Hud**：只读展示型组件，通过 @Prop 接收大量运行时状态，用于绘制血条、精力条、共鸣槽、Boss 进度、调试信息等。内部使用 @State auras 维护本地 UI 状态。
- **CombatControls**：动作触发型组件，提供技能按钮簇、调试面板和操作按钮，将点击映射为 NAPI 调用，驱动原生侧的战斗行为。
- **GameStateOverlay**：遭遇结算界面，提供失败重试和胜利继续功能，包含入场动画和装饰线条。
- **HitFeedback**：受击视觉反馈层，实现屏幕边缘红色暗角闪烁、白色闪光冲击和低血量脉冲效果。
- **ComboCounter**：连击计数浮层，支持动态颜色递进和放大回弹动画。
- **ActionToast**：动作拒绝反馈浮层，提供即时提示避免无效操作困惑。

**关键要点**
- 数据流：原生 → pullSnapshot → GamePage 状态 → @Prop 传递给各组件。
- 事件流：CombatControls → pushAction/startEncounter/... → Native。
- 样式：使用 ArkUI 内置 Progress、Text、Stack/Column/Row、渐变、阴影、圆角等；HitTestMode.Transparent/Block 控制命中测试。

**章节来源**
- [entry/src/main/ets/ui/Hud.ets:1-217](file://entry/src/main/ets/ui/Hud.ets#L1-L217)
- [entry/src/main/ets/ui/CombatControls.ets:1-301](file://entry/src/main/ets/ui/CombatControls.ets#L1-L301)
- [entry/src/main/ets/ui/GameStateOverlay.ets:1-177](file://entry/src/main/ets/ui/GameStateOverlay.ets#L1-L177)
- [entry/src/main/ets/ui/HitFeedback.ets:1-94](file://entry/src/main/ets/ui/HitFeedback.ets#L1-L94)
- [entry/src/main/ets/ui/ComboCounter.ets:1-67](file://entry/src/main/ets/ui/ComboCounter.ets#L1-L67)
- [entry/src/main/ets/ui/ActionToast.ets:1-74](file://entry/src/main/ets/ui/ActionToast.ets#L1-L74)

## 架构总览
UI 与原生通过快照拉取与指令下发解耦，形成"单向数据流 + 事件驱动"的架构。

```mermaid
sequenceDiagram
participant U as "用户"
participant CC as "CombatControls"
participant GSO as "GameStateOverlay"
participant HF as "HitFeedback"
participant CCO as "ComboCounter"
participant AT as "ActionToast"
participant BR as "Bridge.ets"
participant N as "Native 引擎"
participant GP as "GamePage"
participant HUD as "Hud"
U->>CC : 点击技能按钮
CC->>BR : pushAction(type)
BR->>N : native.pushAction(type)
Note over N : 更新游戏状态/播放逻辑
loop 每100ms
GP->>BR : pullSnapshot()
BR-->>GP : Snapshot
GP->>HUD : @Prop 更新
GP->>GSO : encounterState 更新
GP->>HF : hp 更新
GP->>CCO : comboSegment 更新
GP->>AT : lastRejectReason 更新
HUD-->>U : 刷新显示
GSO-->>U : 结算界面动画
HF-->>U : 受击反馈效果
CCO-->>U : 连击动画
AT-->>U : 拒绝提示
end
```

**图表来源**
- [entry/src/main/ets/ui/CombatControls.ets:5-40](file://entry/src/main/ets/ui/CombatControls.ets#L5-L40)
- [entry/src/main/ets/ui/GameStateOverlay.ets:16-37](file://entry/src/main/ets/ui/GameStateOverlay.ets#L16-L37)
- [entry/src/main/ets/ui/HitFeedback.ets:29-46](file://entry/src/main/ets/ui/HitFeedback.ets#L29-L46)
- [entry/src/main/ets/ui/ComboCounter.ets:10-18](file://entry/src/main/ets/ui/ComboCounter.ets#L10-L18)
- [entry/src/main/ets/ui/ActionToast.ets:19-32](file://entry/src/main/ets/ui/ActionToast.ets#L19-L32)
- [entry/src/main/ets/napi/Bridge.ets:85-94](file://entry/src/main/ets/napi/Bridge.ets#L85-L94)
- [entry/src/main/ets/pages/GamePage.ets:219-293](file://entry/src/main/ets/pages/GamePage.ets#L219-L293)

## 详细组件分析

### Hud 组件
职责
- 展示玩家状态（HP、Poise、Stamina）、共鸣槽、目标信息、Boss 阶段与读条、调试面板等。
- 根据 encounterMode 动态显示 Boss 进度条。
- 支持调试开关 showDebugHud/debugHud 切换详细统计。

状态与属性
- @Prop 接收来自 GamePage 的大量字段，涵盖战斗窗口、冷却、附着状态、反应、相位、关卡门/补给、Boss 数值与比例、VFX 标志、输入事件计数等。
- @State auras：预留的本地 UI 状态数组（当前未参与渲染）。

布局与样式
- 使用 Column/Row/Blank 组合布局，Progress 线性进度条，Text 文本标签，阴影与圆角增强可读性。
- HitTestMode.Transparent 确保不拦截底层触摸事件。

响应式更新
- 所有 @Prop 变化触发重新构建；由于仅做展示，无复杂计算，开销较低。

可定制点
- 颜色主题、字体大小、间距、边框圆角、阴影半径等均可按品牌规范调整。
- 调试面板可按 perfLevel/vfxFlags 选择性显示。

**章节来源**
- [entry/src/main/ets/ui/Hud.ets:1-217](file://entry/src/main/ets/ui/Hud.ets#L1-L217)

### CombatControls 组件
职责
- 提供训练、兽群、混战、守卫、流程、首领等遭遇模式入口。
- 提供推进、补给、重试等操作按钮。
- 提供普攻、闪避、辉印、脉流、蚀质、终结等动作按钮。
- 提供调试开关。

事件处理
- 每个 Button.onClick 直接调用 Bridge 暴露的方法，如 startEncounter(mode)、pushAction(type)、advanceLevel()、useSupply()、retryBoss()、toggleDebugHud()。

布局与样式
- 多行 Row 排列按钮，统一 padding/margin，右下角对齐，透明命中区域避免遮挡渲染。
- **重大视觉优化**：实现了渐变背景、阴影效果、边框宽度变化和 HitTestMode.Block 模式改进。

```mermaid
sequenceDiagram
participant U as "用户"
participant CC as "CombatControls"
participant BR as "Bridge.ets"
participant N as "Native 引擎"
U->>CC : 点击"普攻"
CC->>BR : pushAction(0)
BR->>N : native.pushAction(0)
Note over N : 执行对应战斗动作
U->>CC : 点击"终结"
CC->>BR : pushAction(5)
BR->>N : native.pushAction(5)
Note over N : 触发终结技效果
```

**图表来源**
- [entry/src/main/ets/ui/CombatControls.ets:5-40](file://entry/src/main/ets/ui/CombatControls.ets#L5-L40)
- [entry/src/main/ets/napi/Bridge.ets:85-94](file://entry/src/main/ets/napi/Bridge.ets#L85-L94)

**章节来源**
- [entry/src/main/ets/ui/CombatControls.ets:1-301](file://entry/src/main/ets/ui/CombatControls.ets#L1-L301)
- [entry/src/main/ets/napi/Bridge.ets:85-94](file://entry/src/main/ets/napi/Bridge.ets#L85-L94)

### GameStateOverlay 组件
职责
- 遭遇结算界面：失败时提供重试，胜利时提供继续/推进。
- 遮罩阻断下层交互，构成完整的战斗闭环。

状态与属性
- @Prop encounterState：遭遇状态（失败/胜利）。
- @Prop encounterMode：遭遇模式（普通/首领/流程）。
- @State appeared：入场动画控制状态。

动画与视觉效果
- **重大视觉优化**：实现了延迟一帧触发的入场动画，装饰线条渐显效果，文本阴影增强。
- 失败界面：红色主题，装饰线淡入，文字缩放出现。
- 胜利界面：金色主题，装饰线淡入，文字缩放出现。

交互处理
- 根据 encounterMode 智能选择重试或继续逻辑。
- HitTestMode.Default/Transparent 根据状态切换命中模式。

**章节来源**
- [entry/src/main/ets/ui/GameStateOverlay.ets:1-177](file://entry/src/main/ets/ui/GameStateOverlay.ets#L1-L177)

### HitFeedback 组件
职责
- 受击视觉反馈层：受到伤害时屏幕边缘红色暗角闪烁，强度随伤害量缩放。
- 大额伤害（>=15）叠加白色闪光冲击。
- 低血量（<=30）时暗角持续呼吸脉冲，提示危险状态。

状态与属性
- @Prop hp：当前生命值，@Watch('onHpChanged') 监听变化。
- @State flashOpacity：红色暗角透明度。
- @State whiteFlashOpacity：白色闪光透明度。
- @State lowHpPulse：低血量脉冲状态。
- private prevHp：上一帧生命值。

动画与视觉效果
- **重大视觉优化**：实现了白色闪光效果、红色暗角强度缩放。
- 伤害计算：flashOpacity = Math.min(0.9, 0.35 + damage / 35)。
- 大额伤害：whiteFlashOpacity = Math.min(0.5, 0.2 + damage / 60)。
- 低血量脉冲：透明度在 0.35 ~ 0.6 之间往复。

**章节来源**
- [entry/src/main/ets/ui/HitFeedback.ets:1-94](file://entry/src/main/ets/ui/HitFeedback.ets#L1-L94)

### ComboCounter 组件
职责
- 连击计数浮层：连击段数 >= 2 且在连击窗口内时显示。
- 每次段数增加触发放大回弹动画，窗口关闭后自动隐藏。
- 颜色随连击数递进：金色(<5) → 橙色(5-9) → 红色(>=10)。

状态与属性
- @Prop comboSegment：连击段数，@Watch('onComboChanged') 监听变化。
- @Prop comboWindowMs：连击窗口时间。
- @State settled：动画状态控制。

动画与视觉效果
- **重大视觉优化**：实现了动态颜色进度和缩放动画。
- 连击动画：先跳到放大态（scale 1.45），再回弹到常尺寸。
- 颜色递进：金色 #F2D9A8 → 橙色 #E8A84A → 红色 #E06A5E。
- 光晕效果：根据连击数动态调整 textShadow 颜色。

**章节来源**
- [entry/src/main/ets/ui/ComboCounter.ets:1-67](file://entry/src/main/ets/ui/ComboCounter.ets#L1-L67)

### ActionToast 组件
职责
- 动作拒绝反馈浮层：技能冷却/体力不足/共鸣不足等拒绝原因即时提示。
- 让玩家明白按键为何没有生效，避免无效操作困惑。

状态与属性
- @Prop lastRejectReason：拒绝原因代码，@Watch('onRejectChanged') 监听变化。
- @State label：显示的提示文本。
- @State visible：可见性状态。
- private hideTimer：自动隐藏定时器。

动画与视觉效果
- **重大视觉优化**：改进了内边距、边框透明度、圆角和阴影效果。
- 样式：padding(18,8)，borderRadius(16)，border(color:'rgba(217,161,69,0.45)')。
- 阴影：shadow(radius:10,color:'#40000000',offsetY:3)。
- 文本阴影：textShadow(radius:6,color:'#80000000',offsetY:1)。
- 过渡动画：TransitionType.All，opacity 和 translate 动画。

**章节来源**
- [entry/src/main/ets/ui/ActionToast.ets:1-74](file://entry/src/main/ets/ui/ActionToast.ets#L1-L74)

### GamePage 与数据绑定
职责
- 加载模型与环境资源，启动/停止原生渲染。
- 定时拉取 Snapshot，同步到 @State，再透传给所有 UI 组件。
- 管理生命周期（aboutToAppear/aboutToDisappear），清理定时器与资源。

数据绑定与响应式更新
- 使用 setInterval 每 100ms 拉取一次快照，批量赋值给 @State，触发所有组件的 @Prop 更新。
- 对渲染能力与环境就绪状态进行判断，必要时显示错误提示。

```mermaid
flowchart TD
Start(["页面出现"]) --> LoadAssets["加载模型与环境资源"]
LoadAssets --> StartNative["nativeStartIfForeground()"]
StartNative --> Timer["setInterval 每100ms"]
Timer --> Pull["pullSnapshot()"]
Pull --> Sync["同步到 @State"]
Sync --> Render["所有组件基于 @Prop 渲染"]
Render --> Timer
Timer --> Stop(["页面消失"])
Stop --> ClearTimer["clearInterval"]
ClearTimer --> StopNative["nativeStop()"]
```

**图表来源**
- [entry/src/main/ets/pages/GamePage.ets:103-152](file://entry/src/main/ets/pages/GamePage.ets#L103-L152)
- [entry/src/main/ets/pages/GamePage.ets:219-293](file://entry/src/main/ets/pages/GamePage.ets#L219-L293)
- [entry/src/main/ets/napi/Bridge.ets:77-94](file://entry/src/main/ets/napi/Bridge.ets#L77-L94)

**章节来源**
- [entry/src/main/ets/pages/GamePage.ets:1-305](file://entry/src/main/ets/pages/GamePage.ets#L1-L305)
- [entry/src/main/ets/napi/Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)

### 触摸交互与手势识别（原生侧）
- TouchRouter：根据屏幕左右半区分配指针角色（移动/相机），拒绝重复或非法指针，支持 PointerDown/Move/Up/Cancel。
- VirtualJoystick：将拖拽位移转换为归一化向量，支持死区与最大半径配置。
- CameraGesture：累积滑动增量，按帧消费，支持顺序开始/结束。
- 输入转换：TryMapPointerAction 将上层类型映射为 InputAction，TryConvertFloat/Int32 保证数值安全。

```mermaid
flowchart TD
A["收到指针事件"] --> B{"坐标有效?"}
B -- 否 --> E["忽略"]
B -- 是 --> C{"PointerDown?"}
C -- 是 --> D["分配角色(左=移动,右=相机)"]
C -- 否 --> F{"PointerMove/Up/Cancel"}
F --> G["按角色分发到对应处理器"]
G --> H["输出移动向量/相机增量"]
```

**图表来源**
- [native/engine/input/touch_router.h:15-58](file://native/engine/input/touch_router.h#L15-L58)
- [native/engine/input/pointer_input.h:27-35](file://native/engine/input/pointer_input.h#L27-L35)
- [tests/test_touch_controls.cpp:8-110](file://tests/test_touch_controls.cpp#L8-L110)

**章节来源**
- [native/engine/input/touch_router.h:1-59](file://native/engine/input/touch_router.h#L1-L59)
- [native/engine/input/pointer_input.h:1-36](file://native/engine/input/pointer_input.h#L1-L36)
- [tests/test_touch_controls.cpp:1-111](file://tests/test_touch_controls.cpp#L1-L111)

## 依赖关系分析
- GamePage 依赖 Bridge 提供的 NAPI 接口，负责资源加载与快照拉取。
- 所有 UI 组件均被 GamePage 组合使用，Hud 和 CombatControls 为核心组件，其他为辅助反馈组件。
- 原生输入子系统独立于 UI，通过 NAPI 暴露 pushInput/pushAction 等方法供上层调用。

```mermaid
graph LR
GP["GamePage"] --> |调用| BR["Bridge.ets"]
CC["CombatControls"] --> |调用| BR
GP --> |传递@Prop| HUD["Hud"]
GP --> |传递@Prop| GSO["GameStateOverlay"]
GP --> |传递@Prop| HF["HitFeedback"]
GP --> |传递@Prop| CCO["ComboCounter"]
GP --> |传递@Prop| AT["ActionToast"]
BR --> |native_*| NATIVE["Native 引擎"]
NATIVE --> |输入事件| INP["输入子系统"]
```

**图表来源**
- [entry/src/main/ets/pages/GamePage.ets:154-217](file://entry/src/main/ets/pages/GamePage.ets#L154-L217)
- [entry/src/main/ets/ui/CombatControls.ets:5-40](file://entry/src/main/ets/ui/CombatControls.ets#L5-L40)
- [entry/src/main/ets/napi/Bridge.ets:77-94](file://entry/src/main/ets/napi/Bridge.ets#L77-L94)
- [native/engine/input/touch_router.h:15-58](file://native/engine/input/touch_router.h#L15-L58)

**章节来源**
- [entry/src/main/ets/pages/GamePage.ets:154-217](file://entry/src/main/ets/pages/GamePage.ets#L154-L217)
- [entry/src/main/ets/ui/CombatControls.ets:1-301](file://entry/src/main/ets/ui/CombatControls.ets#L1-L301)
- [entry/src/main/ets/napi/Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native/engine/input/touch_router.h:1-59](file://native/engine/input/touch_router.h#L1-L59)

## 性能考量
- 快照频率：默认 100ms 拉取一次，平衡了实时性与 CPU 占用。可根据设备性能调高间隔或采用增量更新策略。
- 渲染开销：所有组件仅做轻量展示，避免在 build 中进行复杂计算；条件渲染减少不必要的节点。
- 命中测试：HitTestMode.Transparent/Block 合理配置，避免 UI 层拦截触摸，降低额外事件处理成本。
- 资源加载：异步并行加载模型与环境资源，失败回退到程序化网格，保障稳定性。
- 内存与 GC：避免在高频回调中创建临时对象；尽量复用数组与对象。
- **动画优化**：合理使用 animateTo 和 Transition API，避免过度动画导致性能问题。

## 故障排查指南
- 渲染不可用：当 rendererReady=false 时，GamePage 显示 GLES 不支持提示，需重建模拟器或使用真机。
- 资源加载失败：若 nativeSetModelAssets/nativeSetEnvironmentAssets 返回 false，将记录错误日志并使用回退资源。
- 输入异常：TouchRouter 对非法坐标/无穷值进行过滤；检查指针 ID 是否已存在，避免重复分配角色。
- 调试面板：开启 showDebugHud/debugHud 后，观察 FPS、坐标、遭遇模式、冷却、窗口时间等指标定位问题。
- **动画问题**：检查 animateTo 配置是否正确，确保 duration、curve、iterations 参数合理。
- **状态同步**：验证 @Prop 数据类型是否与快照一致，避免类型转换错误。

**章节来源**
- [entry/src/main/ets/pages/GamePage.ets:168-181](file://entry/src/main/ets/pages/GamePage.ets#L168-L181)
- [entry/src/main/ets/pages/GamePage.ets:112-151](file://entry/src/main/ets/pages/GamePage.ets#L112-L151)
- [native/engine/input/touch_router.h:17-36](file://native/engine/input/touch_router.h#L17-L36)
- [entry/src/main/ets/ui/Hud.ets:174-208](file://entry/src/main/ets/ui/Hud.ets#L174-L208)

## 结论
UI 组件系统经过重大视觉优化，Hud 与 CombatControls 以 ArkUI 组件形态实现清晰的职责分离：前者专注状态展示，后者专注动作触发。新增的 GameStateOverlay、HitFeedback、ComboCounter、ActionToast 组件提供了丰富的视觉反馈和交互动画。通过 Bridge 与原生引擎解耦，配合稳定的快照拉取机制，实现了高效、可扩展的 UI 系统。原生输入子系统提供了健壮的触摸与手势处理能力，满足移动端多指交互需求。遵循本文的定制与优化建议，可在不同设备上获得一致的体验与良好的性能表现。

## 附录：使用与定制指南

### 组件使用示例
- 在 GamePage 中引入并实例化所有 UI 组件，设置 hitTestBehavior 为 Transparent 或 Block，避免遮挡渲染。
- 将 GamePage 的 @State 字段逐一赋给各组件的 @Prop，保持数据一致性。
- 在 CombatControls 中按需扩展按钮，调用 Bridge 暴露的方法驱动原生逻辑。

**章节来源**
- [entry/src/main/ets/pages/GamePage.ets:183-214](file://entry/src/main/ets/pages/GamePage.ets#L183-L214)
- [entry/src/main/ets/ui/CombatControls.ets:5-40](file://entry/src/main/ets/ui/CombatControls.ets#L5-L40)

### 自定义开发指南
- 新增 HUD 字段：在 Hud.ets 添加 @Prop，并在 GamePage 中同步快照字段，确保类型一致。
- 新增动作按钮：在 CombatControls.ets 添加 Button，并在 Bridge.ets 暴露对应的 native_* 方法。
- 样式主题：集中定义颜色与尺寸常量，替换硬编码值，便于全局换肤。
- 动画效果：可使用 ArkUI 的 Transition/Animation API 对进度条、文字淡入淡出等进行平滑过渡。
- **视觉优化**：参考现有组件的渐变背景、阴影效果、圆角设计，保持统一的视觉风格。

**章节来源**
- [entry/src/main/ets/ui/Hud.ets:1-217](file://entry/src/main/ets/ui/Hud.ets#L1-L217)
- [entry/src/main/ets/ui/CombatControls.ets:1-301](file://entry/src/main/ets/ui/CombatControls.ets#L1-L301)
- [entry/src/main/ets/napi/Bridge.ets:77-94](file://entry/src/main/ets/napi/Bridge.ets#L77-L94)

### 触摸交互与多设备适配
- 左侧半屏优先分配移动角色，右侧半屏分配相机角色，避免冲突。
- 虚拟摇杆支持死区与最大半径配置，适配不同手指力度与设备精度。
- 相机手势按帧消费增量，保证平滑旋转与缩放。
- 针对小屏设备可适当增大按钮尺寸与间距，提升触控可用性。
- **HitTestMode 优化**：根据组件功能选择合适的命中模式，Block 用于需要精确点击的按钮，Transparent 用于不影响底层交互的装饰层。

**章节来源**
- [native/engine/input/touch_router.h:15-58](file://native/engine/input/touch_router.h#L15-L58)
- [tests/test_touch_controls.cpp:8-110](file://tests/test_touch_controls.cpp#L8-L110)

### 性能优化技巧
- 降低快照频率或采用增量更新，减少主线程压力。
- 合并组件更新：仅在必要字段变化时触发重绘。
- 预编译样式与资源，减少运行时开销。
- 使用条件渲染隐藏调试面板，生产环境关闭非必要信息。
- **动画性能**：合理使用 animateTo 和 Transition，避免过度复杂的动画效果。
- **内存管理**：及时清理定时器（如 ActionToast 的 hideTimer），避免内存泄漏。