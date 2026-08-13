# 无限自然世界与任务迁移 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把固定 `8×8` 世界迁移为核心区兼容、外围确定性生成、按画质提前加载并按两圈缓存回收的无限自然世界，同时彻底移除人工建筑、门和墙体并保持核心任务可完成。

**Architecture:** 以 `WorldPosition = ChunkCoord + LocalPosition` 作为新的位置真值，地形和外围内容从稳定 64 位分块哈希生成；`WorldGrid`、`ChunkedTerrain` 与 `StreamScheduler` 全链路使用 `ChunkCoord`，渲染仅消费相对玩家的局部偏移。核心区继续使用现有手工自然特征与永久探索状态，人工结构目标按原 ID 迁移为自然节点或区域触发。

**Tech Stack:** C++17、ArkTS/ArkUI、Node.js 世界布局生成器、OpenGL ES 3、现有宿主 `clang++` 测试、Hvigor/HarmonyOS API 23。

## Global Constraints

- 高/中/低画质活动范围固定为 `9×9 / 7×7 / 5×5`，不得以性能优化为由缩小。
- 所有档位额外保留两圈已加载缓存，超出后同时回收网格、生态、外围实体和表现引用。
- 核心区外不生成剧情、唯一地标、机关、Boss 或一次性奖励。
- 建筑、遗迹、祭坛、围墙和路径门必须从视觉、资源提交和碰撞中彻底移除，不允许透明或隐藏模型残留。
- 保留现有探索 ID 与旧完成位掩码语义，旧 V1–V9 存档必须可读；新存档版本只追加字段。
- 64 位哈希不能依赖有符号整数溢出；负坐标与超远坐标必须确定且有限。
- 最低性能目标为不持续两秒低于 30 FPS，推荐平均 45 FPS；真机未验证时不得声称达到。
- 每次提交必须使用项目格式并包含 `Prompt:` 摘要，每行不超过 100 字。

---

## File Structure

- `native/engine/world/world_position.h/.cpp`：无限分块坐标、局部坐标规范化、相对位置与稳定哈希。
- `native/engine/world/world_grid.h/.cpp`：按 `ChunkCoord` 维护活动区与两圈缓存差量，并给出方向优先加载顺序。
- `native/engine/world/terrain_heightfield.h/.cpp`：无限世界高度查询；核心区手工特征与外围连续缓坡分支。
- `native/engine/render/chunked_terrain.h/.cpp`：以 `ChunkCoord` 构建局部网格，不再依赖有限整数 chunk id。
- `native/engine/world/stream_scheduler.h/.cpp`：`ChunkCoord` 队列、取消、`3×3` 同步安全圈与两圈回收。
- `native/gameplay/world/procedural_chunk_content.h/.cpp`：外围植被、敌人与采集物的分盐确定性描述和有界状态。
- `native/engine/resource/save.h/.cpp`：V10 世界种子、分块坐标和局部位置；V1–V9 迁移。
- `assets/world/world.json`、`config/schema/world.schema.json`、`automation/assets/generate_world_layout.mjs`、`native/generated/world_layout.gen.h`：移除人工结构并把目标迁为自然内容。
- `native/gameplay/world/exploration_content.h/.cpp`：自然节点与区域目标，保留稳定 ID/掩码。
- `native/engine/core/loop.h/.cpp`：无限位置接线、禁用全部墙体/建筑/门碰撞、外围内容生命周期。
- `native/engine/render/surface.h/.cpp`、`entry/src/main/ets/pages/GamePage.ets`：相对原点绘制并停止加载人工环境块。
- `tests/test_world_position.cpp`、`tests/test_procedural_chunk_content.cpp`：新增纯逻辑覆盖。
- `tests/test_world_grid.cpp`、`tests/test_chunked_terrain.cpp`、`tests/test_stream_scheduler.cpp`、`tests/test_terrain_heightfield.cpp`、`tests/test_save_v8.cpp`、`tests/test_exploration_content.cpp`、`tests/test_world_layout_gen.cpp`、`tests/test_loop_integration.cpp`、`tests/test_bridge_contract.mjs`：迁移与集成回归。

### Task 1: 无限位置值对象与稳定哈希

**Files:**
- Create: `native/engine/world/world_position.h`
- Create: `native/engine/world/world_position.cpp`
- Create: `tests/test_world_position.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Produces: `struct ChunkCoord { int64_t x; int64_t y; }`，提供 `==`、`!=`、字典序 `<`。
- Produces: `struct LocalPosition { float x; float y; }`。
- Produces: `struct WorldPosition { ChunkCoord chunk; LocalPosition local; }`。
- Produces: `WorldPosition NormalizeWorldPosition(ChunkCoord chunk, double localX, double localY)`。
- Produces: `Vec2 RelativeWorldPosition(const WorldPosition& target, const WorldPosition& origin)`。
- Produces: `uint64_t StableChunkHash(uint64_t worldSeed, ChunkCoord chunk, uint64_t salt)`。
- Produces: `struct ChunkCoordHash { size_t operator()(ChunkCoord coord) const; }`，供无序容器统一使用。

- [ ] **Step 1: 写入会失败的坐标规范化和哈希测试**

在 `tests/test_world_position.cpp` 写明被防止的回归：跨边界时丢失整数分块、负局部坐标错误截断、同盐哈希不稳定、不同盐相互污染。测试至少包含：

```cpp
const WorldPosition east = NormalizeWorldPosition({3, -2}, 2.25, -1.5);
assert(east.chunk == (ChunkCoord{5, -4}));
assert(close(east.local.x, 0.25f));
assert(close(east.local.y, 0.5f));

const WorldPosition exact = NormalizeWorldPosition({0, 0}, -1.0, 1.0);
assert(exact.chunk == (ChunkCoord{-1, 1}));
assert(close(exact.local.x, 0.0f));
assert(close(exact.local.y, 0.0f));

