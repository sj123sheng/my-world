#pragma once

#include "native/engine/math/vec2.h"

#include <cstdint>
#include <vector>

// 环境布局放置项：与 assets/environment/layout.json 的条目一一对应
//（单一事实来源的镜像，构建期校验见 test_environment_composition）。
// 坐标系为布局米制空间；进入 [0,1] 世界需经 EnvironmentWorldFit 变换。
struct EnvironmentPlacement {
  // OuterRing=0 CenterRift=1 Backdrop=2 Decoration=3（与 EnvironmentBatchKind 一致）。
  int region = 0;
  float translation[3] = {0.0f, 0.0f, 0.0f};
  // 绕 Y 轴旋转的四元数 (x, y, z, w)，环境件仅有 Y 轴旋转。
  float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float scale[3] = {1.0f, 1.0f, 1.0f};
  // 源网格包围盒（布局空间，源自 Poly Haven 网格统计）。
  float halfExtents[3] = {1.0f, 1.0f, 1.0f};
  // 流式分组（Phase 2）：-1 为全局组（始终加载/渲染）；≥0 为所属
  // 8×8 分块 id（id = y * 8 + x），随分块流式启停。
  // 区块批次（blockId≥0）内所有条目统一按 OuterRing 世界适配参数
  // 定位（世界中心锚点），与渲染层 environmentBlockWorldFitMatrix 一致。
  int32_t blockId = -1;
};

// 环境布局数据（layout.json 镜像）。
const EnvironmentPlacement* environmentLayoutPlacements();
std::size_t environmentLayoutPlacementCount();

// 布局空间 → 世界空间锚点：OuterRing/Backdrop 以世界中心为锚，
// CenterRift/Decoration 贴近祭坛锚点 (altarX, altarZ)。
struct EnvironmentFitAnchors {
  float altarX = 0.5f;
  float altarZ = 0.75f;
};

// 布局空间 → [0,1] 世界空间的相似变换参数（与渲染批次共用，
// 保证碰撞体与可见建筑严格对齐）。
struct EnvironmentWorldFit {
  float scale = 0.02f;
  float yBias = 0.0f;
  float centerX = 0.5f;
  float centerZ = 0.5f;
};

EnvironmentWorldFit environmentWorldFitForRegion(int region,
                                                 const EnvironmentFitAnchors& anchors);

// 建筑碰撞盒：布局放置项经世界适配变换后的 2D OBB 足迹 + 盒顶高度。
struct BuildingBox {
  float cx = 0.0f;   // 世界空间中心 x
  float cz = 0.0f;   // 世界空间中心 z（逻辑坐标 y）
  float hx = 0.0f;   // 局部半宽（x）
  float hz = 0.0f;   // 局部半深（z）
  float yaw = 0.0f;  // 绕 Y 旋转（弧度）
  float top = 0.0f;  // 盒顶世界高度：可站立支撑面与攀爬目标高度
};

// 碰撞解算结果。
struct BuildingContact {
  bool touching = false;      // 本帧是否触碰到建筑侧面
  Vec2 normal;                // 推出方向（近似墙体外法线）
  float highestTop = 0.0f;    // 触碰盒中最高的盒顶：攀爬目标高度
};

// 建筑碰撞集：从环境布局生成可碰撞盒（仅阻挡型区域参与：
// 外圈城墙与背景塔楼；中心裂隙平台与装饰碎块不阻挡探索动线）。
// 确定性构造，无随机状态，可被独立测试覆盖。
//
// 分桶策略（Phase 2）：盒子按 8×8 分块分桶。采用“跨界盒归入所有
// 覆盖分块”方案：盒子的旋转包围盒（AABB）向外膨胀 kBucketMargin
//（覆盖碰撞查询半径与迭代推出位移）后，落入其覆盖的全部分块桶；
// 中心落在 [0,1] 世界之外的盒子（远景背景）钳制到最近边缘分块。
// resolve/supportTopAt/standingTopAt 只收集目标点所在分块 + 8 邻块
// 的候选盒（去重并按盒子下标升序，与全量遍历顺序一致），因此与
// 全量遍历语义完全兼容；resolveBruteForce 保留为全量遍历参考实现。
class BuildingCollision {
 public:
  // 分桶网格与 assets/world/world.json 的 8×8 分块一致。
  static constexpr int32_t kBucketCountX = 8;
  static constexpr int32_t kBucketCountY = 8;
  // AABB 膨胀余量：覆盖最大碰撞查询半径（主角 0.012）与推出迭代位移。
  static constexpr float kBucketMargin = 0.02f;

  BuildingCollision() = default;
  explicit BuildingCollision(const std::vector<BuildingBox>& boxes)
      : boxes_(boxes) {
    rebuildBuckets();
  }

  // 从环境布局构建碰撞集。altarX/altarZ 为祭坛锚点世界坐标。
  static BuildingCollision fromEnvironmentLayout(float altarX, float altarZ);

  // 滑动碰撞解算：把 (x, y) 从候选盒内推出（半径膨胀后的 OBB），
  // 多盒逐个迭代推出，保留切向分量实现沿墙滑动。
  // height 高于盒顶时视为越过该盒（不阻挡），支持跳上墙头。
  BuildingContact resolve(float& x, float& y, float radius, float height) const;

  // 支撑高度：包含该点（半径内）的盒中最高的盒顶；无盒返回 veryLow。
  // 供“站上墙头”时的地面贴合与落地判定使用。
  float supportTopAt(float x, float y, float radius) const;

  // 站立支撑盒顶：仅当盒顶不高于 currentHeight + tolerance（角色已经
  // 处于盒顶附近）时才计入支撑，避免贴墙站立时被误判为“站上墙顶”。
  // 无合格盒返回 veryLow。
  float standingTopAt(float x, float y, float radius, float currentHeight,
                      float tolerance) const;

  // 全量遍历参考实现：与分桶查询结果严格一致，供回归对比。
  BuildingContact resolveBruteForce(float& x, float& y, float radius,
                                    float height) const;
  float supportTopAtBruteForce(float x, float y, float radius) const;

  const std::vector<BuildingBox>& boxes() const { return boxes_; }
  // 分桶表：buckets_[bucketY * kBucketCountX + bucketX] → 盒子下标（升序）。
  const std::vector<std::vector<int32_t>>& buckets() const { return buckets_; }
  // 世界点对应的分桶下标；越界坐标钳制到边缘分块。
  static int32_t bucketIndexAt(float x, float y);

 private:
  void rebuildBuckets();
  // 目标点所在分块 + 8 邻块的候选盒下标（去重、升序）。
  std::vector<int32_t> candidatesNear(float x, float y) const;

  std::vector<BuildingBox> boxes_;
  std::vector<std::vector<int32_t>> buckets_;
};
