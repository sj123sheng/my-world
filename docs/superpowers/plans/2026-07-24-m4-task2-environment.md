# M4 Task 2 Lost Ruins Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 HarmonyOS Native 3D 渲染链路中加入 CC0 高细节环形遗迹、中心裂隙、天空与光照，并在 Pura 70 Pro 达到常态 30 FPS、首领区最低 24 FPS。

**Architecture:** 使用独立 `StaticModel` 解析静态 GLB，将环境划分为外围、中心、远景、装饰四个稳定批次；ArkTS 一次并发读取四份 rawfile，并通过独立 N-API 原子提交给 `Surface`。`EnvironmentController` 只根据摄像机位置和既有 `PerformanceGuard` 输出选择可见批次与纹理等级，实际解析、上传、替换和销毁继续限定在有效 GLES context 下。

**Tech Stack:** C++17、OpenGL ES 3.0、EGL、cgltf、stb_image、glm、HarmonyOS N-API、ArkTS rawfile、Node.js 合约测试、Hvigor、HDC

## Global Constraints

- 只使用 CC0 或等价公共领域资产；不接受仅免费下载或许可证不明确的资源。
- 主资产固定为 Poly Haven `modular_fort_01`，补充材质固定为 Poly Haven `rabdentse_ruins_wall`。
- 资产许可依据固定为 `https://polyhaven.com/license`，资源页分别为 `https://polyhaven.com/a/modular_fort_01` 与 `https://polyhaven.com/a/rabdentse_ruins_wall`。
- 原始下载 URL、作者、下载日期、源文件 SHA-256 和派生文件 SHA-256 必须写入项目清单。
- 环境使用外围遗迹环、中心裂隙、边界背景、装饰四个稳定批次；不得依赖容器迭代顺序。
- 高细节模式为默认模式；常态不低于 30 FPS，复杂首领区域不低于 24 FPS。
- 性能等级扩展为五档；降级顺序固定为：Medium 隐藏远景 → Heavy 隐藏装饰 →
  Critical 使用内置半分辨率环境纹理。
- 性能降级不得改变玩家、敌人、首领、战斗状态、AI、输入响应或动画。
- 环境失败必须回退到现有程序化地面和基础几何体，应用仍可启动和战斗。
- 静态环境不得创建、销毁或恢复 NativeWindow/EGL；所有 GL 资源操作必须发生在有效 current context 下。
- 不实现 Task 3 流程、Task 4 演出、动态天气、昼夜循环、实时全局光照或运行时联网加载。
- Git 提交必须使用类型前缀，并包含 `Prompt:` 行。

---

### Task 1: CC0 资产获取、派生与证据清单

**Files:**
- Create: `automation/assets/fetch_environment_assets.mjs`
- Create: `automation/assets/validate_environment_assets.mjs`
- Create: `assets/environment/manifest.json`
- Create: `assets/environment/LICENSES.md`
- Create: `assets/environment/layout.json`
- Create: `tests/test_environment_assets.mjs`
- Create during execution: `entry/src/main/resources/rawfile/environment/outer_ring.glb`
- Create during execution: `entry/src/main/resources/rawfile/environment/center_rift.glb`
- Create during execution: `entry/src/main/resources/rawfile/environment/backdrop.glb`
- Create during execution: `entry/src/main/resources/rawfile/environment/decoration.glb`

**Interfaces:**
- Consumes: Poly Haven public API `https://api.polyhaven.com/files/{assetId}` and CC0 asset IDs `modular_fort_01`, `rabdentse_ruins_wall`
- Produces: `assets/environment/manifest.json` with `sourceAssets[]` and `derivedAssets[]`
- Produces: four self-contained GLB 2.0 rawfiles with embedded buffers and PNG/JPEG textures

- [ ] **Step 1: Write the failing manifest contract test**

Create `tests/test_environment_assets.mjs`:

```js
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';

const manifestUrl = new URL('../assets/environment/manifest.json', import.meta.url);
const manifest = JSON.parse(await readFile(manifestUrl, 'utf8'));

assert.equal(manifest.license, 'CC0-1.0');
assert.equal(manifest.licenseUrl, 'https://polyhaven.com/license');
assert.deepEqual(manifest.sourceAssets.map((asset) => asset.id),
  ['modular_fort_01', 'rabdentse_ruins_wall']);

for (const source of manifest.sourceAssets) {
  assert.match(source.author, /\\S/);
  assert.match(source.pageUrl, /^https:\\/\\/polyhaven\\.com\\/a\\//);
  assert.match(source.downloadUrl, /^https:\\/\\/dl\\.polyhaven\\.org\\//);
  assert.match(source.sha256, /^[a-f0-9]{64}$/);
  assert.match(source.downloadedAt, /^\\d{4}-\\d{2}-\\d{2}$/);
}

for (const asset of manifest.derivedAssets) {
  const bytes = await readFile(asset.path);
  const digest = createHash('sha256').update(bytes).digest('hex');
  assert.equal(digest, asset.sha256, asset.path);
  assert.ok(bytes.length > 20, asset.path);
  assert.equal(bytes.subarray(0, 4).toString('ascii'), 'glTF', asset.path);
}
```

