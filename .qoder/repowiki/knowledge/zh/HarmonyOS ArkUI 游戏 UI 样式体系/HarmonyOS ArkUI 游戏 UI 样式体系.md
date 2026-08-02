---
kind: frontend_style
name: HarmonyOS ArkUI 游戏 UI 样式体系
category: frontend_style
scope:
    - '**'
source_files:
    - entry/src/main/ets/ui/Hud.ets
    - entry/src/main/ets/ui/CombatControls.ets
    - entry/src/main/ets/ui/Joystick.ets
    - entry/src/main/resources/base/element/color.json
    - AppScope/app.json5
---

该仓库为 HarmonyOS 原生游戏项目，前端样式完全基于 ArkTS + ArkUI 声明式组件构建，未使用 CSS/SCSS/Tailwind 等 Web 样式方案。UI 样式集中在 entry/src/main/ets/ui 目录下的三个核心组件中：

**样式系统与主题策略**
- 采用 ArkUI 内联样式（.style()、.color()、.backgroundColor()、.border()、.shadow()、.linearGradient() 等）与组件属性直接绑定，无独立样式文件
- 颜色值以十六进制硬编码为主（如 #D6E2DF、#43CDB5、#AB4F92），仅在 AppScope/app.json5 和 resources/base/element/color.json 中定义少量应用级资源（如 start_window_background: #1A1A2E）
- 通过 @Prop/@State 装饰器实现数据驱动样式更新，如冷却环遮罩根据 remainingMs/totalMs 动态计算 Progress 进度条百分比

**组件化布局约定**
- Hud.ets：HUD 信息面板，使用 Column/Row/Progress 组合展示 HP/Poise/Stamina 三态进度条，背景色 #8A11181D（半透明深灰），圆角 18vp
- CombatControls.ets：右下角弧形技能按钮组，通过 Stack + position/markAnchor 精确定位五个圆形按钮（普攻/闪避/辉印/脉流/蚀质），使用 ButtonStyleType.Circle 统一圆形样式
- Joystick.ets：左下角虚拟摇杆，BASE_SIZE=120vp、KNOB_SIZE=52vp、KNOB_TRAVEL=50vp，旋钮使用 linearGradient 渐变填充（#8FE0D0→#2E8F81）

**动画与交互风格**
- 使用 animateTo API 实现呼吸脉冲效果（终结技窗口开启时缩放 1.08x，duration 650ms，iterations=-1 循环）
- 所有可交互元素启用 stateEffect(true) 提供按压反馈
- 调试面板默认隐藏，通过右上角 ☰ 按钮展开，避免干扰游戏画面

**设计约束与规范**
- 所有 UI 层设置 hitTestBehavior(HitTestMode.Transparent) 确保触摸事件穿透到下层 XComponent 渲染
- 移动端适配：左半屏捕获移动触摸，右半屏透传相机控制，符合移动端手势操作习惯
- 视觉层级：HUD 使用半透明背景（#8A11181D）、文字阴影（textShadow radius: 8, color: #B0000000）增强可读性
- 性能优化：Debug HUD 默认关闭，通过 toggleDebugHud() 按需开启