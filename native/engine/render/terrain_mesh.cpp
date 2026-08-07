// terrain_mesh.cpp: 程序化地形网格生成。
//
// 在 [0,1]x[0,1] 世界平面上铺开 (segments+1)^2 顶点网格，顶点高度
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

// 高度场梯度（中心差分，边界钳制）：返回 (dh/dx, dh/dy)。
// 步长取网格间距，保证法线与网格形状自洽。
glm::vec2 heightGradient(const TerrainHeightfield& terrain, float x, float y,
                         float step) {
  const float x0 = std::max(x - step, 0.0f);
  const float x1 = std::min(x + step, 1.0f);
  const float y0 = std::max(y - step, 0.0f);
  const float y1 = std::min(y + step, 1.0f);
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float gx = dx > 0.0f ? (terrain.heightAt(x1, y) - terrain.heightAt(x0, y)) / dx
                             : 0.0f;
  const float gy = dy > 0.0f ? (terrain.heightAt(x, y1) - terrain.heightAt(x, y0)) / dy
                             : 0.0f;
  return {gx, gy};
}

}  // namespace

Mesh createTerrainChunkMesh(const TerrainHeightfield& terrain,
                            TerrainChunkRect rect, uint32_t segments) {
  Mesh mesh;
  if (segments < 1u) return mesh;

  // 钳制到世界范围并拒绝退化矩形。
  rect.x0 = std::clamp(rect.x0, 0.0f, 1.0f);
  rect.x1 = std::clamp(rect.x1, 0.0f, 1.0f);
  rect.y0 = std::clamp(rect.y0, 0.0f, 1.0f);
  rect.y1 = std::clamp(rect.y1, 0.0f, 1.0f);
  if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) return mesh;

  const uint32_t rows = segments + 1u;
  const float stepX = (rect.x1 - rect.x0) / static_cast<float>(segments);
  const float stepY = (rect.y1 - rect.y0) / static_cast<float>(segments);
  // 法线差分步长取两轴间距较小者，保证与网格形状自洽。
  const float gradientStep = std::min(stepX, stepY);

  mesh.vertices.reserve(static_cast<size_t>(rows) * rows);
  mesh.indices.reserve(static_cast<size_t>(segments) * segments * 6u);

  // 顶点：逻辑坐标 (x, y) -> 3D (x, height, z=y)，UV 存世界坐标。
  for (uint32_t j = 0; j < rows; ++j) {
    const float v = rect.y0 + static_cast<float>(j) * stepY;
    for (uint32_t i = 0; i < rows; ++i) {
      const float u = rect.x0 + static_cast<float>(i) * stepX;
      const float height = terrain.heightAt(u, v);
      const glm::vec2 gradient = heightGradient(terrain, u, v, gradientStep);
      // 高度场曲面法线：(-dh/dx, 1, -dh/dy) 归一化。
      const glm::vec3 normal =
          glm::normalize(glm::vec3(-gradient.x, 1.0f, -gradient.y));
      mesh.vertices.push_back({{u, height, v}, normal, {u, v}});
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
  // 整世界网格即覆盖 [0,1]x[0,1] 的单个分块，复用分块实现避免重复。
  return createTerrainChunkMesh(terrain, TerrainChunkRect{}, segments);
}