- [ ] **Step 2: Run the test to verify RED**

Run:

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_environment_assets.mjs
```

Expected: FAIL with `ENOENT` for `assets/environment/manifest.json`.

- [ ] **Step 3: Implement deterministic acquisition and validation**

Create `automation/assets/fetch_environment_assets.mjs` with these exact public functions:

```js
export async function fetchPolyHavenFileIndex(assetId) {
  const response = await fetch(`https://api.polyhaven.com/files/${assetId}`);
  if (!response.ok) throw new Error(`${assetId}: metadata HTTP ${response.status}`);
  return response.json();
}

export function chooseGltfDownload(index, resolution = '2k') {
  const candidates = [];
  const visit = (node) => {
    if (typeof node === 'string') {
      if (node.startsWith('https://dl.polyhaven.org/') &&
          /gltf|glb/i.test(node) && node.includes(resolution)) {
        candidates.push(node);
      }
      return;
    }
    if (!node || typeof node !== 'object') return;
    if (typeof node.url === 'string' && node.url.startsWith('https://dl.polyhaven.org/') &&
        /gltf|glb/i.test(node.url) && node.url.includes(resolution)) {
      candidates.push(node.url);
    }
    for (const value of Object.values(node)) visit(value);
  };
  visit(index);
  candidates.sort();
  if (candidates.length === 0) {
    throw new Error(`no ${resolution} glTF download in Poly Haven metadata`);
  }
  return candidates[0];
}

export function sha256(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}
```

The same file must implement and export these deterministic conversion
functions; tests import them directly, so none may be left as a command-line-only
helper: `readGlb(bytes)`, `bakeNodeTransforms(document)`,
`partitionNodes(document, layout)`, `mergePrimitivesByMaterial(region)`,
`embedTextureLevels(region, diffuse2k, diffuse1k)`, and `writeGlb(document)`.

Create `assets/environment/layout.json` as the authoritative placement file. It
contains only source node names plus explicit translation/rotation/scale and one
of `outerRing`, `centerRift`, `backdrop`, `decoration`. `partitionNodes` must
reject an unknown region, duplicate placement ID, non-finite transform, or a
source node that does not exist. Sort placements by ID, materials by source
material name, and JSON keys before writing; use fixed PNG encoder settings so
two runs produce byte-identical GLBs.

`readGlb` validates magic/version/total length and accepts exactly one JSON plus
one BIN chunk. `bakeNodeTransforms` multiplies positions by the full node matrix,
normals by its inverse-transpose 3×3 matrix, normalizes them, and resets the node
transform to identity. `mergePrimitivesByMaterial` concatenates accessors while
rebasing indices and retaining material boundaries. `embedTextureLevels` creates
named image records `diffuse_full` and `diffuse_half`, each backed by an embedded
bufferView. `writeGlb` serializes sorted JSON without insignificant whitespace,
pads JSON with spaces and BIN with zero bytes to four-byte boundaries, and writes
the standard 12-byte GLB header followed by JSON and BIN chunk headers.

The script must:

1. Fetch metadata only through Poly Haven’s public API.
2. Select the 2K glTF/GLB source for `modular_fort_01`.
3. Select the 2K glTF material package for `rabdentse_ruins_wall`.
4. Save source downloads under a temporary directory, never under Git.
5. Reject external buffer/image URIs after conversion.
6. Read `assets/environment/layout.json` and build four deterministic scene GLBs:
   - `outer_ring.glb`: perimeter wall, arches, pillars and the walkable ring.
   - `center_rift.glb`: central platform and stone frame; the red rift plane remains procedural.
   - `backdrop.glb`: unreachable distant towers and wall silhouettes.
   - `decoration.glb`: rubble and small non-collision details.
7. Bake node transforms into vertices and merge primitives by material inside each region.
8. Embed the 2K diffuse texture and generate a 1K mip source in the same material record.
9. Write the actual source and derived SHA-256 values to `assets/environment/manifest.json`.

Create `automation/assets/validate_environment_assets.mjs`:

```js
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';

const manifest = JSON.parse(await readFile('assets/environment/manifest.json', 'utf8'));
if (manifest.license !== 'CC0-1.0' ||
    manifest.licenseUrl !== 'https://polyhaven.com/license') {
  throw new Error('environment manifest must declare Poly Haven CC0-1.0');
}
for (const entry of [...manifest.sourceAssets, ...manifest.derivedAssets]) {
  if (!/^[a-f0-9]{64}$/.test(entry.sha256)) {
    throw new Error(`${entry.id ?? entry.path}: invalid SHA-256`);
  }
}
for (const derived of manifest.derivedAssets) {
  const bytes = await readFile(derived.path);
  const actual = createHash('sha256').update(bytes).digest('hex');
  if (actual !== derived.sha256) throw new Error(`${derived.path}: SHA-256 mismatch`);
}
```

Create `assets/environment/LICENSES.md` documenting:

- `Modular Fort 01`, author Rico Cilliers, resource page and Poly Haven license page.
- `Rabdentse Ruins Wall`, author Amal Kumar, resource page and Poly Haven license page.
- The four derived GLBs and their exact source dependencies.
- CC0 does not require attribution, but this project retains provenance voluntarily.

- [ ] **Step 4: Fetch assets and verify GREEN**

Run:

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  automation/assets/fetch_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  automation/assets/validate_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_environment_assets.mjs
```

