# M4 投资 Demo Boss 与分发实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在视觉基线通过后，接入经用户确认的 Boss 资产，完成共鸣核心祭司的标志性登场、两阶段战斗、技术展示和可分发验收包。

**Architecture:** Boss 资产先经过离线许可证和模型适配检查，再通过现有 `SkinnedModel` 桥接接入；环体、碎片、三色核心和破环由程序化 VFX 驱动。技术面板和阶段跳转是默认关闭的演示辅助层，不改变产品 HUD。

**Tech Stack:** HarmonyOS ArkTS、NDK C++17、GLES3、GLB 骨骼模型、现有 Boss/战斗/VFX/PerformanceGuard、Hvigor、HDC。

## Global Constraints

- Boss 预算累计不超过 500 元；购买前必须单独获得用户确认。
- 许可证必须允许商业演示和产品分发；保留来源、价格、下载日期、SHA-256 和修改记录。
- Pura 70 Pro 模拟器只证明视觉/功能；真机才可证明稳定 30 FPS、内存和温度。
- Boss 核心轮廓、三色共鸣和悬浮环不得被性能降级关闭。
- 不做开放世界、多地图、多人、复杂装备或联网运行时资源。

---

### Task 1: Boss 候选审计与适配

**Files:**
- Create: `docs/assets/boss-candidates.md`
- Create: `automation/assets/validate_boss_candidate.mjs`
- Modify: `entry/src/main/resources/rawfile/models/boss.glb`（仅在购买获批后替换）
- Modify: `native/engine/render/asset_profile.*`
- Test: `tests/test_boss_asset_contract.mjs`

**Interfaces:**
- Consumes: 视觉基线 Task 1 的 `AssetProfile` 和现有 Boss 模型桥接。
- Produces: 候选表、许可证证据、`validate_boss_candidate.mjs` CLI，以及可加载的待机/移动/受击/两攻击/死亡动作契约。

- [ ] **Step 1: 编写候选清单模板和失败契约测试**

```js
assert.equal(candidate.license.commercial, true);
assert.ok(candidate.priceCny <= 500);
for (const clip of ['idle', 'move', 'hit', 'attackA', 'attackB', 'death']) {
  assert.ok(candidate.animations.includes(clip));
}
```

- [ ] **Step 2: 运行验证确认未授权候选被拒绝**

Run: `node automation/assets/validate_boss_candidate.mjs docs/assets/boss-candidates.md`。

Expected: 未填完整许可证、价格或动作时退出非零。

- [ ] **Step 3: 完成候选审计并等待购买确认**

只记录商店预览和许可证证据，不下载或购买。若没有同时满足许可、动画、格式和移动端约束的候选，记录回退到免费类人模型的决定。

- [ ] **Step 4: 获批后替换并验证模型链路**

将模型放入 rawfile，运行 GLB 解析、骨骼动作和模型桥接测试；失败时恢复原模型，不修改战斗逻辑。

- [ ] **Step 5: 提交**

```bash
git add docs/assets/boss-candidates.md automation/assets/validate_boss_candidate.mjs native/engine/render/asset_profile.* tests/test_boss_asset_contract.mjs entry/src/main/resources/rawfile/models/boss.glb
git commit -m "feat: 接入合规共鸣祭司Boss资产"
```

### Task 2: Boss 登场、环体与破环演出

**Files:**
- Modify: `native/gameplay/flow/demo_director.*`
- Modify: `native/engine/presentation/vfx_system.*`
- Modify: `native/engine/render/surface.*`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Test: `tests/test_boss_cinematic.cpp`
- Test: `tests/test_vfx_system.cpp`

**Interfaces:**
- Consumes: 基线 `DemoPhase::BossIntro/BossFight`、Boss 资产动作和 `VfxCue`。
- Produces: `BossCinematicState { ringProgress, shardCount, sourceColor, broken }`；`DemoDirector::bossCinematic()`；快照 `bossCinematicProgress`。

- [ ] **Step 1: 写失败测试，锁定 6–8 秒登场和半血破环**

