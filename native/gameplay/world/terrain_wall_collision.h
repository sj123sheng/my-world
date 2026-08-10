#pragma once

#include "native/engine/math/vec2.h"

struct TerrainHeightfield;

// 地形墙体碰撞（原神式悬崖/台地边缘语言）：
// 建筑盒碰撞只覆盖人工建筑，地形特征层生成的悬崖（mesa 壁、回廊
// 悬崖、劣地脊线、边缘山脊环）此前没有任何水平阻挡——角色直接走进
// 陡坡，垂直状态机靠地面高度瞬移吸附，视觉上穿墙而过。本模块提供
// 地形墙体的探测、阻挡、沿墙滑动与嵌入推出，全部为纯函数，
// 确定性可测试。
//
// 判定口径与地形模块一致：坡度 >= climbSlopeThreshold 的面视为墙体。
// 探测点抬升超过“可行走坡度 × 探测距离”即判定为墙阻挡，因此
// 一切可行走坡面永远不阻挡，一切可攀爬坡面必然阻挡，口径单点收敛。
struct TerrainWallContact {
  bool blocked = false;      // 移动方向前方存在不可直接跨越的地形墙
  bool climbable = false;    // 墙面坡度达到攀爬阈值（可进入攀爬）
  float groundAhead = 0.0f;  // 探测点地面高度：攀爬登顶目标
  Vec2 normal;               // 墙面法线（最陡上升方向，即“墙内”方向）
};

// 沿移动方向前方探测地形墙。
// - (x, y) 为探测起点（本帧移动前位置），height 为角色脚底高度；
// - moveDir 为本帧移动方向（无需归一化，零向量返回无接触）；
// - probeDistance 建议取碰撞半径 + 本帧位移，保证在撞墙前拦截。
// 跳跃/滑翔中 height 已越过墙顶时不阻挡（可翻越），水面下潜等
// 特殊状态由宿主层自行跳过调用。
TerrainWallContact terrainWallContact(const TerrainHeightfield& terrain,
                                      float x, float y, float height,
                                      Vec2 moveDir, float probeDistance);

// 墙阻挡后的位移修正：取消挤入墙体的法向分量，保留切向分量，
// 实现建筑碰撞同语言的沿墙滑动。
Vec2 slideAlongTerrainWall(Vec2 moveDelta, Vec2 wallNormal);

// 嵌入推出：脚下地面显著高于脚底（已被击退/传送等方式送进墙体）
// 时沿下坡方向迭代推出，返回修正后的位置；未嵌入原样返回。
Vec2 depenetrateTerrainWall(const TerrainHeightfield& terrain, float x,
                            float y, float height, float probeDistance);