Expected: all commands exit 0; four GLBs exist and every recorded SHA-256 matches.

- [ ] **Step 5: Commit**

```bash
git add automation/assets/fetch_environment_assets.mjs \
  automation/assets/validate_environment_assets.mjs \
  assets/environment/manifest.json assets/environment/LICENSES.md \
  assets/environment/layout.json \
  entry/src/main/resources/rawfile/environment/*.glb \
  tests/test_environment_assets.mjs
git commit -m "feat: 导入 CC0 遗迹环境资产" \
  -m "登记 Poly Haven 来源与哈希，并生成四个自包含环境批次。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```

---

### Task 2: 静态 GLB 模型解析与纹理等级

**Files:**
- Create: `native/engine/render/static_model.h`
- Create: `native/engine/render/static_model.cpp`
- Create: `tests/test_static_model.cpp`
- Modify: `tests/gltf_fixture_builder.h`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `Mesh`, `Shader3D`, cgltf GLB 2.0 parser, stb_image embedded PNG/JPEG decoder
- Produces: `StaticModel::tryInitialize(const std::vector<uint8_t>&, const std::string&) -> bool`
- Produces: `StaticModel::setTextureTier(StaticTextureTier)`, `draw(Shader3D&)`, `destroy()`, `abandonGpuResources()`
- Produces: `StaticModelStats { primitiveCount, triangleCount, textureBytes }`

- [ ] **Step 1: Write failing parser and lifecycle tests**

Add `gltf_fixture::makeStaticSceneGlb(bool embeddedTexture = true)` and create
`tests/test_static_model.cpp`:

```cpp
#include "native/engine/render/static_model.h"
#include "tests/gltf_fixture_builder.h"

#include <cassert>

void testParsesStaticSceneAndBakesNodeTransform() {
  StaticModel model;
  assert(model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "outer.glb"));
  assert(model.ready());
  assert(model.stats().primitiveCount == 1);
  assert(model.stats().triangleCount == 1);
  assert(model.cpuVerticesForTest().front().position.x == 2.0f);
}

void testRejectsSkinAndExternalUris() {
  StaticModel model;
  assert(!model.tryInitialize(gltf_fixture::makeMinimalGlb(), "skinned.glb"));
  assert(model.lastError() == "skinned.glb: static environment must not contain skins");
  assert(!model.tryInitialize(
      gltf_fixture::makeStaticSceneWithExternalUri(), "external.glb"));
  assert(model.lastError() == "external.glb: external buffer/image URI is unsupported");
}

void testTextureTierTransitionIsStable() {
  StaticModel model;
  assert(model.tryInitialize(gltf_fixture::makeStaticSceneGlb(), "outer.glb"));
  assert(model.textureTier() == StaticTextureTier::Full);
  model.setTextureTier(StaticTextureTier::Half);
  assert(model.textureTier() == StaticTextureTier::Half);
  model.setTextureTier(StaticTextureTier::Half);
  assert(model.textureTier() == StaticTextureTier::Half);
}

int main() {
  testParsesStaticSceneAndBakesNodeTransform();
  testRejectsSkinAndExternalUris();
  testTextureTierTransitionIsStable();
}
```

- [ ] **Step 2: Run the test to verify RED**

Run:

```bash
SDKROOT_PATH=$(xcrun --sdk macosx --show-sdk-path)
CLANGXX_PATH=$(xcrun --find clang++)
"$CLANGXX_PATH" -std=c++17 -isysroot "$SDKROOT_PATH" \
  -I"$SDKROOT_PATH/usr/include/c++/v1" -I. -Inative -Inative/engine/math \
  tests/test_static_model.cpp native/engine/render/static_model.cpp \
  native/engine/render/mesh.cpp native/engine/render/texture.cpp \
  -o /tmp/test_static_model_task2
```

Expected: FAIL because `static_model.h` does not exist.

- [ ] **Step 3: Implement the minimal static model API**

Create `native/engine/render/static_model.h`:

```cpp
#pragma once

#include "native/engine/render/mesh.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Shader3D;

enum class StaticTextureTier { Full, Half };

struct StaticModelStats {
  std::size_t primitiveCount = 0;
  std::size_t triangleCount = 0;
  std::size_t textureBytes = 0;
};

class StaticModel {
 public:
  bool tryInitialize(const std::vector<uint8_t>& bytes,
                     const std::string& assetName);
  bool ready() const;
  const std::string& lastError() const;
  const StaticModelStats& stats() const;
  StaticTextureTier textureTier() const;
  void setTextureTier(StaticTextureTier tier);
  void draw(Shader3D& shader);
  void destroy();
  void abandonGpuResources();
  const std::vector<Vertex>& cpuVerticesForTest() const;

 private:
  bool ready_ = false;
  std::string lastError_;
  StaticModelStats stats_;
  StaticTextureTier textureTier_ = StaticTextureTier::Full;
  std::vector<Mesh> primitives_;
  std::vector<Vertex> testVertices_;
};
```