const Vec2 relative = RelativeWorldPosition(
    {{1000000000000LL, -1000000000000LL}, {0.75f, 0.25f}},
    {{999999999999LL, -1000000000001LL}, {0.25f, 0.75f}});
assert(relative == (Vec2{1.5f, 0.5f}));

assert(StableChunkHash(7, {-5, 9}, 11) == StableChunkHash(7, {-5, 9}, 11));
assert(StableChunkHash(7, {-5, 9}, 11) != StableChunkHash(7, {-5, 9}, 12));
assert(StableChunkHash(7, {-5, 9}, 11) != StableChunkHash(7, {5, 9}, 11));
```

- [ ] **Step 2: 编译并确认 RED**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-world-position-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_world_position.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/world_position"
```

Expected: FAIL，提示缺少 `world_position.h` 或上述符号未定义。

- [ ] **Step 3: 最小实现无限位置与无溢出哈希**

在 `world_position.cpp` 使用 `std::floor` 把每个局部轴的整数部分转移到分块；非有限输入回退 `{0,0}+{0,0}`。哈希先把有符号坐标通过 `std::memcpy` 解释为 `uint64_t`，再使用明确的无符号 SplitMix64 混合，所有溢出均发生在 `uint64_t`。

`RelativeWorldPosition` 不直接执行可能溢出的有符号减法；先转为 `long double` 计算分块差，并在差值超出 `float` 安全范围时钳制到 `±16777216`，再加局部差；普通活动范围内必须保留精确整数偏移。

- [ ] **Step 4: 运行 GREEN 与 UBSan**

Run:

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-world-position-green.XXXXXX)
clang++ -std=c++17 -fsanitize=undefined -fno-sanitize-recover=all \
  -I. -Inative tests/test_world_position.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/world_position"
"$TEST_BIN_DIR/world_position"
```

Expected: 退出 `0`，UBSan 无输出。

- [ ] **Step 5: 提交 Task 1**

```bash
git add native/engine/world/world_position.h native/engine/world/world_position.cpp \
  tests/test_world_position.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 增加无限世界坐标" \
  -m "使用64位分块、局部位置规范化和无符号稳定哈希支持负坐标与超远坐标。" \
  -m "Prompt: 真正无限生成并按距离回收地图"
```

### Task 2: 无限活动网格、画质半径与方向优先级

**Files:**
- Modify: `native/engine/world/world_grid.h`
- Modify: `native/engine/world/world_grid.cpp`
- Rewrite tests: `tests/test_world_grid.cpp`

**Interfaces:**
- Consumes: `ChunkCoord`、`WorldPosition` from Task 1。
- Produces: `struct WorldGridConfig { int32_t activeRadius = 4; int32_t cacheRings = 2; }`。
- Produces: `bool WorldGrid::updateStreaming(ChunkCoord playerChunk, Vec2 cameraForward, Vec2 movement)`。
- Produces: `const std::vector<ChunkCoord>& pendingLoads() const`、`pendingUnloads()`、`activeChunks()`、`cachedChunks()`。
- Produces: `static int32_t ActiveRadiusForQuality(int32_t qualityPreset)`，返回高 4、中 3、低 2。

- [ ] **Step 1: 重写测试以锁定无限集合和加载顺序**

在 `tests/test_world_grid.cpp` 使用字面量断言：

```cpp
WorldGrid grid({4, 2});
assert(grid.updateStreaming({0, 0}, {1, 0}, {1, 0}));
assert(grid.activeChunks().size() == 81);       // 9x9
assert(grid.cachedChunks().size() == 169);      // 13x13, 半径 4+2
assert(grid.pendingLoads().front() == (ChunkCoord{0, 0}));
assert(grid.pendingLoads()[1] == (ChunkCoord{1, 0})); // 方向优先

assert(WorldGrid::ActiveRadiusForQuality(0) == 4);
assert(WorldGrid::ActiveRadiusForQuality(1) == 3);
assert(WorldGrid::ActiveRadiusForQuality(2) == 2);

grid.updateStreaming({50, -50}, {-1, 0}, {-1, 0});
for (const ChunkCoord coord : grid.pendingUnloads()) {
  assert(ChebyshevDistance(coord, {50, -50}) > 6);
}
```

同时断言相同输入与候选插入顺序无关，加载排序为：中心、相邻圈、相机/移动前方、距离、坐标。

- [ ] **Step 2: 运行测试确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-world-grid-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_world_grid.cpp \
  native/engine/world/world_grid.cpp native/engine/world/world_position.cpp \
  -o "$TEST_BIN_DIR/world_grid"
```

Expected: FAIL，现有有限 `WorldGridConfig{8,8,2}` 和 `int32_t` chunk id 不满足新接口。

- [ ] **Step 3: 实现活动区、缓存区和稳定排序**

删除 `countX/countY/chunkIndexAt/chunkCount`。`activeChunks` 只含半径 `activeRadius` 方形；`cachedChunks` 含 `activeRadius + cacheRings` 方形。`pendingLoads` 是新活动块差集，`pendingUnloads` 是旧缓存块减新缓存块。

加载比较键使用：

```cpp
std::tuple<int, int, float, int64_t, int64_t>
// centerRank, chebyshev, -forwardDot, y, x
```

中心块固定第一，相邻安全圈优先于外圈；`cameraForward + movement` 归一化后给同距离区块排序，退化向量时只按距离和坐标。

- [ ] **Step 4: 运行 GREEN**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-world-grid-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_world_grid.cpp \
  native/engine/world/world_grid.cpp native/engine/world/world_position.cpp \
  -o "$TEST_BIN_DIR/world_grid"
