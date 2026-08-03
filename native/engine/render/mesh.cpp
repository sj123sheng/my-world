// mesh.cpp: 硬编码几何体生成与 GL 缓冲区管理。
//
// createCube 按 6 面 × 4 顶点展开，保证每面法线独立（共享位置会有法线冲突，
// 所以用 24 顶点而非 8）。createPlane 生成朝上的地面平面。upload/draw/destroy
// 的 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作，保证 macOS 单测安全。

#include "native/engine/render/mesh.h"

#include <cstddef>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

namespace {

constexpr float kPi = 3.14159265358979323846f;

uint32_t pushVertex(std::vector<Vertex>& vertices, const glm::vec3& position,
                     const glm::vec3& normal, const glm::vec2& uv = {0.0f, 0.0f}) {
  vertices.push_back({position, normal, uv});
  return static_cast<uint32_t>(vertices.size() - 1);
}

// 把一条环带（upper 在上、lower 在下，同角度对齐）连成四边形条带。
// 卷绕按 CCW 外翻推导：(U_a, U_b, L_b) 与 (U_a, L_b, L_a)。
void connectRings(std::vector<uint32_t>& indices,
                  const std::vector<uint32_t>& upper,
                  const std::vector<uint32_t>& lower) {
  const uint32_t segments = static_cast<uint32_t>(upper.size());
  for (uint32_t i = 0; i < segments; ++i) {
    const uint32_t j = (i + 1u) % segments;
    indices.push_back(upper[i]);
    indices.push_back(upper[j]);
    indices.push_back(lower[j]);
    indices.push_back(upper[i]);
    indices.push_back(lower[j]);
    indices.push_back(lower[i]);
  }
}

// 把顶部极点以扇形连到第一圈环；卷绕 (ring_i, pole, ring_{i+1}) 为外翻。
void connectTopPole(std::vector<uint32_t>& indices, uint32_t pole,
                    const std::vector<uint32_t>& ring) {
  const uint32_t segments = static_cast<uint32_t>(ring.size());
  for (uint32_t i = 0; i < segments; ++i) {
    indices.push_back(ring[i]);
    indices.push_back(pole);
    indices.push_back(ring[(i + 1u) % segments]);
  }
}

// 把底部极点以扇形连到最后一圈环；卷绕 (ring_i, ring_{i+1}, pole) 为外翻。
void connectBottomPole(std::vector<uint32_t>& indices,
                       const std::vector<uint32_t>& ring, uint32_t pole) {
  const uint32_t segments = static_cast<uint32_t>(ring.size());
  for (uint32_t i = 0; i < segments; ++i) {
    indices.push_back(ring[i]);
    indices.push_back(ring[(i + 1u) % segments]);
    indices.push_back(pole);
  }
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

Mesh createCylinder(float radius, float height, uint32_t segments) {
  Mesh mesh;
  if (radius <= 0.0f || height <= 0.0f || segments < 3) return mesh;
  const float half = height * 0.5f;
  constexpr float kTau = 6.2831853071795864769f;
  mesh.vertices.reserve(segments * 4u);
  mesh.indices.reserve(segments * 12u);
  for (uint32_t i = 0; i < segments; ++i) {
    const uint32_t next = (i + 1u) % segments;
    const float a = kTau * static_cast<float>(i) / static_cast<float>(segments);
    const float b = kTau * static_cast<float>(next) / static_cast<float>(segments);
    const glm::vec3 pa{std::cos(a) * radius, -half, std::sin(a) * radius};
    const glm::vec3 pb{std::cos(b) * radius, -half, std::sin(b) * radius};
    const glm::vec3 ta{pa.x, half, pa.z};
    const glm::vec3 tb{pb.x, half, pb.z};
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({pa, glm::normalize(glm::vec3(pa.x, 0.0f, pa.z)), {0, 0}});
    mesh.vertices.push_back({pb, glm::normalize(glm::vec3(pb.x, 0.0f, pb.z)), {1, 0}});
    mesh.vertices.push_back({tb, glm::normalize(glm::vec3(pb.x, 0.0f, pb.z)), {1, 1}});
    mesh.vertices.push_back({ta, glm::normalize(glm::vec3(pa.x, 0.0f, pa.z)), {0, 1}});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3,
                                             base, base + 3, base + 1, base + 1, base + 3, base + 2});
  }
  return mesh;
}

