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

void auditAsset(const std::string& kind,
                const std::string& expectedAttachment) {
  const std::string path =
      "entry/src/main/resources/rawfile/models/" + kind + ".glb";
  const std::vector<uint8_t> bytes = readAsset(path);
  assert(!bytes.empty());

  SkinnedModel model;
  assert(model.tryInitialize(bytes, kind + ".glb"));
  assert(model.ready());
  assert(model.vertexCount() > 0);
  assert(model.indexCount() > 0);
  assert(model.jointCount() == 41);
  // 6 个本体 primitive + KayKit 模块化装备挂件（挂件默认关闭，
  // 但 primitive 已随模型加载，故总数大于 6）。
  assert(model.primitiveCount() > 6);
  assert(model.clipNames().size() == 76);
  assert(model.embeddedTextureCount() == 1);
  assert(model.hasTexture());

  // 关节名与调色板同序且数量一致；KayKit 右手武器挂点必须存在，
  // 供主角佩剑按名挂载（FindJointIndex 命中）。
  assert(model.jointNames().size() == model.jointCount());
  assert(FindJointIndex(model.jointNames(), "handslot.r") >= 0);
  assert(FindJointIndex(model.jointNames(), "no-such-joint") == -1);

  // 模块化装备挂件：按节点名注册、默认全部关闭、可按名启用。
  const std::vector<std::string> attachments = model.attachmentNames();
  assert(!attachments.empty());
  assert(std::find(attachments.begin(), attachments.end(),
                   expectedAttachment) != attachments.end());
  for (const std::string& name : attachments) {
    assert(!model.attachmentEnabled(name));
  }
  model.setAttachmentEnabled(expectedAttachment, true);
  assert(model.attachmentEnabled(expectedAttachment));

  const std::vector<std::string>& clips = model.clipNames();
  for (const char* required : {"idle", "run", "attack", "hit", "death"}) {
    assert(std::find(clips.begin(), clips.end(), required) != clips.end());
  }
  std::cout << kind << ": joints=" << model.jointCount()
            << ", primitives=" << model.primitiveCount()
            << ", attachments=" << attachments.size()
            << ", clips=" << clips.size()
            << ", embeddedTextures=" << model.embeddedTextureCount() << '\n';
}

}  // namespace

int main() {
  auditAsset("player", "Knight_Helmet");
  auditAsset("enemy", "Mage_Hat");
  auditAsset("boss", "Barbarian_Hat");
  return 0;
}
