# M3-2 glTF 模型加载与骨骼动画 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 HarmonyOS 真机上加载三个可再分发的 GLB 角色并以骨骼动画替换 M3-1 静态角色网格。

**Architecture:** ArkTS 从 `rawfile/models` 读取 GLB 二进制并一次性传给 Native；Native 复制数据，在 GL 上下文可用时以 cgltf 解析为 `SkinnedModel` 再上传 GPU。渲染层只读既有状态选择动画；任一资源、解析或上传失败均保留 M3-1 Mesh 占位绘制。

**Tech Stack:** C++17、OpenHarmony N-API、OpenGL ES 3.0、GLM、cgltf（MIT）、glTF 2.0 GLB。

## Global Constraints

- 仅支持 `.glb`、三角形、单 skin、POSITION/NORMAL/TEXCOORD_0/JOINTS_0/WEIGHTS_0。
- 每顶点四个影响关节；每模型最多 64 根关节；只支持 `LINEAR`/ `STEP` TRS 动画。
- 禁止 PBR、形态键、Draco、扩展、`CUBICSPLINE`、多 skin；失败必须包含资产名和原因日志，并回退 Mesh。
- 三个角色资源须为 CC0 或等效可商用、可再分发许可，并在 `assets/models/LICENSES.md` 记录来源。
- 不改动战斗、AI、输入、Camera3D、HUD 的逻辑契约；训练/战斗真机 FPS >= 30。

---

## File Structure

- Create `native/third_party/cgltf/cgltf.h` — 原样供应商化的 MIT 解析器。
- Create `native/engine/render/skinned_model.h/.cpp` — GLB 验证、采样、调色板、GPU 上传和绘制。
- Create `native/engine/render/render_animation.h` — 纯渲染动画意图与选择函数。
- Modify `native/engine/render/{mesh,shader_3d,surface}.h/.cpp` — 蒙皮属性、shader、生命周期与回退。
- Modify `native/engine/core/loop.cpp` — 将既有状态转换为渲染动画意图。
- Modify `entry/src/main/cpp/{CMakeLists.txt,native_bridge.cpp}`、`entry/src/main/ets/{napi/Bridge.ets,pages/GamePage.ets}` — rawfile 与 N-API 字节桥接。
- Create `entry/src/main/resources/rawfile/models/{player,enemy,boss}.glb`、`assets/models/LICENSES.md`。
- Create `tests/test_skinned_model.cpp`、`tests/test_render_animation.cpp`。

### Task 1: 引入可测试的 glTF 数据与动画核心

**Files:**
- Create: `native/third_party/cgltf/cgltf.h`
- Create: `native/engine/render/skinned_model.h`
- Create: `native/engine/render/skinned_model.cpp`
- Test: `tests/test_skinned_model.cpp`

**Interfaces:**
- Produces: `constexpr uint32_t kMaxSkinJoints = 64;`、`AnimationInterpolation`、`SkinPalette`、`ValidateGltf`、`WrapAnimationTime`、`SampleVec3`、`SampleQuat`、`BuildSkinPalette`。
- Consumes: GLM；纯数据路径不依赖 EGL/GLES。

- [ ] **Step 1: 写失败测试**

```cpp
void testWrapAndStepSampling() {
  assert(close(WrapAnimationTime(2.25f, 2.0f), 0.25f));
  AnimationChannel<glm::vec3> channel{{0.0f, 1.0f}, {{0,0,0}, {2,0,0}},
                                       AnimationInterpolation::Step};
  assert(SampleVec3(channel, 0.75f).x == 0.0f);
}
void testRejectsOver64Joints() {
  GltfValidationInput input = validValidationInput();
  input.jointCount = kMaxSkinJoints + 1;
  std::string reason;
  assert(!ValidateGltf(input, reason));
  assert(reason == "joint count exceeds 64");
}
```