Mesh createRing(float radius, float thickness, uint32_t segments) {
  Mesh mesh;
  if (radius <= 0.0f || thickness <= 0.0f || segments < 3) return mesh;
  constexpr float kTau = 6.2831853071795864769f;
  const float inner = radius - thickness * 0.5f;
  const float outer = radius + thickness * 0.5f;
  if (inner <= 0.0f) return mesh;
  mesh.vertices.reserve(segments * 4u);
  mesh.indices.reserve(segments * 6u);
  for (uint32_t i = 0; i < segments; ++i) {
    const float a = kTau * static_cast<float>(i) / static_cast<float>(segments);
    const float b = kTau * static_cast<float>(i + 1u) /
                    static_cast<float>(segments);
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(
        {{std::cos(a) * inner, 0.0f, std::sin(a) * inner}, {0, 1, 0}, {0, 0}});
    mesh.vertices.push_back(
        {{std::cos(a) * outer, 0.0f, std::sin(a) * outer}, {0, 1, 0}, {0, 1}});
    mesh.vertices.push_back(
        {{std::cos(b) * outer, 0.0f, std::sin(b) * outer}, {0, 1, 0}, {1, 1}});
    mesh.vertices.push_back(
        {{std::cos(b) * inner, 0.0f, std::sin(b) * inner}, {0, 1, 0}, {1, 0}});
    mesh.indices.insert(mesh.indices.end(),
                        {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  return mesh;
}

Mesh createSphere(float radius, uint32_t segments, uint32_t rings) {
  Mesh mesh;
  if (radius <= 0.0f || segments < 3u || rings < 2u) return mesh;
  constexpr float kTau = 6.2831853071795864769f;

  const uint32_t top =
      pushVertex(mesh.vertices, {0.0f, radius, 0.0f}, {0.0f, 1.0f, 0.0f});
  std::vector<std::vector<uint32_t>> rows;
  rows.reserve(rings - 1);
  for (uint32_t r = 1; r < rings; ++r) {
    const float phi =
        kPi * 0.5f - kPi * static_cast<float>(r) / static_cast<float>(rings);
    const float y = std::sin(phi);
    const float ringRadius = std::cos(phi);
    std::vector<uint32_t> row;
    row.reserve(segments);
    for (uint32_t i = 0; i < segments; ++i) {
      const float theta = kTau * static_cast<float>(i) /
                          static_cast<float>(segments);
      const glm::vec3 normal{std::cos(theta) * ringRadius, y,
                             std::sin(theta) * ringRadius};
      row.push_back(pushVertex(mesh.vertices, normal * radius, normal));
    }
    rows.push_back(std::move(row));
  }
  const uint32_t bottom =
      pushVertex(mesh.vertices, {0.0f, -radius, 0.0f}, {0.0f, -1.0f, 0.0f});

  connectTopPole(mesh.indices, top, rows.front());
  for (uint32_t r = 0; r + 1u < rows.size(); ++r) {
    connectRings(mesh.indices, rows[r], rows[r + 1]);
  }
  connectBottomPole(mesh.indices, rows.back(), bottom);
  return mesh;
}

Mesh createCone(float radius, float height, uint32_t segments) {
  Mesh mesh;
  if (radius <= 0.0f || height <= 0.0f || segments < 3u) return mesh;
  constexpr float kTau = 6.2831853071795864769f;
  const glm::vec3 apex{0.0f, height, 0.0f};

  std::vector<uint32_t> ring;
  ring.reserve(segments);
  for (uint32_t i = 0; i < segments; ++i) {
    const float theta = kTau * static_cast<float>(i) /
                        static_cast<float>(segments);
    const glm::vec3 position{std::cos(theta) * radius, 0.0f,
                             std::sin(theta) * radius};
    // 侧面法线：径向分量 + 向上分量，归一化后连续过渡。
    glm::vec3 normal{std::cos(theta) * height, radius,
                     std::sin(theta) * height};
    normal = glm::normalize(normal);
    ring.push_back(pushVertex(mesh.vertices, position, normal));
  }
  const uint32_t apexIndex = pushVertex(
      mesh.vertices, apex,
      glm::normalize(glm::vec3(radius, height, 0.0f)));
  // 侧面：(ring_i, apex, ring_{i+1}) 为 CCW 外翻。
  for (uint32_t i = 0; i < segments; ++i) {
    mesh.indices.push_back(ring[i]);
    mesh.indices.push_back(apexIndex);
    mesh.indices.push_back(ring[(i + 1u) % segments]);
  }
  // 底盖：另建一圈法线朝下的顶点，避免侧面法线干扰底面光照。
  std::vector<uint32_t> bottomRing;
  bottomRing.reserve(segments);
  for (uint32_t i = 0; i < segments; ++i) {
    const float theta = kTau * static_cast<float>(i) /
                        static_cast<float>(segments);
    bottomRing.push_back(pushVertex(
        mesh.vertices,
        {std::cos(theta) * radius, 0.0f, std::sin(theta) * radius},
        {0.0f, -1.0f, 0.0f}));
  }
  const uint32_t bottomCenter = pushVertex(
      mesh.vertices, {0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
  connectBottomPole(mesh.indices, bottomRing, bottomCenter);
  return mesh;
}

Mesh createCapsule(float radius, float height, uint32_t segments,
                   uint32_t rings) {
  Mesh mesh;
  if (radius <= 0.0f || height < 0.0f || segments < 3u || rings < 2u) {
    return mesh;
  }
  constexpr float kTau = 6.2831853071795864769f;
  const float halfCylinder = height * 0.5f;
  const uint32_t hemisphereRings = (rings + 1u) / 2u;

  const uint32_t top = pushVertex(mesh.vertices,
                                  {0.0f, halfCylinder + radius, 0.0f},
                                  {0.0f, 1.0f, 0.0f});
  std::vector<std::vector<uint32_t>> rows;
  rows.reserve(hemisphereRings * 2u);

  // 上半球：从极点下方逐圈降到赤道（y = +halfCylinder）。
  for (uint32_t r = 1; r <= hemisphereRings; ++r) {
    const float phi = kPi * 0.5f * (1.0f - static_cast<float>(r) /
                                               static_cast<float>(hemisphereRings));
    const float ringY = halfCylinder + radius * std::sin(phi);
    const float ringRadius = radius * std::cos(phi);
    const glm::vec3 capCenter{0.0f, halfCylinder, 0.0f};
    std::vector<uint32_t> row;
    row.reserve(segments);
    for (uint32_t i = 0; i < segments; ++i) {
      const float theta = kTau * static_cast<float>(i) /
                          static_cast<float>(segments);
      const glm::vec3 position{std::cos(theta) * ringRadius, ringY,
                               std::sin(theta) * ringRadius};
      const glm::vec3 normal = glm::normalize(position - capCenter);
      row.push_back(pushVertex(mesh.vertices, position, normal));
    }
    rows.push_back(std::move(row));
  }
  // 下半球：从赤道（y = -halfCylinder）逐圈降到底部极点。
  for (uint32_t r = 0; r < hemisphereRings; ++r) {
    const float phi = -kPi * 0.5f * static_cast<float>(r) /
                      static_cast<float>(hemisphereRings);
    const float ringY = -halfCylinder + radius * std::sin(phi);
    const float ringRadius = radius * std::cos(phi);
    const glm::vec3 capCenter{0.0f, -halfCylinder, 0.0f};
    std::vector<uint32_t> row;
    row.reserve(segments);
    for (uint32_t i = 0; i < segments; ++i) {
      const float theta = kTau * static_cast<float>(i) /
                          static_cast<float>(segments);
      const glm::vec3 position{std::cos(theta) * ringRadius, ringY,
                               std::sin(theta) * ringRadius};
      const glm::vec3 normal = glm::normalize(position - capCenter);
      row.push_back(pushVertex(mesh.vertices, position, normal));
    }
    rows.push_back(std::move(row));
  }
  const uint32_t bottom = pushVertex(mesh.vertices,
                                     {0.0f, -halfCylinder - radius, 0.0f},
                                     {0.0f, -1.0f, 0.0f});

  connectTopPole(mesh.indices, top, rows.front());
  for (uint32_t r = 0; r + 1u < rows.size(); ++r) {
    connectRings(mesh.indices, rows[r], rows[r + 1]);
  }
  connectBottomPole(mesh.indices, rows.back(), bottom);
  return mesh;
}

void mergeMesh(Mesh& dst, const Mesh& src, const glm::mat4& transform) {
  const uint32_t base = static_cast<uint32_t>(dst.vertices.size());
  const glm::mat3 normalMatrix(transform);
  dst.vertices.reserve(dst.vertices.size() + src.vertices.size());
  dst.indices.reserve(dst.indices.size() + src.indices.size());
  for (const Vertex& vertex : src.vertices) {
    const glm::vec4 position = transform * glm::vec4(vertex.position, 1.0f);
    glm::vec3 normal = normalMatrix * vertex.normal;
    const float length = glm::length(normal);
    if (length > 1e-6f) {
      normal /= length;
    }
    dst.vertices.push_back({{position.x, position.y, position.z}, normal,
                            vertex.uv});
  }
  for (const uint32_t index : src.indices) {
    dst.indices.push_back(base + index);
  }
}

namespace {

// 常用部件生成器包装：减少角色组装代码中的重复参数。
Mesh sphereAt(float radius) { return createSphere(radius, 12u, 8u); }
Mesh limbAt(float radius, float height) {
  return createCylinder(radius, height, 10u);
}
glm::mat4 translateTo(float x, float y, float z) {
  return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

}  // namespace

Mesh createHumanoid() {
  Mesh mesh;
  // 躯干：上下端分别被胸球与髋球覆盖，避免开口外露。
  mergeMesh(mesh, createCylinder(0.17f, 0.36f, 12u), translateTo(0.0f, -0.04f, 0.0f));
  mergeMesh(mesh, sphereAt(0.165f), translateTo(0.0f, 0.12f, 0.0f));
  mergeMesh(mesh, createSphere(0.13f, 10u, 6u), translateTo(0.0f, -0.24f, 0.0f));
  // 头部与头盔冠饰（角尖恰好在单位包络顶端 y=+0.5）。
  mergeMesh(mesh, sphereAt(0.12f), translateTo(0.0f, 0.31f, 0.0f));
  mergeMesh(mesh, createCone(0.05f, 0.09f, 8u), translateTo(0.0f, 0.41f, 0.0f));
  // 肩甲 + 手臂 + 手部，左右对称。
  for (const float side : {-1.0f, 1.0f}) {
    mergeMesh(mesh, createSphere(0.075f, 8u, 6u),
              translateTo(side * 0.21f, 0.10f, 0.0f));
    mergeMesh(mesh, limbAt(0.055f, 0.26f),
              translateTo(side * 0.24f, -0.06f, 0.0f));
    mergeMesh(mesh, createSphere(0.06f, 8u, 6u),
              translateTo(side * 0.24f, -0.21f, 0.0f));
    // 腿部 + 足部，脚底球心使包络下界贴 y=-0.5。
    mergeMesh(mesh, createCylinder(0.085f, 0.28f, 10u),
              translateTo(side * 0.10f, -0.36f, 0.0f));
    mergeMesh(mesh, createSphere(0.08f, 8u, 6u),
              translateTo(side * 0.10f, -0.42f, 0.02f));
  }
  return mesh;
}

Mesh createBrute() {
  Mesh mesh;
  // 宽厚躯干：肩线下移，头部埋入肩部形成压迫感。
  mergeMesh(mesh, createCylinder(0.21f, 0.30f, 12u), translateTo(0.0f, -0.06f, 0.0f));
  mergeMesh(mesh, sphereAt(0.20f), translateTo(0.0f, 0.10f, 0.0f));
  mergeMesh(mesh, createSphere(0.15f, 10u, 6u), translateTo(0.0f, -0.22f, 0.0f));
  mergeMesh(mesh, createSphere(0.10f, 10u, 6u), translateTo(0.0f, 0.30f, 0.04f));
  for (const float side : {-1.0f, 1.0f}) {
    // 肩甲与向外斜的肩刺。
    mergeMesh(mesh, createSphere(0.10f, 8u, 6u),
              translateTo(side * 0.26f, 0.08f, 0.0f));
    mergeMesh(mesh, createCone(0.045f, 0.10f, 8u),
              translateTo(side * 0.27f, 0.13f, 0.0f) *
                  glm::rotate(glm::mat4(1.0f), side * -0.5f,
                              glm::vec3(0.0f, 0.0f, 1.0f)));
    // 粗手臂与巨掌。
    mergeMesh(mesh, limbAt(0.07f, 0.24f),
              translateTo(side * 0.29f, -0.08f, 0.0f));
    mergeMesh(mesh, createSphere(0.075f, 8u, 6u),
              translateTo(side * 0.29f, -0.23f, 0.0f));
    // 短粗腿。
    mergeMesh(mesh, createCylinder(0.10f, 0.26f, 10u),
              translateTo(side * 0.12f, -0.37f, 0.0f));
    mergeMesh(mesh, createSphere(0.095f, 8u, 6u),
              translateTo(side * 0.12f, -0.405f, 0.02f));
  }
  return mesh;
}

Mesh createBeast() {
  Mesh mesh;
  // 巨型躯干 + 胸口隆起。
  mergeMesh(mesh, createCylinder(0.20f, 0.34f, 12u), translateTo(0.0f, -0.05f, 0.0f));
  mergeMesh(mesh, sphereAt(0.19f), translateTo(0.0f, 0.12f, 0.0f));
  mergeMesh(mesh, createSphere(0.16f, 10u, 6u), translateTo(0.0f, -0.22f, 0.0f));
  // 头颅与向外弯曲的双角。
  mergeMesh(mesh, sphereAt(0.14f), translateTo(0.0f, 0.32f, 0.0f));
  for (const float side : {-1.0f, 1.0f}) {
    mergeMesh(mesh, createCone(0.04f, 0.13f, 8u),
              translateTo(side * 0.09f, 0.39f, -0.01f) *
                  glm::rotate(glm::mat4(1.0f), side * 0.35f,
                              glm::vec3(0.0f, 0.0f, 1.0f)));
    // 肩甲 + 背向尖刺。
    mergeMesh(mesh, createSphere(0.11f, 10u, 6u),
              translateTo(side * 0.24f, 0.10f, 0.0f));
    mergeMesh(mesh, createCone(0.05f, 0.11f, 8u),
              translateTo(side * 0.26f, 0.15f, -0.04f) *
                  glm::rotate(glm::mat4(1.0f), side * -0.45f,
                              glm::vec3(0.0f, 0.0f, 1.0f)) *
                  glm::rotate(glm::mat4(1.0f), -0.3f,
                              glm::vec3(1.0f, 0.0f, 0.0f)));
    // 长臂巨爪。
    mergeMesh(mesh, limbAt(0.075f, 0.30f),
              translateTo(side * 0.27f, -0.06f, 0.0f));
    mergeMesh(mesh, createSphere(0.08f, 8u, 6u),
              translateTo(side * 0.27f, -0.24f, 0.0f));
    // 粗壮下肢。
    mergeMesh(mesh, createCylinder(0.11f, 0.26f, 10u),
              translateTo(side * 0.13f, -0.37f, 0.0f));
    mergeMesh(mesh, createSphere(0.105f, 8u, 6u),
              translateTo(side * 0.13f, -0.395f, 0.02f));
  }
  // 背部中央尖刺，增强 Boss 剖影辨识度。
  mergeMesh(mesh, createCone(0.05f, 0.12f, 8u),
            translateTo(0.0f, 0.16f, -0.16f) *
                glm::rotate(glm::mat4(1.0f), -0.5f,
                            glm::vec3(1.0f, 0.0f, 0.0f)));
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
