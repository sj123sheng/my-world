#include "native/engine/render/camera3d.h"

#include "native/engine/math/camera_ground_basis.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <cmath>

void Camera3D::follow(glm::vec3 targetPos, float yaw, float pitch, float distance) {
  target = targetPos;
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);
  const CameraGroundBasis basis = CameraGroundBasisForYaw(yaw);
  // 相机位于水平前向的反方向。逻辑 y 映射到 3D z；右手坐标系下
  // 屏幕右向为 {-cos(yaw), sin(yaw)}。
  position = targetPos +
             glm::vec3(-basis.forward.x * cp, sp,
                       -basis.forward.y * cp) *
                 distance;
}

glm::mat4 Camera3D::viewMatrix() const {
  return glm::lookAt(position, target, up);
}

glm::mat4 Camera3D::projectionMatrix() const {
  // glm 1.0.1 perspective 参数为 (fovRadians, aspectRatio, nearPlane, farPlane)
  return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

glm::mat4 Camera3D::viewProjection() const {
  return projectionMatrix() * viewMatrix();
}
