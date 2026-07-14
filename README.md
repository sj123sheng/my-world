# 艾瑟兰（Ethelan）

面向 HarmonyOS 的原创开放世界动作 RPG 技术样板。

项目使用 ArkUI 承载系统界面，通过 N-API 连接 HarmonyOS NDK C++ 游戏核心，
并以 XComponent、NativeWindow、EGL 和 OpenGL ES 3 构建原生渲染链路。

> 当前仓库处于 MVP 技术验证阶段，重点是验证原生渲染、输入、固定帧循环、
> 状态快照与基础玩法骨架，并非完整可发行游戏。

## 项目简介

《艾瑟兰》以自由探索、即时战斗、环境解谜和角色成长为目标体验。
玩家扮演能够观测并调谐源质的“巡脉者”，探索受到“静默断层”影响的区域，
修复世界脉网并调查灾变背后的历史。

核心原创机制为“三源共鸣”：

- **辉印**：标记弱点，并揭示隐藏路径或机关纹路。
- **脉流**：提供牵引、传导、位移和区域控制。
- **蚀质**：削减韧性、持续侵蚀并改变目标状态。

不同源质按命中顺序组合，可形成折光、凝滞、崩解和共鸣爆发等效果。
完整玩法规则请阅读[综合设计文档](docs/superpowers/specs/2026-07-12-harmonyos-openworld-rpg-design.md)。

## 当前能力

仓库目前已经具备以下技术能力：

- HarmonyOS Stage 模型应用工程与 API 23 配置。
- ArkTS + ArkUI 页面、HUD 和应用生命周期管理。
- XComponent Surface 与 Native C++ 回调注册。
- N-API 输入事件、启停控制和状态快照桥接。
- HarmonyOS NDK C++ 游戏帧循环。
- EGL 1.4 与 OpenGL ES 3 原生渲染上下文。
- 基础网格、玩家、目标、场景物件和粒子绘制。
- 触控输入驱动玩家移动与目标状态。
- NativeWindow 生命周期串行化和 fence 同步工具。
- GLES 初始化失败时的 ArkUI 安全提示，不进入不稳定的软件 Buffer 路径。
- 资源清单、配置 Schema、存档和确定性战斗相关基础模块。

以下内容属于设计目标或后续阶段，不代表当前已经完整实现：

- 15～30 分钟的完整探索—战斗—成长垂直切片。
- 完整敌人、首领、遗物构件和营地升级内容。
- 多区域开放世界、联网合作、公会和大型团队挑战。
- 权威服务器、匹配、反作弊和在线内容分发。

## 技术架构

```text
┌─ ArkUI 表现与系统交互层 ────────────────────┐
│ 页面 / HUD / 设置 / 系统窗口                 │
├─ N-API 边界层 ─────────────────────────────┤
│ 生命周期 / 输入事件 / 状态快照               │
├─ HarmonyOS NDK C++ 游戏核心 ───────────────┤
│ 帧循环 / 相机 / 渲染 / 资源 / 战斗逻辑       │
├─ HarmonyOS 平台适配层 ─────────────────────┤
│ XComponent / NativeWindow / EGL / GLES3     │
└─────────────────────────────────────────────┘
```

主要技术选型：

| 领域 | 技术 | 职责 |
|---|---|---|
| 系统 UI | ArkTS + ArkUI | 页面、HUD、生命周期和系统交互 |
| 原生承载 | XComponent + NativeWindow | 管理独立渲染表面 |
| 图形 | EGL + OpenGL ES 3 | 创建上下文并提交原生画面 |
| 游戏核心 | HarmonyOS NDK C++ | 帧循环、状态、资源和玩法逻辑 |
| 跨语言 | N-API | 输入、命令和粗粒度状态交换 |
| 构建 | Hvigor + CMake + Ninja | ArkTS 与 Native 模块构建 |

Native 模块包含 `GLES3/gl3.h` 并链接 `libGLESv3.so`。
EGL 初始化遵循 Bind API、选择 Config、创建 WindowSurface、创建 Context、
MakeCurrent 和创建 Shader Program 的顺序。

## 环境要求

- DevEco Studio，建议使用支持 HarmonyOS 6.1 的稳定版本。
- HarmonyOS SDK 6.1.0(23)，API 23 或兼容版本。
- macOS、Windows 或 DevEco Studio 支持的开发环境。
- HarmonyOS 6.1 真机，或图形运行时完整的本地模拟器。
- C++17 编译环境，用于运行独立 Native 测试。
- 已配置调试签名的 HarmonyOS 开发者环境。

工程的兼容和目标 SDK 均配置为：

```text
compatibleSdkVersion: 6.1.0(23)
targetSdkVersion:     6.1.0(23)
runtimeOS:            HarmonyOS
```

## 快速开始

### 1. 获取代码

```bash
git clone git@github.com:sj123sheng/my-world.git
cd my-world
```

### 2. 使用 DevEco Studio 导入

1. 启动 DevEco Studio。
2. 选择 **Open**，打开仓库根目录。
3. 等待 ohpm、Hvigor 和 CMake 同步完成。
4. 在项目签名配置中选择或生成调试签名。
5. 连接 HarmonyOS 真机，或启动兼容的本地模拟器。
6. 选择 `entry` 模块并点击 **Run**。

