# M4 投资人演示版实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 M0-M3 技术骨架和战斗垂直切片打磨为面向投资方的可交互演示版

**Architecture:** 在现有 HarmonyOS NDK C++ + GLES3 渲染管线上，修复输入-动画链路、补环境氛围、构建完整体验流程、实现标志性时刻、嵌入技术展示面板、实现一键启动分发

**Tech Stack:** HarmonyOS NDK C++17, GLES3, ArkTS/ArkUI, N-API, glm, cgltf

## Global Constraints

- 美术资源继续使用 CC0 免费素材，M4 阶段不投入美术预算
- 不做在线服务、多人、付费系统
- 不做录制回放、截图录屏功能
- 质量优先，不设硬期限
- 提交消息必须符合 AGENTS.md：变更类型 + 简述 + Prompt
- 中文沟通
- 真机验证必须使用 HDC 真机日志和截图，不得用宿主测试冒充
- 不得添加软件渲染降级
- 遵循 debugging-harmonyos-native-graphics 和 systematic-debugging

---

## Task 1: 角色输入响应与移动朝向修复

**Files:**
- Modify: `native/engine/render/surface.cpp` — `actorModelMatrix` 函数和 `drawActor` 调用
- Modify: `native/engine/render/surface.h` — 如需新增朝向字段
- Modify: `native/engine/core/loop.cpp` — 确认动画意图映射和按钮输入链路
- Test: `tests/test_render_animation.cpp` — 扩展朝向测试

**Interfaces:**
- Consumes: `Player.angle`（player_controller.h 已有 yaw 字段）、`ActorRenderState`（render_animation.h）
- Produces: `actorModelMatrix` 带 rotation 参数，角色模型随移动方向旋转

### 1.1 修复角色模型朝向

**根因**：`actorModelMatrix` 只做 translate + scale，没有使用 `player.angle` 做 yaw 旋转。

- [ ] **Step 1: 修改 actorModelMatrix 接受 yaw 参数**

```cpp
// native/engine/render/surface.cpp
static glm::mat4 actorModelMatrix(const glm::vec3& position, float scale,
                                  float yaw = 0.0f) {
  return glm::translate(glm::mat4(1.0f), position) *
         glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
         glm::scale(glm::mat4(1.0f), glm::vec3(scale));
}
```

- [ ] **Step 2: 在 drawActor 调用处传入 player.angle**

```cpp
// native/engine/render/surface.cpp，玩家 drawActor 调用
actorModelMatrix(glm::vec3(s.player.x, 0.012f, s.player.y), 0.025f,
                 s.player.angle),
```

敌人也需要朝向。在 `Enemy3DRenderState` 中已有 archetype 等字段，需要新增 `float angle = 0.0f` 并在 loop.cpp 中从 AI 状态映射。

- [ ] **Step 3: 为 Enemy3DRenderState 和 Boss3DRenderState 添加 angle 字段**

```cpp
// native/engine/render/surface.h
struct Enemy3DRenderState {
  // ...existing fields...
  float angle = 0.0f;  // 朝向角，弧度
};

struct Boss3DRenderState {
  // ...existing fields...
  float angle = 0.0f;
};
```

- [ ] **Step 4: 在 loop.cpp 中映射敌人朝向**

在 `publish3DEncounterState` 或等效函数中，从敌人 AI 的 facing 方向计算 angle 并写入 `Enemy3DRenderState.angle`。

- [ ] **Step 5: 在敌人 drawActor 调用处传入 angle**

```cpp
actorModelMatrix(glm::vec3(enemy.x, 0.011f, enemy.y), 0.022f,
                 enemy.angle),
```

首领同理。

- [ ] **Step 6: 构建并真机验证**

构建签名 HAP，安装到真机，移动角色确认转身跟随方向。

### 1.2 排查攻击按钮到动画的链路

**链路**：ArkTS 按钮 → pushInput/Native DispatchTouchEvent → InputQueue → Loop::processInput → ActionStateMachine → CombatSnapshot.currentAction → loop.cpp 设置 player3dAnimation.attacking → drawActor → SkinnedModel::update → ChooseAnimation → clip 切换

从代码看链路是完整的。需要用真机 HiLog 在每层添加埋点，确认断点位置。

- [ ] **Step 7: 在 loop.cpp 添加动画意图埋点**

在 `surface.player3dAnimation.attacking = isAttackingAction(...)` 后添加：

```cpp
#ifdef OHOS_PLATFORM
  if (surface.player3dAnimation.attacking) {
    LOGI("player3dAnimation.attacking=true action=%{public}d",
         static_cast<int>(combatSnapshot.currentAction));
  }
#endif
```

