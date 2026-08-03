// mesh.h: 3D 网格数据结构与硬编码几何体生成。
//
// Vertex 包含 position/normal/uv，Mesh 持有顶点/索引数组与可选 GL 资源句柄。
// createCube/createPlane 生成硬编码几何体，upload/draw/destroy 在非 HarmonyOS 平台
// 为空操作，便于在 macOS 下做纯数据单元测试。

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <vector>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

// glTF 蒙皮顶点布局。关节索引直接作为整型属性传给顶点着色器。
struct SkinnedVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::uvec4 joints;
  glm::vec4 weights;
};

// Shader3D 顶点属性槽位。静态网格仅绑定前 3 个槽位。
constexpr unsigned int kPositionAttribute = 0;
constexpr unsigned int kNormalAttribute = 1;
constexpr unsigned int kUvAttribute = 2;
constexpr unsigned int kJointsAttribute = 3;
constexpr unsigned int kWeightsAttribute = 4;

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // GL 资源句柄（仅在 HarmonyOS 平台由 upload() 创建）。
  unsigned int vbo = 0;
  unsigned int ibo = 0;
  unsigned int texture = 0;

  // 上传顶点/索引缓冲区到 GPU。非 OHOS 平台为空操作。
  void upload();

  // 绑定缓冲区并调用 glDrawElements。非 OHOS 平台为空操作。
  void draw() const;

  // 释放 GPU 资源。非 OHOS 平台为空操作。
  void destroy();

  // context 已不可 current 时仅丢弃 CPU 句柄跟踪，绝不发出 GL 删除调用。
  // 随后的 eglDestroyContext 负责回收实际驱动对象。
  void abandonGpuResources();
};

// 生成立方体网格：24 顶点（6 面 × 4 顶点，每面法线独立），36 索引（12 三角形）。
// size 为立方体边长，几何中心在原点。
Mesh createCube(float size);

// 生成 XZ 平面网格：4 顶点，6 索引，法线朝上 (0,1,0)。
// width 为 X 方向跨度，depth 为 Z 方向跨度，几何中心在原点。
Mesh createPlane(float width, float depth);
Mesh createCylinder(float radius, float height, uint32_t segments);
Mesh createRing(float radius, float thickness, uint32_t segments);

// 生成球体网格：球心在原点，rings 为纬度分层数（>=2），segments 为经向分段（>=3）。
// 卷绕为标准 CCW 外翻，可安全配合 GL_BACK 背面剔除使用。
Mesh createSphere(float radius, uint32_t segments, uint32_t rings);

// 生成圆锥网格：底面圆心在原点，顶点朝 +Y，高度 height。
Mesh createCone(float radius, float height, uint32_t segments);

// 生成朝上的圆形盘（XZ 平面，y=0），用于角色接地接触阴影。
Mesh createDisk(float radius, uint32_t segments);

// 生成胶囊网格：沿 Y 轴，圆柱段高 height，两端为半径 radius 的半球。
Mesh createCapsule(float radius, float height, uint32_t segments,
                   uint32_t rings);

// 将 src 的变换副本追加到 dst：位置经 transform 变换，法线经左上 3x3
// 旋转后重新归一化。用于把多个部件合并为单次绘制的角色网格。
void mergeMesh(Mesh& dst, const Mesh& src, const glm::mat4& transform);

// 风格化角色回退几何体：主体包络 y ∈ [-0.5, +0.5]（Boss 角/刺略超出以增强体量感），
// 脚底贴 y=-0.5，面向 +Z，与 createCube(1.0) 同样的单位包络约定，
// 可直接复用 AssetProfile 缩放。
Mesh createHumanoid();  // 英雄比例：头/躯干/四肢/头盔冠饰（玩家）
Mesh createBrute();     // 粗壮比例：宽肩、肩刺、短腿（敌人）
Mesh createBeast();     // 巨型比例：双角、背棘、巨爪（首领）