首次构建会生成 `oh_modules`、`entry/build` 和 `entry/.cxx` 等本地产物，
这些目录已经加入 `.gitignore`，不应提交到仓库。

### 3. 命令行构建

在已完成 SDK 配置的 DevEco Studio Terminal 中运行：

```bash
hvigorw assembleHap \
  --mode module \
  -p module=entry@default \
  -p product=default
```

如果 Hvigor 报告 `DEVECO_SDK_HOME` 无效或 SDK 组件缺失，请先通过 DevEco Studio
的 SDK Manager 检查 HarmonyOS 6.1 SDK 完整性，不要把该变量指向不完整的 IDE 内置目录。

构建产物位于：

```text
entry/build/default/outputs/default/
```

如果没有配置签名，命令行构建仍可生成 unsigned HAP；安装到真机前需要有效调试签名。

## 真机验证

本项目已经在 HarmonyOS 6.1 真机验证以下链路：

```text
EGL initialized: 1.4
GL_VERSION: OpenGL ES 3.2
Program linked
EGL surface ready
```

建议在 DevEco Studio HiLog 中使用 `Ethelan` 作为过滤关键字。
正常启动应依次看到 XComponent 回调注册、Surface 初始化、EGL 初始化、
Shader Program 链接和帧循环日志。

若页面显示“当前图形环境不支持 GLES”：

1. 确认 Native 模块链接的是 `libGLESv3.so`。
2. 检查设备或模拟器图形运行时是否完整。
3. 优先使用真机验证，不要在 GLES 失败后直接操作 NativeWindow CPU Buffer。
4. 查看[NativeWindow 崩溃修复设计](docs/superpowers/specs/2026-07-13-native-window-crash-fix-design.md)。

## 项目结构

```text
my-world/
├─ AppScope/                    # 应用级配置与资源
├─ entry/
│  └─ src/main/
│     ├─ ets/                   # ArkTS 页面、HUD 和 N-API 封装
│     ├─ cpp/                   # Native 模块入口与 CMake 配置
│     ├─ resources/             # 页面、字符串、颜色和媒体资源
│     └─ module.json5           # entry 模块声明
├─ native/
│  ├─ engine/                   # 帧循环、输入、渲染和资源系统
│  ├─ gameplay/                 # 战斗、角色、敌人和成长逻辑
│  └─ platform/harmony/         # HarmonyOS 生命周期、音频和 fence 工具
├─ config/                      # 版本化 Schema 与开发配置
├─ assets/                      # MVP 资源清单
├─ tests/                       # 独立 C++ 回归测试
├─ docs/superpowers/            # 设计规范、实施计划和评审决策
├─ build-profile.json5          # 产品、SDK、ABI 和构建模式
└─ hvigorfile.ts                # Hvigor 应用构建入口
```

## 测试与验证

### NativeWindow fence 测试

```bash
c++ -std=c++17 tests/test_fence_wait.cpp \
  native/platform/harmony/fence_wait.cpp \
  -o /tmp/test_fence_wait
/tmp/test_fence_wait
```

### 完整 HAP 构建

```bash
hvigorw assembleHap --mode module \
  -p module=entry@default \
  -p product=default
```

提交前建议执行：

```bash
git diff --check
git status --short
```

## 已知限制

- 当前内容主要用于原生技术链路和 MVP 骨架验证。
- Native 软件 Buffer 降级在部分 HarmonyOS 6.1 模拟器中不稳定，因此默认禁用。
- 模拟器是否支持 GLES 取决于 DevEco Studio、SDK 与系统镜像的匹配情况。
- 当前帧循环使用简单定时休眠，尚未接入生产级 VSync 调度。
- 当前渲染内容为灰盒图形，不代表最终美术质量。
- 完整战斗、敌人 AI、区域流式内容和性能基线仍需继续实现与验证。
- 公网联机能力不在当前 MVP 范围内。

## 路线图

- **M0 技术探针**：验证 ArkUI、XComponent、NativeWindow、GLES3、输入和生命周期。
- **M1 战斗样板**：完善固定 tick、碰撞、三源共鸣、敌人状态和调试记录。
- **M2 垂直切片**：加入区域流式加载、成长闭环、配置和存档。
- **M3 鸿蒙验证**：完成目标设备性能、功耗和稳定性验证。
- **M4 长期评审**：依据验证数据决定是否扩展联网与大世界内容。

## 相关文档

- [开放世界动作 RPG 综合设计](docs/superpowers/specs/2026-07-12-harmonyos-openworld-rpg-design.md)
- [NativeWindow 崩溃修复设计](docs/superpowers/specs/2026-07-13-native-window-crash-fix-design.md)
- [NativeWindow 崩溃修复计划](docs/superpowers/plans/2026-07-13-native-window-crash-fix.md)
- [MVP Native 骨架实施计划](docs/superpowers/plans/2026-07-12-ethelan-mvp-native-skeleton.md)
- [M4 长期能力评审](docs/superpowers/plans/M4-decision.md)

## 许可证

当前项目在 `oh-package.json5` 中标记为 `UNLICENSED`。
在仓库补充明确许可证之前，不应视为允许复制、修改或分发。
