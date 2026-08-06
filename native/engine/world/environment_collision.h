#pragma once

#include "native/engine/math/vec2.h"

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
class BuildingCollision {
 public:
  BuildingCollision() = default;
  explicit BuildingCollision(const std::vector<BuildingBox>& boxes)
      : boxes_(boxes) {}

  // 从环境布局构建碰撞集。altarX/altarZ 为祭坛锚点世界坐标。
  static BuildingCollision fromEnvironmentLayout(float altarX, float altarZ);

  // 滑动碰撞解算：把 (x, y) 从所有盒内推出（半径膨胀后的 OBB），
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

  const std::vector<BuildingBox>& boxes() const { return boxes_; }

 private:
  std::vector<BuildingBox> boxes_;
};
