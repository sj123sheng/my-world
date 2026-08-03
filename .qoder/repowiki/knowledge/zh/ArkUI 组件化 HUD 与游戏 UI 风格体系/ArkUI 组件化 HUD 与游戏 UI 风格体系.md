---
kind: frontend_style
name: ArkUI 组件化 HUD 与游戏 UI 风格体系
category: frontend_style
scope:
    - '**'
source_files:
    - entry/src/main/ets/ui/Hud.ets
    - entry/src/main/ets/ui/CombatControls.ets
    - entry/src/main/ets/ui/ActionToast.ets
    - entry/src/main/ets/ui/GameStateOverlay.ets
    - entry/src/main/ets/ui/ComboCounter.ets
    - entry/src/main/ets/ui/HitFeedback.ets
    - entry/src/main/ets/ui/Joystick.ets
    - entry/src/main/ets/ui/PauseMenu.ets
    - entry/src/main/ets/ui/TargetFrame.ets
    - entry/src/main/ets/ui/Tutorial.ets
    - entry/src/main/resources/base/element/color.json
    - entry/src/main/resources/base/element/string.json
    - AppScope/resources/base/element/string.json
---

本仓库的前端样式体系基于 HarmonyOS ArkUI（ETS）构建，采用声明式组件架构，所有 UI 均以 `@Component` 结构体定义，通过 `.ets` 文件内联样式描述视觉表现。3D 渲染由 C++ 原生层通过 EGL+GLES3 完成，UI 层仅负责 HUD、战斗控件、提示浮层等 2D 叠加界面。

**样式方法与工具链**
- 不使用 CSS/SCSS/Tailwind 等 Web 样式方案，全部使用 ArkUI 声明式 API（如 `.backgroundColor()`、`.border()`、`.fontColor()`、`.textShadow()`、`.animation()`）直接描述样式。
- 颜色以十六进制字面量硬编码在组件中（如 `#1A1A2E`、`#D9A145`、`#AB4F92`），仅在 `entry/src/main/resources/base/element/color.json` 中定义了启动页背景色 `start_window_background: #1A1A2E`，其余颜色未集中管理。
- 字体统一使用系统默认字体族，字号在 10–34 之间按层级分布，字重使用 `FontWeight.Medium/Bold`，字间距通过 `letterSpacing` 控制。
- 动画通过 ArkUI 内置 `.animation()` 和 `animateTo()` API 实现，曲线使用 `Curve.EaseInOut/EaseOut/Linear`，时长从 100ms 到 650ms 不等。

**核心 UI 组件与职责划分**
- `Hud.ets`：底部 HUD，包含血条、韧性条、体力条、共鸣槽（辉/流/蚀）、Boss 进度条及调试面板，背景为半透明深色 `#8A11181D`，圆角 18px。
- `CombatControls.ets`：右下角弧形技能按钮组，含普攻、闪避、三种元素技能（辉印/脉流/蚀质）及终结技，每个按钮带环形冷却遮罩，颜色按技能类型区分（金/青/紫）。
- `ActionToast.ets`：动作拒绝反馈浮层，显示“无目标”“技能冷却中”等中文提示，金色边框 `rgba(217,161,69,0.4)`，900ms 自动消失。
- `GameStateOverlay.ets`：遭遇结算遮罩，失败时红色主题（`#E06A5E`），胜利时金色主题（`#F2D9A8`），半透明黑色背景阻断下层交互。
- `ComboCounter.ets`、`HitFeedback.ets`、`Joystick.ets`、`PauseMenu.ets`、`TargetFrame.ets`、`Tutorial.ets`：其他 HUD 子组件，分别处理连击计数、受击反馈、虚拟摇杆、暂停菜单、目标框、教程引导。

**设计约定与约束**
- 所有 UI 组件均设置 `hitTestBehavior(HitTestMode.Transparent|Block|None)` 精确控制触摸穿透行为，HUD 类组件普遍使用 `Transparent` 避免遮挡 3D 渲染。
- 文本阴影统一使用 `textShadow({ radius, color: '#B0000000', offsetX: 0, offsetY: 2 })` 模式，确保文字在任意背景上可读。
- 按钮尺寸遵循固定规格：主操作按钮 76×76，技能按钮 48×48，辅助按钮 40×40，圆角半径 8–18px。
- 布局锚点通过 `markAnchor()` 精确定位，如技能组以 `(278, 244)` 为锚点贴右下角。
- 字符串资源集中在 `AppScope/resources/base/element/string.json` 和 `entry/src/main/resources/base/element/string.json`，但 UI 文案（如“普攻”“终结”“战败”）仍大量硬编码在 ETS 文件中，未完全抽取。
- 无响应式适配逻辑，所有尺寸以绝对像素或百分比指定，未针对不同屏幕密度做媒体查询。

**资源组织**
- 颜色资源：`entry/src/main/resources/base/element/color.json`（仅 1 项）
- 字符串资源：`AppScope/resources/base/element/string.json`（app_name、shared_desc）与 `entry/src/main/resources/base/element/string.json`（module_desc、EntryAbility_*）
- 媒体资源：`entry/src/main/resources/base/media/app_icon.svg`
- 无独立主题文件或样式变量模块，视觉一致性依赖开发者对颜色/字号/边距的经验复用。