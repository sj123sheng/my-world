#pragma once

// 环境质量档：由 PerformanceGuard 的 0..4 性能级统一派生，避免阴影、
// 地表、植被、云层和水面各自做不一致的降级判断。
struct EnvironmentQualityProfile {
  bool dynamicShadows = true;
  int shadowMapSize = 1024;
  float shadowDistance = 0.34f;
  float foliageDensityScale = 1.0f;
  float foliageViewDistance = 0.42f;
  bool terrainDetailNormals = true;
  bool proceduralClouds = true;
  bool shoreFoam = true;
  int waterWaveOctaves = 2;
};

EnvironmentQualityProfile EnvironmentQualityProfileFor(int perfLevel);
int EffectiveEnvironmentQualityLevel(int automaticLevel, bool lowPreset);
