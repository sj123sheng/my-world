#include "native/platform/harmony/model_asset_commit.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

void testCommitsAllAssetsOnlyAfterAllCopiesSucceed() {
  std::vector<size_t> copied;
  std::vector<ModelAssetSlot> committed;
  size_t lifecycleCalls = 0;

  const bool result = CopyAndCommitModelAssets(
      [&copied](ModelAssetSlot slot, std::vector<uint8_t>& out) {
        copied.push_back(static_cast<size_t>(slot));
        out.assign(1, static_cast<uint8_t>(copied.size()));
        return true;
      },
      [&lifecycleCalls, &copied](auto operation) {
        assert(copied.size() == 3);
        ++lifecycleCalls;
        operation();
      },
      [&committed](ModelAssetSlot slot, std::vector<uint8_t> bytes) {
        assert(!bytes.empty());
        committed.push_back(slot);
      });

  assert(result);
  assert(lifecycleCalls == 1);
  assert((copied == std::vector<size_t>{0, 1, 2}));
  assert((committed == std::vector<ModelAssetSlot>{
      ModelAssetSlot::Player, ModelAssetSlot::Enemy, ModelAssetSlot::Boss}));
}

void testCopyFailureDoesNotEnterLifecycleOrCommit() {
  for (size_t failedCopy = 0; failedCopy < 3; ++failedCopy) {
    size_t copyCalls = 0;
    size_t lifecycleCalls = 0;
    size_t commitCalls = 0;
    const bool result = CopyAndCommitModelAssets(
        [&copyCalls, failedCopy](ModelAssetSlot, std::vector<uint8_t>& out) {
          const bool succeeds = copyCalls++ != failedCopy;
          if (succeeds) out.assign(1, 0xA5);
          return succeeds;
        },
        [&lifecycleCalls](auto operation) {
          ++lifecycleCalls;
          operation();
        },
        [&commitCalls](ModelAssetSlot, std::vector<uint8_t>) { ++commitCalls; });

    assert(!result);
    assert(copyCalls == failedCopy + 1);
    assert(lifecycleCalls == 0);
    assert(commitCalls == 0);
  }
}

}  // namespace

int main() {
  testCommitsAllAssetsOnlyAfterAllCopiesSucceed();
  testCopyFailureDoesNotEnterLifecycleOrCommit();
  return 0;
}
