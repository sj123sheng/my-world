// terrain_mesh.cpp: 程序化地形网格生成。
//
// 在局部 [0,1]x[0,1] 平面铺开 (segments+1)^2 顶点网格，顶点高度
// 采样 TerrainHeightfield::heightAt，与逻辑层共享同一确定性函数，
// 保证角色贴地、坡度/水域判定与视觉地形严格一致。法线用高度场
// 中心差分解析求出，避免逐三角形求法线产生的硬边（faceting）。
// 卷绕与 createPlane 一致（CCW 外翻，法线带向上分量），可安全
// 配合 GL_BACK 背面剔除使用。

#include "native/engine/render/terrain_mesh.h"

#include "native/engine/world/terrain_heightfield.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace {

// 高度场梯度（连续世界中心差分）：返回 (dh/dx, dh/dy)。
// 步长取网格间距，保证法线与网格形状自洽。
glm::vec2 heightGradient(const TerrainHeightfield& terrain, ChunkCoord coord,
                         float x, float y, float step) {
  const float gx =
      (terrain.heightAt(coord, x + step, y) -
       terrain.heightAt(coord, x - step, y)) /
      (2.0f * step);
  const float gy =
      (terrain.heightAt(coord, x, y + step) -
       terrain.heightAt(coord, x, y - step)) /
      (2.0f * step);
  return {gx, gy};
}

}  // namespace

Mesh createTerrainChunkMesh(const TerrainHeightfield& terrain,
                            ChunkCoord coord, uint32_t segments,
                            glm::vec2 uvOffset) {
  Mesh mesh;
  if (segments < 1u) return mesh;

  const uint32_t rows = segments + 1u;
  const float step = 1.0f / static_cast<float>(segments);

  mesh.vertices.reserve(static_cast<size_t>(rows) * rows);
  mesh.indices.reserve(static_cast<size_t>(segments) * segments * 6u);

  // 顶点保持局部坐标，渲染时由调用方相对玩家所在分块放置。
  for (uint32_t j = 0; j < rows; ++j) {
    const float v = static_cast<float>(j) * step;
    for (uint32_t i = 0; i < rows; ++i) {
      const float u = static_cast<float>(i) * step;
      const float height = terrain.heightAt(coord, u, v);
      const glm::vec2 gradient = heightGradient(terrain, coord, u, v, step);
      // 高度场曲面法线：(-dh/dx, 1, -dh/dy) 归一化。
      const glm::vec3 normal =
          glm::normalize(glm::vec3(-gradient.x, 1.0f, -gradient.y));
      mesh.vertices.push_back(
          {{u, height, v}, normal, {u + uvOffset.x, v + uvOffset.y}});
    }
  }

  // 索引：每个格子两个三角形，卷绕保证法线朝上（与 createPlane 一致）。
  for (uint32_t j = 0; j < segments; ++j) {
    for (uint32_t i = 0; i < segments; ++i) {
      const uint32_t a = j * rows + i;
      const uint32_t b = a + 1u;
      const uint32_t c = a + rows;
      const uint32_t d = c + 1u;
      mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
    }
  }

  return mesh;
}

Mesh createTerrainMesh(const TerrainHeightfield& terrain, uint32_t segments) {
  return createTerrainChunkMesh(terrain, {0, 0}, segments);
}
