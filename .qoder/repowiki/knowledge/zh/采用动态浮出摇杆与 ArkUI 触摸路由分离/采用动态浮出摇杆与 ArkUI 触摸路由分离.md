---
kind: design
name: 采用动态浮出摇杆与 ArkUI 触摸路由分离
source: session
category: adr
---

# 采用动态浮出摇杆与 ArkUI 触摸路由分离

_来源：caf1efe → eadedab 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
原操控层为开发态：移动是无视觉反馈的隐形触摸区，战斗按钮是默认灰色方块堆叠，10 个调试按钮裸露在游戏画面上。需要达到商业手游水准的操控体验，同时保持 native 的 TouchRouter/VirtualJoystick 逻辑不变。

## 决策驱动
- 纯 ArkTS 改造不触碰 native 逻辑
- 左/右半屏触摸职责清晰分离
- 旋钮视觉行程与 native 速度值严格一致

## 备选方案
- **动态浮出摇杆 + HitTestMode.Transparent 透传** — 优点：左半屏由 ArkUI 捕获并转发 pushInput，右半屏透传给 XComponent 处理相机手势；无侵入性改动 native 代码；数值换算保证满拉=满速
- **在 XComponent 原生层实现摇杆** _（已否决）_ — 优点：与渲染同层，性能可能更优；缺点：需要修改 native 代码，破坏现有 TouchRouter 契约，迁移成本高

## 决策
新建 Joystick.ets 组件，根 Stack 使用 hitTestBehavior(HitTestMode.Transparent)，左半宽触摸面板捕获 Down/Move/Up 事件并通过 pushInput 转发原始坐标；右半屏透明透传继续由 XComponent 原生回调处理相机手势。位移按 m=min(|d|,50vp) 换算为 origin+dir*(m/50*100) 以匹配 native 摇杆半径 100、死区 0.1 的约定。

## 影响
GamePage Stack 中插入 Joystick 层（XComponent 之上、Hud 之下）；CombatControls 重写为弧形技能按钮布局并收纳调试面板；测试断言新增对 Joystick 存在及 pushInput 转发的验证。下一轮需补充振动反馈并将冷却时长下沉到快照消除 UI 硬编码。