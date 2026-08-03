// test_mesh.cpp: 验证 createCube/createPlane 生成的顶点和索引数据正确性。
//
// 仅测试纯数据生成路径（不涉及 GL 调用），GL 上传/绘制由 #ifdef OHOS_PLATFORM 保护，
// 非 HarmonyOS 平台为空操作，便于 macOS 下做单元测试。

#include "native/engine/render/mesh.h"

#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

static_assert(offsetof(SkinnedVertex, position) == 0);

namespace {

bool close(float actual, float expected, float eps = 0.0001f) {
  return std::fabs(actual - expected) < eps;
}

void testCubeHasCorrectVertexCount() {
  Mesh cube = createCube(1.0f);
  assert(cube.vertices.size() == 24);
  assert(cube.indices.size() == 36);
}

void testPlaneHasCorrectVertexCount() {
  Mesh plane = createPlane(10.0f, 10.0f);
  assert(plane.vertices.size() == 4);
  assert(plane.indices.size() == 6);
}

void testCubeNormalsFaceOutward() {
  Mesh cube = createCube(1.0f);
  for (const auto& v : cube.vertices) {
    const float len = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y +
                                v.normal.z * v.normal.z);
    assert(close(len, 1.0f) && "cube normals should be unit length");
  }
}

void testPlaneNormalFacesUp() {
  Mesh plane = createPlane(10.0f, 10.0f);
  for (const auto& v : plane.vertices) {
    assert(close(v.normal.x, 0.0f));
    assert(close(v.normal.y, 1.0f));
    assert(close(v.normal.z, 0.0f));
  }
}

void testCubeVertexPositionsFitSize() {
  // size=2.0 -> half-extent 1.0，每个面位置应在 [-1,1] 范围内。
  Mesh cube = createCube(2.0f);
  for (const auto& v : cube.vertices) {
    assert(v.position.x >= -1.0f - 0.0001f && v.position.x <= 1.0f + 0.0001f);
    assert(v.position.y >= -1.0f - 0.0001f && v.position.y <= 1.0f + 0.0001f);
    assert(v.position.z >= -1.0f - 0.0001f && v.position.z <= 1.0f + 0.0001f);
  }
}

void testPlaneCoversRequestedSize() {
  Mesh plane = createPlane(10.0f, 20.0f);
  float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
  for (const auto& v : plane.vertices) {
    minX = std::min(minX, v.position.x);
    maxX = std::max(maxX, v.position.x);
    minZ = std::min(minZ, v.position.z);
    maxZ = std::max(maxZ, v.position.z);
  }
  assert(close(maxX - minX, 10.0f));
  assert(close(maxZ - minZ, 20.0f));
}

void testCubeIndicesInBounds() {
  Mesh cube = createCube(1.0f);
  const std::size_t n = cube.vertices.size();
  for (const auto idx : cube.indices) {
    assert(idx < n);
  }
}

void testPlaneIndicesInBounds() {
  Mesh plane = createPlane(10.0f, 10.0f);
  const std::size_t n = plane.vertices.size();
  for (const auto idx : plane.indices) {
    assert(idx < n);
  }
}

void testMeshDefaultResourceHandles() {
  // 未 upload 的 Mesh 不应触碰 GL，资源句柄应为 0。
  Mesh cube = createCube(1.0f);
  assert(cube.vbo == 0);
  assert(cube.ibo == 0);
  assert(cube.texture == 0);
  // draw/destroy 在非 OHOS 平台为空操作，应安全返回。
  cube.draw();
  cube.destroy();
}

void testSkinnedVertexUsesExpectedAttributeSlots() {
  assert(kPositionAttribute == 0 && kNormalAttribute == 1 && kUvAttribute == 2);
  assert(kJointsAttribute == 3 && kWeightsAttribute == 4);
}

void testCylinderHasFiniteNormalsAndTriangles() {
  Mesh cylinder = createCylinder(1.0f, 2.0f, 16);
  assert(!cylinder.vertices.empty());
  assert(cylinder.indices.size() == 16u * 12u);
  for (const Vertex& vertex : cylinder.vertices) {
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
  }
}

bool indicesInBounds(const Mesh& mesh) {
  for (const uint32_t index : mesh.indices) {
    if (index >= mesh.vertices.size()) return false;
  }
  return mesh.indices.size() % 3u == 0u;
}

bool normalsUnitLength(const Mesh& mesh) {
  for (const Vertex& vertex : mesh.vertices) {
    const float length = std::sqrt(vertex.normal.x * vertex.normal.x +
                                   vertex.normal.y * vertex.normal.y +
                                   vertex.normal.z * vertex.normal.z);
    if (!close(length, 1.0f, 0.001f)) return false;
  }
  return true;
}

// 球心在原点的凸体：每个三角形的卷绕法线（e1×e2）应指向外侧，
// 即与三角形质心方向点积非负。验证 CCW 外翻约定与 GL_BACK 剔除兼容。
bool trianglesFaceOutward(const Mesh& mesh) {
  for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const Vertex& a = mesh.vertices[mesh.indices[t]];
    const Vertex& b = mesh.vertices[mesh.indices[t + 1]];
    const Vertex& c = mesh.vertices[mesh.indices[t + 2]];
    const glm::vec3 e1 = b.position - a.position;
    const glm::vec3 e2 = c.position - a.position;
    const glm::vec3 faceNormal = glm::cross(e1, e2);
    const glm::vec3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (glm::dot(faceNormal, centroid) < -1e-6f) return false;
  }
  return true;
}

