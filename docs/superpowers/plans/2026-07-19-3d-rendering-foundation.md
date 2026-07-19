# M3-1 3D 渲染基础 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 GLES3 渲染层上搭建 3D 透视渲染管线，用硬编码网格和基础光照验证 3D 渲染可行性。

**Architecture:** glm 提供 3D 数学，Camera3D 用透视投影替代 2D 变换，Mesh 用 VBO/IBO 渲染硬编码几何体，stb_image 加载纹理，3D 着色器实现 MVP 变换和方向光照。2D 游戏逻辑不变，位置映射到 3D 世界坐标。

**Tech Stack:** C++17、glm（头文件库）、stb_image（头文件库）、OpenGL ES 3、HarmonyOS NDK、macOS clang++ 测试

## Global Constraints

- 2D 游戏逻辑（位置、移动、碰撞、AI、战斗）完全不变，只升级渲染层。
- 2D 灰盒渲染保留为调试回退，不删除。
- 3D 着色器与现有 2D 单色着色器独立，不共享 Program。
- 纹理加载失败时回退到纯色渲染，不阻塞游戏循环。
- glm 和 stb_image 随仓库分发，放在 `native/engine/math/glm/` 和 `native/engine/render/stb_image.h`。
- 不修改、暂存或提交用户现有的 `build-profile.json5`。
- 每个提交必须包含 `Prompt: 好的 进入M3-1`。

---

## File Structure

- `native/engine/math/glm/`：glm 头文件库（随仓库分发）。
- `native/engine/render/stb_image.h`：stb_image 头文件库。
- `native/engine/render/camera3d.h`：定义 `Camera3D`。
- `native/engine/render/camera3d.cpp`：实现 3D 透视相机。
- `native/engine/render/mesh.h`：定义 `Vertex`、`Mesh`。
- `native/engine/render/mesh.cpp`：实现网格创建、上传和绘制。
- `native/engine/render/texture.h`：定义 `loadTexture()`。
- `native/engine/render/texture.cpp`：实现 stb_image 纹理加载。
- `native/engine/render/shader_3d.h`：定义 `Shader3D`。
- `native/engine/render/shader_3d.cpp`：实现 3D 着色器程序。
- `native/engine/render/surface.h`：增加 3D 渲染字段。
- `native/engine/render/surface.cpp`：增加 3D 绘制阶段。
- `native/engine/core/loop.cpp`：更新 3D 相机并写入 Surface 扩展字段。
- `entry/src/main/cpp/CMakeLists.txt`：增加 include path 和新源文件。
- `tests/test_camera3d.cpp`：Camera3D 单元测试。
- `tests/test_mesh.cpp`：Mesh 单元测试。

### Task 1: glm 集成与 Camera3D

**Files:**
- Create: `native/engine/render/camera3d.h`
- Create: `native/engine/render/camera3d.cpp`
- Create: `tests/test_camera3d.cpp`
- Create: `native/engine/math/glm/`（glm 头文件）
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `glm::vec3`、`glm::mat4`、`glm::radians`、`glm::perspective`、`glm::lookAt`、`glm::normalize`。
- Produces: `Camera3D`、`Camera3D::follow()`、`Camera3D::viewMatrix()`、`Camera3D::projectionMatrix()`、`Camera3D::viewProjection()`。

- [ ] **Step 1: 获取 glm 头文件**

Run:
```bash
cd native/engine/math
git clone --depth 1 https://github.com/g-truc/glm.git glm_tmp
cp -r glm_tmp/glm ./
rm -rf glm_tmp
```
Expected: `native/engine/math/glm/` 目录存在，包含 `glm.hpp` 等头文件。

- [ ] **Step 2: 写入失败测试**

```cpp
#include "native/engine/render/camera3d.h"
#include <cassert>

void testFollowSetsPositionAndTarget() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.follow({0.5f, 0.0f, 0.5f}, 0.0f, 0.5f, 2.0f);
  assert(cam.position.y > 0.0f);
  assert(cam.target.x == 0.5f);
  assert(cam.target.z == 0.5f);
}

void testViewMatrixIsLookAt() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.follow({0.5f, 0.0f, 0.5f}, 0.0f, 0.5f, 2.0f);
  auto vp = cam.viewProjection();
  assert(vp.length() == 4);
  for (int i = 0; i < 16; i++) {
    assert(std::isfinite(vp[i].x));
  }
}

void testProjectionRespondsToFov() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.fov = 45.0f;
  auto p1 = cam.projectionMatrix();
  cam.fov = 90.0f;
  auto p2 = cam.projectionMatrix();
  assert(p1 != p2);
}
```

