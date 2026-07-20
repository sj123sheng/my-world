// mesh.h: 3D 网格数据结构与硬编码几何体生成。
//
// Vertex 包含 position/normal/uv，Mesh 持有顶点/索引数组与可选 GL 资源句柄。
// createCube/createPlane 生成硬编码几何体，upload/draw/destroy 在非 HarmonyOS 平台
// 为空操作，便于在 macOS 下做纯数据单元测试。

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
