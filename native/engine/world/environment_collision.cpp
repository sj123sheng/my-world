// environment_collision.cpp: 建筑碰撞集与滑动解算。
//
// 布局数据镜像 assets/environment/layout.json（该文件不随 HAP 打包，
// 只在构建期由资产脚本消费；逻辑层因此内嵌同构数据）。仅外圈城墙
//（OuterRing）与背景塔楼（Backdrop）参与碰撞：前者形成可玩区边界
// 与可攀爬墙体，后者作为远景屏障；中心裂隙平台与装饰碎块不阻挡
// 探索动线，避免卡住战斗与祭坛交互。
// 盒体由放置项平移/旋转/缩放与源网格半尺寸经 EnvironmentWorldFit
// 相似变换求得，与渲染批次的绘制变换严格一致，保证碰撞与可见建筑对齐。
//
// Phase 2：镜像条目携带 blockId（-1 全局 / ≥0 所属 8×8 分块）。
// blockId≥0 的区块条目统一按 OuterRing 世界适配参数定位（世界中心
// 锚点），与渲染层区块批次 environmentBlockWorldFitMatrix 一致。
// 碰撞盒按分块分桶（见 environment_collision.h 分桶策略说明），
// 查询仅遍历目标点所在分块 + 邻块的候选盒，结果与全量遍历一致。

#include "native/engine/world/environment_collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kRegionOuterRing = 0;
constexpr int kRegionCenterRift = 1;
constexpr int kRegionBackdrop = 2;
constexpr int kRegionDecoration = 3;

// 全局组 blockId。
constexpr int32_t kGlobalBlock = -1;