"$TEST_BIN_DIR/world_grid"
```

Expected: 退出 `0`。

- [ ] **Step 5: 提交 Task 2**

```bash
git add native/engine/world/world_grid.h native/engine/world/world_grid.cpp \
  tests/test_world_grid.cpp
git commit -m "refactor: 迁移无限分块网格" \
  -m "按9x9、7x7、5x5活动半径和额外两圈缓存生成方向优先加载差量。" \
  -m "Prompt: 地图提前渲染大块区域并持续向外扩展"
```

### Task 3: 无限连续高度场与局部分块网格

**Files:**
- Modify: `native/engine/world/terrain_heightfield.h`
- Modify: `native/engine/world/terrain_heightfield.cpp`
- Modify: `native/engine/render/terrain_mesh.h`
- Modify: `native/engine/render/terrain_mesh.cpp`
- Modify: `native/engine/render/chunked_terrain.h`
- Modify: `native/engine/render/chunked_terrain.cpp`
- Rewrite tests: `tests/test_terrain_heightfield.cpp`
- Rewrite tests: `tests/test_chunked_terrain.cpp`

**Interfaces:**
- Consumes: `ChunkCoord` from Task 1。
- Produces: `float TerrainHeightfield::heightAt(double worldX, double worldY) const`，不钳制。
- Produces: `float TerrainHeightfield::heightAt(ChunkCoord chunk, float localX, float localY) const`。
- Produces: `TerrainChunkCpuMesh ChunkedTerrain::buildChunkMesh(ChunkCoord coord, uint32_t segments) const`。
- Produces: `std::map<ChunkCoord, TerrainChunkCpuMesh>` CPU 缓存。

- [ ] **Step 1: 写失败测试锁定跨块连续和超远稳定**

在 `test_terrain_heightfield.cpp` 添加：

```cpp
TerrainHeightfield terrain = makeWorldTerrain();
assert(close(terrain.heightAt({0, 0}, 1.0f, 0.25f),
             terrain.heightAt({1, 0}, 0.0f, 0.25f)));
assert(close(terrain.heightAt({-1, 3}, 1.0f, 0.75f),
             terrain.heightAt({0, 3}, 0.0f, 0.75f)));
assert(std::isfinite(terrain.heightAt({1000000000LL, -1000000000LL}, 0.5f, 0.5f)));
assert(terrain.slopeAt({9, -4}, 0.5f, 0.5f) < terrain.config().climbSlopeThreshold);
```

在 `test_chunked_terrain.cpp` 用 `ChunkCoord{-4,7}` 构建网格，断言表面顶点局部 `x/z` 落在 `[0,1]`，高度来自同一世界查询；相邻 `{0,0}` 东边界与 `{1,0}` 西边界高度完全相等，侧裙数量不变。

- [ ] **Step 2: 运行并确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-infinite-terrain-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_terrain_heightfield.cpp \
  native/engine/world/terrain_heightfield.cpp native/gameplay/world/world_terrain.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/heightfield"
```

Expected: FAIL，新分块重载不存在或越界坐标仍被钳制。

- [ ] **Step 3: 实现核心区与外围高度分支**

保留核心区手工特征查询，核心区定义为 `ChunkCoord{0,0}` 内的旧 `[0,1]` 范围。删除 `edgeMountainMask` 和所有 `clamp01(x/y)`。

外围基础高度只使用连续世界坐标的解析正弦/值噪声，具体上限：总幅度不超过 `0.025f`，有限差分坡度保持小于 `0.45f`。同一连续基础函数也用于核心区；核心手工特征贡献在核心区四边进入 `0.08` 宽过渡带时平滑衰减到零，确保 `{0,0}` 与四个相邻外围块共享边界完全一致。区域生态盐只影响材质参数，不允许逐块改变边界高度。对 `double worldX = chunk.x + localX` 使用范围缩减后的正弦输入，避免超远坐标传给三角函数失真。

- [ ] **Step 4: 迁移 ChunkedTerrain 到 ChunkCoord**

`buildChunkMesh` 生成局部 `[0,1]` 网格；高度采样使用目标 `ChunkCoord`，UV 使用稳定的局部 UV 加哈希偏移。`segmentsFor` 改为两个 `ChunkCoord` 的切比雪夫距离。`chunkRect` 不再返回全局固定矩形；`TerrainChunkCpuMesh` 新增 `ChunkCoord coord`。

- [ ] **Step 5: 运行两个 GREEN 测试**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-infinite-terrain-green.XXXXXX)
COMMON=(-std=c++17 -I. -Inative)
clang++ "${COMMON[@]}" tests/test_terrain_heightfield.cpp \
  native/engine/world/terrain_heightfield.cpp native/gameplay/world/world_terrain.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/heightfield"
"$TEST_BIN_DIR/heightfield"
clang++ "${COMMON[@]}" tests/test_chunked_terrain.cpp \
  native/engine/render/chunked_terrain.cpp native/engine/render/terrain_mesh.cpp \
  native/engine/world/terrain_heightfield.cpp native/gameplay/world/world_terrain.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/chunked"
"$TEST_BIN_DIR/chunked"
```

Expected: 全部退出 `0`。

- [ ] **Step 6: 提交 Task 3**

```bash
git add native/engine/world/terrain_heightfield.* native/engine/render/terrain_mesh.* \
  native/engine/render/chunked_terrain.* tests/test_terrain_heightfield.cpp \
  tests/test_chunked_terrain.cpp
git commit -m "feat: 生成连续无限地形" \
  -m "核心区保留自然特征，外围用连续缓坡函数生成并以局部分块网格渲染。" \
  -m "Prompt: 移除地形边界并让地图持续扩展"