- [ ] **Step 2: 确认测试失败**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model`

Expected: FAIL，缺少 `skinned_model.h` 或测试符号。

- [ ] **Step 3: 实现最小核心**

```cpp
inline float WrapAnimationTime(float seconds, float duration) {
  return duration > 0.0f ? std::fmod(std::max(seconds, 0.0f), duration) : 0.0f;
}
bool ValidateGltf(const GltfValidationInput& input, std::string& reason) {
  if (input.jointCount > kMaxSkinJoints) { reason = "joint count exceeds 64"; return false; }
  if (!input.hasPosition || !input.hasNormal || !input.hasUv ||
      !input.hasJoints || !input.hasWeights) { reason = "missing required vertex attribute"; return false; }
  if (!input.singleSkin || input.hasCubicSpline || !input.trianglesOnly) {
    reason = "unsupported glTF feature"; return false;
  }
  return true;
}
```

以 `glm::mix` / `glm::slerp` 实现 LINEAR，STEP 取左关键帧；按父节点递推全局矩阵，以 `global * inverseBind` 填充调色板。错误包含资产名，例如 `player.glb: animation interpolation CUBICSPLINE is unsupported`。

- [ ] **Step 4: 验证核心**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model`

Expected: PASS，覆盖循环、LINEAR、STEP、父子调色板、缺属性、65 关节和 CUBICSPLINE 拒绝。

- [ ] **Step 5: 提交**

```bash
git add native/third_party/cgltf/cgltf.h native/engine/render/skinned_model.h native/engine/render/skinned_model.cpp tests/test_skinned_model.cpp
git commit -m "feat: 添加 glTF 骨骼动画核心" -m "Prompt: M3-2 接入 glTF 模型加载与骨骼动画。"
```

### Task 2: 资产、N-API 字节桥接与许可证

**Files:**
- Create: `entry/src/main/resources/rawfile/models/{player,enemy,boss}.glb`
- Create: `assets/models/LICENSES.md`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Test: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Produces: `nativeSetModelAssets(player: ArrayBuffer, enemy: ArrayBuffer, boss: ArrayBuffer): boolean`。
- Consumes: `Surface::setModelAsset(ModelKind, std::vector<uint8_t>)`（Task 4）。

- [ ] **Step 1: 写失败桥接契约测试**

```js
assert.match(nativeBridge, /nativeSetModelAssets/);
assert.match(gamePage, /getRawFileContent\('models\/player\.glb'\)/);
assert.match(nativeBridgeCpp, /napi_is_arraybuffer/);
assert.match(nativeBridgeCpp, /std::vector<uint8_t>/);
```

- [ ] **Step 2: 确认失败**

Run: `node tests/test_bridge_contract.mjs`

Expected: FAIL，缺少桥接和 rawfile 调用。

- [ ] **Step 3: 导入资源并复制 ArrayBuffer**

从许可已核验的来源下载、审核和转换三个模型，使每份都符合全局约束并含 `idle/run/attack/hit/death`。最终 GLB 放入 rawfile；许可证清单记录文件、来源 URL、版本、作者、许可 URL 和 SHA-256。

```cpp
static bool CopyArrayBuffer(napi_env env, napi_value value, std::vector<uint8_t>& out) {
  bool isArrayBuffer = false; void* bytes = nullptr; size_t length = 0;
  return napi_is_arraybuffer(env, value, &isArrayBuffer) == napi_ok && isArrayBuffer &&
      napi_get_arraybuffer_info(env, value, &bytes, &length) == napi_ok && length > 0 &&
      (out.assign(static_cast<uint8_t*>(bytes), static_cast<uint8_t*>(bytes) + length), true);
}
```

在 `aboutToAppear` 并行读取三个 rawfile，成功后一次性 bridge；失败时记录错误但仍 `nativeStart()`，确保 Native 回退网格。

- [ ] **Step 4: 验证桥接与资产**

