#include "native/engine/render/foliage_renderer.h"

#include "native/engine/render/shader_3d.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

int BatchIndex(FoliageKind kind, int lod) {
  return static_cast<int>(kind) * 2 + std::clamp(lod, 0, 1);
}

glm::vec3 InstanceScale(FoliageKind kind, float scale) {
  switch (kind) {
    case FoliageKind::Grass: return {0.009f * scale, 0.030f * scale, 0.009f * scale};
    case FoliageKind::Shrub: return {0.036f * scale, 0.031f * scale, 0.036f * scale};
    case FoliageKind::Tree: return {0.064f * scale, 0.090f * scale, 0.064f * scale};
    case FoliageKind::Flower: return {0.007f * scale, 0.024f * scale, 0.007f * scale};
    case FoliageKind::Rock: return {0.018f * scale, 0.013f * scale, 0.021f * scale};
  }
  return glm::vec3(0.01f * scale);
}

Mesh CrossBlade() {
  Mesh mesh;
  const glm::vec3 front{0.0f, 0.0f, 1.0f};
  const glm::vec3 side{1.0f, 0.0f, 0.0f};
  mesh.vertices = {
      {{-0.5f, 0.0f, 0.0f}, front, {0.0f, 0.0f}},
      {{0.5f, 0.0f, 0.0f}, front, {1.0f, 0.0f}},
      {{0.5f, 1.0f, 0.0f}, front, {1.0f, 1.0f}},
      {{-0.5f, 1.0f, 0.0f}, front, {0.0f, 1.0f}},
      {{0.0f, 0.0f, -0.5f}, side, {0.0f, 0.0f}},
      {{0.0f, 0.0f, 0.5f}, side, {1.0f, 0.0f}},
      {{0.0f, 1.0f, 0.5f}, side, {1.0f, 1.0f}},
      {{0.0f, 1.0f, -0.5f}, side, {0.0f, 1.0f}},
  };
  mesh.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  return mesh;
}

Mesh SingleBlade() {
  Mesh mesh;
  const glm::vec3 front{0.0f, 0.0f, 1.0f};
  mesh.vertices = {
      {{-0.5f, 0.0f, 0.0f}, front, {0.0f, 0.0f}},
      {{0.5f, 0.0f, 0.0f}, front, {1.0f, 0.0f}},
      {{0.5f, 1.0f, 0.0f}, front, {1.0f, 1.0f}},
      {{-0.5f, 1.0f, 0.0f}, front, {0.0f, 1.0f}},
  };
  mesh.indices = {0, 1, 2, 0, 2, 3};
  return mesh;
}

Mesh BaseMesh(FoliageKind kind, int lod) {
  switch (kind) {
    case FoliageKind::Grass:
    case FoliageKind::Shrub:
    case FoliageKind::Tree:
    case FoliageKind::Flower:
      return lod == 0 ? CrossBlade() : SingleBlade();
    case FoliageKind::Rock:
      return createCube(1.0f);
  }
  return {};
}

}  // namespace

std::vector<FoliageRenderBatch> BuildFoliageRenderBatches(
    const std::vector<FoliageInstance>& instances,
    const glm::vec2& cameraPosition,
    const EnvironmentQualityProfile& quality) {
  return BuildFoliageRenderBatches(instances, cameraPosition, quality,
                                   cameraPosition);
}

std::vector<FoliageRenderBatch> BuildFoliageRenderBatches(
    const std::vector<FoliageInstance>& instances,
    const glm::vec2& cameraPosition,
    const EnvironmentQualityProfile& quality,
    const glm::vec2& billboardFacingPosition) {
  std::array<FoliageRenderBatch, 10> grouped;
  for (int kind = 0; kind < 5; ++kind) {
    for (int lod = 0; lod < 2; ++lod) {
      FoliageRenderBatch& batch = grouped[BatchIndex(static_cast<FoliageKind>(kind), lod)];
      batch.kind = static_cast<FoliageKind>(kind);
      batch.lod = lod;
    }
  }
  const uint32_t densityCutoff = static_cast<uint32_t>(
      std::clamp(quality.foliageDensityScale, 0.0f, 1.0f) * 65535.0f);
  for (std::size_t index = 0; index < instances.size(); ++index) {
    const FoliageInstance& instance = instances[index];
    const uint32_t stable = static_cast<uint32_t>(index * 1103515245u +
                                                  static_cast<int>(instance.kind) * 12345u);
    if ((stable & 0xffffu) > densityCutoff) continue;
    const glm::vec2 delta{instance.position.x - cameraPosition.x,
                          instance.position.z - cameraPosition.y};
    const float distance = glm::length(delta);
    if (distance > quality.foliageViewDistance) continue;
    const int lod = distance < quality.foliageViewDistance * 0.55f ? 0 : 1;
    const float yaw = lod == 0
                          ? instance.yaw
                          : std::atan2(
                                billboardFacingPosition.x - instance.position.x,
                                billboardFacingPosition.y - instance.position.z);
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f), instance.position) *
        glm::rotate(glm::mat4(1.0f), yaw, {0.0f, 1.0f, 0.0f}) *
        glm::scale(glm::mat4(1.0f), InstanceScale(instance.kind, instance.scale));
    FoliageRenderBatch& batch = grouped[BatchIndex(instance.kind, lod)];
    batch.castsShadow = batch.castsShadow || instance.castsShadow;
    batch.transforms.push_back(transform);
  }
  std::vector<FoliageRenderBatch> result;
  result.reserve(10);
  for (FoliageRenderBatch& batch : grouped) {
    if (!batch.transforms.empty()) result.push_back(std::move(batch));
  }
  return result;
}

