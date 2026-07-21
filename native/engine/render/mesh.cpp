// mesh.cpp: 硬编码几何体生成与 GL 缓冲区管理。
//
// createCube 按 6 面 × 4 顶点展开，保证每面法线独立（共享位置会有法线冲突，
// 所以用 24 顶点而非 8）。createPlane 生成朝上的地面平面。upload/draw/destroy
// 的 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作，保证 macOS 单测安全。

#include "native/engine/render/mesh.h"

#include <cstddef>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

namespace {

// 向 vertices 追加一个面（4 顶点 + 6 索引）。normal 为该面统一法线。
// 四个角按 UV (0,0)(1,0)(1,1)(0,1) 顺序，索引按两三角形卷绕。
void appendFace(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                const glm::vec3& p0, const glm::vec3& p1,
                const glm::vec3& p2, const glm::vec3& p3,
                const glm::vec3& normal) {
  const uint32_t base = static_cast<uint32_t>(vertices.size());
  vertices.push_back({p0, normal, {0.0f, 0.0f}});
  vertices.push_back({p1, normal, {1.0f, 0.0f}});
  vertices.push_back({p2, normal, {1.0f, 1.0f}});
  vertices.push_back({p3, normal, {0.0f, 1.0f}});
  // 两个三角形：base, base+1, base+2 与 base, base+2, base+3
  indices.push_back(base + 0);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base + 0);
  indices.push_back(base + 2);
  indices.push_back(base + 3);
}

}  // namespace

Mesh createCube(float size) {
  const float h = size * 0.5f;
  Mesh mesh;
  mesh.vertices.reserve(24);
  mesh.indices.reserve(36);

  // +X 面
  appendFace(mesh.vertices, mesh.indices,
             {+h, -h, -h}, {+h, -h, +h}, {+h, +h, +h}, {+h, +h, -h},
             {1.0f, 0.0f, 0.0f});
  // -X 面
  appendFace(mesh.vertices, mesh.indices,
             {-h, -h, +h}, {-h, -h, -h}, {-h, +h, -h}, {-h, +h, +h},
             {-1.0f, 0.0f, 0.0f});
  // +Y 面（顶面）
  appendFace(mesh.vertices, mesh.indices,
             {-h, +h, -h}, {+h, +h, -h}, {+h, +h, +h}, {-h, +h, +h},
             {0.0f, 1.0f, 0.0f});
  // -Y 面（底面）
  appendFace(mesh.vertices, mesh.indices,
             {-h, -h, +h}, {+h, -h, +h}, {+h, -h, -h}, {-h, -h, -h},
             {0.0f, -1.0f, 0.0f});
  // +Z 面
  appendFace(mesh.vertices, mesh.indices,
             {+h, -h, +h}, {-h, -h, +h}, {-h, +h, +h}, {+h, +h, +h},
             {0.0f, 0.0f, 1.0f});
  // -Z 面
  appendFace(mesh.vertices, mesh.indices,
             {-h, -h, -h}, {+h, -h, -h}, {+h, +h, -h}, {-h, +h, -h},
             {0.0f, 0.0f, -1.0f});

  return mesh;
}

Mesh createPlane(float width, float depth) {
  const float hw = width * 0.5f;
  const float hd = depth * 0.5f;
  Mesh mesh;
  mesh.vertices.reserve(4);
  mesh.indices.reserve(6);

  const glm::vec3 up{0.0f, 1.0f, 0.0f};
  // 顶点顺序：左前、右前、右后、左后（Z 递增方向为后）
  mesh.vertices.push_back({{-hw, 0.0f, -hd}, up, {0.0f, 0.0f}});
  mesh.vertices.push_back({{+hw, 0.0f, -hd}, up, {1.0f, 0.0f}});
  mesh.vertices.push_back({{+hw, 0.0f, +hd}, up, {1.0f, 1.0f}});
  mesh.vertices.push_back({{-hw, 0.0f, +hd}, up, {0.0f, 1.0f}});

  mesh.indices.push_back(0);
  mesh.indices.push_back(1);
  mesh.indices.push_back(2);
  mesh.indices.push_back(0);
  mesh.indices.push_back(2);
  mesh.indices.push_back(3);

  return mesh;
}

void Mesh::upload() {
#ifdef OHOS_PLATFORM
  if (vbo != 0u || ibo != 0u) {
    return;  // 已上传，避免重复创建造成资源泄漏
  }
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
               indices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
}

void Mesh::draw() const {
#ifdef OHOS_PLATFORM
  if (vbo == 0u || ibo == 0u) {
    return;  // 未上传，无可绘制内容
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

  const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
  // 静态 Mesh 只绑定 aPosition/aNormal/aUV（槽位 0–2）。
  glEnableVertexAttribArray(kPositionAttribute);
  glVertexAttribPointer(kPositionAttribute, 3, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<void*>(offsetof(Vertex, position)));
  // aNormal
  glEnableVertexAttribArray(kNormalAttribute);
  glVertexAttribPointer(kNormalAttribute, 3, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<void*>(offsetof(Vertex, normal)));
  // aUV
  glEnableVertexAttribArray(kUvAttribute);
  glVertexAttribPointer(kUvAttribute, 2, GL_FLOAT, GL_FALSE, stride,
                         reinterpret_cast<void*>(offsetof(Vertex, uv)));

  if (texture != 0u) {
    glBindTexture(GL_TEXTURE_2D, texture);
  }
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                 GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
}

void Mesh::destroy() {
#ifdef OHOS_PLATFORM
  if (vbo != 0u) {
    glDeleteBuffers(1, &vbo);
    vbo = 0;
  }
  if (ibo != 0u) {
    glDeleteBuffers(1, &ibo);
    ibo = 0;
  }
  // texture 由 texture.cpp 的 loadTexture 创建，此处不释放以免双重释放；
  // 调用方如需释放纹理应在 Surface 销毁时单独 glDeleteTextures。
#endif
}

void Mesh::abandonGpuResources() {
  vbo = 0;
  ibo = 0;
  texture = 0;
}