- [ ] **Step 8: 在 drawActor 添加 clip 切换埋点**

在 `model.update` 调用后添加 clip 名日志，确认实际播放的 clip。

- [ ] **Step 9: 真机运行，点击攻击按钮，采集 HiLog**

```bash
hdc shell hilog | grep -E "player3dAnimation|clip|attacking"
```

- [ ] **Step 10: 根据日志定位断点并修复**

可能的情况：
- `attacking` 从未为 true：按钮事件没有触发战斗动作，需要检查 ArkTS 按钮到 ActionStateMachine 的输入映射
- `attacking` 为 true 但 clip 没切换：GLB 中 clip 名不匹配，需要检查实际 clip 名
- clip 切换了但画面没变化：蒙皮矩阵计算问题

- [ ] **Step 11: 修复后移除调试埋点，构建并真机验证**

点击攻击/闪避/技能按钮，确认角色播放对应动画。

- [ ] **Step 12: 提交**

```bash
git add native/engine/render/surface.cpp native/engine/render/surface.h native/engine/core/loop.cpp
git commit -m "fix: 修复角色模型朝向和动画输入响应

actorModelMatrix 增加 yaw 旋转，角色随移动方向转身；
排查并修复攻击按钮到骨骼动画的链路断点

Prompt: M4 Task 1 角色输入响应与移动朝向修复"
```

---

## Task 2: 环境与场景氛围

**Files:**
- Modify: `native/engine/render/surface.cpp` — 地面渲染、环境物件、光照
- Modify: `native/engine/render/surface.h` — 新增环境物件数据结构
- Modify: `native/engine/render/mesh.cpp` / `mesh.h` — 如需新增网格生成函数
- Create: `native/engine/render/environment.h` — 环境物件定义

**Interfaces:**
- Consumes: GLES3 着色器管线（Shader3D）、Mesh 基本几何体
- Produces: 场景地面纹理、环境物件、光照、首领区域视觉分区

### 2.1 程序化地面纹理

- [ ] **Step 1: 创建网格地面纹理**

在 surface.cpp 的 3D 渲染阶段，将纯色地面替换为网格纹理或深浅交替地砖。可用 fragment shader 的 `fract(gl_FragCoord)` 或顶点坐标计算网格线。

- [ ] **Step 2: 真机验证地面效果**

### 2.2 环境物件

- [ ] **Step 3: 定义环境物件结构**

```cpp
// native/engine/render/environment.h
#pragma once
#include <vector>

enum class PropKind { Pillar, Wall, Marker };

struct EnvironmentProp {
  float x;
  float z;
  float scale;
  PropKind kind;
};

struct Environment {
  std::vector<EnvironmentProp> props;
};
```

- [ ] **Step 4: 在 Surface 中添加 Environment 字段**

- [ ] **Step 5: 用 createCube/createCylinder 生成物件网格并渲染**

石柱用圆柱体，矮墙用立方体，边界标记用细长立方体。全部用现有 Mesh 基本几何体。

- [ ] **Step 6: 在场景中布置物件**

在区域入口到首领区域之间放置石柱和矮墙，形成路径引导。

- [ ] **Step 7: 真机验证环境物件**

### 2.3 光照与天空

- [ ] **Step 8: 调整方向光和环境光参数**

Surface 已有 `lightDir`、`lightColor`、`ambient` 字段。调整使角色有明显明暗对比。

- [ ] **Step 9: 添加渐变背景色**

在 `glClear` 时使用非纯色 clear color，或在着色器中渲染天空渐变。

- [ ] **Step 10: 首领区域地面配色**

首领区域用不同地面颜色或标记，形成视觉分区。

- [ ] **Step 11: 真机验证光照和背景**

- [ ] **Step 12: 提交**

```bash
git add native/engine/render/surface.cpp native/engine/render/surface.h native/engine/render/environment.h
git commit -m "feat: 添加环境物件、地面纹理和光照氛围

程序化地面网格、石柱矮墙环境物件、方向光明暗、渐变背景、首领区域视觉分区

Prompt: M4 Task 2 环境与场景氛围"
```

---

## Task 3: 完整体验流程

**Files:**
- Modify: `native/engine/core/loop.cpp` — 流程状态机和阶段切换
- Modify: `native/engine/core/game_snapshot.h` — 流程阶段枚举
- Modify: `entry/src/main/ets/pages/GamePage.ets` — 文字引导和结算画面
- Create: `native/gameplay/flow/demo_flow.h` — 演示流程控制器
- Create: `native/gameplay/flow/demo_flow.cpp`

**Interfaces:**
- Consumes: EncounterController（阶段 5 已有）、PlayerController、ActionStateMachine
- Produces: DemoFlow 状态机，管理开场→探索→遭遇→首领→结算→沙盒的切换

