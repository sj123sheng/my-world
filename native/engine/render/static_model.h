#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "native/engine/render/mesh.h"

class Shader3D;

enum class StaticTextureTier { Full, Half };

struct StaticModelStats {
  std::size_t primitiveCount = 0;
  std::size_t triangleCount = 0;
  std::size_t textureBytes = 0;
};

class StaticModel {
 public:
  bool tryInitialize(const std::vector<uint8_t> &bytes,
                     const std::string &assetName);
  bool ready() const;
  const std::string &lastError() const;
  const StaticModelStats &stats() const;
  StaticTextureTier textureTier() const;
  void setTextureTier(StaticTextureTier tier);
  void draw(Shader3D &shader);
  void destroy();
  void abandonGpuResources();
  const std::vector<Vertex> &cpuVerticesForTest() const;

 private:
  void clearOwnedState();
  const std::vector<uint8_t> &selectedTextureBytes() const;

  bool ready_ = false;
  std::string lastError_;
  StaticModelStats stats_;
  StaticTextureTier textureTier_ = StaticTextureTier::Full;
  std::vector<Mesh> primitives_;
  std::vector<bool> primitiveUsesTexture_;
  std::vector<Vertex> testVertices_;
  std::vector<uint8_t> fullTextureBytes_;
  std::vector<uint8_t> halfTextureBytes_;
  unsigned int texture_ = 0;
  bool textureDirty_ = false;
};
