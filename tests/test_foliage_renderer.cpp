#include "native/engine/render/foliage_renderer.h"

#include <cassert>
#include <vector>

int main() {
  std::vector<FoliageInstance> instances;
  for (int kind = 0; kind < 5; ++kind) {
    for (int index = 0; index < 20; ++index) {
      instances.push_back({{0.5f + index * 0.006f, 0.02f, 0.12f},
                           index * 0.21f, 0.8f + index * 0.01f,
                           static_cast<FoliageKind>(kind), kind >= 2});
    }
  }
  EnvironmentQualityProfile full = EnvironmentQualityProfileFor(0);
  const auto fullBatches = BuildFoliageRenderBatches(instances, {0.5f, 0.1f}, full);
  assert(!fullBatches.empty());
  assert(fullBatches.size() <= 12u);
  assert(EstimateFoliageTriangles(fullBatches) > 0u);
  for (const FoliageRenderBatch& batch : fullBatches) {
    const uint32_t perInstance = batch.kind == FoliageKind::Rock
                                     ? 12u
                                     : (batch.lod == 0 ? 4u : 2u);
    assert(EstimateFoliageTriangles({batch}) ==
           perInstance * batch.transforms.size());
  }
  std::size_t fullCount = 0;
  for (const FoliageRenderBatch& batch : fullBatches) {
    assert(batch.lod == 0 || batch.lod == 1);
    assert(!batch.transforms.empty());
    assert(batch.castsShadow == (static_cast<int>(batch.kind) >= 2));
    fullCount += batch.transforms.size();
  }

  const EnvironmentQualityProfile critical = EnvironmentQualityProfileFor(4);
  const auto reduced = BuildFoliageRenderBatches(instances, {0.5f, 0.1f}, critical);
  std::size_t reducedCount = 0;
  for (const FoliageRenderBatch& batch : reduced) reducedCount += batch.transforms.size();
  assert(reducedCount > 0u);
  assert(reducedCount < fullCount);

  const auto repeated = BuildFoliageRenderBatches(instances, {0.5f, 0.1f}, critical);
  assert(repeated.size() == reduced.size());
  for (std::size_t index = 0; index < reduced.size(); ++index) {
    assert(repeated[index].kind == reduced[index].kind);
    assert(repeated[index].lod == reduced[index].lod);
    assert(repeated[index].transforms == reduced[index].transforms);
  }
  return 0;
}
