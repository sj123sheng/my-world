#pragma once

#include "native/engine/render/mesh.h"
#include "native/engine/world/world_position.h"

#include <cstdint>

class TerrainHeightfield;

// 生成核心分块 [0,1]x[0,1] 的地形网格：(segments+1)^2 个顶点、
// segments^2 x 2 个三角形。顶点高度采样 TerrainHeightfield::heightAt，
// 保证渲染网格与逻辑层地面判定（贴合/坡度/水域）严格一致。
// 法线用高度场中心差分解析求出；segments < 1 时返回空网格。
// 逻辑坐标 (x, y) 对应 3D 世界 (x, height, z=y)。
Mesh createTerrainMesh(const TerrainHeightfield& terrain, uint32_t segments);

// 生成 coord 分块的局部 [0,1]x[0,1] 网格。position.x/z 永远为局部坐标，
// 高度/法线采样连续世界高度场；UV 为局部坐标加稳定哈希偏移。
Mesh createTerrainChunkMesh(const TerrainHeightfield& terrain,
                            ChunkCoord coord, uint32_t segments,
                            glm::vec2 uvOffset = {});