Implement `static_model.cpp` with these exact validation rules:

- Input must be GLB 2.0.
- Scene must contain no skins or animations.
- Primitive mode must be triangles.
- `POSITION` and `NORMAL` are required; `TEXCOORD_0` is optional and defaults to zero.
- Indices may be unsigned byte, unsigned short or unsigned int.
- Buffers and images must be embedded.
- Node world transforms are baked into positions and inverse-transpose transformed normals.
- Primitives sharing one embedded base-color texture are merged.
- Full and Half textures use the GLB 内置 `diffuse_full` 与 `diffuse_half`
  image records; runtime 不执行缩放。
- Tier changes mark textures dirty; the selected embedded image uploads during
  the next draw while context is current.
- `destroy()` deletes GL objects; `abandonGpuResources()` only clears tracked handles.

Add `static_model.cpp` to `entry/src/main/cpp/CMakeLists.txt`.

- [ ] **Step 4: Run tests to verify GREEN**

Run the compile command from Step 2, then:

```bash
/tmp/test_static_model_task2
```

Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add native/engine/render/static_model.h \
  native/engine/render/static_model.cpp tests/test_static_model.cpp \
  tests/gltf_fixture_builder.h entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 增加静态环境 GLB 管线" \
  -m "解析合批静态场景并支持全分辨率和半分辨率纹理。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```

---

### Task 3: 环境批次状态、可见性与性能降级

**Files:**
- Create: `native/engine/render/environment.h`
- Create: `native/engine/render/environment.cpp`
- Create: `tests/test_environment.cpp`
- Modify: `native/engine/render/performance_guard.h`
- Modify: `native/engine/render/performance_guard.cpp`
- Modify: `tests/test_performance_guard.cpp`

**Interfaces:**
- Consumes: camera target position in normalized gameplay coordinates and `PerformanceGuard::level()`
- Produces: `EnvironmentController::evaluate(glm::vec2 cameraTarget, int32_t perfLevel) -> EnvironmentRenderPlan`
- Produces: stable `EnvironmentBatchKind` values and `EnvironmentBatchStatus`

- [ ] **Step 1: Write failing policy tests**

Create `tests/test_environment.cpp`:

```cpp
#include "native/engine/render/environment.h"

#include <cassert>

void testFullQualityShowsAllBatches() {
  EnvironmentController controller;
  const EnvironmentRenderPlan plan = controller.evaluate({0.5f, 0.5f}, 0);
  assert(plan.outerRing);
  assert(plan.centerRift);
  assert(plan.backdrop);
  assert(plan.decoration);
  assert(plan.textureTier == StaticTextureTier::Full);
}

void testDegradationOrderIsFixed() {
  EnvironmentController controller;
  const auto light = controller.evaluate({0.5f, 0.5f}, 1);
  assert(light.backdrop && light.decoration);
  const auto medium = controller.evaluate({0.5f, 0.5f}, 2);
  assert(!medium.backdrop && medium.decoration);
  const auto heavy = controller.evaluate({0.5f, 0.5f}, 3);
  assert(!heavy.backdrop && !heavy.decoration);
  assert(heavy.textureTier == StaticTextureTier::Full);
  const auto critical = controller.evaluate({0.5f, 0.5f}, 4);
  assert(!critical.backdrop && !critical.decoration);
  assert(critical.outerRing && critical.centerRift);
  assert(critical.textureTier == StaticTextureTier::Half);
}

void testCenterRiftNeverDisappears() {
  EnvironmentController controller;
  for (int level = 0; level <= 4; ++level) {
    assert(controller.evaluate({1.0f, 1.0f}, level).centerRift);
  }
}

void testCameraZoneSuppressesCloseRangeClutter() {
  EnvironmentController controller;
  const auto center = controller.evaluate({0.5f, 0.75f}, 0);
  const auto outer = controller.evaluate({0.15f, 0.15f}, 0);
  assert(!center.decoration);
  assert(outer.decoration);
  assert(center.outerRing && center.centerRift);
}

int main() {
  testFullQualityShowsAllBatches();
  testDegradationOrderIsFixed();
  testCenterRiftNeverDisappears();
  testCameraZoneSuppressesCloseRangeClutter();
}
```

- [ ] **Step 2: Run the test to verify RED**

Run:

```bash
SDKROOT_PATH=$(xcrun --sdk macosx --show-sdk-path)
CLANGXX_PATH=$(xcrun --find clang++)
"$CLANGXX_PATH" -std=c++17 -isysroot "$SDKROOT_PATH" \
  -I"$SDKROOT_PATH/usr/include/c++/v1" -I. -Inative -Inative/engine/math \
  tests/test_environment.cpp native/engine/render/environment.cpp \
  -o /tmp/test_environment_task2
