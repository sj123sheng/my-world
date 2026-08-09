// test_model_assets.cpp: 角色模型资产清单校验（主角重制模型升级）。
//
// 两层校验：
// 1. 契约层（所有资产必须满足）：引擎加载器硬约束 + 按资产声明的
//    gameplay 必需 clip 与武器挂点契约（挂点允许 handslot.r →
//    R_Hand 回退链，见 FindWeaponJointIndex）。
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
  // gameplay 必需 clip（nullptr 结尾）：其余变体 clip 缺失时
  // ResolveClip 自动回退，不在必需集合内。
  const std::vector<const char*>* requiredClips;
};

// 当前资产清单：enemy/boss 为 KayKit Adventurers 1.0（身份层锁定）；
// npc 与 enemy_<archetype> 是独立高模可选槽位，缺失时跳过，注入时
// 按 KayKit 动作契约校验。player 为重制模型（UE 风格 41 骨 +
// prepare_player_glb.py 补 handslot.r 挂点、9 条语义 clip、剥离根
// 运动、整体旋转对齐 +Z 前向），动作集与 KayKit 不同，必需 clip
// 按新语言声明（无 attack/hit/death 时 ResolveClip 回退 idle）。
const std::vector<const char*> kKayKitRequiredClips = {"idle", "run",
                                                       "attack", "hit",
                                                       "death"};
const std::vector<const char*> kPlayerRequiredClips = {
    "idle",  "walk",   "run",  "Jump_Idle", "glide",
    "cast",  "Dive",   "Turn_180", "climb"};

const std::vector<AssetExpectation> kManifest = {
    {"player", true, nullptr, 42, 9, &kPlayerRequiredClips},
    {"enemy", true, "Mage_Hat", 41, 76, &kKayKitRequiredClips},
    {"boss", true, "Barbarian_Hat", 41, 76, &kKayKitRequiredClips},
    {"npc", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_0", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_1", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_2", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_3", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_4", false, nullptr, -1, -1, &kKayKitRequiredClips},
    {"enemy_5", false, nullptr, -1, -1, &kKayKitRequiredClips},
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
  // 关节名与调色板同序且数量一致；右手武器挂点按回退链挂载
  //（handslot.r 优先，自定义骨架回退 R_Hand），必须可解析。
  assert(model.jointNames().size() == model.jointCount());
  assert(FindWeaponJointIndex(model.jointNames()) >= 0);
  assert(FindJointIndex(model.jointNames(), "no-such-joint") == -1);
  // 卡通管线依赖内嵌 base color 纹理。
  assert(model.embeddedTextureCount() >= 1);
  assert(model.hasTexture());
  // gameplay 必需 clip 按资产声明校验。
  const std::vector<std::string>& clips = model.clipNames();
  for (const char* requiredClip : *expectation.requiredClips) {
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
