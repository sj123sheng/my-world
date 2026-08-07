#pragma once

// 视锥剔除纯数学工具（Phase 5）：
// 从 VP（projection * view）矩阵提取 6 个视锥平面，提供球体与
// AABB 的快速可见性测试。全部为无状态纯函数，不依赖渲染 API，
// 便于宿主侧单元测试；绘制层只在绘制前做判断，不改变任何逻辑。
//
// 平面约定：每个平面 (a, b, c, d) 法线朝内（指向视锥内部），
// 点 p 在该平面内侧当且仅当 a*p.x + b*p.y + c*p.z + d >= 0。

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/geometric.hpp>

#include <cmath>

struct FrustumPlanes {
  // 顺序：0=Left 1=Right 2=Bottom 3=Top 4=Near 5=Far。
  glm::vec4 planes[6] = {};
};

// 从 clip 空间矩阵（VP = projection * view）提取并归一化 6 个平面。
// 采用 Gribb/Hartmann 行组合法：left = row3 + row0 等；glm 为列主序，
// row_i 的第 j 分量为 m[j][i]。零法线平面保持原样（调用方通常不会遇到）。
inline FrustumPlanes FrustumPlanesFromViewProjection(const glm::mat4& m) {
  const auto row = [&m](int i) {
    return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]);
  };
  FrustumPlanes out;
  out.planes[0] = row(3) + row(0);  // Left
  out.planes[1] = row(3) - row(0);  // Right
  out.planes[2] = row(3) + row(1);  // Bottom
  out.planes[3] = row(3) - row(1);  // Top
  out.planes[4] = row(3) + row(2);  // Near
  out.planes[5] = row(3) - row(2);  // Far
  for (glm::vec4& plane : out.planes) {
    const float length =
        std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    if (length > 0.0f) plane /= length;
  }
  return out;
}

// 球体可见性：球体与任一平面外侧距离超过半径即剔除。
// 保守测试（可能保留少量视锥外交集），适合角色/环境件点状包围球。
inline bool FrustumContainsSphere(const FrustumPlanes& frustum,
                                   const glm::vec3& center, float radius) {
  for (const glm::vec4& plane : frustum.planes) {
    const float distance = plane.x * center.x + plane.y * center.y +
                           plane.z * center.z + plane.w;
    if (distance < -radius) return false;
  }
  return true;
}

// AABB 可见性：逐平面取法线方向上的最远顶点（p-vertex）测试，
// p-vertex 在平面外侧则整个盒体在外，剔除；否则保留。
inline bool FrustumContainsAabb(const FrustumPlanes& frustum,
                                const glm::vec3& boxMin,
                                const glm::vec3& boxMax) {
  for (const glm::vec4& plane : frustum.planes) {
    const glm::vec3 pVertex{
        plane.x >= 0.0f ? boxMax.x : boxMin.x,
        plane.y >= 0.0f ? boxMax.y : boxMin.y,
        plane.z >= 0.0f ? boxMax.z : boxMin.z,
    };
    const float distance = plane.x * pVertex.x + plane.y * pVertex.y +
                           plane.z * pVertex.z + plane.w;
    if (distance < 0.0f) return false;
  }
  return true;
}
