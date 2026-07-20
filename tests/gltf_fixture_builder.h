#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

inline uint32_t readU32(const std::vector<uint8_t>& bytes, std::size_t offset) {
  if (offset + sizeof(uint32_t) > bytes.size()) {
    throw std::runtime_error("GLB uint32 read is out of bounds");
  }
  uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

inline std::vector<uint8_t> rebuildGlb(std::string json,
                                       std::vector<uint8_t> bin) {
  while (!json.empty() && json.back() == ' ') json.pop_back();
  while ((json.size() & 3u) != 0u) json.push_back(' ');
  while ((bin.size() & 3u) != 0u) bin.push_back(0);

  std::vector<uint8_t> result;
  result.reserve(12 + 8 + json.size() + 8 + bin.size());
  appendU32(result, 0x46546c67u);
  appendU32(result, 2u);
  appendU32(result,
            static_cast<uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
  appendU32(result, static_cast<uint32_t>(json.size()));
  appendU32(result, 0x4e4f534au);
  result.insert(result.end(), json.begin(), json.end());
  appendU32(result, static_cast<uint32_t>(bin.size()));
  appendU32(result, 0x004e4942u);
  result.insert(result.end(), bin.begin(), bin.end());
  return result;
}

inline std::vector<uint8_t> replaceJsonText(const std::vector<uint8_t>& glb,
                                            const std::string& from,
                                            const std::string& to) {
  const uint32_t jsonLength = readU32(glb, 12);
  const std::size_t jsonOffset = 20;
  const std::size_t oldBinHeader = jsonOffset + jsonLength;
  const uint32_t binLength = readU32(glb, oldBinHeader);
  const std::size_t binOffset = oldBinHeader + 8;
  if (binOffset + binLength > glb.size()) {
    throw std::runtime_error("GLB BIN chunk is out of bounds");
  }

  std::string json(reinterpret_cast<const char*>(glb.data() + jsonOffset), jsonLength);
  const std::size_t position = json.find(from);
  if (position == std::string::npos) {
    throw std::runtime_error("GLB fixture JSON replacement target was not found");
  }
  json.replace(position, from.size(), to);
  std::vector<uint8_t> bin(glb.begin() + static_cast<std::ptrdiff_t>(binOffset),
                           glb.begin() + static_cast<std::ptrdiff_t>(binOffset + binLength));
  return rebuildGlb(std::move(json), std::move(bin));
}

inline std::vector<uint8_t> makeMinimalGlb(bool includeAttack = false) {
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
      "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]})";
  if (includeAttack) {
    json << R"(,
    {"name":"attack","samplers":[{"input":7,"output":9,"interpolation":"STEP"}],
      "channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]})";
  }
  json << "]}";

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

inline std::vector<uint8_t> makeTwoPrimitiveGlb() {
  const std::string secondPrimitive =
      R"(,"indices":5,"mode":4},{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"JOINTS_0":3,"WEIGHTS_0":4},"indices":5,"mode":4)";
  return replaceJsonText(makeMinimalGlb(), ",\"indices\":5,\"mode\":4",
                         secondPrimitive);
}

inline std::vector<uint8_t> makeEmbeddedTextureGlb() {
  const std::vector<uint8_t> source = makeMinimalGlb();
  const uint32_t jsonLength = readU32(source, 12);
  const std::size_t binHeader = 20 + jsonLength;
  const uint32_t binLength = readU32(source, binHeader);
  std::string json(reinterpret_cast<const char*>(source.data() + 20), jsonLength);
  while (!json.empty() && json.back() == ' ') json.pop_back();
  std::vector<uint8_t> bin(source.begin() + static_cast<std::ptrdiff_t>(binHeader + 8),
                           source.begin() + static_cast<std::ptrdiff_t>(binHeader + 8 + binLength));
  while ((bin.size() & 3u) != 0u) bin.push_back(0);
  const std::size_t imageOffset = bin.size();
  const std::vector<uint8_t> png{
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
      0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
      0x54, 0x08, 0xd7, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
      0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99,
      0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
      0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
  };
  bin.insert(bin.end(), png.begin(), png.end());

  const std::string oldBuffer =
      "\"buffers\":[{\"byteLength\":" + std::to_string(binLength) + "}]";
  const std::string newBuffer =
      "\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]";
  json.replace(json.find(oldBuffer), oldBuffer.size(), newBuffer);

  const std::string viewClose = "],\"accessors\":[";
  const std::string imageView =
      ",{\"buffer\":0,\"byteOffset\":" + std::to_string(imageOffset) +
      ",\"byteLength\":" + std::to_string(png.size()) + "}]" +
      ",\"accessors\":[";
  json.replace(json.find(viewClose), viewClose.size(), imageView);

  const std::string meshes = "\"meshes\":[";
  const std::string textureObjects =
      "\"images\":[{\"bufferView\":10,\"mimeType\":\"image/png\"}],"
      "\"textures\":[{\"source\":0}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
      "\"meshes\":[";
  json.replace(json.find(meshes), meshes.size(), textureObjects);
  const std::string indices = "\"indices\":5,\"mode\":4";
  json.replace(json.find(indices), indices.size(),
               "\"indices\":5,\"material\":0,\"mode\":4");
  return rebuildGlb(std::move(json), std::move(bin));
}

inline std::vector<uint8_t> makeInvalidEmbeddedTextureGlb() {
  std::vector<uint8_t> glb = makeEmbeddedTextureGlb();
  const uint32_t jsonLength = readU32(glb, 12);
  const std::size_t binOffset = 20 + jsonLength + 8;
  constexpr std::size_t kImageOffset = 348;
  if (binOffset + kImageOffset >= glb.size()) {
    throw std::runtime_error("image fixture range is out of bounds");
  }
  glb[binOffset + kImageOffset] = 0;
  return glb;
}

inline std::vector<uint8_t> makeZeroWeightsGlb() {
  std::vector<uint8_t> glb = makeMinimalGlb();
  const uint32_t jsonLength = readU32(glb, 12);
  const std::size_t binOffset = 20 + jsonLength + 8;
  constexpr std::size_t kWeightsOffset = 108;
  constexpr std::size_t kWeightsSize = 48;
  if (binOffset + kWeightsOffset + kWeightsSize > glb.size()) {
    throw std::runtime_error("weight fixture range is out of bounds");
  }
  std::fill(glb.begin() + static_cast<std::ptrdiff_t>(binOffset + kWeightsOffset),
            glb.begin() + static_cast<std::ptrdiff_t>(binOffset + kWeightsOffset +
                                                      kWeightsSize),
            0);
  return glb;
}

}  // namespace gltf_fixture
