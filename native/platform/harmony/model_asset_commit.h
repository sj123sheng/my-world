#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// 不依赖 N-API 或 Surface，保持桥接的全有或全无边界：三份输入均复制成功后，
// 才进入生命周期锁并替换任一模型资产。
enum class ModelAssetSlot : size_t { Player = 0, Enemy = 1, Boss = 2 };

using ModelAssetBatch = std::array<std::vector<uint8_t>, 3>;

template <typename Copy, typename WithLifecycle, typename Commit>
bool CopyAndCommitModelAssets(Copy&& copy, WithLifecycle&& withLifecycle,
                              Commit&& commit) {
  ModelAssetBatch assets;
  for (size_t index = 0; index < assets.size(); ++index) {
    if (!copy(static_cast<ModelAssetSlot>(index), assets[index])) return false;
  }

  withLifecycle([&assets, &commit]() {
    commit(ModelAssetSlot::Player, std::move(assets[0]));
    commit(ModelAssetSlot::Enemy, std::move(assets[1]));
    commit(ModelAssetSlot::Boss, std::move(assets[2]));
  });
  return true;
}