```

Expected: FAIL because `environment.h` does not exist.

- [ ] **Step 3: Implement stable environment contracts**

Create `native/engine/render/environment.h`:

```cpp
#pragma once

#include "native/engine/render/static_model.h"

#include <glm/vec2.hpp>
#include <cstdint>

enum class EnvironmentBatchKind : uint8_t {
  OuterRing = 0,
  CenterRift = 1,
  Backdrop = 2,
  Decoration = 3,
};

enum class EnvironmentBatchStatus : uint8_t {
  Empty,
  Pending,
  Ready,
  Failed,
};

struct EnvironmentRenderPlan {
  bool outerRing = true;
  bool centerRift = true;
  bool backdrop = true;
  bool decoration = true;
  StaticTextureTier textureTier = StaticTextureTier::Full;
};

class EnvironmentController {
 public:
  EnvironmentRenderPlan evaluate(glm::vec2 cameraTarget,
                                 int32_t perfLevel) const;
};
```

Implement `evaluate`:

```cpp
EnvironmentRenderPlan EnvironmentController::evaluate(
    glm::vec2 cameraTarget, int32_t perfLevel) const {
  EnvironmentRenderPlan plan;
  const glm::vec2 centerDelta = cameraTarget - glm::vec2(0.5f, 0.75f);
  if (glm::dot(centerDelta, centerDelta) <= 0.24f * 0.24f) {
    plan.decoration = false;
  }
  if (perfLevel >= 2) plan.backdrop = false;
  if (perfLevel >= 3) plan.decoration = false;
  if (perfLevel >= 4) {
    plan.textureTier = StaticTextureTier::Half;
  }
  return plan;
}
```

This task remains a pure policy module. Add render observability to
`GameSnapshot` only in Task 5, where `Surface` can publish real draw statistics;
do not synthesize render values inside `Loop`.

- [ ] **Step 4: Run policy and regression tests**

Run:

```bash
/tmp/test_environment_task2
```

Then compile and run `tests/test_performance_guard.cpp` and
`tests/test_loop_integration.cpp`.

Expected: all exit 0; existing combat snapshot fields remain unchanged.

- [ ] **Step 5: Commit**

```bash
git add native/engine/render/environment.h \
  native/engine/render/environment.cpp tests/test_environment.cpp \
  native/engine/render/performance_guard.h \
  native/engine/render/performance_guard.cpp tests/test_performance_guard.cpp
git commit -m "feat: 定义环境分区和降级策略" \
  -m "固定远景、装饰和纹理等级的降级顺序并发布观测指标。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```

---

### Task 4: ArkTS 与 N-API 环境资产原子桥接

**Files:**
- Create: `native/platform/harmony/environment_asset_commit.h`
- Create: `tests/test_environment_asset_commit.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: four `ArrayBuffer` values ordered outer, center, backdrop, decoration
- Produces: `nativeSetEnvironmentAssets(outer, center, backdrop, decoration) -> boolean`
- Produces: all-or-nothing CPU-byte commit under one `Loop::withLifecycle` call

- [ ] **Step 1: Write failing atomic commit tests**

Create `tests/test_environment_asset_commit.cpp`:

```cpp
#include "native/platform/harmony/environment_asset_commit.h"

#include <cassert>
#include <vector>

int main() {
  std::vector<EnvironmentAssetSlot> committed;
  int lifecycleCalls = 0;
  const bool ok = CopyAndCommitEnvironmentAssets(
      [](EnvironmentAssetSlot slot, std::vector<uint8_t>& out) {
        out.assign(1, static_cast<uint8_t>(slot) + 1);
        return true;
      },
      [&lifecycleCalls](auto operation) {
        ++lifecycleCalls;
        operation();
      },
      [&committed](EnvironmentAssetSlot slot, std::vector<uint8_t> bytes) {
        assert(!bytes.empty());
        committed.push_back(slot);
      });
  assert(ok);
  assert(lifecycleCalls == 1);
  assert(committed.size() == 4);

  int failedCommits = 0;
  assert(!CopyAndCommitEnvironmentAssets(
      [](EnvironmentAssetSlot slot, std::vector<uint8_t>& out) {
        if (slot == EnvironmentAssetSlot::Backdrop) return false;
        out.assign(1, 1);
        return true;
      },
      [](auto operation) { operation(); },
      [&failedCommits](EnvironmentAssetSlot, std::vector<uint8_t>) {
        ++failedCommits;
      }));
  assert(failedCommits == 0);
}
```

Extend `tests/test_bridge_contract.mjs` to require:

```js
for (const batch of ['outer_ring', 'center_rift', 'backdrop', 'decoration']) {
  assert.match(page,
    new RegExp(`getRawFileContent\\\\(['"]environment/${batch}\\\\.glb['"]\\\\)`));
}
assert.match(bridge, /nativeSetEnvironmentAssets/);
assert.match(nativeBridge, /CopyAndCommitEnvironmentAssets/);
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```bash
c++ -std=c++17 -I. tests/test_environment_asset_commit.cpp \
  -o /tmp/test_environment_asset_commit
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_bridge_contract.mjs
```

