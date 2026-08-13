# Task 7 实施报告：移除人工结构并迁移自然任务数据

## 实现摘要

- `world.json` 将 `puzzles` / `traversalGates` 迁为 `naturalNodes` /
  `regionTriggers`，保留节点 ID 70..73、区域 ID 80..83 及声明位序。
- POI 统一为启明巨树、翠风花谷、辉光湖湾、中枢岩脊、湖心岩台；锚点、NPC、
  奖励、路线、地貌描述与验证相机同步自然化。
- 删除 `environmentVisual.visualTerrainCells`，Native 生成头不再产生人工视觉地貌
  类型/数组/GLB 路径，ArkTS 清单保留兼容空数组但不引用任何 GLB。
- schema 精确约束 `NaturalNode` 六字段与 `RegionTrigger` 六字段，拒绝额外旧碰撞
  字段；生成器拒绝人工顶层字段，并校验 reward / prerequisite 引用。
- `ExplorationContent` 删除 `TraversalGate`、`gateById`、旧 puzzle 接口和门状态，
  新增自然节点激活与半径区域进入；`nearestTarget` 只发布 POI、自然节点、可进入区域
  和已解锁奖励。
- V9 第四掩码槽与原 80..83 位序保持不变；`openGateMask/openGateCount` 仅以兼容名
  存在，内部语义和注释均明确为 completed regions。
- 开场目标改为触碰启明巨树下的源质晶簇。
- 因直接契约影响，额外更新 `tests/test_bridge_contract.mjs`，要求 ArkTS 环境清单为空
  且不含 GLB；未修改 Loop、Surface 或 Task 8+ 生产模块。

## TDD / RED

- 先修改布局、探索、剧情、环境资源、loop contract、terrain 行为测试。
- 有效 RED：
  - `test_exploration_content` 编译失败：缺少 `naturalNodes`、`regionTriggers`、
    `activateNaturalNode`、`enterRegion`、`isRegionCompleted`、`regionById`。
  - `test_world_layout_gen` 编译失败：缺少 `kNaturalNodes` / `kRegionTriggers`，且旧
    人工视觉单元数量为 3。
  - `test_story_director` 运行失败：开场文本仍含“祭坛”。
  - `test_environment_visual_assets.mjs` 运行失败：源 JSON 仍含 `traversalGates`。
- 本机默认 clang++ 未自动发现 libc++；使用
  `/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/usr/include/c++/v1`
  作为 `-isystem` 后取得上述真实 C++ RED。

## GREEN 与回归命令

- 运行 `node automation/assets/generate_world_layout.mjs`，只从源 JSON/schema 重建
  `world_layout.gen.h` 与 `EnvironmentVisualManifest.ets`。
- 简报三项 C++、`test_environment_visual_assets.mjs`、
  `test_exploration_loop_contract.cpp`、`test_terrain_heightfield.cpp`、
  `test_bridge_contract.mjs` 全部通过。
- 三项严格模式均覆盖 world layout / exploration content / story director /
  exploration loop contract / terrain heightfield：
  - normal：通过；
  - `-Wall -Wextra -Wpedantic -Werror`：通过；
  - `-fsanitize=undefined -fno-omit-frame-pointer`：通过。
- terrain 相关回归 `test_terrain_biome`、`test_terrain_wall_collision`、
  `test_chunked_terrain`、`test_terrain_mesh` 通过。
- `node automation/hvigor/check_rules.js` 通过。
- schema 负例覆盖未知 reward、未知 prerequisite、遗留人工顶层字段，均被拒绝。

## 生成器幂等

- 连续运行生成器两次均报告 `up to date`。
- 两次前后 SHA-256 一致：
  - `world_layout.gen.h`：
    `11a98f89e22c4f3817af7c3d60c2097df440f4ab34f64c0663571082f2a6d33d`
  - `EnvironmentVisualManifest.ets`：
    `e5f6b08418a25f821d556716e8a7876e4a71538110e052c67e72c77106875726`

## 文件

- 修改简报列出的 14 个源/测试/生成文件。
- 额外修改直接受契约影响的 `tests/test_bridge_contract.mjs`。
- 未更新 PROJECT_STATE / DECISIONS / TASKS：本次是多任务计划中的中间迁移，Task 8
  尚未清理下游消费者，此时写入“已完成全链路”会造成长期状态失真。

## 自审

- 源 JSON 无 `puzzles`、`traversalGates`、`environmentBatches`、
  `visualTerrainBlocks`、`visualTerrainCells`。
- 生成头无 `kTraversalGates`、`WorldTraversalGateDef`、`kPuzzleNodes`、
  `WorldVisualTerrainCellDef` 或 `.glb`；ArkTS 生成清单无 `.glb`。
- NaturalNode 仅包含 `id,x,y,label,requiredMotion,rewardId`；RegionTrigger 仅包含
  `id,x,y,radius,label,prerequisiteNodeId`。
- ID/位序、reward/prerequisite 引用、V9 mask 恢复均有测试；全部 POI、NPC、节点、
  区域及路线端点经 terrain 测试验证为干地且坡度可行走。
- 运行时任务输入/生成输出和剧情文本已清除祭坛、门庭、遗迹、残塔、浮桥、哨站、
  渡口、观景台、机关、路径门等人工语义（测试中的负例字符串除外）。

## 顾虑 / 后续边界

- `entry/.../rawfile/environment/` 仍存在未被本次生成清单引用的旧物理 GLB，
  `assets/environment/manifest.json` 仍是历史下载/构建资源清单；两者不在 Task 7
  允许修改的文件清单，未删除。若计划要求物理资产也从仓库删除，应在对应资产任务
  明确授权后处理。
- `Surface` 仍直接消费旧 `WorldVisualTerrainCellDef/kVisualTerrainCells`，旧
  `ExplorationGateCollision` 仍消费 `TraversalGate`；生成契约迁移后这些旧消费者会
  编译断开，按任务边界留给 Task 8，不在本次越界兼容或保留隐藏旧类型。