- [ ] **Step 3: 编译确认 RED**

Run:
```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CXX="$(xcrun --find clang++)"
"$CXX" -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative tests/test_camera3d.cpp native/engine/render/camera3d.cpp -o /tmp/test_camera3d
```
Expected: `camera3d.h` 不存在导致失败。

- [ ] **Step 4: 实现 Camera3D**

`Camera3D` 使用 glm。`follow()` 用球坐标计算 position：`position = target + distance * (cos(pitch)*cos(yaw), sin(pitch), cos(pitch)*sin(yaw))`。`viewMatrix()` 返回 `glm::lookAt(position, target, {0,1,0})`。`projectionMatrix()` 返回 `glm::perspective(radians(fov), aspectRatio, nearPlane, farPlane)`。`viewProjection()` 返回 `projectionMatrix() * viewMatrix()`。

- [ ] **Step 5: 运行测试确认 GREEN**

Run: `/tmp/test_camera3d`
Expected: exit code 0。

- [ ] **Step 6: 更新 CMakeLists 并提交**

在 CMakeLists 增加 `native/engine/math` 到 include path（已有），增加 `camera3d.cpp` 到源文件列表。

```bash
git add native/engine/render/camera3d.h native/engine/render/camera3d.cpp tests/test_camera3d.cpp native/engine/math/glm/ entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 集成 glm 并实现 3D 透视相机" \
  -m "Camera3D 用球坐标跟随目标，输出 view/projection 矩阵。" \
  -m "Prompt: 好的 进入M3-1"
```

### Task 2: 网格与纹理

**Files:**
- Create: `native/engine/render/mesh.h`
- Create: `native/engine/render/mesh.cpp`
- Create: `native/engine/render/texture.h`
- Create: `native/engine/render/texture.cpp`
- Create: `native/engine/render/stb_image.h`
- Create: `tests/test_mesh.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `glm::vec3`、`glm::vec2`、GLES3 `glGenBuffers`/`glDrawElements`。
- Produces: `Vertex`、`Mesh`、`createCube()`、`createPlane()`、`loadTexture()`。

- [ ] **Step 1: 获取 stb_image 头文件**

Run:
```bash
curl -sL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o native/engine/render/stb_image.h
```
Expected: `native/engine/render/stb_image.h` 存在。

- [ ] **Step 2: 写入失败测试**

```cpp
#include "native/engine/render/mesh.h"
#include <cassert>

void testCubeHasCorrectVertexCount() {
  Mesh cube = createCube(1.0f);
  assert(cube.vertices.size() == 24);
  assert(cube.indices.size() == 36);
}

void testPlaneHasCorrectVertexCount() {
  Mesh plane = createPlane(10.0f, 10.0f);
  assert(plane.vertices.size() == 4);
  assert(plane.indices.size() == 6);
}

void testCubeNormalsFaceOutward() {
  Mesh cube = createCube(1.0f);
  for (const auto& v : cube.vertices) {
    assert(v.normal.x != 0.0f || v.normal.y != 0.0f || v.normal.z != 0.0f);
  }
}
```

- [ ] **Step 3: 编译确认 RED**

Expected: `mesh.h` 不存在导致失败。

- [ ] **Step 4: 实现 Mesh 和纹理加载**

`Vertex` 包含 `glm::vec3 position`、`glm::vec3 normal`、`glm::vec2 uv`。`createCube(size)` 生成 24 顶点（6 面 × 4 顶点，每面法线独立）和 36 索引（12 三角形）。`createPlane(width, depth)` 生成 4 顶点和 6 索引，法线朝上 `(0,1,0)`。`Mesh::upload()` 创建 VBO/IBO（`#ifdef OHOS_PLATFORM` 保护 GL 调用）。`Mesh::draw()` 绑定缓冲区并调用 `glDrawElements`。`loadTexture(path)` 用 stb_image 加载 PNG，生成 GL 纹理，失败返回 0。

- [ ] **Step 5: 运行测试确认 GREEN**

Run: `/tmp/test_mesh`
Expected: exit code 0。

- [ ] **Step 6: 提交**

```bash
git add native/engine/render/mesh.h native/engine/render/mesh.cpp native/engine/render/texture.h native/engine/render/texture.cpp native/engine/render/stb_image.h tests/test_mesh.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 实现网格创建与纹理加载" \
  -m "createCube/createPlane 生成硬编码几何体，stb_image 加载 PNG 纹理。" \
  -m "Prompt: 好的 进入M3-1"
```

### Task 3: 3D 着色器

