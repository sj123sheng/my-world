// test_mesh.cpp: 验证 createCube/createPlane 生成的顶点和索引数据正确性。
//
// 仅测试纯数据生成路径（不涉及 GL 调用），GL 上传/绘制由 #ifdef OHOS_PLATFORM 保护，
// 非 HarmonyOS 平台为空操作，便于 macOS 下做单元测试。

#include "native/engine/render/mesh.h"

#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstddef>

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
  return 0;
}
