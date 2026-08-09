// test_model_assets.cpp: 角色模型资产清单校验（独立高模资产升级）。
//
// 两层校验：
// 1. 契约层（所有资产必须满足）：引擎加载器硬约束 + gameplay 必需
//    的动作 clip 与武器挂点契约。资产替换后该层不变。
// 2. 身份层（按当前资产清单）：当前在库资产的关节数/clip 数/已知
//    挂件名，防止意外替换或回归。换入新高模资产时只需更新下方
//    kManifest 对应条目（身份字段置 -1/nullptr 即退化为纯契约校验）。

#include "native/engine/render/skinned_model.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct AssetExpectation {
  const char* kind;                // rawfile/models/<kind>.glb
  bool required;                   // false：文件缺失时跳过（可选槽位）
  const char* identityAttachment;  // nullptr：不做身份层挂件断言
  int identityJointCount;          // -1：不断言精确关节数
  int identityClipCount;           // -1：不断言精确 clip 数
};

// 当前资产清单：三类主角为 KayKit Adventurers 1.0（身份层锁定）；
// npc 与 enemy_<archetype> 是独立高模可选槽位，缺失时跳过，注入时
// 只跑契约层。新高模资产入库时把对应条目改为 required 并填写新身份。
// player 已替换为独立高模（Midjourney 概念图 + Tripo 出模 + rig_to_kaykit
// 绑定 KayKit 骨架），无刚性挂件，身份层附件置 nullptr。
const std::vector<AssetExpectation> kManifest = {
    {"player", true, nullptr, 41, 76},
    {"enemy", true, "Mage_Hat", 41, 76},
    {"boss", true, "Barbarian_Hat", 41, 76},
    {"npc", false, nullptr, -1, -1},
    {"enemy_0", false, nullptr, -1, -1},
    {"enemy_1", false, nullptr, -1, -1},
    {"enemy_2", false, nullptr, -1, -1},
    {"enemy_3", false, nullptr, -1, -1},
    {"enemy_4", false, nullptr, -1, -1},
    {"enemy_5", false, nullptr, -1, -1},
};

bool fileExists(const std::string& path) {
  std::ifstream probe(path, std::ios::binary);
  return probe.good();
}

std::vector<uint8_t> readAsset(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  assert(input && "required model asset must exist");
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void auditAsset(const AssetExpectation& expectation) {
  const std::string path = std::string("entry/src/main/resources/rawfile/") +
                           "models/" + expectation.kind + ".glb";
  if (!fileExists(path)) {
    assert(!expectation.required && "required model asset missing");
    std::cout << expectation.kind << ": absent (optional slot, fallback "
              << "remains active)\n";
    return;
  }
  const std::vector<uint8_t> bytes = readAsset(path);
  assert(!bytes.empty());

  SkinnedModel model;
  assert(model.tryInitialize(bytes, std::string(expectation.kind) + ".glb"));
  assert(model.ready());

  // ---- 契约层：引擎加载器硬约束 + gameplay 动作/挂点契约 ----
  assert(model.vertexCount() > 0);
  assert(model.indexCount() > 0);
  assert(model.jointCount() >= 1);
  assert(model.jointCount() <= kMaxSkinJoints);
  // 关节名与调色板同序且数量一致；右手武器挂点按名挂载，必须存在。
  assert(model.jointNames().size() == model.jointCount());
  assert(FindJointIndex(model.jointNames(), "handslot.r") >= 0);
  assert(FindJointIndex(model.jointNames(), "no-such-joint") == -1);
  // 卡通管线依赖内嵌 base color 纹理。
  assert(model.embeddedTextureCount() >= 1);
  assert(model.hasTexture());
  // gameplay 必需 clip：其余变体 clip 缺失时 ResolveClip 自动回退。
  const std::vector<std::string>& clips = model.clipNames();
  for (const char* requiredClip : {"idle", "run", "attack", "hit", "death"}) {
    assert(std::find(clips.begin(), clips.end(), requiredClip) !=
           clips.end());
  }
  // 挂件加载契约：默认全部关闭，可按名启用（逐实例覆盖在渲染层）。
  const std::vector<std::string> attachments = model.attachmentNames();
  for (const std::string& name : attachments) {
    assert(!model.attachmentEnabled(name));
  }
  if (!attachments.empty()) {
    model.setAttachmentEnabled(attachments.front(), true);
    assert(model.attachmentEnabled(attachments.front()));
  }

  // ---- 身份层：锁定当前在库资产，防止意外替换或回归 ----
  if (expectation.identityJointCount >= 0) {
    assert(model.jointCount() ==
           static_cast<std::size_t>(expectation.identityJointCount));
  }
  if (expectation.identityClipCount >= 0) {
    assert(clips.size() ==
           static_cast<std::size_t>(expectation.identityClipCount));
  }
  if (expectation.identityAttachment != nullptr) {
    assert(std::find(attachments.begin(), attachments.end(),
                     expectation.identityAttachment) != attachments.end());
  }

  std::cout << expectation.kind << ": OK (joints=" << model.jointCount()
            << ", primitives=" << model.primitiveCount()
            << ", attachments=" << attachments.size()
            << ", clips=" << clips.size()
            << ", embeddedTextures=" << model.embeddedTextureCount() << ")\n";
}

}  // namespace

int main() {
  for (const AssetExpectation& expectation : kManifest) {
    auditAsset(expectation);
  }
  std::cout << "model asset manifest: all present entries satisfy contract\n";
  return 0;
}
