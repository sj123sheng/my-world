# M4 投资 Demo 视觉基线实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不购买资产的前提下，把现有遗迹、角色、VFX 和 HUD 统一为“风格化暗黑遗迹”，并交付可稳定跑通的 5–7 分钟引导演示基线。

**Architecture:** 复用现有 `Surface`、模型资产桥接、战斗和快照管线，在渲染层增加统一视觉参数与可复用共鸣 VFX，在 Native 核心增加独立 `DemoDirector` 状态机；ArkTS `Hud` 只消费快照并默认隐藏调试信息。Task 3A→3B→3C 依次形成资产适配、首屏视觉和完整演示三个可独立验收的交付物。

**Tech Stack:** HarmonyOS ArkTS、NDK C++17、GLES3、现有 GLB `StaticModel`/`SkinnedModel` 管线、CMake、Node 契约测试、C++ focused tests。

## Global Constraints

- 美术方向固定为“风格化暗黑遗迹”，环境低饱和蓝灰/石墨黑，三源固定为青绿/紫红/琥珀。
- Pura 70 Pro 模拟器只用于视觉、功能和流程验证；真实性能结论必须来自真机。
- 演示流程控制在 5–7 分钟，启动后 20 秒内出现合格首屏。
- 不购买 Boss 资产、不改动第三方资源许可证、不引入联网运行时资源。
- 玩家、普通敌人、Boss、场景和 HUD 不得保留基础几何占位物作为最终验收画面。
- 所有新增逻辑必须有 focused test 或契约测试；每个任务独立提交。

---

### Task 1: 资产适配描述与视觉令牌

**Files:**
- Create: `native/engine/render/asset_profile.h`
- Create: `native/engine/render/asset_profile.cpp`
- Create: `native/engine/presentation/visual_tokens.h`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`
- Test: `tests/test_asset_profile.cpp`
- Test: `tests/test_visual_tokens.cpp`

**Interfaces:**
- Consumes: 现有 `ModelKind`、`SkinnedModel`、`StaticModel` 和 `PendingModelAsset`。
- Produces: `AssetProfile { scale, yawOffset, materialTint, outlineColor, mountPoints }`；`VisualTokens::environmentPalette()`、`VisualTokens::sourceColor(SourceType)`；`Surface::applyAssetProfile(ModelKind, const AssetProfile&)`。

- [ ] **Step 1: 写失败测试，锁定模型适配和三源色值**

```cpp
TEST(AssetProfile, AppliesScaleAndYawWithoutChangingModelKind) {
  AssetProfile profile = AssetProfile::forModel(ModelKind::Boss);
  EXPECT_GT(profile.scale, 0.0f);
  EXPECT_NE(profile.yawOffset, 0.0f);
}