Run: `node tests/test_bridge_contract.mjs && test -s entry/src/main/resources/rawfile/models/player.glb && test -s entry/src/main/resources/rawfile/models/enemy.glb && test -s entry/src/main/resources/rawfile/models/boss.glb && test -f assets/models/LICENSES.md`

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add entry/src/main/resources/rawfile/models assets/models/LICENSES.md entry/src/main/cpp/native_bridge.cpp entry/src/main/ets/napi/Bridge.ets entry/src/main/ets/pages/GamePage.ets tests/test_bridge_contract.mjs
git commit -m "feat: 打包授权的角色 GLB 资源" -m "Prompt: M3-2 接入 glTF 模型加载与骨骼动画。"
```

### Task 3: GPU 蒙皮属性与统一 3D 着色器

**Files:**
- Modify: `native/engine/render/mesh.h`
- Modify: `native/engine/render/mesh.cpp`
- Modify: `native/engine/render/shader_3d.h`
- Modify: `native/engine/render/shader_3d.cpp`
- Test: `tests/test_mesh.cpp`

**Interfaces:**
- Produces: `SkinnedVertex`、属性槽 0–4、`Shader3D::setSkinPalette(const SkinPalette&)`、`Shader3D::setSkinned(bool)`。
- Consumes: Task 1 `SkinPalette`；静态 `Mesh` 继续绑定槽 0–2。

- [ ] **Step 1: 写失败布局测试**

```cpp
static_assert(offsetof(SkinnedVertex, position) == 0);
assert(kPositionAttribute == 0 && kNormalAttribute == 1 && kUvAttribute == 2);
assert(kJointsAttribute == 3 && kWeightsAttribute == 4);
```

- [ ] **Step 2: 确认失败**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_mesh.cpp native/engine/render/mesh.cpp -o /tmp/test_mesh && /tmp/test_mesh`

Expected: FAIL，骨骼顶点类型未定义。

- [ ] **Step 3: 实现 64 骨骼 shader 分支**

```glsl
uniform bool uSkinned;
uniform mat4 uJoints[64];
layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;
mat4 skin = uJoints[aJoints.x] * aWeights.x + uJoints[aJoints.y] * aWeights.y +
            uJoints[aJoints.z] * aWeights.z + uJoints[aJoints.w] * aWeights.w;
vec4 localPosition = uSkinned ? skin * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);
```

法线用 `mat3(skin)`；`setSkinPalette` 以 `glUniformMatrix4fv` 上传，若矩阵数为 0 或 >64 拒绝绘制。

- [ ] **Step 4: 验证**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_mesh.cpp native/engine/render/mesh.cpp -o /tmp/test_mesh && /tmp/test_mesh`

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/mesh.h native/engine/render/mesh.cpp native/engine/render/shader_3d.h native/engine/render/shader_3d.cpp tests/test_mesh.cpp
git commit -m "feat: 支持 GLES 骨骼蒙皮着色器" -m "Prompt: M3-2 接入 glTF 模型加载与骨骼动画。"
```

### Task 4: Surface 生命周期、动画选择和 Mesh 回退

**Files:**
- Create: `native/engine/render/render_animation.h`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`
- Test: `tests/test_render_animation.cpp`

**Interfaces:**
- Produces: `enum class RenderAnimation { Idle, Run, Attack, Hit, Death };`、`ChooseAnimation`、`Surface::setModelAsset(ModelKind, std::vector<uint8_t>)`。
- Consumes: Tasks 1–3；既有 `Player::moving`、`CombatSnapshot::currentAction`、敌人/Boss 存活状态。

- [ ] **Step 1: 写失败的优先级和 clip 回退测试**

```cpp
assert(ChooseAnimation({.alive = false}) == RenderAnimation::Death);
assert(ChooseAnimation({.alive = true, .attacking = true}) == RenderAnimation::Attack);
assert(ChooseAnimation({.alive = true, .moving = true}) == RenderAnimation::Run);
assert(ChooseAnimation({.alive = true}) == RenderAnimation::Idle);
assert(ResolveClip({"idle"}, RenderAnimation::Attack) == "idle");
```

- [ ] **Step 2: 确认失败**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_render_animation.cpp -o /tmp/test_render_animation && /tmp/test_render_animation`

Expected: FAIL，缺少 `render_animation.h`。

- [ ] **Step 3: 接入状态、GL 生命周期和降级**

`Loop::updateFixed` 只填充渲染意图：死亡 > 攻击 > 受击短时计时 > 移动 > 待机，不向 gameplay 写回。Surface 保存三份字节与 `SkinnedModel`；`init3DResources` 在有字节时解析/上传，若 bridge 晚于 Surface 创建，则在下一次 `draw3DPhase` 的 current context 尝试一次。

