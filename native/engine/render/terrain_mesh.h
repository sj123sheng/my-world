#pragma once

#include "native/engine/render/mesh.h"

#include <cstdint>

class TerrainHeightfield;

// 分块矩形的逻辑坐标（世界 [0,1]x[0,1] 内），x0 <= x1、y0 <= y1。
struct TerrainChunkRect {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 1.0f;
  float y1 = 1.0f;
};

// 生成覆盖世界 [0,1]x[0,1] 的地形网格：(segments+1)^2 个顶点、
// segments^2 x 2 个三角形。顶点高度采样 TerrainHeightfield::heightAt，
// 保证渲染网格与逻辑层地面判定（贴合/坡度/水域）严格一致。
// 法线用高度场中心差分解析求出；UV = 世界坐标，供着色器做
// 噪声打碎与平铺。segments < 1 时返回空网格。
// 逻辑坐标 (x, y) 对应 3D 世界 (x, height, z=y)。
Mesh createTerrainMesh(const TerrainHeightfield& terrain, uint32_t segments);

// 生成覆盖矩形区域 rect 的分块地形网格：(segments+1)^2 个顶点、
// segments^2 x 2 个三角形，采样/法线/UV 约定与 createTerrainMesh 完全
// 一致（UV = 世界坐标，法线为高度场中心差分）。rect 会被钳制到
// [0,1]x[0,1]；segments < 1 或退化矩形返回空网格。
Mesh createTerrainChunkMesh(const TerrainHeightfield& terrain,
                            TerrainChunkRect rect, uint32_t segments);