```

### Task 4: ChunkCoord 流送、取消、三乘三安全圈与两圈回收

**Files:**
- Modify: `native/engine/world/stream_scheduler.h`
- Modify: `native/engine/world/stream_scheduler.cpp`
- Rewrite tests: `tests/test_stream_scheduler.cpp`

**Interfaces:**
- Consumes: `ChunkCoord`、新 `WorldGrid`、新 `ChunkedTerrain`。
- Produces: `requestLoads(const std::vector<ChunkCoord>&, ChunkCoord player, int32_t perfLodLevel)`。
- Produces: `requestUnloads(const std::vector<ChunkCoord>&)`、`drainReady()`、`applyUnloads()` 均返回 `std::vector<ChunkCoord>`。
- Produces: `loadSafeRingSync(ChunkCoord landingChunk, int32_t perfLodLevel)`，固定生成半径 1 的 `3×3`。
- Produces: `setKeepRadius(int32_t activeRadius, int32_t cacheRings = 2)`。

- [ ] **Step 1: 重写同步调度测试**

在 `tests/test_stream_scheduler.cpp` 断言：负坐标可加载；传入方向优先列表时 Ready 顺序不被字典序重排；生成中卸载后不提交；`loadSafeRingSync({50,-50}, 0)` 返回 9 块且中心第一；玩家移动后只有切比雪夫距离大于 `activeRadius+2` 的块被 `applyUnloads` 回收。

加入有界性用例：沿 x 轴移动 50 块，每次提交与回收后 `activeCount + readyCount + pendingLoadCount <= 13*13 + 81`，返回原点后相同块的网格顶点完全一致。

- [ ] **Step 2: 运行并确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-stream-red.XXXXXX)
clang++ -std=c++17 -pthread -I. -Inative tests/test_stream_scheduler.cpp \
  native/engine/world/stream_scheduler.cpp native/engine/world/world_grid.cpp \
  native/engine/world/world_position.cpp native/engine/render/chunked_terrain.cpp \
  native/engine/render/terrain_mesh.cpp native/engine/world/terrain_heightfield.cpp \
  -o "$TEST_BIN_DIR/stream"
```

Expected: FAIL，现有调度仍接收有限 `int32_t` id 且缓存只有一圈。

- [ ] **Step 3: 全链路迁移到 ChunkCoord 并保留请求优先级**

所有 `std::vector<int32_t>`、`std::map<int32_t,...>` 和 `LoadTask::chunkId` 改为 `ChunkCoord`。`requestLoads` 去重但必须稳定保留调用方优先顺序，不能再次按字典序排序；相同优先级由 `WorldGrid` 已稳定排序。

取消集合、生成中集合和 CPU 缓存使用 `std::set<ChunkCoord>` 或字典序向量。`applyUnloads` 基于 `lastPlayerChunk` 与 `activeRadius + cacheRings` 判断；真正卸载时从 Ready、Active 和 `ChunkedTerrain` 同时清除。

- [ ] **Step 4: 实现同步安全圈和失败兜底**

`loadSafeRingSync` 生成顺序为中心、八邻居；任一网格为空时，用 `ChunkedTerrain::buildFlatFallbackChunk(coord, farSegments)` 替代。返回的 9 块全部进入 Ready；调用方恢复画面前要 drain 并确认均 Active。

- [ ] **Step 5: 运行 GREEN 与 ThreadSanitizer 可用性检查**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-stream-green.XXXXXX)
clang++ -std=c++17 -pthread -I. -Inative tests/test_stream_scheduler.cpp \
  native/engine/world/stream_scheduler.cpp native/engine/world/world_grid.cpp \
  native/engine/world/world_position.cpp native/engine/render/chunked_terrain.cpp \
  native/engine/render/terrain_mesh.cpp native/engine/world/terrain_heightfield.cpp \
  -o "$TEST_BIN_DIR/stream"
"$TEST_BIN_DIR/stream"
```

Expected: 退出 `0`。若当前 macOS 运行库支持，再用 `-fsanitize=thread` 重编译运行；不支持时记录工具限制，不冒充通过。

- [ ] **Step 6: 提交 Task 4**

```bash
git add native/engine/world/stream_scheduler.* tests/test_stream_scheduler.cpp
git commit -m "refactor: 支持无限区块流送" \
  -m "保留方向优先队列，传送同步准备3x3安全圈并在额外两圈外完整回收。" \
  -m "Prompt: 提前渲染大区域并在远离后回收旧区块"
```

### Task 5: 确定性外围生态、敌人和采集物描述

**Files:**
- Create: `native/gameplay/world/procedural_chunk_content.h`
- Create: `native/gameplay/world/procedural_chunk_content.cpp`
- Create: `tests/test_procedural_chunk_content.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `StableChunkHash`、`TerrainHeightfield`、`EnemyArchetype`。
- Produces: `struct ProceduralFoliageSpawn { LocalPosition position; uint8_t kind; float scale; }`。
- Produces: `struct ProceduralEnemySpawn { uint64_t stableId; LocalPosition position; EnemyArchetype archetype; }`。
- Produces: `struct ProceduralCollectibleSpawn { uint64_t stableId; LocalPosition position; int32_t itemId; }`。
- Produces: `struct ProceduralChunkContent` with the above vectors。
- Produces: `ProceduralChunkContent GenerateProceduralChunk(uint64_t worldSeed, ChunkCoord coord, const TerrainHeightfield&)`。

- [ ] **Step 1: 写失败测试验证分盐、位置约束和核心区空结果**

测试同种子同坐标深比较一致、不同坐标至少一项不同、核心区 `{0,0}` 返回空外围内容。将仅植被 salt 改动的测试入口作为显式参数，断言敌人与采集物不变。逐个生成点断言局部坐标在 `[0.08,0.92]`、高度有限、非深水、坡度 `<0.55`。

