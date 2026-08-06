#include "native/engine/world/world_grid.h"

#include <cassert>
#include <cmath>

int main() {
  // 初始状态：无激活分块；首次更新触发加载。
  WorldGrid grid{WorldGridConfig{8, 8, 2}};
  assert(grid.activeChunks().empty());
  assert(grid.chunkCount() == 64);
  assert(grid.chunkIndexAt({0.0f, 0.0f}) == 0);
  assert(grid.chunkIndexAt({0.999f, 0.999f}) == 63);
  assert(grid.chunkIndexAt({0.125f, 0.0f}) == 1);
  // 越界/非法输入钳制到有效分块：(-1, 2) → (0, 1) → 末行首列。
  assert(grid.chunkIndexAt({-1.0f, 2.0f}) == 7 * 8);
  assert(grid.chunkIndexAt({std::nanf(""), std::nanf("")}) == 0);

  // 中心出生：加载 5x5 窗口 = 25 个分块（半径 2 不触及边界）。
  const bool first = grid.updateStreaming({0.5f, 0.5f});
  assert(first);
  assert(grid.activeChunks().size() == 25);
  assert(grid.pendingLoads().size() == 25);
  assert(grid.pendingUnloads().empty());
  // 激活与加载序列均升序。
  for (size_t i = 1; i < grid.activeChunks().size(); ++i) {
    assert(grid.activeChunks()[i] > grid.activeChunks()[i - 1]);
  }

  // 原地重复更新：无变化。
  assert(!grid.updateStreaming({0.51f, 0.51f}));
  assert(grid.pendingLoads().empty());
  assert(grid.pendingUnloads().empty());

  // 小幅移动不跨分块边界：仍无变化（0.5 在分块 4,4；0.55 也在 4,4）。
  assert(!grid.updateStreaming({0.55f, 0.55f}));

  // 大幅移动到角落：窗口收缩到边界，集合变化。
  const bool moved = grid.updateStreaming({0.01f, 0.01f});
  assert(moved);
  // 角落窗口 3x3 = 9 个分块（半径 2 被世界边界钳制）。
  assert(grid.activeChunks().size() == 9);
  assert(!grid.pendingLoads().empty());
  assert(!grid.pendingUnloads().empty());
  // 差集不相交：新加载的分块不应出现在卸载列表。
  for (int32_t load : grid.pendingLoads()) {
    for (int32_t unload : grid.pendingUnloads()) {
      assert(load != unload);
    }
  }
  // 角落激活集合应为 3x3 窗口 {0,1,2,8,9,10,16,17,18}。
  const std::vector<int32_t> expectedCorner{0, 1, 2, 8, 9, 10, 16, 17, 18};
  assert(grid.activeChunks() == expectedCorner);

  // 确定性：同一轨迹在两个全新实例上产生相同序列。
  WorldGrid replay{WorldGridConfig{8, 8, 2}};
  replay.updateStreaming({0.5f, 0.5f});
  replay.updateStreaming({0.01f, 0.01f});
  assert(replay.activeChunks() == grid.activeChunks());
  assert(replay.pendingLoads() == grid.pendingLoads());
  assert(replay.pendingUnloads() == grid.pendingUnloads());

  // 退化配置：半径 0 只保留玩家所在分块。
  WorldGrid single{WorldGridConfig{4, 4, 0}};
  single.updateStreaming({0.3f, 0.3f});
  assert(single.activeChunks().size() == 1);
  assert(single.activeChunks()[0] == single.chunkIndexAt({0.3f, 0.3f}));

  // 非法配置被规范化。
  WorldGrid degenerate{WorldGridConfig{0, -3, -5}};
  assert(degenerate.chunkCount() >= 1);
  degenerate.updateStreaming({0.5f, 0.5f});
  assert(degenerate.activeChunks().size() == 1);

  // 动态半径：缩小后激活窗口收缩并触发卸载。
  WorldGrid dynamic{WorldGridConfig{8, 8, 2}};
  dynamic.updateStreaming({0.5f, 0.5f});
  assert(dynamic.activeChunks().size() == 25);
  assert(dynamic.setStreamingRadius(1));
  assert(dynamic.updateStreaming({0.5f, 0.5f}));
  assert(dynamic.activeChunks().size() == 9);
  assert(!dynamic.pendingUnloads().empty());
  assert(dynamic.pendingLoads().empty());
  // 相同半径重复设置不视为变化。
  assert(!dynamic.setStreamingRadius(1));
  // 负半径钳制为 0。
  assert(dynamic.setStreamingRadius(-4));
  dynamic.updateStreaming({0.5f, 0.5f});
  assert(dynamic.activeChunks().size() == 1);
  return 0;
}
