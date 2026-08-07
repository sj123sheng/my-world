#include "native/engine/render/environment.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

void testSpawnFramesTheAltarAlongTheMainRoute() {
  const EnvironmentComposition composition =
      EnvironmentController::defaultComposition();

  assert(composition.spawn.z < composition.combatAnchor.z);
  assert(composition.combatAnchor.z < composition.altarAnchor.z);
  assert(composition.cameraFocus.z >= composition.altarAnchor.z);
}

void testOpeningShotHasForegroundAndDistantFocus() {
  const EnvironmentComposition composition =
      EnvironmentController::defaultComposition();

  assert(composition.foregroundOccluder.z > composition.spawn.z);
  assert(composition.foregroundOccluder.z < composition.combatAnchor.z);
  assert(composition.cameraFocus != composition.spawn);
}

void testSpawnStartsBeforeCombatTrigger() {
  const EnvironmentComposition composition =
      EnvironmentController::defaultComposition();
  assert(composition.spawn.z < 0.45f);
  assert(composition.combatAnchor.z >= 0.45f);
}

// ---- Phase 2：blockId 分组与流式启停 ----

void testBlockIdAtMatchesGridConvention() {
  // id = row * 8 + col，越界坐标钳制到边缘分块。
  assert(environmentBlockIdAt(0.0f, 0.0f) == 0);
  assert(environmentBlockIdAt(0.999f, 0.0f) == 7);
  assert(environmentBlockIdAt(0.0f, 0.999f) == 56);
  assert(environmentBlockIdAt(0.5f, 0.12f) == 0 * 8 + 4);
  assert(environmentBlockIdAt(-5.0f, 3.0f) == 56);
  assert(environmentBlockIdAt(5.0f, 3.0f) == 63);
  assert(environmentBlockIdAt(0.5f, 3.0f) == 56 + 4);
}

void testRenderPlanBlockActivation() {
  EnvironmentRenderPlan plan;
  // 未填充激活集：所有区块批次关闭，全局批次不受影响。
  assert(!plan.blockActive(0));
  assert(plan.outerRing && plan.centerRift && plan.backdrop);
  plan.activeBlocks = {3, 11, 12};
  assert(plan.blockActive(3) && plan.blockActive(11) && plan.blockActive(12));
  assert(!plan.blockActive(2) && !plan.blockActive(13));
}

void testBlockFitSharesOuterRingParams() {
  const EnvironmentComposition composition =
      EnvironmentController::defaultComposition();
  const glm::mat4 blockFit = environmentBlockWorldFitMatrix(composition);
  const glm::mat4 outerFit = environmentWorldFitMatrix(
      static_cast<size_t>(EnvironmentBatchKind::OuterRing), composition);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      assert(blockFit[col][row] == outerFit[col][row]);
    }
  }
}

void testLayoutMirrorBlockIds() {
  // 镜像条目的 blockId 必须落在 [-1, kEnvironmentBlockCount)；
  // 区块条目（≥0）限定为可碰撞/氛围资产类型（outerRing/decoration）。
  const EnvironmentPlacement* placements = environmentLayoutPlacements();
  const std::size_t count = environmentLayoutPlacementCount();
  assert(count > 0);
  int blockEntries = 0;
  int globalEntries = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const EnvironmentPlacement& placement = placements[i];
    assert(placement.blockId >= -1 &&
           placement.blockId < kEnvironmentBlockCount);
    if (placement.blockId >= 0) {
      ++blockEntries;
      assert(placement.region == 0 || placement.region == 3);
    } else {
      ++globalEntries;
    }
  }
  // 6 个 district 各有地标与氛围条目。
  assert(blockEntries >= 12);
  assert(globalEntries >= 21);
}

void testLayoutJsonAgreesWithMirrorBlockIds() {
  // 构建期一致性：assets/environment/layout.json 的 blockId 多重集
  // 与 C++ 镜像严格一致（测试从仓库根目录运行）。
  std::ifstream file("assets/environment/layout.json");
  assert(file.good());
  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string text = buffer.str();

  std::vector<int> jsonBlockIds;
  const std::regex blockIdPattern("\"blockId\"\\s*:\\s*(-?[0-9]+)");
  for (std::sregex_iterator it(text.begin(), text.end(), blockIdPattern),
       end;
       it != end; ++it) {
    jsonBlockIds.push_back(std::stoi((*it)[1].str()));
  }
  assert(jsonBlockIds.size() == environmentLayoutPlacementCount());

  std::vector<int> mirrorBlockIds;
  for (std::size_t i = 0; i < environmentLayoutPlacementCount(); ++i) {
    mirrorBlockIds.push_back(environmentLayoutPlacements()[i].blockId);
  }
  std::sort(jsonBlockIds.begin(), jsonBlockIds.end());
  std::sort(mirrorBlockIds.begin(), mirrorBlockIds.end());
  assert(jsonBlockIds == mirrorBlockIds);

  // 区块条目的地标 id 必须同时存在于两份数据。
  assert(text.find("\"westlands_tower\"") != std::string::npos);
  assert(text.find("\"sanctum_highlands_tower\"") != std::string::npos);
}

}  // namespace

int main() {
  testSpawnFramesTheAltarAlongTheMainRoute();
  testOpeningShotHasForegroundAndDistantFocus();
  testSpawnStartsBeforeCombatTrigger();
  testBlockIdAtMatchesGridConvention();
  testRenderPlanBlockActivation();
  testBlockFitSharesOuterRingParams();
  testLayoutMirrorBlockIds();
  testLayoutJsonAgreesWithMirrorBlockIds();
  return 0;
}