- [ ] **Step 2: 运行并确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-content-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_procedural_chunk_content.cpp \
  native/gameplay/world/procedural_chunk_content.cpp \
  native/engine/world/world_position.cpp native/engine/world/terrain_heightfield.cpp \
  -o "$TEST_BIN_DIR/content"
```

Expected: FAIL，模块或符号不存在。

- [ ] **Step 3: 实现固定尝试次数的确定性生成**

每类内容最多尝试固定 32 次，禁止“直到成功”的无界循环。使用 salt：terrain `0x10`、foliage `0x20`、enemy `0x30`、collectible `0x40`。外围每块植被上限 24、敌人上限 3、采集物上限 4；核心区返回空，交给现有数据驱动内容负责。

稳定实体 ID 由世界种子、区块和槽位哈希组成，不用累计全局计数器。生成结果按 stableId 排序，输入重放确定。

- [ ] **Step 4: 运行 GREEN**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-content-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_procedural_chunk_content.cpp \
  native/gameplay/world/procedural_chunk_content.cpp \
  native/engine/world/world_position.cpp native/engine/world/terrain_heightfield.cpp \
  -o "$TEST_BIN_DIR/content"
"$TEST_BIN_DIR/content"
```

Expected: 退出 `0`。

- [ ] **Step 5: 提交 Task 5**

```bash
git add native/gameplay/world/procedural_chunk_content.* \
  tests/test_procedural_chunk_content.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "feat: 生成外围自然内容" \
  -m "按独立盐确定性生成植被、普通敌人和采集物，核心区不重复唯一内容。" \
  -m "Prompt: 无限区域生成地形植被敌人和基础采集物"
```

### Task 6: V10 存档世界位置与旧版本迁移

**Files:**
- Modify: `native/engine/resource/save.h`
- Modify: `native/engine/resource/save.cpp`
- Modify: `tests/test_save_v8.cpp`
- Modify: `tests/test_progress_save.cpp`

**Interfaces:**
- Consumes: `ChunkCoord`、`LocalPosition` from Task 1。
- Produces in `SaveState`: `uint64_t worldSeed = 1`、`int64_t playerChunkX/Y = 0`、`float playerLocalX = 0.5f`、`playerLocalY = 0.12f`。
- Produces: V10 文本格式，在 V9 所有字段之后追加上述 5 项。

- [ ] **Step 1: 添加 V10 往返、V9 迁移和非法值测试**

`test_save_v8.cpp` 添加字面量用例：V10 `{-1234567890123, 987654321012}` 与局部 `{0.25,0.75}` 往返；现有 V9 fixture 读取后得到核心区默认 `{0,0}+{0.5,0.12}`；V10 非有限局部坐标读取失败或由调用方迁移函数回退出生点，不能进入 Loop。

- [ ] **Step 2: 运行并确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-save-v10-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_save_v8.cpp \
  native/engine/resource/save.cpp -o "$TEST_BIN_DIR/save"
"$TEST_BIN_DIR/save"
```

Expected: FAIL，V10 字段未保存或未迁移。

- [ ] **Step 3: 追加 V10 写入与读取分支**

写入版本改为 `V10`，不重排 V9 字段；读取公共 V8/V9/V10 主体后，V9 保留迁移默认，V10 再读取世界字段。限制局部值有限且在 `[0,1)`；`worldSeed == 0` 规范到 `1`。保留 V1–V8 现有分支。

- [ ] **Step 4: 运行 GREEN 与原子存档测试**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-save-v10-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_save_v8.cpp \
  native/engine/resource/save.cpp -o "$TEST_BIN_DIR/save"
"$TEST_BIN_DIR/save"
clang++ -std=c++17 -I. -Inative tests/test_save_atomic.cpp \
  native/engine/resource/save.cpp -o "$TEST_BIN_DIR/save_atomic"
"$TEST_BIN_DIR/save_atomic"
```

Expected: 全部退出 `0`。

- [ ] **Step 5: 提交 Task 6**

```bash
git add native/engine/resource/save.* tests/test_save_v8.cpp tests/test_progress_save.cpp
git commit -m "feat: 保存无限世界位置" \
  -m "存档升级V10并兼容V1到V9，将旧位置迁移到核心区出生坐标。" \
  -m "Prompt: 无限地图卸载后返回仍保持世界位置"
```

### Task 7: 移除人工结构并迁移自然任务数据

**Files:**
- Modify: `assets/world/world.json`
- Modify: `config/schema/world.schema.json`
- Modify: `automation/assets/generate_world_layout.mjs`
- Regenerate: `native/generated/world_layout.gen.h`
- Regenerate: `entry/src/main/ets/generated/EnvironmentVisualManifest.ets`
- Modify: `native/gameplay/world/exploration_content.h`
- Modify: `native/gameplay/world/exploration_content.cpp`
- Modify: `native/gameplay/flow/story_director.cpp`
- Modify: `tests/test_world_layout_gen.cpp`
- Modify: `tests/test_exploration_content.cpp`
- Modify: `tests/test_exploration_loop_contract.cpp`
- Modify: `tests/test_terrain_heightfield.cpp`
- Modify: `tests/test_story_director.cpp`
- Modify: `tests/test_environment_visual_assets.mjs`

**Interfaces:**
- Produces schema: `naturalNodes[]` with `{id,x,y,label,requiredMotion,rewardId}`。
- Produces schema: `regionTriggers[]` with `{id,x,y,radius,label,prerequisiteNodeId}`。
- Produces: `ExplorationTargetKind::NaturalNode`、`RegionTrigger`，删除运行时 `TraversalGate`。
- Preserves: 原 puzzle ID `70..73` 与原 gate ID `80..83` 的完成位序；`explorationGateMask` 字段改作 region trigger 完成掩码，但磁盘位置不变。