```cpp
void drawActor(Surface& s, SkinnedModel& model, const Mesh& fallback,
               const ActorRenderState& actor, const glm::mat4& matrix) {
  s.shader3d.setMVP(s.camera3d.viewProjection() * matrix);
  if (model.ready()) {
    s.shader3d.setSkinned(true);
    s.shader3d.setSkinPalette(model.update(actor, 1.0f / 60.0f));
    model.draw();
  } else {
    s.shader3d.setSkinned(false);
    fallback.draw();
  }
}
```

保留地面、M3-1 Mesh、2D 绘制和相机；3D 阶段开启深度测试、背面剔除，结束时关闭。销毁时先 destroy 三份 SkinnedModel，再 destroy shader。

- [ ] **Step 4: 验证**

Run: `c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_render_animation.cpp -o /tmp/test_render_animation && /tmp/test_render_animation && git diff --check`

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/render_animation.h native/engine/render/surface.h native/engine/render/surface.cpp native/engine/core/loop.cpp entry/src/main/cpp/CMakeLists.txt tests/test_render_animation.cpp
git commit -m "feat: 在场景中播放角色骨骼动画" -m "Prompt: M3-2 接入 glTF 模型加载与骨骼动画。"
```

### Task 5: 构建、真机验证与交付证据

**Files:**
- Modify: `docs/superpowers/specs/2026-07-20-gltf-skeletal-animation-design.md`（仅当最终资源来源或范围与规格不一致）
- Test: `tests/test_skinned_model.cpp`、`tests/test_render_animation.cpp`、`tests/test_mesh.cpp`、`tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: Tasks 1–4；已连接解锁的 HarmonyOS 真机。
- Produces: HAP、日志、截图/录屏和 FPS 证据。

- [ ] **Step 1: 运行新增回归测试**

```bash
c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model
c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_render_animation.cpp -o /tmp/test_render_animation && /tmp/test_render_animation
c++ -std=c++17 -I. -Inative -Inative/engine/math tests/test_mesh.cpp native/engine/render/mesh.cpp -o /tmp/test_mesh && /tmp/test_mesh
node tests/test_bridge_contract.mjs
git diff --check
```

Expected: 全部退出码为 0。

- [ ] **Step 2: 构建并安装 HAP**

Run: `DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk /Applications/DevEco-Studio.app/Contents/tools/node/bin/node /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js --mode module -p module=entry@default -p product=default -p requiredDeviceType=phone assembleHap --analyze=normal --parallel --incremental`

Expected: `BUILD SUCCESSFUL`，随后以已验证的 `hdc install -r` 安装并冷启动。

- [ ] **Step 3: 真机场景验收**

日志必须包含三份 `GLB loaded`、`3D program linked`、`EGL surface ready`，且不含 `SIGSEGV`、`EGL.*failed`、`shader.*failed`。在训练、普通敌人和 Boss 场景截图/录屏，确认三种角色不同，移动/攻击/受击/死亡动画可辨，HUD FPS >= 30。

- [ ] **Step 4: 提交验证文档（如新增）**

```bash
git add docs/superpowers/specs/2026-07-20-gltf-skeletal-animation-design.md
git commit -m "docs: 记录 M3-2 真机验证结果" -m "Prompt: M3-2 接入 glTF 模型加载与骨骼动画。"
```

- [ ] **Step 5: 最终状态检查**

Run: `git status --short && git log --oneline -5`

Expected: 仅保留用户原有的 `build-profile.json5` 未提交修改；M3-2 文件均已提交。

## Self-Review

- 规格覆盖：Task 1 实现 GLB 子集/64 骨骼/插值与拒绝；Task 2 实现资源和许可；Task 3 实现 GLES 蒙皮；Task 4 实现状态映射和 Mesh 降级；Task 5 覆盖构建、真机、FPS、日志。
- 占位符扫描：无 TBD/TODO 或未定义的“适当处理”；资源审核标准在全局约束和 Task 2 明确规定。
- 类型一致性：`SkinnedModel`、`SkinPalette`、`RenderAnimation`、`ModelKind` 在生产/消费任务中均有精确声明；实现不得改名。