### 3.1 演示流程状态机

- [ ] **Step 1: 定义流程阶段枚举**

```cpp
// native/gameplay/flow/demo_flow.h
#pragma once

enum class DemoPhase {
  Intro,       // 开场镜头
  Explore,     // 探索+训练假人
  Encounter,   // 敌人遭遇
  Boss,        // 首领战
  Settlement,  // 通关结算
  Sandbox,     // 自由探索
};

struct DemoFlowState {
  DemoPhase phase = DemoPhase::Intro;
  float phaseTimer = 0.0f;
  bool introDone = false;
};
```

- [ ] **Step 2: 实现阶段切换逻辑**

Intro → Explore：镜头动画完成后切换
Explore → Encounter：训练假人被击败后切换
Encounter → Boss：全部敌人被击败后切换
Boss → Settlement：首领被击败后切换
Settlement → Sandbox：玩家点击继续后切换

- [ ] **Step 3: 在 loop.cpp 中集成 DemoFlow**

- [ ] **Step 4: 真机验证阶段切换**

### 3.2 开场镜头

- [ ] **Step 5: 实现开场镜头动画**

从俯瞰视角平滑过渡到角色身后第三人称视角。

- [ ] **Step 6: 真机验证开场镜头**

### 3.3 文字引导

- [ ] **Step 7: 在 GamePage.ets 中添加阶段文字引导**

每段开始时显示简短引导文字（"前往脉网节点"、"尝试普攻"等），2-3 秒后淡出。

- [ ] **Step 8: 真机验证文字引导**

### 3.4 通关结算画面

- [ ] **Step 9: 实现结算画面**

显示用时、击败数、关键数据，提供"进入沙盒"和"重新演示"按钮。

- [ ] **Step 10: 真机验证结算画面**

### 3.5 沙盒模式

- [ ] **Step 11: 实现沙盒模式入口和切换**

沙盒模式可自由切换敌人类型/数量/技能组合，无引导。

- [ ] **Step 12: 真机验证沙盒模式**

- [ ] **Step 13: 提交**

```bash
git add native/gameplay/flow/ native/engine/core/loop.cpp native/engine/core/game_snapshot.h entry/src/main/ets/pages/GamePage.ets
git commit -m "feat: 实现完整演示流程和沙盒模式

开场镜头、探索引导、遭遇阶段、首领高潮、通关结算、沙盒自由探索

Prompt: M4 Task 3 完整体验流程"
```

---

## Task 4: 标志性时刻

**Files:**
- Modify: `native/engine/presentation/vfx_system.cpp` — 共鸣爆发粒子、屏幕震动、慢动作
- Modify: `native/engine/presentation/vfx_system.h`
- Modify: `native/engine/render/surface.cpp` — 首领登场镜头和地面变色
- Modify: `native/engine/core/loop.cpp` — 慢动作时间缩放

**Interfaces:**
- Consumes: CombatSnapshot（共鸣爆发事件）、Boss3DRenderState
- Produces: VFX 共鸣爆发演出、首领登场镜头、韧性击破聚焦

### 4.1 共鸣爆发演出

- [ ] **Step 1: 扩展 VfxSystem 支持共鸣爆发事件**

在 VfxSystem 中新增共鸣爆发 VFX 状态：粒子爆发数量、屏幕震动幅度、慢动作时长。

```cpp
// vfx_system.h 新增
struct ResonanceBurstVfx {
  bool active = false;
  float duration = 0.0f;
  float shakeAmplitude = 0.0f;
  int particleCount = 0;
};
```

- [ ] **Step 2: 实现粒子爆发**

角色周围源质粒子向外爆发，三种源质颜色（辉印/脉流/蚀质）混合。

- [ ] **Step 3: 实现屏幕震动**

在 Camera3D 或 Surface 的 view matrix 上叠加震动偏移。

- [ ] **Step 4: 实现慢动作**

在 loop.cpp 中对 dtSeconds 乘以时间缩放因子（如 0.3），持续 0.5-1 秒后恢复。

- [ ] **Step 5: 真机验证共鸣爆发**

### 4.2 首领登场

- [ ] **Step 6: 实现首领登场镜头**

首领出现时镜头切换到俯瞰，地面变色从首领位置扩散，首领从远处走近。

- [ ] **Step 7: 真机验证首领登场**

### 4.3 韧性击破聚焦

- [ ] **Step 8: 实现韧性击破视觉反馈**

敌人韧性归零时暴露核心（发光球体），画面短暂聚焦，攻击核心时特效放大。

- [ ] **Step 9: 真机验证韧性击破**

