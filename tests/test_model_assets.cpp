#include "native/engine/render/skinned_model.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> readAsset(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  assert(input && "model asset must exist");
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void auditAsset(const std::string& kind) {
  const std::string path =
      "entry/src/main/resources/rawfile/models/" + kind + ".glb";
  const std::vector<uint8_t> bytes = readAsset(path);
  assert(!bytes.empty());

  SkinnedModel model;
  assert(model.tryInitialize(bytes, kind + ".glb"));
  assert(model.ready());
  assert(model.vertexCount() > 0);
  assert(model.indexCount() > 0);
  assert(model.jointCount() > 0);
  assert(model.jointCount() <= kMaxSkinJoints);
  assert(model.hasTexture());

  const std::vector<std::string>& clips = model.clipNames();
  for (const char* required : {"idle", "run", "attack", "hit", "death"}) {
    assert(std::find(clips.begin(), clips.end(), required) != clips.end());
  }
  std::cout << kind << ": joints=" << model.jointCount()
            << ", primitives=" << model.primitiveCount()
            << ", clips=" << clips.size()
            << ", embeddedTextures=" << model.embeddedTextureCount() << '\n';
}

}  // namespace

int main() {
  auditAsset("player");
  auditAsset("enemy");
  auditAsset("boss");
  return 0;
}