```cpp
TEST(BossCinematic, IntroCompletesWithinEightSeconds) {
  BossCinematicState state;
  for (int i = 0; i < 8; ++i) state = state.tick(1000);
  EXPECT_TRUE(state.readyForFight);
  EXPECT_EQ(state.shardCount, 3);
}

TEST(BossCinematic, HalfHealthBreaksOuterRing) {
  EXPECT_TRUE(BossCinematicState::fromBossHp(0.49f).broken);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `ctest --test-dir build/tests -R 'BossCinematic'`。

Expected: FAIL，演出状态不存在。

- [ ] **Step 3: 实现程序化环体、碎片、三色核心和破环**

在 GLES3 现有几何与粒子能力上实现，不依赖新运行时资源；Boss 资产仅负责本体轮廓和动作。半血以下触发环体断裂、三色切换和共鸣反制窗口。

- [ ] **Step 4: 接入导演和快照，恢复输入**

BossIntro 期间暂时接管镜头/输入；6–8 秒结束或 skip 后保证 `inputRestored`，BossFight 重新进入玩家控制。运行 Boss、VFX、快照和流程回归测试。

- [ ] **Step 5: 模拟器验收并提交**

在 Pura 70 Pro 模拟器固定机位检查 Boss 静态轮廓、三色核心、登场镜头和破环画面，连续触发两次。

```bash
git add native/gameplay/flow native/engine/presentation/vfx_system.* native/engine/render/surface.* native/engine/core/game_snapshot.h entry/src/main/ets/ui/Hud.ets tests/test_boss_cinematic.cpp tests/test_vfx_system.cpp
git commit -m "feat: 完成共鸣祭司Boss标志性演出"
```

### Task 3: 技术面板、现场恢复和演示脚本

**Files:**
- Modify: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `native/engine/presentation/performance_guard.*`
- Create: `docs/superpowers/demo/m4-investor-script.md`
- Test: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: `DemoDirector` 阶段跳转、PerformanceGuard、现有 debug toggle。
- Produces: 默认隐藏技术面板、阶段快捷入口、7 分钟讲解脚本和故障恢复说明。

- [ ] **Step 1: 写契约测试**

```js
assert.equal(typeof api.toggleDebugHud, 'function');
assert.equal(typeof api.skipDemoPhase, 'function');
assert.equal(typeof snapshot.debugHud, 'boolean');
```

- [ ] **Step 2: 实现技术面板和阶段跳转**

技术面板显示 FPS、draw calls、骨骼数、动画 clip 数和环境批次；默认关闭。阶段跳转只在开发手势或隐藏入口可达，不出现在产品 HUD。

- [ ] **Step 3: 编写并校对演示脚本**

按 0:00–6:40 列出每分钟讲解、操作、预期画面、备用跳转和失败恢复动作；明确模拟器截图不能作为性能宣传证据。

- [ ] **Step 4: 运行契约测试并提交**

Run: `node tests/test_bridge_contract.mjs && git diff --check`。

```bash
git add entry/src/main/ets/ui/Hud.ets entry/src/main/ets/pages/GamePage.ets entry/src/main/ets/napi/Bridge.ets native/engine/presentation/performance_guard.* docs/superpowers/demo/m4-investor-script.md tests/test_bridge_contract.mjs
git commit -m "feat: 增加投资演示技术面板与恢复入口"
```

### Task 4: 验收、真机证据与分发

**Files:**
- Create: `automation/perf/profile_m4_investor_demo.sh`
- Create: `docs/superpowers/demo/m4-acceptance-report.md`
- Modify: `README.md`
- Verify: `entry/build/default/outputs/default/entry-default-signed.hap`

**Interfaces:**
- Consumes: 完整 DemoDirector、Boss 演出、技术面板和分发配置。
- Produces: 模拟器功能/视觉证据、真机 7 分钟性能证据、签名 HAP、离线演示操作卡和验收报告。

- [ ] **Step 1: 添加脚本语法和证据目录检查**

Run: `bash -n automation/perf/profile_m4_investor_demo.sh && node automation/assets/validate_boss_candidate.mjs docs/assets/boss-candidates.md`。

- [ ] **Step 2: 模拟器连续两次完整流程**

记录启动首屏、基础战斗、三源激活、Boss 登场、破环和收尾截图/录屏；报告明确标注“模拟器视觉/功能验证”。

- [ ] **Step 3: 真机持续 7 分钟性能采集**

使用 `hdc` 采集帧时间、内存和温度，确认稳定不低于 30 FPS；若无可用真机，报告将性能状态标为未完成，不用模拟器数据替代。

- [ ] **Step 4: 构建、签名和离线安装验证**

运行 Hvigor assembleHap，使用有效签名配置安装到验收设备，冷启动确认 20 秒内进入首屏，断网后重复安装/启动。

- [ ] **Step 5: 提交验收报告**

```bash
git add automation/perf/profile_m4_investor_demo.sh docs/superpowers/demo/m4-acceptance-report.md README.md
git commit -m "test: 完成投资演示验收与分发证据"
```

## 交付完成门槛

- 模拟器连续两次完整走完 5–7 分钟流程，且任一阶段可恢复。
- 首屏、Boss 登场、最终破环三个画面可固定机位复现。
- 技术面板默认隐藏，Boss 核心视觉未被降级关闭。
- 真机稳定 30 FPS、内存和温度证据齐全；否则明确标注未完成。
- HAP 可离线安装，来源/许可证/价格/SHA-256 记录完整。