- [ ] **Step 1: 先改行为测试表达自然内容契约**

`test_world_layout_gen.cpp` 不再断言 2–5 个 traversal gates；改为断言：无 `kTraversalGates`、无人工环境批次；自然节点 ID 保留 70–73，区域触发 ID 保留 80–83；所有位置可达。

`test_exploration_content.cpp` 断言激活自然节点 71 后区域触发 81 可通过 `enterRegion(81)` 完成，并使迁移后的 `openGateMask()` 对应位为 1；V9 掩码恢复同一完成状态。

`test_story_director.cpp` 断言开场台词不再出现“祭坛”“门庭”“遗迹”，目标文本改为“源质晶簇”或具体自然地标。

- [ ] **Step 2: 运行测试确认 RED**

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-natural-layout-red.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_exploration_content.cpp \
  native/gameplay/world/exploration_content.cpp -o "$TEST_BIN_DIR/exploration"
```

Expected: FAIL，`NaturalNode/RegionTrigger/enterRegion` 尚不存在。

- [ ] **Step 3: 修改 world.json 和 schema**

删除 `environmentBatches` 中全部建筑/遗迹批次、`environmentVisual.visualTerrainBlocks` 中人工地貌块、`traversalGates` 和人工命名。将 POI 标签迁为：启明巨树、翠风花谷、辉光湖湾、中枢岩脊、湖心岩台。将 puzzle 迁为自然节点，将 gate 迁为半径区域；NPC 调整到各自然地标附近干地。

生成器必须校验人工结构顶层字段不存在；自然节点的 `rewardId` 必须有效，区域触发的 `prerequisiteNodeId` 必须引用节点。不要保留半径、yaw、top 等碰撞字段。

- [ ] **Step 4: 迁移 ExplorationContent 并保留掩码位置**

删除 `TraversalGate` 与 `gateById`，新增：

```cpp
bool activateNaturalNode(int32_t id, MotionState currentMotion);
bool enterRegion(int32_t id, Vec2 playerPosition);
bool isRegionCompleted(int32_t id) const;
const RegionTrigger* regionById(int32_t id) const;
```

`openGateMask/openGateCount` 暂保留作为存档兼容 API，但内部读取 `completedRegions_`；注释明确磁盘兼容含义。`nearestTarget` 发布自然节点与未完成区域，不再产生“门被阻挡”目标。

- [ ] **Step 5: 运行生成器并验证 GREEN**

```bash
node automation/assets/generate_world_layout.mjs
node tests/test_environment_visual_assets.mjs
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-natural-layout-green.XXXXXX)
clang++ -std=c++17 -I. -Inative tests/test_world_layout_gen.cpp \
  -o "$TEST_BIN_DIR/world_layout"
"$TEST_BIN_DIR/world_layout"
clang++ -std=c++17 -I. -Inative tests/test_exploration_content.cpp \
  native/gameplay/world/exploration_content.cpp -o "$TEST_BIN_DIR/exploration"
"$TEST_BIN_DIR/exploration"
clang++ -std=c++17 -I. -Inative tests/test_story_director.cpp \
  native/gameplay/flow/story_director.cpp -o "$TEST_BIN_DIR/story"
"$TEST_BIN_DIR/story"
```

Expected: 全部退出 `0`；生成清单不含人工 GLB。

- [ ] **Step 6: 提交 Task 7**

```bash
git add assets/world/world.json config/schema/world.schema.json \
  automation/assets/generate_world_layout.mjs native/generated/world_layout.gen.h \
  entry/src/main/ets/generated/EnvironmentVisualManifest.ets \
  native/gameplay/world/exploration_content.* native/gameplay/flow/story_director.cpp \
  tests/test_world_layout_gen.cpp tests/test_exploration_content.cpp \
  tests/test_exploration_loop_contract.cpp tests/test_terrain_heightfield.cpp \
  tests/test_story_director.cpp tests/test_environment_visual_assets.mjs
git commit -m "feat: 迁移纯自然探索目标" \
  -m "移除建筑、遗迹、祭坛、围墙和路径门数据，保留原ID并改为自然节点与区域触发。" \
  -m "Prompt: 建筑和门全部移除并将任务改成自然地标"
```

### Task 8: Loop、外围生命周期与全部碰撞移除

**Files:**
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/gameplay/ai/wild_spawn_system.h`
- Modify: `native/gameplay/ai/wild_spawn_system.cpp`
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `entry/src/main/ets/ui/ExplorationHud.ets`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `tests/test_loop_integration.cpp`
- Modify: `tests/test_wild_spawn_system.cpp`
- Modify: `tests/test_bridge_contract.mjs`
- Delete: `native/gameplay/world/terrain_wall_collision.h`
- Delete: `native/gameplay/world/terrain_wall_collision.cpp`
- Delete: `native/gameplay/world/exploration_gate_collision.h`
- Delete: `native/gameplay/world/exploration_gate_collision.cpp`
- Delete: `tests/test_terrain_wall_collision.cpp`
- Delete: `tests/test_exploration_gate_collision.cpp`
- Delete: `tests/test_encounter_building_collision.cpp`

**Interfaces:**
- Consumes: Tasks 1–7 all interfaces。
- Produces snapshot: `playerChunkX/Y`、`playerLocalX/Y`、`activeChunkCount`、`cachedChunkCount`、`streamingPendingCount`。
- Produces Loop invariant: 玩家/敌人/Boss 更新路径不调用任何地形墙、建筑或门碰撞。
- Produces bounded map: `std::map<ChunkCoord, ProceduralChunkRuntime> proceduralChunks`，只覆盖缓存区。

- [ ] **Step 1: 先修改集成测试表达跨 50 块和无碰撞行为**