Expected: C++ compile fails because the helper does not exist; Node test fails because
environment rawfiles and bridge export are absent.

- [ ] **Step 3: Implement atomic four-buffer bridge**

Create `native/platform/harmony/environment_asset_commit.h`:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

enum class EnvironmentAssetSlot : size_t {
  OuterRing = 0,
  CenterRift = 1,
  Backdrop = 2,
  Decoration = 3,
};

using EnvironmentAssetBatch = std::array<std::vector<uint8_t>, 4>;

template <typename Copy, typename WithLifecycle, typename Commit>
bool CopyAndCommitEnvironmentAssets(Copy&& copy,
                                    WithLifecycle&& withLifecycle,
                                    Commit&& commit) {
  EnvironmentAssetBatch assets;
  for (size_t index = 0; index < assets.size(); ++index) {
    if (!copy(static_cast<EnvironmentAssetSlot>(index), assets[index])) {
      return false;
    }
  }
  withLifecycle([&assets, &commit]() {
    for (size_t index = 0; index < assets.size(); ++index) {
      commit(static_cast<EnvironmentAssetSlot>(index),
             std::move(assets[index]));
    }
  });
  return true;
}
```

Implement `NativeSetEnvironmentAssets` with exactly four arguments. Add:

```ts
export const nativeSetEnvironmentAssets = (
  outer: ArrayBuffer,
  center: ArrayBuffer,
  backdrop: ArrayBuffer,
  decoration: ArrayBuffer
): boolean => native.nativeSetEnvironmentAssets(
  outer, center, backdrop, decoration);
```

In `GamePage.loadModelAssets`, keep character assets and environment assets in
separate failure domains:

```ts
const characterAssets = await Promise.all([
  this.loadRawfile('models/player.glb'),
  this.loadRawfile('models/enemy.glb'),
  this.loadRawfile('models/boss.glb')
]);
if (generation !== this.modelLoadGeneration) return;
nativeSetModelAssets(...characterAssets);
try {
  const environmentAssets = await Promise.all([
    this.loadRawfile('environment/outer_ring.glb'),
    this.loadRawfile('environment/center_rift.glb'),
    this.loadRawfile('environment/backdrop.glb'),
    this.loadRawfile('environment/decoration.glb')
  ]);
  if (generation === this.modelLoadGeneration) {
    nativeSetEnvironmentAssets(...environmentAssets);
  }
} catch (error) {
  hilog.error(0x0000, 'MyWorld', 'environment asset load failed: %{public}s',
    `${error}`);
} finally {
  if (generation === this.modelLoadGeneration) this.nativeStartIfForeground();
}
```

Retain the existing outer character-load error handling. Thus an environment
failure is logged and falls back to procedural geometry, while a character
failure retains Task 1 behavior.

- [ ] **Step 4: Run tests to verify GREEN**

Run the two commands from Step 2.

Expected: both exit 0.

- [ ] **Step 5: Commit**

```bash
git add native/platform/harmony/environment_asset_commit.h \
  tests/test_environment_asset_commit.cpp \
  entry/src/main/cpp/native_bridge.cpp \
  entry/src/main/ets/napi/Bridge.ets \
  entry/src/main/cpp/types/libnative_game/Index.d.ts \
  entry/src/main/ets/pages/GamePage.ets tests/test_bridge_contract.mjs
git commit -m "feat: 接入环境资产原子桥接" \
  -m "并发读取四个环境批次并在单次生命周期锁内提交。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```

---

### Task 5: Surface 环境生命周期、光照和程序化回退

**Files:**
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/render/shader_3d.h`
- Modify: `native/engine/render/shader_3d.cpp`
- Modify: `native/engine/render/mesh.h`
- Modify: `native/engine/render/mesh.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `tests/test_render_animation.cpp`
- Modify: `tests/test_shader_3d.cpp`
- Modify: `tests/test_mesh.cpp`

**Interfaces:**
- Consumes: four pending environment byte buffers and `EnvironmentRenderPlan`
- Produces: GL-context-bound batch initialization and atomic per-batch replacement
- Produces: `Shader3D::setEnvironmentTint`, procedural gradient sky, center rift plane
- Produces: fallback pillars/walls/markers when environment batches fail
- Produces: snapshot fields `environmentReady`, `environmentDrawCalls`,
  `environmentTriangles` copied from the most recently completed `Surface` frame

- [ ] **Step 1: Write failing lifecycle and fallback tests**

Extend `tests/test_render_animation.cpp`:

```cpp
void testSurfaceKeepsEnvironmentBytesUntilContextBoundInitialization() {
  Surface surface;
  surface.setEnvironmentAsset(EnvironmentBatchKind::OuterRing, {1, 2, 3});
  assert(surface.environmentAssets[0].pending);
  assert(surface.environmentAssets[0].bytes.size() == 3);
}

void testEnvironmentFailureKeepsFallbackEnabled() {
  Surface surface;
  surface.environmentStatuses[0] = EnvironmentBatchStatus::Failed;
  assert(surface.shouldDrawEnvironmentFallback());
}
```

Extend `tests/test_mesh.cpp`:

