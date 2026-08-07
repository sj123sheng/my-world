// 视锥剔除纯数学单测（Phase 5）：平面提取 + 球体/AABB 测试。
#include "../native/engine/render/frustum_cull.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <cmath>

int main() {
  // 相机位于原点朝 -Z，90 度视场角，near=1 far=50。
  const glm::mat4 proj = glm::perspective(
      static_cast<float>(M_PI) * 0.5f, 1.0f, 1.0f, 50.0f);
  const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 0.0f, -1.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
  const FrustumPlanes frustum =
      FrustumPlanesFromViewProjection(proj * view);

  // 视锥中心的球体可见。
  assert(FrustumContainsSphere(frustum, {0.0f, 0.0f, -10.0f}, 1.0f));
  // 相机背后（near 平面外侧）剔除。
  assert(!FrustumContainsSphere(frustum, {0.0f, 0.0f, 10.0f}, 1.0f));
  // 超过 far 平面剔除。
  assert(!FrustumContainsSphere(frustum, {0.0f, 0.0f, -60.0f}, 1.0f));
  // 90 度视场：z=-10 处水平半宽约 10；x=25 远在右侧平面外。
  assert(!FrustumContainsSphere(frustum, {25.0f, 0.0f, -10.0f}, 1.0f));
  // 恰好擦边的球体（保守保留）。
  assert(FrustumContainsSphere(frustum, {11.0f, 0.0f, -10.0f}, 2.0f));

  // 视锥内的 AABB 可见。
  assert(FrustumContainsAabb(frustum, {-1.0f, -1.0f, -11.0f},
                             {1.0f, 1.0f, -9.0f}));
  // 左侧平面外的 AABB 剔除。
  assert(!FrustumContainsAabb(frustum, {-30.0f, -1.0f, -11.0f},
                              {-20.0f, 1.0f, -9.0f}));
  // 与视锥相交的 AABB 保守保留（p-vertex 测试不做过度剔除）。
  assert(FrustumContainsAabb(frustum, {-20.0f, -1.0f, -11.0f},
                             {0.0f, 1.0f, -9.0f}));

  // 平移后的视锥：相机看向 +X 方向 (5,0,5) 处的目标。
  const glm::mat4 view2 = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                      glm::vec3(5.0f, 0.0f, 5.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
  const FrustumPlanes f2 = FrustumPlanesFromViewProjection(proj * view2);
  assert(FrustumContainsSphere(f2, {3.0f, 0.0f, 3.0f}, 0.5f));
  assert(!FrustumContainsSphere(f2, {-3.0f, 0.0f, -3.0f}, 0.5f));
  return 0;
}
