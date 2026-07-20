#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

// Camera3D: 3D 透视第三人称相机。
//
// 用球坐标在目标周围计算 position，输出 view/projection 矩阵供 3D 着色器使用。
// 与现有 2D ThirdPersonCamera 独立，仅升级渲染层，不影响 2D 游戏逻辑。
class Camera3D {
 public:
  // 相机参数（公开，便于外部直接配置）。
  float aspectRatio = 1.7777f;  // 16:9 默认
  float fov = 45.0f;           // 透视垂直视场角（度）
  float nearPlane = 0.1f;
  float farPlane = 100.0f;

  // 相机当前状态（由 follow() 更新）。
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 target{0.0f, 0.0f, 0.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};

  // 用球坐标在 target 周围计算 position。
  // position = target + distance * (cos(pitch)*cos(yaw), sin(pitch), cos(pitch)*sin(yaw))
  void follow(glm::vec3 targetPos, float yaw, float pitch, float distance);

  // 返回 view 矩阵（glm::lookAt(position, target, up)）。
  glm::mat4 viewMatrix() const;

  // 返回 perspective 投影矩阵。
  glm::mat4 projectionMatrix() const;

  // 返回 projection * view。
  glm::mat4 viewProjection() const;
};
