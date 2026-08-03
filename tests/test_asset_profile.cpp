#include "native/engine/render/asset_profile.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float left, float right) {
  return std::fabs(left - right) < 0.0001f;
}

void testProfilesProduceUsableActorTransforms() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);

  assert(nearlyEqual(player.scale, 0.025f / 3.0f));
  assert(nearlyEqual(enemy.scale, 0.022f / 3.0f));
  assert(nearlyEqual(boss.scale, 0.045f / 3.0f));
  assert(boss.scale > player.scale);
  assert(!nearlyEqual(boss.yawOffsetRadians, 0.0f));
}

void testProfilesKeepVisualRolesDistinct() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);

  assert(player.outlineStrength > enemy.outlineStrength);
  assert(boss.coreMountCount == 3);
  assert(player.materialTint != enemy.materialTint);
}

}  // namespace

int main() {
  testProfilesProduceUsableActorTransforms();
  testProfilesKeepVisualRolesDistinct();
  return 0;
}