`test_loop_integration.cpp` 添加测试入口或测试友元，连续把移动位置跨过 50 个 x 分块，逐次 tick 并断言：分块坐标增长、局部坐标始终 `[0,1)`、活动数符合画质、缓存数量有界、返回原点高度一致。

增加人工结构回归：旧建筑/门坐标附近移动不再产生推出或阻挡，`explorationBlockedGateId` 字段从快照删除，自然区域进入仍完成对应任务。

`test_wild_spawn_system.cpp` 断言外围稳定 ID 在区块卸载时离场，重新加载同块生成相同原型和位置；击杀状态缓存总块数不超过当前缓存区块数。

- [ ] **Step 2: 运行 Loop RED**

使用计划末尾“相关自动化”中的 Loop 编译命令运行 `test_loop_integration`。

Expected: FAIL，Loop 仍使用 `surface.player.x/y` 固定世界坐标并调用三类碰撞。

- [ ] **Step 3: 接入 WorldPosition 并移除碰撞调用**

Loop 持有 `WorldPosition playerWorldPosition`；`surface.player.x/y` 只作为当前玩家原点附近渲染局部值。控制器更新后用 `NormalizeWorldPosition` 转移跨块量。

删除 `resolvePlayerWorldCollision`、`refreshExplorationGateCollision`、`terrainWallContact`、`slideAlongTerrainWall`、`depenetrateTerrainWall`、`buildingCollision.resolve`、`explorationGateCollision.resolve` 的玩家和 enemy resolver 接线。敌人 resolver 只做局部位置规范化与地面高度查询，不推开实体。

运动状态的 `terrainClimbing` 固定 false；保留跳跃、滑翔、游泳和地面贴合。大高度差使用 `ExplorationMotionConfig::maxGroundFollowPerSecond` 限速靠近地面，测试每帧高度变化不超过 `speed * dt`。

- [ ] **Step 4: 接入流送与外围运行时内容**

每帧以玩家 `ChunkCoord`、相机 forward 和移动向量更新 `WorldGrid`。加载完成时生成对应 `ProceduralChunkContent` 并注入外围植被、`WildSpawnSystem` 与采集注册表；真正回收时先通知目标/血条系统清除关联 ID，再释放运行时块。

传送先调用 `loadSafeRingSync`，drain 9 块均 Active 后重置相机与恢复画面；未完成时保持现有 `teleportFlashMs` 遮罩。

- [ ] **Step 5: 迁移快照、Bridge 和 ArkTS**

快照 `x/y` 继续发布核心兼容显示值，但新增 64 位分块字段通过 JS `number` 发布；当前可精确表示的 50 块验收范围不会超过 `2^53`。`GamePage` 不再按旧固定 blockId 预载 `environment/block_*.glb` 或 `visual_terrain/*.glb`，删除 `refreshEnvironmentBlocks` 人工批次链。

删除 `ExplorationHud` 的门关闭提示和 Bridge 对应三个字段；新增调试 HUD 的分块坐标、活动/缓存/待生成数。

- [ ] **Step 6: 运行 GREEN 集成测试**

Run:

```bash
set -e
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-infinite-loop-green.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative -Inative/engine/math)
HOST_SOURCES=()
while IFS= read -r source; do HOST_SOURCES+=("$source"); done < <(
  find native -name '*.cpp' ! -path '*render/surface.cpp' \
    ! -path '*core/loop.cpp' ! -path '*harmony/fence_wait.cpp' \
    ! -path '*harmony/lifecycle.cpp' | sort)
clang++ "${COMMON[@]}" tests/test_loop_integration.cpp \
  native/engine/core/loop.cpp "${HOST_SOURCES[@]}" -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"
clang++ "${COMMON[@]}" tests/test_wild_spawn_system.cpp \
  "${HOST_SOURCES[@]}" -o "$TEST_BIN_DIR/wild"
"$TEST_BIN_DIR/wild"
node tests/test_bridge_contract.mjs
```

Expected: 全部退出 `0`。

- [ ] **Step 7: 提交 Task 8**

```bash
git add -A native/engine/core native/gameplay/ai/wild_spawn_system.* \
  native/gameplay/world native/engine/render/surface.* entry/src/main/ets \
  entry/src/main/cpp/native_bridge.cpp entry/src/main/cpp/types/libnative_game/Index.d.ts \
  entry/src/main/cpp/CMakeLists.txt tests
git commit -m "feat: 接入无限自然世界循环" \
  -m "跨区规范化玩家位置、加载外围内容并彻底移除地形墙、建筑和路径门碰撞及视觉链。" \
  -m "Prompt: 无限地图远距回收且建筑门和墙全部移除"
```

### Task 9: 相对原点 GLES 绘制与区块 GPU 回收

**Files:**
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `native/engine/render/environment.h`
- Modify: `native/engine/render/environment.cpp`
- Modify: `tests/test_camera_render_transform.cpp`
- Modify: `tests/test_environment_composition.cpp`
- Modify: `tests/test_frustum_cull.cpp`

**Interfaces:**
- Consumes: `ChunkCoord` active meshes and player origin from Task 8。
- Produces: `glm::vec3 ChunkRenderTranslation(ChunkCoord target, ChunkCoord origin, LocalPosition originLocal)`。
- Produces Surface maps keyed by `ChunkCoord` for terrain mesh and procedural foliage batches。

- [ ] **Step 1: 添加相对变换与回收测试**

`test_camera_render_transform.cpp` 添加：原点 `{10^12,-10^12}+{.75,.25}`，目标相邻块得到有限且精确的 `x/z` 平移；目标超出活动半径不提交。`test_environment_composition.cpp` 断言区块回收差量同步移除 terrain 和 foliage GPU key。

- [ ] **Step 2: 运行并确认 RED**

编译运行上述两个测试，Expected: FAIL，Surface 仍以 `int32_t` id 和绝对 `[0,1]` 坐标绘制。

