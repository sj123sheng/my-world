#pragma once

#include "native/engine/render/environment_quality.h"
#include "native/engine/render/mesh.h"
#include "native/engine/world/foliage_system.h"

#include <array>
#include <vector>

class Shader3D;

struct FoliageRenderBatch {
  FoliageKind kind = FoliageKind::Grass;
  int lod = 0;
  bool castsShadow = false;
  std::vector<glm::mat4> transforms;
};

// 按种类与近/中 LOD 合批，最多 5×2=10 批；密度退化使用稳定索引散列，
// 性能档变化时不会每帧重新随机导致整片闪烁。
std::vector<FoliageRenderBatch> BuildFoliageRenderBatches(
    const std::vector<FoliageInstance>& instances,
    const glm::vec2& cameraPosition,
    const EnvironmentQualityProfile& quality);
std::vector<FoliageRenderBatch> BuildFoliageRenderBatches(
    const std::vector<FoliageInstance>& instances,
    const glm::vec2& cameraPosition,
    const EnvironmentQualityProfile& quality,
    const glm::vec2& billboardFacingPosition);
uint32_t EstimateFoliageTriangles(
    const std::vector<FoliageRenderBatch>& batches);

class FoliageRenderer {
 public:
  void initialize();
  void draw(Shader3D& shader, const glm::mat4& viewProjection,
            const std::vector<FoliageRenderBatch>& batches,
            bool atlasReady, bool shadowOnly = false);
  void destroy();
  void abandonGpuResources();

 private:
  std::array<Mesh, 10> meshes_;
  std::array<unsigned int, 10> instanceBuffers_{};
};
