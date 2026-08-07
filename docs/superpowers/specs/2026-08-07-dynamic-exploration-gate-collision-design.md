# 动态探索路径门碰撞设计

## 目标

让垂直切片中的 `TraversalGate` 从“有状态的探索数据”变成实际影响空间通行的动态障碍：

- 路径门关闭时，玩家、普通敌人和首领不能穿过。
- 关联机关激活后，路径门立即开启并停止阻挡。
- V9 存档保存和恢复路径门状态，加载后空间通行结果一致。
- 不引入运行时 JSON 解析，不破坏现有静态建筑碰撞和移动状态机。

本子项目不负责新增地图、美术资源、首领机制或复杂导航网格。

## 现状与边界

当前 `ExplorationContent` 已从生成的 `WorldLayout` 构造 POI、机关、路径门和奖励，
并维护 `openGateMask`。Loop 已在玩家接近机关时激活机关，但 `BuildingCollision`
只包含静态环境布局，关闭的路径门尚未进入碰撞查询。

现有碰撞接口支持：

- OBB 盒体的侧面推出和沿墙滑动。
- 盒顶高度、站立支撑和攀爬目标。
- 玩家在 Loop 中的碰撞解算。
- 敌人与首领通过 `EncounterController` 注入位置解算器。

因此本次采用与静态建筑相同的 `BuildingBox` 语义，把动态路径门作为独立的、每帧按探索状态重建的碰撞集合。

## 方案

### 动态碰撞集合

新增 `ExplorationGateCollision`，职责仅包括：

- 从 `ExplorationContent` 的路径门配置生成关闭门的 `BuildingBox` 列表。
- 根据 `isGateOpen(gate.id)` 过滤已开启路径门。
- 对外提供 `resolve(float&, float&, float radius, float height)`。
- 在无关闭路径门时返回不触碰结果。

路径门数据需要补充或派生以下碰撞参数：

- `cx/cz`：使用世界布局坐标。
- `hx/hz`：由门类型的固定切片默认值派生，保持小范围、可调和可测试。
- `top`：用于攀爬或跳越判断。
- `yaw`：默认沿门横向方向，必要时由世界数据增加旋转字段。

优先使用 `TraversalGate` 的数据字段承载这些参数，使世界内容仍由 `world.json` 单一事实来源控制。
如果当前四个门的几何无法从现有坐标可靠推导，则在 `traversalGates` 配置中显式增加 `halfExtents` 和 `yaw`，
由生成器校验后进入生成头。

### Loop 组合

Loop 每个固定步在玩家移动后执行动态门碰撞，并与静态建筑碰撞保持确定顺序：

1. 玩家控制器积分位置。
2. 静态建筑碰撞推出。
3. 关闭路径门碰撞推出。
4. 用最终位置执行地形、机关、POI 和奖励检测。

静态建筑与动态路径门分别解算，避免修改已有 `BuildingCollision` 的数据所有权。两次解算都保留切向滑动。
如果两者同时接触，固定顺序保证结果可复现；测试覆盖这一顺序。

敌人和首领使用同一个组合解算器回调，先调用静态建筑碰撞，再调用动态路径门碰撞，确保三类移动主体使用相同的门状态。

### 状态更新与存档

- 机关激活后，`ExplorationContent` 修改路径门状态。
- 下一次固定步构建动态门碰撞集合时立即移除对应门。
- `saveProgress` 继续写入 `openGateMask` 到 V9 探索字段。
- `loadProgress` 恢复 `openGateMask` 后，下一固定步动态门集合即与存档一致。
- 不新增独立的门状态副本，避免 `ExplorationContent` 与碰撞系统出现状态分叉。

### HUD 反馈

探索 HUD 继续显示机关和路径门计数，并增加当前交互门的状态文案：

- 关闭：`路径受阻：需要激活“<机关名>”`。
- 已开启：不显示阻挡提示，保留常规目标或奖励提示。

文案来源于 `TraversalGate` 与关联 `PuzzleNode` 的配置，不在 ArkTS 中硬编码门 ID。

## 数据流

```text
world.json
  -> generate_world_layout.mjs
  -> world_layout.gen.h
  -> ExplorationContent
  -> ExplorationGateCollision
  -> Loop / EncounterController
  -> GameSnapshot / ExplorationHud
```

存档路径为：

```text
ExplorationContent.openGateMask
  -> SaveState.explorationGateMask
  -> V9
  -> loadProgress
  -> ExplorationContent.restoreMasks
  -> ExplorationGateCollision
```

## 测试设计

先增加失败测试，再实现生产代码：

1. `test_exploration_gate_collision`
   - 关闭门会推出玩家位置。
   - 已开启门不产生碰撞。
   - 高度高于门顶时行为与静态建筑一致。
   - 多门状态过滤和无效状态输入安全。
2. 扩展 `test_exploration_content`
   - 门配置的碰撞参数稳定。
   - 机关激活前后门碰撞状态切换。
3. 扩展 `test_loop_integration` 或新增 Loop 合同测试
   - 玩家解算顺序为静态建筑后动态门。
   - 机关激活后下一固定步可以通过。
4. 扩展 `test_encounter_building_collision`
   - 敌人和首领回调同时应用动态门。
5. 扩展 `test_save_v8.cpp`
   - V9 的路径门位掩码保存、加载和旧版本归零保持通过。
6. 更新 Node 桥接契约，锁定 HUD 门状态字段和目标链路。

## 验收标准

- 至少一个主路线关闭门在玩家首次接近时实际阻挡通行。
- 激活关联机关后，玩家可以通过同一路径，且无需重启或重新加载场景。
- 敌人和首领不能穿过关闭的主路线门。
- 保存后重新加载，门的阻挡/通行状态与保存前一致。
- 现有静态碰撞、移动、探索内容、桥接和 HAP 构建测试不回归。
- 不引入运行时 JSON 依赖，不复制第二份门状态真相。

## 不在本次范围

- 自动寻路和导航网格。
- 门的独立动画资源、复杂开门演出和联网同步。
- 地图六区域全部内容化。
- 首领两阶段机制和玩家测试流程。
