#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

enum class EnvironmentAssetSlot : size_t {
  OuterRing = 0,
  CenterRift = 1,
  Backdrop = 2,
  Decoration = 3,
};

using EnvironmentAssetBatch = std::array<std::vector<uint8_t>, 4>;

template <typename Copy, typename WithLifecycle, typename Commit>
bool CopyAndCommitEnvironmentAssets(Copy&& copy,
                                    WithLifecycle&& withLifecycle,
                                    Commit&& commit) {
  EnvironmentAssetBatch assets;
  for (size_t index = 0; index < assets.size(); ++index) {
    if (!copy(static_cast<EnvironmentAssetSlot>(index), assets[index])) {
      return false;
    }
  }
  withLifecycle([&assets, &commit]() {
    for (size_t index = 0; index < assets.size(); ++index) {
      commit(static_cast<EnvironmentAssetSlot>(index),
             std::move(assets[index]));
    }
  });
  return true;
}
