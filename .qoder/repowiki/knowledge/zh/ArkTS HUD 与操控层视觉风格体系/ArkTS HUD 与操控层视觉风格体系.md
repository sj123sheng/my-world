---
kind: frontend_style
name: ArkTS HUD 与操控层视觉风格体系
category: frontend_style
scope:
    - '**'
source_files:
    - entry/src/main/ets/ui/Hud.ets
    - entry/src/main/ets/ui/CombatControls.ets
    - entry/src/main/ets/ui/Joystick.ets
    - entry/src/main/ets/ui/PauseMenu.ets
    - entry/src/main/resources/base/element/color.json
---

本项目基于 HarmonyOS ArkTS/ArkUI 构建移动端游戏的 HUD 与操控层，采用纯声明式组件 + 内联样式的方式实现视觉表现，未引入 CSS/SCSS/Tailwind 等外部样式系统。整体风格围绕“深色游戏 UI、半透明磨砂质感、渐变高亮与呼吸动画”展开。

**样式系统与主题**
- 颜色资源集中定义在 `entry/src/main/resources/base/element/color.json`，目前仅包含启动背景色 `start_window_background: #1A1A2E`，其余颜色均以硬编码十六进制值或 `rgba()` 形式直接写在组件中。
- 所有 UI 组件（Hud、CombatControls、Joystick、PauseMenu、ActionToast、ComboCounter、HitFeedback、TargetFrame、Tutorial）均使用 ArkUI 的 `.color()`、`.backgroundColor()`、`.border()`、`.shadow()`、`.linearGradient()`、`.textShadow()` 等链式 API 进行内联样式设置，无独立样式文件。
- 色彩体系以深灰蓝底色（`#1A1A2E`、`#201A1D`、`rgba(20,26,30,0.5)`）为基底，配合三类源技能专属色：辉（Radiance）金色 `#D9A145/#C8A55F`、流（Flow）青色 `#43CDB5/#5EAB9E`、蚀（Corruption）紫色 `#AB4F92/#AB6A9C`，以及普攻/闪避的青绿色调 `#2F4858/#5B8FA8`。

**组件布局与视觉约定**
- 摇杆（Joystick）：左下角浮出式虚拟摇杆，按下时从触点生成底盘与旋钮，空闲状态显示淡色提示；旋钮使用青绿渐变 `['#A8F0E4','#4DB8A8','#2E8F81']`，行程限制为 50vp。
- 战斗按钮（CombatControls）：右下角弧形扇形布局，普攻主按钮贴底角（76px），三个源技能按钮（48px）沿左上弧线排列，终结技按钮位于普攻上方（56px）。每个按钮通过 `Progress(Ring)` 叠加冷却遮罩，剩余时间 > 0 时显示环形进度与倒计时数字。
- HUD（Hud）：底部居中信息面板，包含 HP/Poise/Stamina 三段线性进度条、三源共鸣槽位发光圆点（激活时带径向阴影脉冲）、Boss 阶段进度条与目标血条。
- 暂停菜单（PauseMenu）：右上角入口按钮，展开后全屏半透明遮罩 + 居中方块面板，提供继续/重开/振动开关。

**动画与反馈**
- 使用 ArkUI 内置 `animateTo` 与 `.animation()` 实现呼吸脉冲（终结技可用时金色缩放 1.08×，源技能可用时边框加粗与阴影增强）。
- 进度条变化统一配置 `duration: 120ms, curve: Curve.EaseOut`，冷却环使用 `Curve.Linear` 保证平滑收缩。
- 触感反馈通过 `Haptics.ets` 模块调用 native 震动，由 PauseMenu 的 Toggle 控制开关。

**设计约束与规范**
- 所有交互区域显式设置 `hitTestBehavior(HitTestMode.Block|Transparent|None)` 控制事件穿透，确保游戏画面（XComponent native）与 UI 层互不干扰。
- 调试面板默认隐藏，通过 CombatControls 右上角「☰」按钮切换，避免影响正常游戏画面。
- 组件尺寸与位置使用绝对像素或百分比混合定位，未使用响应式断言或媒体查询，适配策略依赖 ArkUI 的 vp 单位与屏幕比例。

**关键文件**
- `entry/src/main/ets/ui/Hud.ets` — 主 HUD 面板（血条/耐力/共鸣槽/Boss 进度）
- `entry/src/main/ets/ui/CombatControls.ets` — 战斗按钮簇与冷却环
- `entry/src/main/ets/ui/Joystick.ets` — 虚拟摇杆视觉层
- `entry/src/main/ets/ui/PauseMenu.ets` — 暂停菜单与设置
- `entry/src/main/resources/base/element/color.json` — 全局颜色资源
- `entry/src/main/ets/ui/ActionToast.ets`、`ComboCounter.ets`、`HitFeedback.ets`、`TargetFrame.ets`、`Tutorial.ets` — 辅助 UI 组件