```cpp
void testCylinderHasFiniteNormalsAndTriangles() {
  Mesh cylinder = createCylinder(1.0f, 2.0f, 16);
  assert(!cylinder.vertices.empty());
  assert(cylinder.indices.size() == 16u * 12u);
  for (const Vertex& vertex : cylinder.vertices) {
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
  }
}
```

- [ ] **Step 2: Run tests to verify RED**

Compile and run `tests/test_render_animation.cpp`, `tests/test_mesh.cpp` and
`tests/test_shader_3d.cpp`.

Expected: compile failures for missing environment APIs and `createCylinder`.

- [ ] **Step 3: Implement environment ownership and draw order**

Add to `Surface`:

```cpp
std::array<PendingModelAsset, 4> environmentAssets;
std::array<StaticModel, 4> environmentModels;
std::array<EnvironmentBatchStatus, 4> environmentStatuses{
    EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty,
    EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty};
EnvironmentController environmentController;
EnvironmentRenderPlan environmentPlan;
Mesh fallbackPillarMesh;
Mesh fallbackWallMesh;
Mesh riftPlaneMesh;
bool environmentReady = false;
uint32_t environmentDrawCalls = 0;
uint32_t environmentTriangles = 0;
```

Add:

```cpp
void setEnvironmentAsset(EnvironmentBatchKind kind, std::vector<uint8_t> bytes);
bool shouldDrawEnvironmentFallback() const;
```

`surface_draw` order must be:

1. Gradient sky/clear background.
2. Ground plane with alternating cold-gray grid.
3. Backdrop batch when visible.
4. Outer ring batch or fallback pillars/walls.
5. Decoration batch when visible.
6. Center batch and procedural red rift plane.
7. Player, training target, enemies and boss.
8. Existing particles/VFX and UI-owned overlays.

Implement `createCylinder(float radius, float height, uint32_t segments)` for
fallback pillars. Fallback layout must use fixed transforms representing the ring,
not random placement.

Update `Shader3D` fragment output with:

```glsl
vec3 lit = baseColor.rgb * (uAmbient + uLightColor * diffuse);
vec3 finalColor = mix(lit, uEnvironmentTint, uEnvironmentTintStrength);
```

Use:

- sky top `#18243d`, horizon `#46515d`;
- outer ambient `{0.18, 0.20, 0.24}`;
- light direction normalized `{0.35, 0.85, 0.25}`;
- center tint `{0.35, 0.03, 0.02}`;
- rift emissive-looking base `{0.95, 0.08, 0.04}`.

Extend `PerformanceGuard` with `Critical = 4`; enter it only after Heavy remains
below 24 FPS for its existing downgrade window, and recover through Heavy before
Medium. On `PerfLevel::Critical`, call `setTextureTier(Half)` only on ready
environment models. On recovery, return to `Full`. Log only batch status/tier
changes.

Reset the three per-frame counters before environment drawing, increment them
only after a batch or fallback draw is submitted, and set `environmentReady`
when both mandatory batches (`OuterRing`, `CenterRift`) are either ready or
covered by fallback geometry. At the existing `Loop` snapshot publication point,
copy these three `Surface` fields into `GameSnapshot`; rendering must never mutate
combat or movement fields.

Extend all existing GL resource release paths so `StaticModel::destroy()` occurs
before shader/context destruction and `abandonGpuResources()` occurs after
context loss.

- [ ] **Step 4: Run focused regressions**

Run:

```bash
/tmp/test_static_model_task2
/tmp/test_environment_task2
```

Compile and run `test_render_animation`, `test_mesh`, `test_shader_3d`,
`test_loop_lifecycle` and `test_performance_guard`.

Expected: all exit 0; `git diff --check` has no output.

- [ ] **Step 5: Commit**

```bash
git add native/engine/render/surface.h native/engine/render/surface.cpp \
  native/engine/render/shader_3d.h native/engine/render/shader_3d.cpp \
  native/engine/render/mesh.h native/engine/render/mesh.cpp \
  native/engine/core/game_snapshot.h native/engine/core/loop.cpp \
  tests/test_render_animation.cpp tests/test_shader_3d.cpp tests/test_mesh.cpp
git commit -m "feat: 渲染环形遗迹与裂隙氛围" \
  -m "按环境分区绘制高细节遗迹，并提供固定几何回退和纹理降级。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```

---

### Task 6: 完整回归、HAP 构建和 Pura 70 Pro 验收

**Files:**
- Modify: `README.md`
- Modify: `automation/perf/profile_collect.sh`
- Test: `tests/test_environment_assets.mjs`
- Test: `tests/test_static_model.cpp`
- Test: `tests/test_environment.cpp`
- Test: `tests/test_environment_asset_commit.cpp`
- Test: `tests/test_bridge_contract.mjs`

**Interfaces:**
- Consumes: signed HAP, HDC target `127.0.0.1:5555`, environment metrics in snapshot/HiLog
- Produces: Task 2 verification record, screenshots and HAP SHA-256

- [ ] **Step 1: Extend performance collection**

Update `automation/perf/profile_collect.sh` so each sample records:

