#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace gltf_fixture {

struct BufferSlice {
  std::size_t offset = 0;
  std::size_t size = 0;
};

class BinaryBuilder {
 public:
  template <typename T>
  BufferSlice append(const std::vector<T>& values) {
    align4();
    const BufferSlice slice{bytes_.size(), values.size() * sizeof(T)};
    const auto* first = reinterpret_cast<const uint8_t*>(values.data());
    bytes_.insert(bytes_.end(), first, first + slice.size);
    return slice;
  }

  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  void align4() {
    while ((bytes_.size() & 3u) != 0u) bytes_.push_back(0);
  }

  std::vector<uint8_t> bytes_;
};

inline void appendU32(std::vector<uint8_t>& bytes, uint32_t value) {
  const auto* first = reinterpret_cast<const uint8_t*>(&value);
  bytes.insert(bytes.end(), first, first + sizeof(value));
}

inline std::vector<uint8_t> makeMinimalGlb() {
  BinaryBuilder bin;
  const BufferSlice positions = bin.append<float>({
      0.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
  });
  const BufferSlice normals = bin.append<float>({
      0.0f, 0.0f, 1.0f,
      0.0f, 0.0f, 1.0f,
      0.0f, 0.0f, 1.0f,
  });
  const BufferSlice texcoords = bin.append<float>({
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
  });
  const BufferSlice joints = bin.append<uint8_t>({
      0, 1, 0, 0,
      0, 1, 0, 0,
      0, 1, 0, 0,
  });
  const BufferSlice weights = bin.append<float>({
      0.75f, 0.25f, 0.0f, 0.0f,
      0.75f, 0.25f, 0.0f, 0.0f,
      0.75f, 0.25f, 0.0f, 0.0f,
  });
  const BufferSlice indices = bin.append<uint16_t>({0, 1, 2});

  std::vector<float> inverseBinds(32, 0.0f);
  for (std::size_t matrix = 0; matrix < 2; ++matrix) {
    inverseBinds[matrix * 16 + 0] = 1.0f;
    inverseBinds[matrix * 16 + 5] = 1.0f;
    inverseBinds[matrix * 16 + 10] = 1.0f;
    inverseBinds[matrix * 16 + 15] = 1.0f;
  }
  const BufferSlice inverseBindMatrices = bin.append(inverseBinds);
  const BufferSlice times = bin.append<float>({0.0f, 1.0f});
  const BufferSlice idleTranslations = bin.append<float>({
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f,
  });
  const BufferSlice runTranslations = bin.append<float>({
      0.0f, 0.0f, 0.0f,
      2.0f, 0.0f, 0.0f,
  });

  const std::array<BufferSlice, 10> views{
      positions, normals, texcoords, joints, weights, indices,
      inverseBindMatrices, times, idleTranslations, runTranslations,
  };

  std::ostringstream json;
  json << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)"
       << bin.bytes().size() << R"(}],"bufferViews":[)";
  for (std::size_t i = 0; i < views.size(); ++i) {
    if (i != 0) json << ',';
    json << R"({"buffer":0,"byteOffset":)" << views[i].offset
         << R"(,"byteLength":)" << views[i].size << '}';
  }
  json << R"(],"accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":3,"componentType":5121,"count":3,"type":"VEC4"},
    {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"},
    {"bufferView":5,"componentType":5123,"count":3,"type":"SCALAR"},
    {"bufferView":6,"componentType":5126,"count":2,"type":"MAT4"},
    {"bufferView":7,"componentType":5126,"count":2,"type":"SCALAR"},
    {"bufferView":8,"componentType":5126,"count":2,"type":"VEC3"},
    {"bufferView":9,"componentType":5126,"count":2,"type":"VEC3"}
  ],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,
    "TEXCOORD_0":2,"JOINTS_0":3,"WEIGHTS_0":4},"indices":5,"mode":4}]}],
  "skins":[{"inverseBindMatrices":6,"joints":[1,2],"skeleton":1}],
  "nodes":[{"translation":[3,0,0],"children":[1]},
    {"children":[2]},{"translation":[0,1,0]},
    {"mesh":0,"skin":0}],
  "scenes":[{"nodes":[0,3]}],"scene":0,
  "animations":[
    {"name":"idle","samplers":[{"input":7,"output":8,"interpolation":"LINEAR"}],
      "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},
    {"name":"run","samplers":[{"input":7,"output":9,"interpolation":"LINEAR"}],
      "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]}
  ]})";

  std::string jsonChunk = json.str();
  while ((jsonChunk.size() & 3u) != 0u) jsonChunk.push_back(' ');
  std::vector<uint8_t> binChunk = bin.bytes();
  while ((binChunk.size() & 3u) != 0u) binChunk.push_back(0);

  std::vector<uint8_t> glb;
  glb.reserve(12 + 8 + jsonChunk.size() + 8 + binChunk.size());
  appendU32(glb, 0x46546c67u);
  appendU32(glb, 2u);
  appendU32(glb, static_cast<uint32_t>(12 + 8 + jsonChunk.size() + 8 + binChunk.size()));
  appendU32(glb, static_cast<uint32_t>(jsonChunk.size()));
  appendU32(glb, 0x4e4f534au);
  glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
  appendU32(glb, static_cast<uint32_t>(binChunk.size()));
  appendU32(glb, 0x004e4942u);
  glb.insert(glb.end(), binChunk.begin(), binChunk.end());
  return glb;
}

}  // namespace gltf_fixture