- [ ] **Step 10: 提交**

```bash
git add native/engine/presentation/vfx_system.cpp native/engine/presentation/vfx_system.h native/engine/render/surface.cpp native/engine/core/loop.cpp
git commit -m "feat: 实现共鸣爆发演出和首领登场高光

粒子爆发、屏幕震动、慢动作、首领登场镜头、韧性击破聚焦

Prompt: M4 Task 4 标志性时刻"
```

---

## Task 5: 技术亮点展示

**Files:**
- Modify: `native/engine/presentation/performance_guard.cpp` / `.h` — 扩展调试面板
- Modify: `native/engine/render/surface.cpp` — 渲染模式切换（线框/骨骼/UV）
- Modify: `native/engine/render/shader_3d.cpp` / `.h` — 调试渲染模式
- Modify: `entry/src/main/ets/pages/GamePage.ets` — 调试面板 UI

**Interfaces:**
- Consumes: PerformanceGuard（已有 FPS 监控）、Shader3D（已有着色器管线）
- Produces: 调试面板（FPS/draw call/骨骼数/clip 数）、渲染模式切换、AI 决策树展示

### 5.1 调试面板

- [ ] **Step 1: 扩展 PerformanceGuard 输出 draw call 和骨骼数**

- [ ] **Step 2: 在 GamePage.ets 中实现可切换的调试面板**

显示 FPS、draw call 数、骨骼数、动画 clip 数。

- [ ] **Step 3: 真机验证调试面板**

### 5.2 渲染模式切换

- [ ] **Step 4: 在 Shader3D 中添加调试渲染模式**

```cpp
enum class DebugRenderMode {
  Normal,
  Wireframe,
  Skeleton,
  UV,
};
```

- [ ] **Step 5: 实现线框模式**

使用 `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` 或着色器实现。

- [ ] **Step 6: 实现骨骼显示模式**

渲染关节点为小线段，连接父子骨骼。

- [ ] **Step 7: 真机验证渲染模式切换**

### 5.3 AI 决策树展示

- [ ] **Step 8: 在沙盒模式中可选展示三源反应矩阵**

在 HUD 中以文字形式展示当前共鸣组合和反应表。

- [ ] **Step 9: 真机验证 AI 展示**

- [ ] **Step 10: 提交**

```bash
git add native/engine/presentation/ native/engine/render/shader_3d.cpp native/engine/render/shader_3d.h entry/src/main/ets/pages/GamePage.ets
git commit -m "feat: 添加技术亮点调试面板

FPS/draw call/骨骼数监控、线框/骨骼/UV渲染模式、AI决策树展示

Prompt: M4 Task 5 技术亮点展示"
```

---

## Task 6: 一键启动与分发

**Files:**
- Modify: `entry/src/main/ets/pages/GamePage.ets` — 启动流程入口
- Modify: `entry/src/main/ets/EntryAbility.ets` — 启动逻辑
- Modify: `native/engine/core/loop.cpp` — 默认进入演示流程

**Interfaces:**
- Consumes: DemoFlow（Task 3 产出）
- Produces: 安装即进入演示流程，沙盒作为结束后选项

### 6.1 一键启动

- [ ] **Step 1: 确保全新安装后启动直接进入演示流程**

EntryAbility 启动后 GamePage 加载，nativeStart 后默认进入 DemoPhase::Intro。

- [ ] **Step 2: 确保流程结束后展示沙盒入口**

Settlement 阶段显示"进入沙盒"和"重新演示"按钮。

- [ ] **Step 3: 真机验证一键启动**

卸载后重新安装，确认启动即进入演示流程。

### 6.2 HAP 体积管理

- [ ] **Step 4: 检查 HAP 体积**

确认 HAP 体积合理（当前约 17MB），可直接安装分发。

- [ ] **Step 5: 提交**

```bash
git add entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/EntryAbility.ets native/engine/core/loop.cpp
git commit -m "feat: 实现一键启动演示流程

安装即进入演示流程，沙盒模式作为结束后选项

Prompt: M4 Task 6 一键启动与分发"
```

---

## 依赖关系

```
Task 1（输入修复）─┬─→ Task 2（环境氛围）
                   ├─→ Task 3（体验流程）
                   ├─→ Task 4（标志性时刻）
                   └─→ Task 5（技术展示）
                                         └─→ Task 6（一键启动）
```

Task 1 必须最先完成。Task 2-5 在 Task 1 完成后可并行规划，但实施时按 2→3→4→5 顺序推进。Task 6 最后收尾。

每个 Task 完成后需要：
1. 宿主 C++ 测试通过
2. 签名 HAP 构建成功
3. 真机验证
4. `git diff --check` 通过
5. 提交
