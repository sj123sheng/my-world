#include "environment.h"

#include <algorithm>
#include <cmath>

EnvironmentComposition EnvironmentController::defaultComposition() {
  return {{0.50f, 0.0f, 0.12f},
          {0.34f, 0.0f, 0.26f},
          {0.50f, 0.0f, 0.48f},
          {0.50f, 0.0f, 0.75f},
          {0.50f, 0.12f, 0.82f}};
}

namespace {

// float 可精确表示的最大整数（2^24）；分块差超出该范围时钳制，
// 保证超远坐标下输出仍然有限且确定。
constexpr long double kMaxSafeChunkDelta = 16777216.0L;

float SafeChunkDelta(int64_t target, int64_t origin) {
  // 先提升到 long double 再做整数差，避免 10^12 级坐标先转 float
  // 造成的精度塌缩；再钳制到 float 安全整数范围。
  const long double delta =
      static_cast<long double>(target) - static_cast<long double>(origin);
  const long double clamped =
      std::clamp(delta, -kMaxSafeChunkDelta, kMaxSafeChunkDelta);
  return static_cast<float>(clamped);
}

}  // namespace

glm::vec3 ChunkRenderTranslation(ChunkCoord target, ChunkCoord origin,
                                 LocalPosition originLocal) {
  // 渲染空间以 origin 分块角点为原点，玩家锚定在 originLocal；
  // target 分块平移为整数分块差，与 originLocal 的具体值无关，
  // 保留该参数是为了让调用方显式表达渲染原点约定。
  (void)originLocal;
  return {SafeChunkDelta(target.x, origin.x), 0.0f,
          SafeChunkDelta(target.y, origin.y)};
}

bool ChunkRenderCommittable(ChunkCoord target, ChunkCoord origin,
                            int32_t activeRadius) {
  // 切比雪夫距离用 long double 计算，避免 int64 差值溢出；
  // 半径钳制到非负。
  const long double radius =
      static_cast<long double>(std::max<int32_t>(activeRadius, 0));
  const long double dx = std::abs(static_cast<long double>(target.x) -
                                  static_cast<long double>(origin.x));
  const long double dy = std::abs(static_cast<long double>(target.y) -
                                  static_cast<long double>(origin.y));
  return std::max(dx, dy) <= radius;
}
