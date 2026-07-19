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
  position = targetPos + glm::vec3(cp * cy, sp, cp * sy) * distance;
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