- [ ] **Step 3: 实现局部网格平移和 GPU 生命周期**

地形 Mesh 保持局部坐标；绘制 model matrix 使用 `ChunkRenderTranslation`。所有区块关联环境状态改为 `std::unordered_map<ChunkCoord,...,ChunkCoordHash>` 或 `std::map`；`applyUnloads` 返回后调用 Mesh/foliage batch 的 `abandonGpuResources` 并 erase。

删除 `drawCenterFallback` 的围墙几何和旧 `fallbackWallMesh`；资源加载失败仅保留自然地表/植被兜底。

- [ ] **Step 4: 运行 GREEN 与 shader 校验**

Run focused host tests，随后从 `surface.cpp` 中提取现有 shader 字符串继续运行仓库已有 glslang 校验命令（若工具存在）。Expected: 测试退出 `0`，shader 零错误；工具缺失则记录未运行。

- [ ] **Step 5: 提交 Task 9**

```bash
git add native/engine/render/surface.* native/engine/render/environment.* \
  tests/test_camera_render_transform.cpp tests/test_environment_composition.cpp \
  tests/test_frustum_cull.cpp
git commit -m "feat: 使用相对原点渲染区块" \
  -m "以玩家分块为渲染原点并在区块离开两圈缓存后释放地形和植被GPU资源。" \
  -m "Prompt: 无限远探索保持稳定并回收旧地图渲染"
```

### Task 10: 全面验证、项目记忆与设备清单

**Files:**
- Modify: `PROJECT_STATE.md`
- Modify: `DECISIONS.md`
- Modify: `TASKS.md`
- Modify if needed: `docs/environment_vertical_slice_validation.md`

**Interfaces:**
- Consumes: 完成本计划 Tasks 1–9。
- Produces: 可复现验证记录与未完成真机清单。

- [ ] **Step 1: 运行全部相关自动化**

Run:

```bash
set -e
node automation/assets/generate_world_layout.mjs
node tests/test_environment_visual_assets.mjs
node tests/test_bridge_contract.mjs
git diff --check

TEST_BIN_DIR=$(mktemp -d /tmp/myworld-infinite-focused.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -pthread -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\.cpp$'))

clang++ "${COMMON[@]}" tests/test_world_position.cpp \
  native/engine/world/world_position.cpp -o "$TEST_BIN_DIR/world_position"
"$TEST_BIN_DIR/world_position"
clang++ "${COMMON[@]}" tests/test_world_grid.cpp \
  native/engine/world/world_grid.cpp native/engine/world/world_position.cpp \
  -o "$TEST_BIN_DIR/world_grid"
"$TEST_BIN_DIR/world_grid"
clang++ "${COMMON[@]}" tests/test_procedural_chunk_content.cpp \
  native/gameplay/world/procedural_chunk_content.cpp \
  native/engine/world/world_position.cpp native/engine/world/terrain_heightfield.cpp \
  -o "$TEST_BIN_DIR/content"
"$TEST_BIN_DIR/content"
clang++ "${COMMON[@]}" tests/test_save_v8.cpp native/engine/resource/save.cpp \
  -o "$TEST_BIN_DIR/save"
"$TEST_BIN_DIR/save"
```

继续运行 `test_terrain_heightfield`、`test_chunked_terrain`、`test_stream_scheduler`、`test_world_layout_gen`、`test_exploration_content`、`test_wild_spawn_system` 与 `test_loop_integration`，编译源列表沿用各 Task 的 GREEN 命令。

Expected: 所有命令退出 `0`；`git diff --check` 无输出。

- [ ] **Step 2: 从当前源码重建完整宿主测试集**

使用仓库上一份已验证计划的完整宿主脚本：编译除 `surface.cpp`、`loop.cpp` 和平台文件外的全部 Native 对象，逐个构建运行 `tests/test_*.cpp`，再单独链接运行 Loop 集成测试。显式跳过的平台测试仅限 `test_fence_wait` 与确实依赖 Harmony 生命周期的测试。

Expected: 零失败；任何编译失败立即停止，不能把未执行项记为通过。

- [ ] **Step 3: 尝试 HAP 构建**

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  /Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p product=default -p module=entry@default assembleHap \
  --analyze=normal --parallel --incremental
```

Expected when SDK complete: `BUILD SUCCESSFUL`。失败时记录首个真实错误。

- [ ] **Step 4: 更新项目记忆**

`PROJECT_STATE.md` 只记录已由自动化证明的实现；`DECISIONS.md` 记录 `ChunkCoord + LocalPosition`、核心区/外围生成边界、固定活动半径和两圈缓存、V10 迁移、自然任务 ID 兼容。`TASKS.md` 添加未完成真机验收：连续跨 50 块、折返一致、传送安全圈、三画质 FPS/内存、人工结构完全不可见、核心剧情可达。

- [ ] **Step 5: 提交 Task 10**

```bash
git add PROJECT_STATE.md DECISIONS.md TASKS.md docs/environment_vertical_slice_validation.md
git commit -m "docs: 记录无限自然世界验证" \
  -m "记录坐标、流送、存档和自然任务决策，并保留真机性能与视觉验收边界。" \
  -m "Prompt: 无限地图与人工结构移除验收"
```

## Plan 1 Completion Gate

进入锁定与敌人站位计划前，必须满足：

- 聚焦测试、完整宿主测试和 Node 契约零失败。
- HAP 构建成功，或明确记录 SDK 阻塞；不能静默跳过。
- 连续 50 块宿主仿真有界且折返一致。
- 世界生成产物和资源清单没有人工结构。
- 核心任务的自然节点/区域流程和旧 V9 掩码迁移通过。
- `git status --short` 只包含明确属于后续计划的改动或为空。
