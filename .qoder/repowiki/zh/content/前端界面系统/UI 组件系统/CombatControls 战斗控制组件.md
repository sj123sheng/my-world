# CombatControls 战斗控制组件

<cite>
**本文引用的文件**   
- [CombatControls.ets](file://entry/src/main/ets/ui/CombatControls.ets)
- [GamePage.ets](file://entry/src/main/ets/pages/GamePage.ets)
- [Bridge.ets](file://entry/src/main/ets/napi/Bridge.ets)
- [native_bridge.cpp](file://entry/src/main/cpp/native_bridge.cpp)
- [Joystick.ets](file://entry/src/main/ets/ui/Joystick.ets)
- [Hud.ets](file://entry/src/main/ets/ui/Hud.ets)
- [input_event.h](file://native/engine/input/input_event.h)
- [pointer_input.h](file://native/engine/input/pointer_input.h)
- [changed_pointer_forwarder.h](file://native/engine/input/changed_pointer_forwarder.h)
- [touch_router.h](file://native/engine/input/touch_router.h)
</cite>

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
10. [附录：集成与配置示例](#附录集成与配置示例)

## 简介
本技术文档围绕 my-world 的 CombatControls 战斗控制组件，系统阐述其触摸交互实现、事件处理机制、与 C++ 引擎的 NAPI 桥接通信、设备适配策略，以及在 GamePage 中的集成方式。该组件负责呈现并响应普攻、闪避、技能按键（辉印、脉流、蚀质）以及终结技等按钮，配合虚拟摇杆完成移动输入，并通过 NAPI 将用户操作传递给原生游戏逻辑层。

## 项目结构
- UI 层
  - CombatControls.ets：战斗控制按钮布局与冷却动画、调试面板入口。
  - Joystick.ets：左侧半屏虚拟摇杆，捕获触摸并转发坐标到原生。
  - Hud.ets：HUD 信息展示，包含状态条、目标信息、调试信息等。
  - GamePage.ets：页面容器，承载 XComponent 渲染面、Joystick、Hud、CombatControls，并定时拉取快照驱动 UI 更新。
- 桥接层
  - Bridge.ets：NAPI 接口封装，暴露 pushAction/pushInput/pullSnapshot 等方法。
  - native_bridge.cpp：N-API 绑定，接收 JS 调用，校验参数，入队输入或触发游戏流程控制。
- 原生输入子系统
  - input_event.h：定义 InputAction 枚举与 InputEvent 结构体。
  - pointer_input.h：类型转换与 PointerAction 映射。
  - changed_pointer_forwarder.h：通用指针事件转发器。
  - touch_router.h：多点触控角色分配（左屏移动、右屏相机）。

```mermaid
graph TB
subgraph "UI 层"
GC["CombatControls.ets"]
GJ["Joystick.ets"]
GH["Hud.ets"]
GP["GamePage.ets"]
end
subgraph "桥接层"
B["Bridge.ets"]
NB["native_bridge.cpp"]
end
subgraph "原生输入子系统"
IE["input_event.h"]
PI["pointer_input.h"]
CPF["changed_pointer_forwarder.h"]
TR["touch_router.h"]
end
GP --> GC
GP --> GJ
GP --> GH
GC --> B
GJ --> B
B --> NB
NB --> IE
NB --> PI
NB --> CPF
NB --> TR
```

**图表来源** 
- [CombatControls.ets:1-248](file://entry/src/main/ets/ui/CombatControls.ets#L1-L248)
- [Joystick.ets:1-170](file://entry/src/main/ets/ui/Joystick.ets#L1-L170)
- [Hud.ets:1-145](file://entry/src/main/ets/ui/Hud.ets#L1-L145)
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)
- [input_event.h:1-24](file://native/engine/input/input_event.h#L1-L24)
- [pointer_input.h:1-36](file://native/engine/input/pointer_input.h#L1-L36)
- [changed_pointer_forwarder.h:1-12](file://native/engine/input/changed_pointer_forwarder.h#L1-L12)
- [touch_router.h:1-59](file://native/engine/input/touch_router.h#L1-L59)

**章节来源**
- [CombatControls.ets:1-248](file://entry/src/main/ets/ui/CombatControls.ets#L1-L248)
- [Joystick.ets:1-170](file://entry/src/main/ets/ui/Joystick.ets#L1-L170)
- [Hud.ets:1-145](file://entry/src/main/ets/ui/Hud.ets#L1-L145)
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)

## 核心组件
- CombatControls：右下角弧形技能按钮簇，包含普攻、闪避、辉印、脉流、蚀质、终结技；提供冷却环遮罩与呼吸动画；右上角调试入口，展开后提供多种测试功能。
- Joystick：左侧半屏虚拟摇杆，按下生成底盘与旋钮，移动时计算位移并转换为原生半径下的满量程值，通过 pushInput 转发。
- GamePage：页面容器，使用 XComponent 作为渲染面，叠加 Joystick、Hud、CombatControls，并以固定间隔拉取快照驱动 UI 状态同步。
- Bridge：NAPI 方法封装，统一暴露 pushAction/pushInput/pullSnapshot 等能力。
- native_bridge.cpp：N-API 绑定，负责参数校验、类型转换、事件入队、资源提交、生命周期回调注册。

**章节来源**
- [CombatControls.ets:1-248](file://entry/src/main/ets/ui/CombatControls.ets#L1-L248)
- [Joystick.ets:1-170](file://entry/src/main/ets/ui/Joystick.ets#L1-L170)
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)

## 架构总览
CombatControls 与 Joystick 通过 Bridge 调用 N-API 方法，将用户输入事件推送到 native_bridge.cpp，再由 Loop 入队至原生输入子系统。TouchRouter 根据屏幕左右侧分配指针角色（移动/相机），避免手势冲突。GamePage 定时 pullSnapshot 获取原生状态，驱动 HUD 与控制按钮的视觉反馈（如冷却时间、终结窗口）。

```mermaid
sequenceDiagram
participant U as "用户"
participant CC as "CombatControls.ets"
participant BJ as "Bridge.ets"
participant NB as "native_bridge.cpp"
participant L as "Loop(原生)"
participant TR as "TouchRouter"
U->>CC : 点击“普攻”
CC->>BJ : pushAction(0)
BJ->>NB : pushAction(type=0)
NB->>NB : 校验参数/映射 InputAction : : Attack
NB->>L : enqueueInput(Attack, -1, 0, 0)
L-->>TR : 分派输入事件
Note over L,TR : 非指针动作不经过 TouchRouter
U->>GJ : 左侧触摸移动
GJ->>BJ : pushInput({type, pointerId, x, y})
BJ->>NB : pushInput(event)
NB->>NB : TryMapPointerAction -> InputAction : : PointerMove
NB->>L : enqueueInput(PointerMove, id, x, y)
L->>TR : handle(event, width, height)
TR-->>L : 返回角色(Movement/Camera/Ignored)
```

**图表来源** 
- [CombatControls.ets:74-167](file://entry/src/main/ets/ui/CombatControls.ets#L74-L167)
- [Joystick.ets:24-86](file://entry/src/main/ets/ui/Joystick.ets#L24-L86)
- [Bridge.ets:85-94](file://entry/src/main/ets/napi/Bridge.ets#L85-L94)
- [native_bridge.cpp:252-275](file://entry/src/main/cpp/native_bridge.cpp#L252-L275)
- [native_bridge.cpp:212-250](file://entry/src/main/cpp/native_bridge.cpp#L212-L250)
- [pointer_input.h:27-35](file://native/engine/input/pointer_input.h#L27-L35)
- [touch_router.h:17-36](file://native/engine/input/touch_router.h#L17-L36)

## 详细组件分析

### CombatControls 组件
- 布局与交互
  - 右下角弧形按钮簇：普攻（主按钮）、闪避（左侧）、辉印/脉流/蚀质（扇形展开）、终结技（普攻上方）。
  - 每个技能按钮带有圆形样式、边框、字体颜色与状态效果；点击时调用 pushAction 传入对应动作类型。
  - 冷却环遮罩：当剩余冷却时间大于 0 时，以环形进度覆盖按钮，显示倒计时秒数，颜色与技能主题一致。
  - 终结技呼吸动画：当终结窗口开启时，按钮按周期缩放，提示可用。
  - 调试面板：右上角小圆钮切换，展开后可快速启动不同遭遇模式、推进关卡、补给、重试首领、切换调试 HUD。
- 数据绑定
  - 通过 @Prop 接收 radianceCooldownMs、currentCooldownMs、corruptionCooldownMs、ultimateWindowMs，驱动按钮背景色与冷却环。
  - 内部 @State debugOpen 控制调试面板显隐；@State ultimatePulse 控制呼吸动画开关。
- 事件处理
  - 所有按钮 onClick 直接调用 pushAction(type)，由 Bridge 转交 N-API 层。
  - 冷却环使用 Progress(Ring) 与 animation 线性过渡，保证平滑收缩。
- 可访问性与命中测试
  - 外层 Stack 设置 hitTestBehavior(HitTestMode.Transparent)，确保按钮区域不被遮挡影响命中。

```mermaid
flowchart TD
Start(["按钮点击"]) --> CheckType{"动作类型?"}
CheckType --> |普攻| Push0["pushAction(0)"]
CheckType --> |闪避| Push1["pushAction(1)"]
CheckType --> |辉印| Push2["pushAction(2)"]
CheckType --> |脉流| Push3["pushAction(3)"]
CheckType --> |蚀质| Push4["pushAction(4)"]
CheckType --> |终结| Push5["pushAction(5)"]
Push0 --> End(["进入原生队列"])
Push1 --> End
Push2 --> End
Push3 --> End
Push4 --> End
Push5 --> End
```

**图表来源** 
- [CombatControls.ets:74-167](file://entry/src/main/ets/ui/CombatControls.ets#L74-L167)

**章节来源**
- [CombatControls.ets:1-248](file://entry/src/main/ets/ui/CombatControls.ets#L1-L248)

### 虚拟摇杆（Joystick）
- 触摸捕获
  - 左侧半屏透明 Stack 捕获 onTouch，Down/Move/Up/Cancel 均被处理。
  - Down：记录 trackedPointer、originX/Y，初始化底盘与旋钮位置，active=true，并 forward(DOWN)。
  - Move：计算 dx/dy 与长度，限制在 KNOB_TRAVEL 内，换算为原生半径 NATIVE_RADIUS 下的坐标，forward(MOVE)。
  - Up/Cancel：forward(UP/CANCEL)，重置 trackedPointer 与 active。
- 数值映射
  - 视觉行程 KNOB_TRAVEL=50vp，原生半径 NATIVE_RADIUS=100，确保旋钮拉满即原生满速。
  - 死区：原生 VirtualJoystickConfig.deadZone=0.1f，小于死区则输出空值。
- 事件转发
  - ArkUI TouchType(0=Down/1=Up/2=Move/3=Cancel) 与原生 InputAction 序号一致，直接 pushInput。
- 视觉表现
  - 空闲提示淡出，底盘与旋钮跟随手指移动，带阴影与渐变填充。

```mermaid
classDiagram
class Joystick {
+bool active
+number baseCenterX
+number baseCenterY
+number knobCenterX
+number knobCenterY
-number trackedPointer
-number originX
-number originY
+handleTouch(event) void
+forward(type, pointerId, x, y) void
}
```

**图表来源** 
- [Joystick.ets:14-86](file://entry/src/main/ets/ui/Joystick.ets#L14-L86)

**章节来源**
- [Joystick.ets:1-170](file://entry/src/main/ets/ui/Joystick.ets#L1-L170)
- [pointer_input.h:27-35](file://native/engine/input/pointer_input.h#L27-L35)

### GamePage 集成与快照驱动
- 渲染面
  - XComponent(id='gameSurface', type='surface', libraryname='native_game') 作为原生渲染面。
- 组件叠加
  - Joystick、Hud、CombatControls 均以 HitTestMode.Transparent 叠加于渲染面之上，互不拦截。
- 快照驱动
  - setInterval 每 100ms 调用 pullSnapshot()，将原生状态映射到 @State 字段，驱动 HUD 与控制按钮的视觉反馈。
  - 关键字段包括 radianceCooldownMs、currentCooldownMs、corruptionCooldownMs、ultimateWindowMs 等，用于 CombatControls 的冷却与终结窗显示。

```mermaid
sequenceDiagram
participant GP as "GamePage.ets"
participant BR as "Bridge.ets"
participant NB as "native_bridge.cpp"
participant L as "Loop(原生)"
loop 每 100ms
GP->>BR : pullSnapshot()
BR->>NB : pullSnapshot()
NB->>L : snapshot()
L-->>NB : GameSnapshot
NB-->>BR : Snapshot(JSON)
BR-->>GP : Snapshot
GP->>GP : 更新 @State 字段
GP->>GC : 传递冷却/窗口属性
GP->>GH : 更新 HUD 显示
end
```

**图表来源** 
- [GamePage.ets:226-312](file://entry/src/main/ets/pages/GamePage.ets#L226-L312)
- [Bridge.ets:93-94](file://entry/src/main/ets/napi/Bridge.ets#L93-L94)
- [native_bridge.cpp:360-516](file://entry/src/main/cpp/native_bridge.cpp#L360-L516)

**章节来源**
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)

### 事件处理机制与优先级管理
- 多点触控识别
  - Joystick 仅处理 trackedPointer 对应的触摸，忽略其他指针，避免多指干扰。
  - TouchRouter 维护 roles_ 映射，按屏幕左右侧分配 Movement/Camera 角色，若已有相同角色则返回 Ignored，防止冲突。
- 手势冲突处理
  - 左侧半屏透明 Stack 捕获移动触摸，右侧透传到 XComponent 原生回调，相机控制不受影响。
  - 非指针动作（攻击/闪避/技能）通过 pushAction 直接入队，不经过 TouchRouter，优先级高于指针事件。
- 输入优先级
  - 按钮动作 > 指针移动（因为按钮动作不依赖 TouchRouter，且直接入队）。
  - 指针移动中，TouchRouter 决定是否接受该指针（Movement/Camera），否则忽略。

```mermaid
flowchart TD
A["输入到达"] --> B{"是否为按钮动作?"}
B --> |是| C["pushAction -> 原生队列"]
B --> |否| D{"是否为指针事件?"}
D --> |是| E["TouchRouter 分配角色"]
E --> F{"角色有效?"}
F --> |是| G["enqueueInput(Pointer*)"]
F --> |否| H["忽略"]
D --> |否| I["丢弃"]
```

**图表来源** 
- [touch_router.h:17-36](file://native/engine/input/touch_router.h#L17-L36)
- [native_bridge.cpp:252-275](file://entry/src/main/cpp/native_bridge.cpp#L252-L275)
- [native_bridge.cpp:212-250](file://entry/src/main/cpp/native_bridge.cpp#L212-L250)

**章节来源**
- [Joystick.ets:24-86](file://entry/src/main/ets/ui/Joystick.ets#L24-L86)
- [touch_router.h:1-59](file://native/engine/input/touch_router.h#L1-L59)
- [native_bridge.cpp:212-275](file://entry/src/main/cpp/native_bridge.cpp#L212-L275)

### 与 C++ 引擎的通信（NAPI 桥接）
- 接口封装
  - Bridge.ets 导出 pushAction、pushInput、pullSnapshot 等函数，供 UI 调用。
- 参数校验与映射
  - native_bridge.cpp 对 pushAction 的类型进行范围校验（0~5），映射为 InputAction 枚举。
  - pushInput 要求对象包含 type/pointerId/x/y，并进行类型转换与有限性检查。
- 事件入队
  - 按钮动作：enqueueInput(action, -1, 0.0f, 0.0f)。
  - 指针事件：ForwardChangedPointer -> TryMapPointerAction -> enqueueInput(action, pointerId, x, y)。
- 资源与生命周期
  - nativeSetModelAssets/nativeSetEnvironmentAssets 提交模型与环境资源。
  - OnSurfaceCreated/OnSurfaceChanged/OnSurfaceDestroyed 管理渲染面生命周期。

```mermaid
classDiagram
class Bridge {
+pushAction(type) void
+pushInput(event) void
+pullSnapshot() Snapshot
}
class NativeBridge {
+NativePushAction(env, info) napi_value
+NativePushInput(env, info) napi_value
+NativePullSnapshot(env, info) napi_value
+OnDispatchTouchEvent(...)
}
class InputEvent {
+action : InputAction
+pointerId : int32
+x : float
+y : float
+sequence : uint64
}
Bridge --> NativeBridge : "N-API 调用"
NativeBridge --> InputEvent : "构造/入队"
```

**图表来源** 
- [Bridge.ets:85-94](file://entry/src/main/ets/napi/Bridge.ets#L85-L94)
- [native_bridge.cpp:252-275](file://entry/src/main/cpp/native_bridge.cpp#L252-L275)
- [native_bridge.cpp:212-250](file://entry/src/main/cpp/native_bridge.cpp#L212-L250)
- [input_event.h:1-24](file://native/engine/input/input_event.h#L1-L24)

**章节来源**
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)
- [input_event.h:1-24](file://native/engine/input/input_event.h#L1-L24)

### 设备适配策略
- 屏幕尺寸适配
  - CombatControls 使用百分比定位与 markAnchor，确保在不同分辨率下按钮相对位置稳定。
  - Joystick 使用 vp 单位与固定比例（KNOB_TRAVEL/NATIVE_RADIUS），保证视觉与原生速度一致。
- 触摸区域优化
  - 左侧半屏透明 Stack 捕获移动触摸，右侧透传相机控制，避免重叠冲突。
  - 按钮区域通过 hitTestBehavior(HitTestMode.Transparent) 提升命中容错。
- 响应式设计
  - 冷却环与呼吸动画基于 CSS-like 动画，随状态变化平滑过渡。
  - HUD 与调试面板采用自适应布局，文本与进度条宽度百分比化。

**章节来源**
- [CombatControls.ets:53-177](file://entry/src/main/ets/ui/CombatControls.ets#L53-L177)
- [Joystick.ets:117-126](file://entry/src/main/ets/ui/Joystick.ets#L117-L126)
- [Hud.ets:57-145](file://entry/src/main/ets/ui/Hud.ets#L57-L145)

## 依赖关系分析
- UI 层依赖 Bridge 暴露的 N-API 方法。
- Bridge 依赖 libnative_game.so 提供的原生函数。
- native_bridge.cpp 依赖 Loop、Surface、Input 子系统。
- TouchRouter 依赖 InputEvent 与屏幕尺寸，负责指针角色分配。
- GamePage 依赖所有 UI 组件与 Bridge，承担状态同步职责。

```mermaid
graph LR
GC["CombatControls.ets"] --> B["Bridge.ets"]
GJ["Joystick.ets"] --> B
GP["GamePage.ets"] --> GC
GP --> GJ
GP --> GH["Hud.ets"]
B --> NB["native_bridge.cpp"]
NB --> L["Loop(原生)"]
NB --> TR["touch_router.h"]
NB --> IE["input_event.h"]
```

**图表来源** 
- [CombatControls.ets:1-248](file://entry/src/main/ets/ui/CombatControls.ets#L1-L248)
- [Joystick.ets:1-170](file://entry/src/main/ets/ui/Joystick.ets#L1-L170)
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)
- [touch_router.h:1-59](file://native/engine/input/touch_router.h#L1-L59)
- [input_event.h:1-24](file://native/engine/input/input_event.h#L1-L24)

**章节来源**
- [GamePage.ets:1-312](file://entry/src/main/ets/pages/GamePage.ets#L1-L312)
- [Bridge.ets:1-94](file://entry/src/main/ets/napi/Bridge.ets#L1-L94)
- [native_bridge.cpp:1-577](file://entry/src/main/cpp/native_bridge.cpp#L1-L577)

## 性能考量
- 快照频率：GamePage 每 100ms 拉取一次快照，平衡刷新率与 CPU 占用。
- 动画开销：冷却环与呼吸动画使用轻量级 CSS-like 动画，避免复杂计算。
- 输入路径：按钮动作直接入队，指针事件经 ForwardChangedPointer 与 TouchRouter，路径短且无额外拷贝。
- 内存与资源：模型与环境资源通过 ArrayBuffer 提交，避免频繁分配。

[本节为通用指导，不直接分析具体文件]

## 故障排查指南
- 按钮无响应
  - 检查 pushAction 调用是否正确传入类型（0~5）。
  - 确认 Bridge 已正确导入并加载 libnative_game.so。
- 摇杆无效
  - 确认左侧半屏透明 Stack 未被遮挡。
  - 检查 TouchType 与 InputAction 映射是否一致（0=Down/1=Up/2=Move/3=Cancel）。
- 快照未更新
  - 检查 setInterval 是否正常运行。
  - 确认 pullSnapshot 返回值字段完整。
- 原生崩溃
  - 查看 OH_LOG_Print 日志，关注 surface_init/resize 失败与参数校验错误。

**章节来源**
- [native_bridge.cpp:252-275](file://entry/src/main/cpp/native_bridge.cpp#L252-L275)
- [native_bridge.cpp:212-250](file://entry/src/main/cpp/native_bridge.cpp#L212-L250)
- [GamePage.ets:226-312](file://entry/src/main/ets/pages/GamePage.ets#L226-L312)

## 结论
CombatControls 组件通过清晰的 UI 分层、稳健的 NAPI 桥接与原生输入子系统协作，实现了移动端战斗控制的完整闭环。其事件处理机制兼顾多点触控识别与手势冲突处理，设备适配策略确保在不同屏幕尺寸下的一致体验。结合 GamePage 的快照驱动，UI 能够实时反映游戏状态，提供流畅的用户交互。

[本节为总结，不直接分析具体文件]

## 附录：集成与配置示例
- 在 GamePage 中使用 CombatControls
  - 引入 CombatControls 组件，传入 radianceCooldownMs、currentCooldownMs、corruptionCooldownMs、ultimateWindowMs 等属性。
  - 设置 hitTestBehavior(HitTestMode.Transparent) 避免遮挡。
- 自定义按键映射
  - 修改 CombatControls 中各按钮 onClick 调用的 pushAction(type)，确保与原生 InputAction 映射一致。
- 布局调整
  - 调整按钮 position 与 markAnchor，适应不同屏幕比例。
  - 调整 Joystick 的 BASE_SIZE/KNOB_TRAVEL/NATIVE_RADIUS 以匹配手感。

**章节来源**
- [GamePage.ets:215-221](file://entry/src/main/ets/pages/GamePage.ets#L215-L221)
- [CombatControls.ets:74-167](file://entry/src/main/ets/ui/CombatControls.ets#L74-L167)
- [Joystick.ets:6-12](file://entry/src/main/ets/ui/Joystick.ets#L6-L12)