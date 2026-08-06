// environment_collision.cpp: 建筑碰撞集与滑动解算。
//
// 布局数据镜像 assets/environment/layout.json（该文件不随 HAP 打包，
// 只在构建期由资产脚本消费；逻辑层因此内嵌同构数据）。仅外圈城墙
//（OuterRing）与背景塔楼（Backdrop）参与碰撞：前者形成可玩区边界
// 与可攀爬墙体，后者作为远景屏障；中心裂隙平台与装饰碎块不阻挡
// 探索动线，避免卡住战斗与祭坛交互。
// 盒体由放置项平移/旋转/缩放与源网格半尺寸经 EnvironmentWorldFit
// 相似变换求得，与渲染批次的绘制变换严格一致，保证碰撞与可见建筑对齐。

#include "native/engine/world/environment_collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kRegionOuterRing = 0;
constexpr int kRegionCenterRift = 1;
constexpr int kRegionBackdrop = 2;
constexpr int kRegionDecoration = 3;

// layout.json 条目镜像（halfExtents 为源网格近似半尺寸，布局空间米制）。
// rotation 仅含绕 Y 分量：(0, sin(yaw/2), 0, cos(yaw/2))。
constexpr EnvironmentPlacement kLayout[] = {
    // ---- OuterRing ----
    {kRegionOuterRing, {20.0f, -1.0f, 0.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.0f, 1.0f, 1.0f}, {1.0f, 2.0f, 1.0f}},       // outer_east_corner
    {kRegionOuterRing, {20.0f, -1.0f, 10.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 2.0f, 0.6f}},       // outer_east_wall
    {kRegionOuterRing, {0.0f, -1.0f, -20.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {1.0f, 1.0f, 1.0f}, {2.2f, 2.4f, 0.5f}},       // outer_north_arch
    {kRegionOuterRing, {10.0f, -1.0f, -20.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 1.7f, 0.6f}},       // outer_north_walkway
    {kRegionOuterRing, {16.0f, -1.0f, -16.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.22f, 0.75f, 0.22f}, {1.6f, 4.2f, 1.6f}},    // outer_northeast_pillar
    {kRegionOuterRing, {0.0f, -1.0f, 20.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.0f, 1.0f, 1.0f}, {2.2f, 2.4f, 0.5f}},       // outer_south_arch
    {kRegionOuterRing, {-10.0f, -1.0f, 20.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 1.7f, 0.6f}},       // outer_south_walkway
    {kRegionOuterRing, {-16.0f, -1.0f, 16.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.22f, 0.75f, 0.22f}, {1.6f, 4.2f, 1.6f}},    // outer_southwest_pillar
    {kRegionOuterRing, {-20.0f, -1.0f, 0.0f},
     {0.0f, -0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.0f, 1.0f, 1.0f}, {1.0f, 2.0f, 1.0f}},       // outer_west_corner
    {kRegionOuterRing, {-20.0f, -1.0f, -10.0f},
     {0.0f, -0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 2.0f, 0.6f}},       // outer_west_wall
    // ---- Backdrop ----
    {kRegionBackdrop, {0.0f, -2.0f, -34.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.35f, 1.7f, 1.35f}, {1.6f, 5.0f, 1.6f}},     // backdrop_north_tower
    {kRegionBackdrop, {0.0f, -2.0f, -31.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {3.0f, 1.5f, 1.0f}, {2.5f, 2.0f, 0.6f}},       // backdrop_north_wall
    {kRegionBackdrop, {-32.0f, -2.0f, -18.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.25f, 1.55f, 1.25f}, {1.6f, 5.0f, 1.6f}},    // backdrop_west_tower
    {kRegionBackdrop, {-29.0f, -2.0f, 0.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {2.5f, 1.4f, 1.0f}, {2.5f, 2.0f, 0.6f}},       // backdrop_west_wall
    // ---- CenterRift / Decoration 不参与碰撞，仅保留批次枚举完整性 ----
    {kRegionCenterRift, {}, {}, {}, {}},
    {kRegionDecoration, {}, {}, {}, {}},
};

// 绕 Y 四元数 → yaw：q = (0, sin(yaw/2), 0, cos(yaw/2))。
float yawFromQuat(const float rotation[4]) {
  return 2.0f * std::atan2(rotation[1], rotation[3]);
}

}  // namespace

const EnvironmentPlacement* environmentLayoutPlacements() { return kLayout; }

std::size_t environmentLayoutPlacementCount() {
  return sizeof(kLayout) / sizeof(kLayout[0]);
}

EnvironmentWorldFit environmentWorldFitForRegion(
    int region, const EnvironmentFitAnchors& anchors) {
  EnvironmentWorldFit fit;
  switch (region) {
    case kRegionOuterRing:
      fit.scale = 0.02f;
      fit.yBias = -0.012f;
      break;
    case kRegionCenterRift:
      fit.scale = 0.02f;
      fit.yBias = -0.010f;
      fit.centerX = anchors.altarX;
      fit.centerZ = anchors.altarZ;
      break;
    case kRegionBackdrop:
      fit.scale = 0.02f;
      fit.yBias = -0.022f;
      break;
    default:  // Decoration 等：与渲染同参，锚定祭坛。
      fit.scale = 0.02f;
      fit.yBias = -0.012f;
      fit.centerX = anchors.altarX;
      fit.centerZ = anchors.altarZ;
      break;
  }
  return fit;
}

BuildingCollision BuildingCollision::fromEnvironmentLayout(float altarX,
                                                           float altarZ) {
  const EnvironmentFitAnchors anchors{altarX, altarZ};
  std::vector<BuildingBox> boxes;
  for (std::size_t i = 0; i < environmentLayoutPlacementCount(); ++i) {
    const EnvironmentPlacement& placement = kLayout[i];
    if (placement.region != kRegionOuterRing &&
        placement.region != kRegionBackdrop) {
      continue;  // 平台与装饰不阻挡探索动线。
    }
    const EnvironmentWorldFit fit =
        environmentWorldFitForRegion(placement.region, anchors);
    BuildingBox box;
    box.cx = fit.centerX + fit.scale * placement.translation[0];
    box.cz = fit.centerZ + fit.scale * placement.translation[2];
    box.hx = fit.scale * placement.halfExtents[0] * placement.scale[0];
    box.hz = fit.scale * placement.halfExtents[2] * placement.scale[2];
    box.yaw = yawFromQuat(placement.rotation);
    box.top = fit.yBias +
              fit.scale * (placement.translation[1] +
                           placement.halfExtents[1] * placement.scale[1] * 2.0f);
    boxes.push_back(box);
  }
  return BuildingCollision{boxes};
}

namespace {

// 把世界点变换到盒局部坐标（先去心，再反向旋转 yaw）。
void toLocal(const BuildingBox& box, float x, float y, float& lx, float& ly) {
  const float dx = x - box.cx;
  const float dy = y - box.cz;
  const float c = std::cos(-box.yaw);
  const float s = std::sin(-box.yaw);
  lx = dx * c - dy * s;
  ly = dx * s + dy * c;
}

}  // namespace

BuildingContact BuildingCollision::resolve(float& x, float& y, float radius,
                                           float height) const {
  BuildingContact contact;
  const float r = std::max(0.0f, radius);
  // 登顶容忍：高度接近盒顶时不再阻挡，允许翻上墙头（mantle），
  // 由宿主层的站立支撑查询接管后续贴合。
  constexpr float kMantleTolerance = 0.004f;
  for (const BuildingBox& box : boxes_) {
    if (height > box.top - kMantleTolerance) continue;  // 越过/接近盒顶。
    float lx = 0.0f;
    float ly = 0.0f;
    toLocal(box, x, y, lx, ly);
    const float ex = box.hx + r;
    const float ez = box.hz + r;
    if (std::abs(lx) >= ex || std::abs(ly) >= ez) continue;
    // 取穿透最浅轴推出，保留另一轴分量形成沿墙滑动。
    const float penetrationX = ex - std::abs(lx);
    const float penetrationY = ez - std::abs(ly);
    float normalLocalX = 0.0f;
    float normalLocalY = 0.0f;
    if (penetrationX <= penetrationY) {
      normalLocalX = lx >= 0.0f ? 1.0f : -1.0f;
      lx += normalLocalX * penetrationX;
    } else {
      normalLocalY = ly >= 0.0f ? 1.0f : -1.0f;
      ly += normalLocalY * penetrationY;
    }
    // 局部推出位移变换回世界坐标并应用。
    const float c = std::cos(box.yaw);
    const float s = std::sin(box.yaw);
    const float wx = box.cx + lx * c - ly * s;
    const float wy = box.cz + lx * s + ly * c;
    const float pushX = wx - x;
    const float pushY = wy - y;
    x = wx;
    y = wy;
    contact.touching = true;
    contact.normal = contact.normal + Vec2{pushX, pushY};
    contact.highestTop = std::max(contact.highestTop, box.top);
  }
  if (contact.touching && contact.normal.length() > 1e-6f) {
    const float inv = 1.0f / contact.normal.length();
    contact.normal = contact.normal * inv;
  }
  return contact;
}

float BuildingCollision::supportTopAt(float x, float y, float radius) const {
  const float r = std::max(0.0f, radius);
  float top = -std::numeric_limits<float>::infinity();
  for (const BuildingBox& box : boxes_) {
    float lx = 0.0f;
    float ly = 0.0f;
    toLocal(box, x, y, lx, ly);
    if (std::abs(lx) <= box.hx + r && std::abs(ly) <= box.hz + r) {
      top = std::max(top, box.top);
    }
  }
  return top;
}

float BuildingCollision::standingTopAt(float x, float y, float radius,
                                       float currentHeight,
                                       float tolerance) const {
  const float r = std::max(0.0f, radius);
  float top = -std::numeric_limits<float>::infinity();
  for (const BuildingBox& box : boxes_) {
    // 盒顶明显高于角色当前高度：那是面前的墙而不是脚下的支撑面。
    if (box.top > currentHeight + tolerance) continue;
    float lx = 0.0f;
    float ly = 0.0f;
    toLocal(box, x, y, lx, ly);
    if (std::abs(lx) <= box.hx + r && std::abs(ly) <= box.hz + r) {
      top = std::max(top, box.top);
    }
  }
  return top;
}
