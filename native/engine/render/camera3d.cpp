#include "native/engine/render/camera3d.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <cmath>

void Camera3D::follow(glm::vec3 targetPos, float yaw, float pitch, float distance) {
  target = targetPos;
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  // 轨道偏移与 2D ThirdPersonCamera 约定一致：
  // 屏幕上方 = 世界 {sin(yaw), cos(yaw)}，屏幕右方 = {cos(yaw), -sin(yaw)}。
  // 相机位于目标身后：backward = -forward，右移相机时 yaw 增大（标准第三人称）。
  position = targetPos + glm::vec3(sy * cp, sp, -cy * cp) * distance;
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