void testSphereBoundsNormalsAndWinding() {
  Mesh sphere = createSphere(0.5f, 12, 8);
  assert(!sphere.vertices.empty());
  assert(indicesInBounds(sphere));
  assert(normalsUnitLength(sphere));
  for (const Vertex& vertex : sphere.vertices) {
    const float length = std::sqrt(vertex.position.x * vertex.position.x +
                                   vertex.position.y * vertex.position.y +
                                   vertex.position.z * vertex.position.z);
    assert(close(length, 0.5f, 0.001f) && "sphere vertices lie on surface");
  }
  assert(trianglesFaceOutward(sphere));
  // 非法参数返回空网格。
  assert(createSphere(-1.0f, 12, 8).vertices.empty());
  assert(createSphere(0.5f, 2, 8).vertices.empty());
}

void testConeBoundsAndWinding() {
  Mesh cone = createCone(0.3f, 0.6f, 10);
  assert(!cone.vertices.empty());
  assert(indicesInBounds(cone));
  for (const Vertex& vertex : cone.vertices) {
    assert(vertex.position.y >= -1e-5f && vertex.position.y <= 0.6f + 1e-5f);
  }
  // 侧面三角形的卷绕法线应带向上的分量（外翻）。
  bool hasUpwardFacingTriangle = false;
  for (std::size_t t = 0; t + 2 < cone.indices.size(); t += 3) {
    const Vertex& a = cone.vertices[cone.indices[t]];
    const Vertex& b = cone.vertices[cone.indices[t + 1]];
    const Vertex& c = cone.vertices[cone.indices[t + 2]];
    const glm::vec3 n = glm::cross(b.position - a.position,
                                   c.position - a.position);
    if (n.y > 0.0f) hasUpwardFacingTriangle = true;
  }
  assert(hasUpwardFacingTriangle);
}

void testCapsuleEnvelopeAndWinding() {
  Mesh capsule = createCapsule(0.2f, 0.4f, 10, 6);
  assert(!capsule.vertices.empty());
  assert(indicesInBounds(capsule));
  assert(normalsUnitLength(capsule));
  for (const Vertex& vertex : capsule.vertices) {
    assert(vertex.position.y >= -0.4f - 1e-5f && vertex.position.y <= 0.4f + 1e-5f);
  }
  assert(trianglesFaceOutward(capsule));
}

void testMergeMeshTransformsPositionsAndNormals() {
  Mesh dst = createCube(1.0f);
  const std::size_t vertexCount = dst.vertices.size();
  const std::size_t indexCount = dst.indices.size();
  const glm::mat4 up = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
  mergeMesh(dst, createCube(1.0f), up);
  assert(dst.vertices.size() == vertexCount * 2);
  assert(dst.indices.size() == indexCount * 2);
  // 追加部分的位置应整体抬高 2，法线保持不变。
  for (std::size_t i = vertexCount; i < dst.vertices.size(); ++i) {
    const Vertex& moved = dst.vertices[i];
    const Vertex& original = dst.vertices[i - vertexCount];
    assert(close(moved.position.y - 2.0f, original.position.y));
    assert(close(moved.normal.x, original.normal.x));
    assert(close(moved.normal.y, original.normal.y));
    assert(close(moved.normal.z, original.normal.z));
  }
}

void testCharacterMeshesHaveValidStructureAndFootBaseline() {
  const Mesh characters[] = {createHumanoid(), createBrute(), createBeast()};
  for (const Mesh& mesh : characters) {
    assert(!mesh.vertices.empty());
    assert(indicesInBounds(mesh));
    assert(normalsUnitLength(mesh));
    float minY = 1e9f;
    float maxY = -1e9f;
    for (const Vertex& vertex : mesh.vertices) {
      minY = std::min(minY, vertex.position.y);
      maxY = std::max(maxY, vertex.position.y);
    }
    // 脚底基线贴 y=-0.5，主体不超过单位包络过多（Boss 角/刺允许少量超出）。
    assert(close(minY, -0.5f, 0.02f));
    assert(maxY >= 0.4f && maxY <= 0.62f);
  }
}

}  // namespace

int main() {
  testCubeHasCorrectVertexCount();
  testPlaneHasCorrectVertexCount();
  testCubeNormalsFaceOutward();
  testPlaneNormalFacesUp();
  testCubeVertexPositionsFitSize();
  testPlaneCoversRequestedSize();
  testCubeIndicesInBounds();
  testPlaneIndicesInBounds();
  testMeshDefaultResourceHandles();
  testSkinnedVertexUsesExpectedAttributeSlots();
  testCylinderHasFiniteNormalsAndTriangles();
  testSphereBoundsNormalsAndWinding();
  testConeBoundsAndWinding();
  testCapsuleEnvelopeAndWinding();
  testMergeMeshTransformsPositionsAndNormals();
  testCharacterMeshesHaveValidStructureAndFootBaseline();
  return 0;
}
