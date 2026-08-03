---
kind: design
name: 采用动态浮出摇杆与 ArkUI 触摸路由
source: session
category: adr
---

# 采用动态浮出摇杆与 ArkUI 触摸路由

_来源：eadedab → fad818d 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
原操控层为开发态：移动是无视觉反馈的隐形触摸区，战斗按钮是默认灰色方块堆叠，10 个调试按钮裸露在游戏画面上。需要把操控体验提升到商业手游水准，且必须保持与现有 native TouchRouter/VirtualJoystick 兼容。

## 决策驱动
- 纯 ArkTS 改造不改动 native 逻辑
- 左半屏移动/右半屏相机手势并发处理
- 旋钮视觉行程与 native 速度值严格一致
- 保留 Button+onClick 契约测试兼容性

## 备选方案
- **全屏透明触摸面板 + 虚拟摇杆组件** — 优点：可精确控制触摸区域、提供视觉反馈、与 native 坐标系统对齐
- **直接修改 native 触摸路由** _（已否决）_ — 优点：性能可能更好；缺点：破坏现有桥接、增加 native 改动风险、违背本轮纯 ArkTS 目标

## 决策
新建 Joystick.ets 组件，根 Stack 使用 HitTestMode.Transparent 透传右半屏触摸给 XComponent 原生回调；左半屏触摸面板捕获 Down/Move/Up 事件，经 pushInput 转发换算后的坐标（50vp 视觉行程映射到 native 半径 100），保证旋钮拉满=满速。CombatControls.ets 重写技能按钮区，保留 Button('标签')+onClick 配对模式以兼容契约测试。

## 影响
触摸路由完全在 ArkUI 层实现，native 的 TouchRouter/VirtualJoystick 无需改动。左/右半屏手势互不干扰，冷却环与终结技高亮等 UI 状态通过 @Prop 从 GamePage 注入。后续振动反馈和打击感包装可独立迭代。