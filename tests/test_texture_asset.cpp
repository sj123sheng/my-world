#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

#include "native/engine/render/texture_asset.h"

namespace {
std::vector<uint8_t> ReadBytes(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}
}  // namespace

int main() {
  TextureAsset atlas;
  const auto atlasBytes = ReadBytes(
      "entry/src/main/resources/rawfile/environment/terrain_material_atlas.png");
  assert(atlas.tryInitialize(atlasBytes, TextureWrap::Repeat));
  assert(atlas.ready());
  assert(atlas.width() == 1024);
  assert(atlas.height() == 1024);
  assert(atlas.wrap() == TextureWrap::Repeat);
  assert(atlas.cpuByteCount() == 1024u * 1024u * 4u);

  TextureAsset control;
  const auto controlBytes = ReadBytes(
      "entry/src/main/resources/rawfile/environment/terrain_control_spawn.png");
  assert(control.tryInitialize(controlBytes, TextureWrap::Clamp));
  assert(control.width() == 256);
  assert(control.height() == 256);
  assert(control.wrap() == TextureWrap::Clamp);

  TextureAsset foliage;
  const auto foliageBytes = ReadBytes(
      "entry/src/main/resources/rawfile/environment/foliage_atlas.png");
  assert(foliage.tryInitialize(foliageBytes, TextureWrap::Clamp));
  assert(foliage.width() == 1024);
  assert(foliage.height() == 1024);

  TextureAsset invalid;
  assert(!invalid.tryInitialize({0x13, 0x37, 0x00}, TextureWrap::Repeat));
  assert(!invalid.ready());

  atlas.abandonGpuResource();
  assert(atlas.ready());
  assert(atlas.width() == 1024);
  atlas.clear();
  assert(!atlas.ready());
  assert(atlas.cpuByteCount() == 0);
  return 0;
}
