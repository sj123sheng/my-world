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
}