**Files:**
- Create: `native/engine/render/shader_3d.h`
- Create: `native/engine/render/shader_3d.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: GLES3 着色器编译 API。
- Produces: `Shader3D`、`Shader3D::use()`、`Shader3D::setMVP()`、`Shader3D::setLight()`。

- [ ] **Step 1: 实现 3D 着色器**

`Shader3D` 编译顶点和片段着色器源码（见设计规格 §3.5），链接为 Program。`use()` 调用 `glUseProgram`。`setMVP(mat4)` 设置 `uMVP` uniform。`setLight(vec3 dir, vec3 color, vec3 ambient)` 设置光照 uniform。`setTexture(bool hasTexture)` 设置 `uHasTexture`。所有 GL 调用在 `#ifdef OHOS_PLATFORM` 内，非平台侧为空操作。

- [ ] **Step 2: 语法检查**

Run:
```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CXX="$(xcrun --find clang++)"
"$CXX" -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative -fsyntax-only native/engine/render/shader_3d.cpp
```
Expected: exit code 0。

- [ ] **Step 3: 提交**

```bash
git add native/engine/render/shader_3d.h native/engine/render/shader_3d.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 实现 3D 着色器程序" \
  -m "MVP 变换加方向光照和纹理采样，与 2D 着色器独立。" \
  -m "Prompt: 好的 进入M3-1"
```

### Task 4: Surface 集成与 3D 绘制

**Files:**
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `Camera3D`、`Mesh`、`Shader3D`、`EncounterSnapshot`、`Player`。
- Produces: `Surface` 3D 字段、`surface_draw` 3D 绘制阶段、`Loop` 3D 相机更新。

- [ ] **Step 1: 扩展 Surface**

`Surface` 增加 `Camera3D camera3d`、`Mesh playerMesh`、`Mesh groundMesh`、`Shader3D shader3d`、`glm::vec3 lightDir`、`glm::vec3 lightColor`、`glm::vec3 ambient` 字段。`surface_init` 中创建立方体、平面和着色器。`surface_destroy` 中释放网格资源。

- [ ] **Step 2: 实现 3D 绘制**

`surface_draw` 在现有 2D 绘制后增加 3D 阶段（`#ifdef OHOS_PLATFORM`）：
1. `glUseProgram(shader3d)`
2. 设置 VP 矩阵和光照 uniform
3. 画地面平面（大平面，纯色）
4. 画玩家立方体（在 `(player.x, 0.5, player.y)` 位置，缩放为合理大小）
5. 画训练假人立方体（在假人位置，按 alive 跳过）
6. 画敌人立方体（在敌人位置，按存活状态跳过）
7. 画首领立方体（在首领位置，按阶段配色）

- [ ] **Step 3: 更新 Loop**

`Loop::tickOnce` 在 `surface_draw` 前更新 `surface.camera3d`：调用 `follow()` 传入玩家 3D 位置、`camera.yaw()`、`camera.pitch()` 和 `camera.distance()`。把 `EncounterSnapshot` 的敌人位置写入 Surface 扩展字段。

- [ ] **Step 4: 语法检查**

Run:
```bash
"$CXX" -std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative -fsyntax-only native/engine/core/loop.cpp
```
Expected: exit code 0。

- [ ] **Step 5: 提交**

```bash
git add native/engine/render/surface.h native/engine/render/surface.cpp native/engine/core/loop.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 集成 3D 渲染到 Surface 和 Loop" \
  -m "surface_draw 增加 3D 绘制阶段，Loop 更新 3D 相机。" \
  -m "Prompt: 好的 进入M3-1"
```

### Task 5: 文档、批量验证与真机验收

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-19-3d-rendering-foundation.md`

- [ ] **Step 1: 运行自动化测试**

Run focused tests: `test_camera3d`、`test_mesh`。随后运行本地可编译 C++ 批量测试，确认现有测试不回归。

- [ ] **Step 2: 运行格式与构建验证**

Run: `git diff --check`。需要 HAP 时临时复制本机签名配置，构建后恢复 `build-profile.json5`。

- [ ] **Step 3: 真机渲染验证**

使用 HDC 安装 signed HAP，启动应用，确认 3D 立方体可见、透视正确、光照有明暗、2D 玩法不回归、帧率 >= 30 FPS。

- [ ] **Step 4: 更新验收文档**

记录测试矩阵、HAP SHA-256、设备 ID 和截图路径。

- [ ] **Step 5: 提交**

```bash
git add README.md docs/superpowers/plans/2026-07-19-3d-rendering-foundation.md
git commit -m "docs: 完成 M3-1 3D 渲染基础验收" \
  -m "记录自动化、构建和真机 3D 渲染验证结果。" \
  -m "Prompt: 好的 进入M3-1"
```
