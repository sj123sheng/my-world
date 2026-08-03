#pragma once

#include "native/engine/math/vec2.h"

#include <cmath>

// 逻辑世界 (x, y) 映射到 3D 地面 (x, z)。在右手坐标系中，
// 相机前方与屏幕右方必须组成以下正交基，不能直接套用二维顺时针旋转。
struct CameraGroundBasis {
  Vec2 forward;
  Vec2 right;
};

inline CameraGroundBasis CameraGroundBasisForYaw(float yaw) {
  return {{std::sin(yaw), std::cos(yaw)},
          {-std::cos(yaw), std::sin(yaw)}};
}
