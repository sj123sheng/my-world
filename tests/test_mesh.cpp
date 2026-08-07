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

void testCubeWindingMatchesFaceNormals() {
  // 卷绕法线（e1×e2）必须与面法线同向（外翻 = 正面），否则
  // GL_BACK 剔除下立方体各面不可见（角色回退/剑柄都依赖它）。
  Mesh cube = createCube(1.0f);
  assert(indicesInBounds(cube));
  for (std::size_t t = 0; t + 2 < cube.indices.size(); t += 3) {
    const Vertex& v0 = cube.vertices[cube.indices[t]];
    const Vertex& v1 = cube.vertices[cube.indices[t + 1]];
    const Vertex& v2 = cube.vertices[cube.indices[t + 2]];
    const glm::vec3 winding = glm::cross(v1.position - v0.position,
                                         v2.position - v0.position);
    assert(glm::dot(winding, v0.normal) > 0.0f);
  }
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

void testSlashArcHasExpectedStructure() {
  const uint32_t segments = 20;
  Mesh arc = createSlashArc(0.55f, 1.0f, 2.4f, segments);
  assert(arc.vertices.size() == (segments + 1) * 2);
  assert(arc.indices.size() == segments * 6);
  assert(indicesInBounds(arc));
  assert(normalsUnitLength(arc));
  // 弧线位于 XZ 平面，法线朝上。
  for (const Vertex& vertex : arc.vertices) {
    assert(close(vertex.position.y, 0.0f));
    assert(close(vertex.normal.y, 1.0f));
  }
}

void testSlashArcTapersTipsAndCentersOnForward() {
  const uint32_t segments = 20;
  Mesh arc = createSlashArc(0.55f, 1.0f, 2.4f, segments);
  // 首尾顶点内外径收拢到同一半径（新月尖端）。
  const auto radiusAt = [&arc](std::size_t index) {
    const glm::vec3& p = arc.vertices[index].position;
    return std::sqrt(p.x * p.x + p.z * p.z);
  };
  assert(close(radiusAt(0), radiusAt(1), 0.0001f));  // 首端收拢
  const std::size_t tail = arc.vertices.size() - 2;
  assert(close(radiusAt(tail), radiusAt(tail + 1), 0.0001f));  // 尾端收拢
  // 中段外径接近最大值，明显大于内径。
  const std::size_t mid = arc.vertices.size() / 2;
  assert(radiusAt(mid) > 0.9f);
  assert(radiusAt(mid) - radiusAt(mid - 1) > 0.2f);
  // 弧段中点指向模型局部 +Z 前方。
  const glm::vec3 midDirection = glm::normalize(arc.vertices[mid].position);
  assert(midDirection.z > 0.99f);
  assert(std::fabs(midDirection.x) < 0.05f);
}

void testSlashArcRejectsDegenerateInputs() {
  assert(createSlashArc(0.0f, 1.0f, 2.4f, 20).vertices.empty());
  assert(createSlashArc(1.0f, 0.5f, 2.4f, 20).vertices.empty());
  assert(createSlashArc(0.55f, 1.0f, 0.0f, 20).vertices.empty());
  assert(createSlashArc(0.55f, 1.0f, 2.4f, 1).vertices.empty());
}

void testSwordHasValidStructureAndEnvelope() {
  Mesh sword = createSword();
  assert(!sword.vertices.empty());
  assert(indicesInBounds(sword));
  assert(normalsUnitLength(sword));
  float minY = 1e9f, maxY = -1e9f;
  for (const Vertex& vertex : sword.vertices) {
    minY = std::min(minY, vertex.position.y);
    maxY = std::max(maxY, vertex.position.y);
  }
  // 柄头略低于原点（手握住柄中段），剑尖朝 +Y 约 1.05。
  assert(minY < 0.0f && minY > -0.1f);
  assert(close(maxY, 1.05f, 0.01f));
}

void testSwordWindingMatchesStoredNormals() {
  Mesh sword = createSword();
  // 每个三角形的卷绕法线（e1×e2）应与存储的外翻法线同向，保证
  // GL_BACK 剔除下剑柄盒体与剑刃棱柱各面均可见。
  assert(indicesInBounds(sword));
  for (std::size_t t = 0; t + 2 < sword.indices.size(); t += 3) {
    const Vertex& v0 = sword.vertices[sword.indices[t]];
    const Vertex& v1 = sword.vertices[sword.indices[t + 1]];
    const Vertex& v2 = sword.vertices[sword.indices[t + 2]];
    const glm::vec3 winding = glm::cross(v1.position - v0.position,
                                         v2.position - v0.position);
    assert(glm::dot(winding, v0.normal) > 0.0f);
  }
}

void testSwordBladeSideFacesPointAwayFromAxis() {
  Mesh sword = createSword();
  // 仅对剑刃段（y ∈ [0.21, 1.05]）的侧面做径向检查：卷绕法线应
  // 背离剑身中轴，避免棱柱面内翻导致剔除后剑刃消失。
  for (std::size_t t = 0; t + 2 < sword.indices.size(); t += 3) {
    const glm::vec3& p0 = sword.vertices[sword.indices[t]].position;
    const glm::vec3& p1 = sword.vertices[sword.indices[t + 1]].position;
    const glm::vec3& p2 = sword.vertices[sword.indices[t + 2]].position;
    const glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;
    if (centroid.y < 0.21f) continue;  // 跳过柄/护手盒体
    const glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    const glm::vec3 outward{centroid.x, 0.0f, centroid.z};
    if (glm::length(outward) > 0.001f) {
      assert(glm::dot(normal, glm::normalize(outward)) > 0.0f);
    }
  }
}

}  // namespace

int main() {
  testCubeHasCorrectVertexCount();
  testPlaneHasCorrectVertexCount();
  testCubeNormalsFaceOutward();
  testPlaneNormalFacesUp();
  testCubeWindingMatchesFaceNormals();
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
  testSlashArcHasExpectedStructure();
  testSlashArcTapersTipsAndCentersOnForward();
  testSlashArcRejectsDegenerateInputs();
  testSwordHasValidStructureAndEnvelope();
  testSwordWindingMatchesStoredNormals();
  testSwordBladeSideFacesPointAwayFromAxis();
  return 0;
}
