#include "native/platform/harmony/environment_asset_commit.h"

#include <cassert>
#include <vector>

int main() {
  std::vector<EnvironmentAssetSlot> committed;
  int lifecycleCalls = 0;
  const bool ok = CopyAndCommitEnvironmentAssets(
      [](EnvironmentAssetSlot slot, std::vector<uint8_t>& out) {
        out.assign(1, static_cast<uint8_t>(slot) + 1);
        return true;
      },
      [&lifecycleCalls](auto operation) {
        ++lifecycleCalls;
        operation();
      },
      [&committed](EnvironmentAssetSlot slot, std::vector<uint8_t> bytes) {
        assert(!bytes.empty());
        committed.push_back(slot);
      });
  assert(ok);
  assert(lifecycleCalls == 1);
  assert(committed.size() == 4);

  int failedCommits = 0;
  assert(!CopyAndCommitEnvironmentAssets(
      [](EnvironmentAssetSlot slot, std::vector<uint8_t>& out) {
        if (slot == EnvironmentAssetSlot::Backdrop) return false;
        out.assign(1, 1);
        return true;
      },
      [](auto operation) { operation(); },
      [&failedCommits](EnvironmentAssetSlot, std::vector<uint8_t>) {
        ++failedCommits;
      }));
  assert(failedCommits == 0);

  // Phase 2：区块批次单资产提交（copy→withLifecycle→commit 同模式）。
  {
    int blockLifecycleCalls = 0;
    int32_t committedBlock = -2;
    std::vector<uint8_t> committedBytes;
    const bool blockOk = CopyAndCommitBlockEnvironmentAsset(
        11,
        [](std::vector<uint8_t>& out) {
          out.assign(3, 7);
          return true;
        },
        [&blockLifecycleCalls](auto operation) {
          ++blockLifecycleCalls;
          operation();
        },
        [&committedBlock, &committedBytes](int32_t blockId,
                                           std::vector<uint8_t> bytes) {
          committedBlock = blockId;
          committedBytes = std::move(bytes);
        });
    assert(blockOk);
    assert(blockLifecycleCalls == 1);
    assert(committedBlock == 11);
    assert(committedBytes.size() == 3);
  }
  {
    // 拷贝失败：不进入 lifecycle，也不提交。
    int blockLifecycleCalls = 0;
    int blockCommits = 0;
    const bool blockOk = CopyAndCommitBlockEnvironmentAsset(
        9,
        [](std::vector<uint8_t>&) { return false; },
        [&blockLifecycleCalls](auto operation) {
          ++blockLifecycleCalls;
          operation();
        },
        [&blockCommits](int32_t, std::vector<uint8_t>) { ++blockCommits; });
    assert(!blockOk);
    assert(blockLifecycleCalls == 0);
    assert(blockCommits == 0);
  }
}