TEST(VisualTokens, SourceColorsAreStableAndDistinct) {
  EXPECT_NE(VisualTokens::sourceColor(SourceType::Radiance),
            VisualTokens::sourceColor(SourceType::Current));
  EXPECT_NE(VisualTokens::sourceColor(SourceType::Current),
            VisualTokens::sourceColor(SourceType::Corruption));
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake --build build/tests --target test_asset_profile test_visual_tokens && ctest --test-dir build/tests -R 'AssetProfile|VisualTokens'`

Expected: FAIL，缺少 `AssetProfile`/`VisualTokens` 类型或工厂方法。

- [ ] **Step 3: 实现最小适配层**

为三类模型提供固定默认值：玩家深色材质与青绿轮廓，敌人暗色材质与单色核心，Boss 深色材质与三色核心挂点。适配层只保存描述，不在无有效 GLES context 时上传 GPU 资源。

- [ ] **Step 4: 接入 Surface 并运行测试**

将描述应用到模型 draw 参数，保持 `setModelAsset` 的 CPU pending/GL context 提交生命周期不变。运行：`ctest --test-dir build/tests -R 'AssetProfile|VisualTokens|ModelAsset|Environment'`。

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/asset_profile.* native/engine/presentation/visual_tokens.h native/engine/render/surface.* entry/src/main/cpp/CMakeLists.txt tests/test_asset_profile.cpp tests/test_visual_tokens.cpp
git commit -m "feat: 建立投资演示视觉令牌与资产适配"
```

### Task 2: 首屏场景重排与低成本氛围渲染

**Files:**
- Modify: `native/engine/render/environment.h`
- Modify: `native/engine/render/environment.cpp`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/render/shader_3d.*`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Test: `tests/test_environment.cpp`
- Test: `tests/test_environment_composition.cpp`

**Interfaces:**
- Consumes: Task 1 `VisualTokens`、现有四个环境 GLB 批次和 `EnvironmentController`。
- Produces: `EnvironmentComposition { spawn, combatAnchor, altarAnchor, cameraFocus }`；`EnvironmentController::composition()`；`Surface::setEnvironmentPalette(EnvironmentPalette)`。

- [ ] **Step 1: 写失败测试，锁定首屏空间关系**

```cpp
TEST(EnvironmentComposition, SpawnFramesAltarAndBossFocus) {
  const auto composition = EnvironmentController::defaultComposition();
  EXPECT_LT(composition.spawn.distance(composition.altarAnchor), 1.0f);
  EXPECT_GT(composition.cameraFocus.z, composition.spawn.z);
  EXPECT_NE(composition.combatAnchor, composition.spawn);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake --build build/tests --target test_environment test_environment_composition && ctest --test-dir build/tests -R 'EnvironmentComposition'`

Expected: FAIL，缺少组合结构或当前布局仍为默认灰地。

- [ ] **Step 3: 实现固定主路线和焦点参数**

在环境控制器中定义出生点、前景遮挡、战斗区、祭坛和远景 Boss 焦点；优先调整批次变换矩阵与可见性，不改变碰撞逻辑。边界背景和装饰仍按现有性能等级可隐藏。

- [ ] **Step 4: 增加氛围参数并验证模拟器首屏**

将清屏色、距离雾、方向光、环境光、祭坛冷色光和三色发光材质统一接入 GLES3；不引入延迟渲染或实时全局光照。运行 focused C++ tests 后构建 HAP，在 Pura 70 Pro 模拟器确认启动 20 秒内可见前/中/远景和 Boss 焦点。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/environment.* native/engine/render/surface.* native/engine/render/shader_3d.* entry/src/main/ets/pages/GamePage.ets tests/test_environment.cpp tests/test_environment_composition.cpp
git commit -m "feat: 重排暗黑遗迹首屏与氛围渲染"
```

### Task 3: 共鸣 VFX 与电影化 HUD

**Files:**
- Modify: `native/engine/presentation/vfx_system.*`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Test: `tests/test_vfx_system.cpp`
- Test: `tests/test_snapshot_contract.mjs`

**Interfaces:**
- Consumes: Task 1 `VisualTokens`、现有 `VfxSystem` 事件和战斗快照。
- Produces: `VfxCue { type, source, intensity, durationMs }`；快照字段 `objectiveLabel`、`resonanceSlots`、`showDebugHud`；Hud 的中心轴布局和一次性提示淡出。

- [ ] **Step 1: 写失败测试，锁定三色 VFX 事件和 HUD 契约**

```cpp
TEST(VfxSystem, ResonanceCueUsesOnlyVisualTokenSourceColor) {
  const auto cue = VfxCue::resonance(SourceType::Current, 1.0f, 600);
  EXPECT_EQ(cue.color, VisualTokens::sourceColor(SourceType::Current));
  EXPECT_EQ(cue.durationMs, 600);
}
```

```js
assert.equal(typeof snapshot.objectiveLabel, 'string');
assert.equal(snapshot.resonanceSlots.length, 3);
assert.equal(typeof snapshot.showDebugHud, 'boolean');
```

- [ ] **Step 2: 运行测试确认失败**

Run: `ctest --test-dir build/tests -R 'VfxSystem'` and `node tests/test_snapshot_contract.mjs`。

Expected: C++ 或 Node 契约缺少新增字段。

- [ ] **Step 3: 实现共鸣视觉事件和快照字段**

复用现有事件队列生成环形波纹、拖尾、符文、命中闪光和轮廓脉冲；三种来源只改变颜色、速度和扰动。将目标、三个槽位和调试开关写入快照，保持旧字段向后兼容。

- [ ] **Step 4: 重做 ArkTS HUD 并运行测试**

移除常驻 FPS 和密集诊断文字；顶部中心显示目标，下方显示细生命/能量条与三个共鸣槽，首次操作提示淡出。调试面板仅由已有 `toggleDebugHud` 打开。运行 C++ focused tests、Node 契约和 `git diff --check`。

- [ ] **Step 5: 提交**

```bash
git add native/engine/presentation/vfx_system.* native/engine/core/game_snapshot.h native/engine/core/loop.cpp entry/src/main/ets/ui/Hud.ets entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/napi/Bridge.ets tests/test_vfx_system.cpp tests/test_snapshot_contract.mjs
git commit -m "feat: 统一共鸣特效与电影化HUD"
```

### Task 4: DemoDirector 引导演示流程

**Files:**
- Create: `native/gameplay/flow/demo_director.h`
- Create: `native/gameplay/flow/demo_director.cpp`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Test: `tests/test_demo_director.cpp`

**Interfaces:**
- Consumes: Task 2 `EnvironmentComposition`、Task 3 `VfxCue`/快照、现有 `EncounterController` 和 `Boss`。
- Produces: `DemoPhase { Intro, Explore, Encounter, Resonance, BossIntro, BossFight, Outro }`；`DemoDirector::tick(Tick, const DemoSignals&)`；`DemoDirector::skipTo(DemoPhase)`；`DemoDirector::snapshot()`。

- [ ] **Step 1: 写失败测试，覆盖阶段、超时和恢复**

```cpp
TEST(DemoDirector, AdvancesAfterEncounterAndResetsCameraOnSkip) {
  DemoDirector director;
  director.tick(0, DemoSignals::introComplete());
  EXPECT_EQ(director.phase(), DemoPhase::Explore);
  director.skipTo(DemoPhase::BossIntro);
  EXPECT_TRUE(director.inputRestoredAfterSkip());
}

TEST(DemoDirector, TimeoutProvidesSafeProgression) {
  DemoDirector director;
  director.tick(0, DemoSignals::exploreStalled(31000));
  EXPECT_NE(director.phase(), DemoPhase::Explore);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake --build build/tests --target test_demo_director && ctest --test-dir build/tests -R 'DemoDirector'`。

Expected: FAIL，类型和状态机尚不存在。

- [ ] **Step 3: 实现状态机与信号**

实现 7 个阶段、空间触发/完成条件/超时兜底、错误恢复和隐藏跳转入口。镜头接管只通过 signal 控制，不直接操作 EGL 或输入队列；skip、正常完成和异常退出都发布 `inputRestored=true`。

- [ ] **Step 4: 接入 Loop、Encounter 和 HUD**

在 `Loop::updateFixed` 中按固定顺序消费导演信号，再更新战斗和快照；默认启动 `Intro`，将目标文案和当前阶段推送到 HUD。将新源文件加入 CMake，运行全量 focused C++ tests。

- [ ] **Step 5: 模拟器两次完整验收并提交**

构建并安装 HAP，在 Pura 70 Pro 模拟器连续跑通两次 5–7 分钟流程；记录启动首屏、遭遇、三源激活、Boss 入口和收尾截图。若阶段卡住，使用隐藏跳转入口恢复并记录原因。

```bash
git add native/gameplay/flow native/engine/core/loop.* entry/src/main/cpp/CMakeLists.txt entry/src/main/ets/ui/Hud.ets tests/test_demo_director.cpp
git commit -m "feat: 增加引导演示流程状态机"
```

## 基线完成门槛

- `ctest` focused suite、Node 契约和 `git diff --check` 全部通过。
- 模拟器连续两次完成 5–7 分钟流程，启动后 20 秒内出现合格首屏。
- 截图中不出现常驻调试 HUD、大片空地或未统一材质的基础几何占位物。
- Boss 资产采购、Boss 两阶段、最终收尾和分发不在本计划内，进入第二份计划。