uint32_t EstimateFoliageTriangles(
    const std::vector<FoliageRenderBatch>& batches) {
  uint64_t total = 0;
  for (const FoliageRenderBatch& batch : batches) {
    uint32_t trianglesPerInstance = batch.lod == 0 ? 4u : 2u;
    if (batch.kind == FoliageKind::Rock) {
      trianglesPerInstance = 12u;
    }
    total += static_cast<uint64_t>(trianglesPerInstance) *
             batch.transforms.size();
  }
  return static_cast<uint32_t>(std::min<uint64_t>(
      total, std::numeric_limits<uint32_t>::max()));
}

void FoliageRenderer::initialize() {
  for (int kind = 0; kind < 5; ++kind) {
    for (int lod = 0; lod < 2; ++lod) {
      const int slot = BatchIndex(static_cast<FoliageKind>(kind), lod);
      meshes_[slot] = BaseMesh(static_cast<FoliageKind>(kind), lod);
      meshes_[slot].upload();
    }
  }
#ifdef OHOS_PLATFORM
  glGenBuffers(static_cast<GLsizei>(instanceBuffers_.size()), instanceBuffers_.data());
#endif
}

void FoliageRenderer::draw(Shader3D& shader, const glm::mat4& viewProjection,
                           const std::vector<FoliageRenderBatch>& batches,
                           bool atlasReady, bool shadowOnly) {
#ifdef OHOS_PLATFORM
  shader.setInstanced(true);
  shader.setMVP(viewProjection);
  shader.setModel(glm::mat4(1.0f));
  for (const FoliageRenderBatch& batch : batches) {
    if (shadowOnly && !batch.castsShadow) continue;
    const bool textured = atlasReady && batch.kind != FoliageKind::Rock;
    const int region = batch.kind == FoliageKind::Grass ? 0 :
                       (batch.kind == FoliageKind::Shrub ? 1 :
                       (batch.kind == FoliageKind::Tree ? 2 : 3));
    shader.setHasTexture(textured);
    shader.setFoliageMaterial(textured, region);
    const int slot = BatchIndex(batch.kind, batch.lod);
    const Mesh& mesh = meshes_[slot];
    if (mesh.vbo == 0u || mesh.ibo == 0u || batch.transforms.empty()) continue;
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    const GLsizei vertexStride = static_cast<GLsizei>(sizeof(Vertex));
    glEnableVertexAttribArray(kPositionAttribute);
    glVertexAttribPointer(kPositionAttribute, 3, GL_FLOAT, GL_FALSE, vertexStride,
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(kNormalAttribute);
    glVertexAttribPointer(kNormalAttribute, 3, GL_FLOAT, GL_FALSE, vertexStride,
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(kUvAttribute);
    glVertexAttribPointer(kUvAttribute, 2, GL_FLOAT, GL_FALSE, vertexStride,
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffers_[slot]);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(batch.transforms.size() * sizeof(glm::mat4)),
                 batch.transforms.data(), GL_STREAM_DRAW);
    for (unsigned int column = 0; column < 4; ++column) {
      const unsigned int attribute = 5u + column;
      glEnableVertexAttribArray(attribute);
      glVertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            reinterpret_cast<void*>(sizeof(glm::vec4) * column));
      glVertexAttribDivisor(attribute, 1);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()),
                            GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(batch.transforms.size()));
    for (unsigned int attribute = 5u; attribute <= 8u; ++attribute) {
      glVertexAttribDivisor(attribute, 0);
      glDisableVertexAttribArray(attribute);
    }
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  shader.setInstanced(false);
  shader.setFoliageMaterial(false, 0);
  shader.setHasTexture(false);
#else
  (void)shader;
  (void)viewProjection;
  (void)batches;
  (void)atlasReady;
  (void)shadowOnly;
#endif
}

void FoliageRenderer::destroy() {
#ifdef OHOS_PLATFORM
  for (Mesh& mesh : meshes_) mesh.destroy();
  glDeleteBuffers(static_cast<GLsizei>(instanceBuffers_.size()), instanceBuffers_.data());
#endif
  instanceBuffers_.fill(0u);
}

void FoliageRenderer::abandonGpuResources() {
  for (Mesh& mesh : meshes_) mesh.abandonGpuResources();
  instanceBuffers_.fill(0u);
}
