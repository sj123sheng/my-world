// test_terrain_mesh.cpp: 程序化地形网格生成器回归测试。
// 覆盖：网格规模、顶点高度与高度场一致、索引合法、法线朝上、
// 卷绕外翻、确定性。

#include "native/engine/render/terrain_mesh.h"

#include "native/engine/world/terrain_heightfield.h"

#include <cassert>
#include <cmath>
#include <glm/geometric.hpp>

namespace {

bool indicesInBounds(const Mesh& mesh) {
  for (const uint32_t index : mesh.indices) {
    if (index >= mesh.vertices.size()) return false;
  }
  return true;
}

}  // namespace

int main() {
  TerrainHeightfield terrain;

  // segments < 1 返回空网格。
  assert(createTerrainMesh(terrain, 0u).vertices.empty());

  const uint32_t segments = 8u;
  const Mesh mesh = createTerrainMesh(terrain, segments);
  const uint32_t rows = segments + 1u;

  // 网格规模：(segments+1)^2 顶点、segments^2 * 2 三角形。
  assert(mesh.vertices.size() == static_cast<size_t>(rows) * rows);
  assert(mesh.indices.size() == static_cast<size_t>(segments) * segments * 6u);
  assert(indicesInBounds(mesh));

  // 顶点高度与高度场严格一致；UV 存世界坐标。
  for (uint32_t j = 0; j < rows; ++j) {
    for (uint32_t i = 0; i < rows; ++i) {
      const Vertex& vertex = mesh.vertices[j * rows + i];
      const float x = static_cast<float>(i) / static_cast<float>(segments);
      const float y = static_cast<float>(j) / static_cast<float>(segments);
      assert(vertex.position.x == x);
      assert(vertex.position.z == y);
      assert(vertex.position.y == terrain.heightAt(x, y));
      assert(vertex.uv.x == x && vertex.uv.y == y);
      // 高度场曲面法线恒带向上分量。
      assert(vertex.normal.y > 0.0f);
      const float length = glm::length(vertex.normal);
      assert(std::abs(length - 1.0f) < 1e-4f);
    }
  }

  // 卷绕外翻：抽样三角形的几何法线带向上分量。
  bool hasUpwardFacingTriangle = false;
  for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const Vertex& a = mesh.vertices[mesh.indices[t]];
    const Vertex& b = mesh.vertices[mesh.indices[t + 1]];
    const Vertex& c = mesh.vertices[mesh.indices[t + 2]];
    const glm::vec3 n = glm::cross(b.position - a.position,
                                   c.position - a.position);
    if (n.y > 0.0f) hasUpwardFacingTriangle = true;
  }
  assert(hasUpwardFacingTriangle);

  // 确定性：同高度场两次生成完全一致。
  const Mesh again = createTerrainMesh(terrain, segments);
  assert(again.vertices.size() == mesh.vertices.size());
  assert(again.indices == mesh.indices);
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    assert(again.vertices[i].position == mesh.vertices[i].position);
    assert(again.vertices[i].normal == mesh.vertices[i].normal);
  }

  // 边缘山脊体现于网格：角落顶点显著高于出生点顶点。
  const float cornerHeight = mesh.vertices.front().position.y;
  const float spawnHeight =
      mesh.vertices[(segments / 2u) * rows + segments / 2u].position.y;
  assert(cornerHeight > spawnHeight + 0.02f);

  return 0;
}
