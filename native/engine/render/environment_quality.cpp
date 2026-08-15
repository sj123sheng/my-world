#include "native/engine/render/environment_quality.h"

#include <algorithm>

int EffectiveEnvironmentQualityLevel(int automaticLevel, bool lowPreset) {
  const int automatic = std::clamp(automaticLevel, 0, 4);
  return lowPreset ? std::max(automatic, 3) : automatic;
}

EnvironmentQualityProfile EnvironmentQualityProfileFor(int perfLevel) {
  const int level = std::clamp(perfLevel, 0, 4);
  EnvironmentQualityProfile profile;
  if (level == 1) {
    profile.shadowDistance = 0.30f;
    profile.foliageDensityScale = 0.85f;
    profile.foliageViewDistance = 0.36f;
  } else if (level == 2) {
    profile.shadowMapSize = 768;
    profile.shadowDistance = 0.25f;
    profile.foliageDensityScale = 0.65f;
    profile.foliageViewDistance = 0.30f;
    profile.waterWaveOctaves = 1;
  } else if (level == 3) {
    profile.shadowMapSize = 512;
    profile.shadowDistance = 0.19f;
    profile.foliageDensityScale = 0.42f;
    profile.foliageViewDistance = 0.23f;
    profile.terrainDetailNormals = false;
    profile.proceduralClouds = false;
    profile.shoreFoam = false;
    profile.waterWaveOctaves = 1;
  } else if (level == 4) {
    profile.dynamicShadows = false;
    profile.shadowMapSize = 0;
    profile.shadowDistance = 0.0f;
    profile.foliageDensityScale = 0.24f;
    profile.foliageViewDistance = 0.17f;
    profile.terrainDetailNormals = false;
    profile.proceduralClouds = false;
    profile.shoreFoam = false;
    profile.waterWaveOctaves = 1;
  }
  return profile;
}
