#include "native/engine/render/environment.h"

#include <cassert>

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

}  // namespace

int main() {
  testSpawnFramesTheAltarAlongTheMainRoute();
  testOpeningShotHasForegroundAndDistantFocus();
  return 0;
}