// layout.json 条目镜像（halfExtents 为源网格近似半尺寸，布局空间米制）。
// rotation 仅含绕 Y 分量：(0, sin(yaw/2), 0, cos(yaw/2))。
// 条目顺序与 layout.json 保持一致；最后一列为 blockId。
constexpr EnvironmentPlacement kLayout[] = {
    // ---- Backdrop（全局远景）----
    {kRegionBackdrop, {0.0f, -2.0f, -34.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.35f, 1.7f, 1.35f}, {1.6f, 5.0f, 1.6f}, kGlobalBlock},  // backdrop_north_tower
    {kRegionBackdrop, {0.0f, -2.0f, -31.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {3.0f, 1.5f, 1.0f}, {2.5f, 2.0f, 0.6f}, kGlobalBlock},    // backdrop_north_wall
    {kRegionBackdrop, {-32.0f, -2.0f, -18.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.25f, 1.55f, 1.25f}, {1.6f, 5.0f, 1.6f}, kGlobalBlock}, // backdrop_west_tower
    {kRegionBackdrop, {-29.0f, -2.0f, 0.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {2.5f, 1.4f, 1.0f}, {2.5f, 2.0f, 0.6f}, kGlobalBlock},    // backdrop_west_wall
    // ---- CenterRift（全局祭坛平台，不阻挡）----
    {kRegionCenterRift, {6.0f, -1.0f, 0.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {0.7f, 0.65f, 0.7f}, {2.2f, 2.4f, 0.5f}, kGlobalBlock},   // center_east_frame
    {kRegionCenterRift, {0.0f, -1.2f, -4.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.8f, 0.28f, 0.8f}, {2.5f, 1.7f, 0.6f}, kGlobalBlock},   // center_north_platform
    {kRegionCenterRift, {0.0f, -1.2f, 4.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {0.8f, 0.28f, 0.8f}, {2.5f, 1.7f, 0.6f}, kGlobalBlock},   // center_south_platform
    {kRegionCenterRift, {-6.0f, -1.0f, 0.0f},
     {0.0f, -0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {0.7f, 0.65f, 0.7f}, {2.2f, 2.4f, 0.5f}, kGlobalBlock},   // center_west_frame
    // ---- Decoration（全局装饰碎块，不阻挡）----
    {kRegionDecoration, {12.0f, -1.4f, 7.0f},
     {0.0f, 0.3826834323650898f, 0.0f, 0.9238795325112867f},
     {0.38f, 0.25f, 0.55f}, {1.0f, 1.6f, 0.9f}, kGlobalBlock}, // decoration_east_rubble
    {kRegionDecoration, {-7.0f, -1.5f, -12.0f},
     {0.0f, -0.25881904510252074f, 0.0f, 0.9659258262890683f},
     {0.3f, 0.22f, 0.42f}, {1.0f, 1.6f, 0.9f}, kGlobalBlock},  // decoration_north_rubble
    {kRegionDecoration, {5.0f, -1.45f, 13.0f},
     {0.0f, 0.6087614290087207f, 0.0f, 0.7933533402912352f},
     {0.42f, 0.3f, 0.36f}, {1.4f, 1.7f, 0.6f}, kGlobalBlock},  // decoration_south_rubble
    {kRegionDecoration, {-13.0f, -1.45f, 4.0f},
     {0.0f, -0.5f, 0.0f, 0.8660254037844386f},
     {0.34f, 0.24f, 0.46f}, {1.0f, 1.6f, 0.9f}, kGlobalBlock}, // decoration_west_rubble
    // ---- OuterRing（全局外圈城墙）----
    {kRegionOuterRing, {20.0f, -1.0f, 0.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.0f, 1.0f, 1.0f}, {1.0f, 2.0f, 1.0f}, kGlobalBlock},    // outer_east_corner
    {kRegionOuterRing, {20.0f, -1.0f, 10.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 2.0f, 0.6f}, kGlobalBlock},    // outer_east_wall
    {kRegionOuterRing, {0.0f, -1.0f, -20.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {1.0f, 1.0f, 1.0f}, {2.2f, 2.4f, 0.5f}, kGlobalBlock},    // outer_north_arch
    {kRegionOuterRing, {10.0f, -1.0f, -20.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 1.7f, 0.6f}, kGlobalBlock},    // outer_north_walkway
    {kRegionOuterRing, {16.0f, -1.0f, -16.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.22f, 0.75f, 0.22f}, {1.6f, 4.2f, 1.6f}, kGlobalBlock}, // outer_northeast_pillar
    {kRegionOuterRing, {0.0f, -1.0f, 20.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {1.0f, 1.0f, 1.0f}, {2.2f, 2.4f, 0.5f}, kGlobalBlock},    // outer_south_arch
    {kRegionOuterRing, {-10.0f, -1.0f, 20.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 1.7f, 0.6f}, kGlobalBlock},    // outer_south_walkway
    {kRegionOuterRing, {-16.0f, -1.0f, 16.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.22f, 0.75f, 0.22f}, {1.6f, 4.2f, 1.6f}, kGlobalBlock}, // outer_southwest_pillar
    {kRegionOuterRing, {-20.0f, -1.0f, 0.0f},
     {0.0f, -0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.0f, 1.0f, 1.0f}, {1.0f, 2.0f, 1.0f}, kGlobalBlock},    // outer_west_corner
    {kRegionOuterRing, {-20.0f, -1.0f, -10.0f},
     {0.0f, -0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {1.6f, 1.0f, 1.0f}, {2.5f, 2.0f, 0.6f}, kGlobalBlock},    // outer_west_wall
    // ---- 区块地标/氛围（Phase 2，blockId≥0，随分块流式启停）----
    // 启明台地（spawn_plateau）
    {kRegionOuterRing, {-4.0f, -1.0f, -15.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.8f, 0.75f, 0.8f}, {2.2f, 2.4f, 0.5f}, 11},             // spawn_plateau_gate
    {kRegionDecoration, {4.0f, -1.0f, -16.0f},
     {0.0f, 0.29552020666133955f, 0.0f, 0.955336489125606f},
     {0.34f, 0.24f, 0.4f}, {1.0f, 1.6f, 0.9f}, 12},            // spawn_plateau_rubble
    // 翠风低地（westlands）
    {kRegionOuterRing, {-16.5f, -1.0f, -18.5f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.26f, 0.8f, 0.26f}, {1.6f, 4.2f, 1.6f}, 9},             // westlands_tower
    {kRegionDecoration, {-11.0f, -1.0f, -8.0f},
     {0.0f, -0.24740395925452294f, 0.0f, 0.9689124217106447f},
     {0.36f, 0.26f, 0.4f}, {1.4f, 1.7f, 0.6f}, 18},            // westlands_rubble
    // 辉光湖畔（gimmerlake）
    {kRegionOuterRing, {16.5f, -1.0f, -16.5f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.3f, 0.9f, 0.3f}, {1.6f, 4.2f, 1.6f}, 14},              // gimmerlake_tower
    {kRegionDecoration, {9.0f, -1.0f, -9.0f},
     {0.0f, 0.8414709848078965f, 0.0f, 0.5403023058681398f},
     {0.32f, 0.22f, 0.42f}, {1.0f, 1.6f, 0.9f}, 21},           // gimmerlake_rubble
    // 中枢回廊（central_corridor）
    {kRegionOuterRing, {-3.0f, -1.0f, -9.5f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {0.9f, 0.8f, 0.9f}, {2.2f, 2.4f, 0.5f}, 19},              // central_corridor_gate
    {kRegionOuterRing, {3.0f, -1.0f, -6.0f},
     {0.0f, 0.7071067811865476f, 0.0f, 0.7071067811865476f},
     {0.9f, 1.0f, 0.9f}, {2.5f, 1.7f, 0.6f}, 28},              // central_corridor_walkway
    // 灰烬荒原（ashen_wastes）
    {kRegionOuterRing, {-14.0f, -1.0f, 11.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.34f, 1.0f, 0.34f}, {1.6f, 4.2f, 1.6f}, 41},            // ashen_wastes_tower
    {kRegionDecoration, {-10.0f, -1.0f, 5.0f},
     {0.0f, 0.5646424733950354f, 0.0f, 0.8253356149096783f},
     {0.4f, 0.28f, 0.5f}, {1.0f, 1.6f, 0.9f}, 34},             // ashen_wastes_rubble
    // 圣所高地（sanctum_highlands）
    {kRegionOuterRing, {14.0f, -1.0f, 14.0f},
     {0.0f, 0.0f, 0.0f, 1.0f},
     {0.36f, 1.1f, 0.36f}, {1.6f, 4.2f, 1.6f}, 54},            // sanctum_highlands_tower
    {kRegionDecoration, {6.0f, -1.0f, 9.0f},
     {0.0f, -0.479425538604203f, 0.0f, 0.8775825618903728f},
     {0.38f, 0.26f, 0.44f}, {1.4f, 1.7f, 0.6f}, 44},           // sanctum_highlands_rubble
};

// 绕 Y 四元数 → yaw：q = (0, sin(yaw/2), 0, cos(yaw/2))。
float yawFromQuat(const float rotation[4]) {
  return 2.0f * std::atan2(rotation[1], rotation[3]);
}

float clampUnit(float value) {
  return std::min(1.0f, std::max(0.0f, value));
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
    // 区块批次条目统一按 OuterRing 参数定位（世界中心锚点），
    // 与渲染层区块批次的世界适配矩阵一致；全局条目沿用区域参数。
    const EnvironmentWorldFit fit =
        placement.blockId >= 0
            ? environmentWorldFitForRegion(kRegionOuterRing, anchors)
            : environmentWorldFitForRegion(placement.region, anchors);
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

// 对候选盒集合执行滑动解算（候选按盒子下标升序，保证与全量遍历同序）。
BuildingContact resolveBoxes(const std::vector<BuildingBox>& boxes,
                             const std::vector<int32_t>& candidates,
                             float& x, float& y, float radius, float height) {
  BuildingContact contact;
  const float r = std::max(0.0f, radius);
  // 登顶容忍：高度接近盒顶时不再阻挡，允许翻上墙头（mantle），
  // 由宿主层的站立支撑查询接管后续贴合。
  constexpr float kMantleTolerance = 0.004f;
  for (const int32_t index : candidates) {
    const BuildingBox& box = boxes[static_cast<size_t>(index)];
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

}  // namespace

int32_t BuildingCollision::bucketIndexAt(float x, float y) {
  const int32_t bx = std::min(
      kBucketCountX - 1,
      std::max(0, static_cast<int32_t>(std::floor(clampUnit(x) *
                                                  kBucketCountX))));
  const int32_t by = std::min(
      kBucketCountY - 1,
      std::max(0, static_cast<int32_t>(std::floor(clampUnit(y) *
                                                  kBucketCountY))));
  return by * kBucketCountX + bx;
}

void BuildingCollision::rebuildBuckets() {
  buckets_.assign(static_cast<size_t>(kBucketCountX * kBucketCountY), {});
  for (std::size_t i = 0; i < boxes_.size(); ++i) {
    const BuildingBox& box = boxes_[i];
    // OBB 的轴对齐包围盒半尺寸（含旋转）。
    const float c = std::abs(std::cos(box.yaw));
    const float s = std::abs(std::sin(box.yaw));
    const float ax = box.hx * c + box.hz * s + kBucketMargin;
    const float az = box.hx * s + box.hz * c + kBucketMargin;
    // 中心越界的盒子（远景背景）钳制到最近边缘，保证至少归属一个分块。
    const float cx = clampUnit(box.cx);
    const float cz = clampUnit(box.cz);
    const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(
                                       (cx - ax) * kBucketCountX)));
    const int32_t x1 = std::min(kBucketCountX - 1,
                                static_cast<int32_t>(std::floor(
                                    (cx + ax) * kBucketCountX)));
    const int32_t y0 = std::max(0, static_cast<int32_t>(std::floor(
                                       (cz - az) * kBucketCountY)));
    const int32_t y1 = std::min(kBucketCountY - 1,
                                static_cast<int32_t>(std::floor(
                                    (cz + az) * kBucketCountY)));
    for (int32_t by = y0; by <= y1; ++by) {
      for (int32_t bx = x0; bx <= x1; ++bx) {
        buckets_[static_cast<size_t>(by * kBucketCountX + bx)].push_back(
            static_cast<int32_t>(i));
      }
    }
  }
}

std::vector<int32_t> BuildingCollision::candidatesNear(float x, float y) const {
  if (buckets_.empty()) return {};
  const int32_t center = bucketIndexAt(x, y);
  const int32_t bx = center % kBucketCountX;
  const int32_t by = center / kBucketCountX;
  std::vector<int32_t> candidates;
  // 目标点所在分块 + 8 邻块；贴墙推出可能跨越分块边界，邻块查询容忍
  // 与盒子的跨块归属一起保证候选集覆盖所有可能触碰的盒。
  for (int32_t dy = -1; dy <= 1; ++dy) {
    const int32_t ny = std::min(kBucketCountY - 1, std::max(0, by + dy));
    for (int32_t dx = -1; dx <= 1; ++dx) {
      const int32_t nx = std::min(kBucketCountX - 1, std::max(0, bx + dx));
      const std::vector<int32_t>& bucket =
          buckets_[static_cast<size_t>(ny * kBucketCountX + nx)];
      candidates.insert(candidates.end(), bucket.begin(), bucket.end());
    }
  }
  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());
  return candidates;
}

BuildingContact BuildingCollision::resolve(float& x, float& y, float radius,
                                           float height) const {
  if (buckets_.empty()) {
    // 未经分桶构造（例如默认构造后直接填充）：退化为全量遍历。
    std::vector<int32_t> all(boxes_.size());
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
      all[i] = static_cast<int32_t>(i);
    }
    return resolveBoxes(boxes_, all, x, y, radius, height);
  }
  const std::vector<int32_t> candidates = candidatesNear(x, y);
  return resolveBoxes(boxes_, candidates, x, y, radius, height);
}

namespace {

float supportTopAmong(const std::vector<BuildingBox>& boxes,
                      const std::vector<int32_t>& candidates, float x, float y,
                      float radius, float heightLimit, bool useLimit) {
  const float r = std::max(0.0f, radius);
  float top = -std::numeric_limits<float>::infinity();
  for (const int32_t index : candidates) {
    const BuildingBox& box = boxes[static_cast<size_t>(index)];
    // 盒顶明显高于角色当前高度：那是面前的墙而不是脚下的支撑面。
    if (useLimit && box.top > heightLimit) continue;
    float lx = 0.0f;
    float ly = 0.0f;
    toLocal(box, x, y, lx, ly);
    if (std::abs(lx) <= box.hx + r && std::abs(ly) <= box.hz + r) {
      top = std::max(top, box.top);
    }
  }
  return top;
}

std::vector<int32_t> allIndices(std::size_t count) {
  std::vector<int32_t> all(count);
  for (std::size_t i = 0; i < count; ++i) all[i] = static_cast<int32_t>(i);
  return all;
}

}  // namespace

float BuildingCollision::supportTopAt(float x, float y, float radius) const {
  const std::vector<int32_t> candidates =
      buckets_.empty() ? allIndices(boxes_.size()) : candidatesNear(x, y);
  return supportTopAmong(boxes_, candidates, x, y, radius, 0.0f, false);
}

float BuildingCollision::standingTopAt(float x, float y, float radius,
                                       float currentHeight,
                                       float tolerance) const {
  const std::vector<int32_t> candidates =
      buckets_.empty() ? allIndices(boxes_.size()) : candidatesNear(x, y);
  return supportTopAmong(boxes_, candidates, x, y, radius,
                         currentHeight + tolerance, true);
}

BuildingContact BuildingCollision::resolveBruteForce(float& x, float& y,
                                                     float radius,
                                                     float height) const {
  return resolveBoxes(boxes_, allIndices(boxes_.size()), x, y, radius, height);
}

float BuildingCollision::supportTopAtBruteForce(float x, float y,
                                                float radius) const {
  return supportTopAmong(boxes_, allIndices(boxes_.size()), x, y, radius, 0.0f,
                         false);
}