```text
timestamp,pid,fps,perf_level,environment_ready,environment_draw_calls,
environment_triangles,environment_texture_tier,encounter_mode
```

The script must fail when:

- application PID disappears;
- any normal traversal sample is below 30 FPS for more than two consecutive seconds;
- any boss-area sample is below 24 FPS for more than two consecutive seconds;
- HiLog contains `SIGSEGV|cppcrash|glGetString.*invalid|RequestBuffer.*fail|EGL.*error`.

- [ ] **Step 2: Run complete focused regression**

Run:

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  automation/assets/validate_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_bridge_contract.mjs
```

Compile and run:

- `test_static_model`
- `test_environment`
- `test_environment_asset_commit`
- `test_render_animation`
- `test_skinned_model`
- `test_performance_guard`
- `test_loop_integration`
- `test_loop_lifecycle`
- `test_action_state_machine`
- `test_encounter_controller`
- `test_boss_controller`

Expected: all exit 0 and `git diff --check` has no output.

- [ ] **Step 3: Build signed HAP**

Preflight: `build-profile.json5` must remain free of committed certificates,
passwords and key material. Configure auto-signing through the current
developer's DevEco Studio local credential store. If DevEco temporarily writes
signing data into the tracked file, build first, then restore that file to the
clean repository version before staging anything; never print or commit its
credential values.

Run:

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal --parallel \
--incremental --daemon
```

Expected: `BUILD SUCCESSFUL` and
`entry/build/default/outputs/default/entry-default-signed.hap` exists.

- [ ] **Step 4: Install and capture immutable build/device evidence**

Run:

```bash
HDC=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc
HAP=entry/build/default/outputs/default/entry-default-signed.hap
shasum -a 256 "$HAP"
"$HDC" list targets
"$HDC" install -r "$HAP"
"$HDC" shell hilog -r
"$HDC" shell aa force-stop com.ethelandev.myworld
"$HDC" shell aa start -a EntryAbility -b com.ethelandev.myworld -m entry
"$HDC" shell pidof com.ethelandev.myworld
"$HDC" shell param get const.product.model
"$HDC" shell uname -m
```

Expected: target `127.0.0.1:5555`, install success, non-empty PID and `aarch64`.

- [ ] **Step 5: Validate visual route and performance**

Capture screenshots at:

1. Spawn platform looking toward the center rift.
2. Outer ring with arches/pillars and stone material visible.
3. Ruined altar exploration node.
4. Collapsed camp exploration node.
5. Center boss region with distinct red tint.

Traverse the full ring once, then start the boss encounter and collect at least
30 seconds of samples in each phase. Expected:

- normal route has no sustained FPS below 30;
- boss area has no sustained FPS below 24;
- center and outer batches remain visible at all quality levels;
- forced Heavy quality hides backdrop and decoration but retains full textures;
- forced Critical quality uses half textures;
- restoring performance returns full texture tier;
- input, Task 1 animations and combat remain responsive.

- [ ] **Step 6: Validate failure fallback and stability**

Create the failure HAP from a temporary detached Git worktree, leaving the main
worktree and production HAP untouched:

```bash
FAILURE_WORKTREE=$(mktemp -d /tmp/my-world-task2-failure.XXXXXX)
git worktree add --detach "$FAILURE_WORKTREE" HEAD
cd "$FAILURE_WORKTREE"
printf 'invalid-glb' > entry/src/main/resources/rawfile/environment/decoration.glb
# Run the same signed assembleHap command from Step 3 using local DevEco signing.
cp entry/build/default/outputs/default/entry-default-signed.hap \
  /tmp/my-world-task2-invalid-environment.hap
cd /Users/xiling/Documents/project/game/my-world
git worktree remove "$FAILURE_WORKTREE"
"$HDC" install -r /tmp/my-world-task2-invalid-environment.hap
```

The temporary tracked-file change exists only in the disposable worktree and is
removed with it. Do not stage it. Expected:

- decoration status becomes `Failed`;
- fallback ring/ground and all character models remain visible;
- combat still starts;
- application PID remains alive.

Restore the production HAP, then scan HiLog for:

```text
SIGSEGV|cppcrash|glGetString.*invalid|RequestBuffer.*fail|EGL.*error
```

Expected: no application-owned matches.

- [ ] **Step 7: Update README**

Record:

- exact Poly Haven asset IDs, authors, pages, CC0 license URL and hashes;
- four derived GLB hashes and sizes;
- focused test results;
- HAP path, size and SHA-256;
- device model, ABI and PID;
- normal and boss-area min/average FPS;
- environment draw calls, triangles and texture tier;
- screenshot paths;
- fallback and graph-stability results.

- [ ] **Step 8: Final verification and commit**

Run all focused tests again, the Hvigor build, `git diff --check` and
`git status --short`.

```bash
git add README.md automation/perf/profile_collect.sh
git commit -m "test: 完成 Task 2 环境设备验收" \
  -m "记录遗迹场景、性能降级、失败回退和 Pura 70 Pro 证据。" \
  -m "Prompt: M4 Task 2 环境与场景氛围"
```
