// skinned_model.h: glTF 骨骼动画的纯数据核心。
//
// 这里不依赖 EGL/GLES，便于在宿主机验证 glTF 输入、动画采样和蒙皮矩阵计算。

#pragma once

#include "native/engine/render/render_animation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

constexpr uint32_t kMaxSkinJoints = 64;

enum class AnimationInterpolation {
  Linear,
  Step,
};

template <typename T>
struct AnimationChannel {
  std::vector<float> times;
  std::vector<T> values;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
};

struct SkinPalette {
  std::vector<glm::mat4> matrices;
};

class SkinnedModel;
class Shader3D;

// 每个渲染实体独立持有的动画播放状态。网格、纹理和 clip 数据仍由
// SkinnedModel 共享，避免为每个实体复制 GPU 资产。
struct SkinnedAnimationState {
  void reset();
  bool shouldReport(RenderAnimation animation, const std::string& clip) {
    return logState.shouldReport(animation, clip);
  }

 private:
  friend class SkinnedModel;
  const SkinnedModel* owner = nullptr;
  uint64_t assetRevision = 0;
  int currentClip = -1;
  int previousClip = -1;
  float currentTime = 0.0f;
  float previousTime = 0.0f;
  float blendElapsed = 0.0f;
  float blendDurationSeconds = 0.0f;
  RenderAnimation requestedAnimation = RenderAnimation::Idle;
  // 当前/上一个 clip 的播放速率（跑动步频缩放），混合期间各自推进。
  float currentRate = 1.0f;
  float previousRate = 1.0f;
  AnimationLogState logState;
};

enum class GltfAssetFormat {
  Glb,
  Gltf,
};

enum class GltfPrimitiveMode {
  Triangles,
  Other,
};

struct GltfValidationInput {
  std::string assetName;
  uint32_t jointCount = 0;
  GltfAssetFormat assetFormat = GltfAssetFormat::Gltf;
  GltfPrimitiveMode primitiveMode = GltfPrimitiveMode::Other;
  bool hasPosition = false;
  bool hasNormal = false;
  bool hasTexcoord0 = false;
  bool hasJoints0 = false;
  bool hasWeights0 = false;
  bool hasJoints1 = false;
  bool hasWeights1 = false;
  uint32_t maxVertexInfluences = 0;
  bool singleSkin = false;
  bool hasCubicSpline = false;
};

bool ValidateGltf(const GltfValidationInput& input, std::string& reason);

float WrapAnimationTime(float seconds, float duration);

glm::vec3 SampleVec3(const AnimationChannel<glm::vec3>& channel, float time);
glm::quat SampleQuat(const AnimationChannel<glm::quat>& channel, float time);

SkinPalette BuildSkinPalette(const std::vector<int>& parents,
                             const std::vector<glm::mat4>& localTransforms,
                             const std::vector<glm::mat4>& inverseBindMatrices);

// 按名查找关节索引（武器/挂饰挂点）；未找到返回 -1，调用方回退不挂载。
inline int FindJointIndex(const std::vector<std::string>& names,
                          const std::string& target) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == target) return static_cast<int>(i);
  }
  return -1;
}

// 武器挂点关节查找（兼容自定义骨架）：优先 KayKit 约定 handslot.r，
// 缺失时回退右手腕关节 R_Hand/RightHand，保证重制主角等无 handslot.r
// 的自研骨架仍能挂载程序化武器；全部缺失返回 -1（调用方不挂载）。
inline int FindWeaponJointIndex(const std::vector<std::string>& names) {
  static const char* const kCandidates[] = {"handslot.r", "R_Hand",
                                            "RightHand"};
  for (const char* candidate : kCandidates) {
    const int index = FindJointIndex(names, candidate);
    if (index >= 0) return index;
  }
  return -1;
}

// 挂件启用判定（纯函数）：逐实例覆盖表优先（按下标），越界/无覆盖
// 时回退全局开关；供共享模型的多实例差异化装备组合。
inline bool AttachmentEnabledFor(const std::vector<bool>* overrideFlags,
                                 int attachmentIndex, bool globalEnabled) {
  if (overrideFlags != nullptr && attachmentIndex >= 0 &&
      attachmentIndex < static_cast<int>(overrideFlags->size())) {
    return (*overrideFlags)[static_cast<std::size_t>(attachmentIndex)];
  }
  return globalEnabled;
}

// NPC 装备变体选择（纯函数）：按 id 取模确定性分配变体，同一 NPC
// 每次渲染装备组合稳定；variantCount<=0 回退 0（调用侧再兜底）。
inline int NpcAttachmentVariantFor(uint32_t id, int variantCount) {
  if (variantCount <= 0) return 0;
  return static_cast<int>(id % static_cast<uint32_t>(variantCount));
}

class SkinnedModel {
 public:
  SkinnedModel();
  ~SkinnedModel();
  SkinnedModel(const SkinnedModel&) = delete;
  SkinnedModel& operator=(const SkinnedModel&) = delete;

  bool tryInitialize(const std::vector<uint8_t>& bytes,
                     const std::string& assetName);
  bool ready() const;
  SkinPalette update(SkinnedAnimationState& animation,
                     const ActorRenderState& actor, float dtSeconds) const;
  // 保留旧调用签名用于一次性采样；连续播放应显式传入实例状态。
  SkinPalette update(const ActorRenderState& actor, float dtSeconds) const;
  void draw() const;
  void draw(Shader3D& shader) const;
  // 逐实例挂件启用覆盖：attachmentOverride 与 attachmentNames() 同序，
  // nullptr 使用全局开关（setAttachmentEnabled）；供共享同一模型的
  // 敌人原型按 archetype 差异化装备组合。
  void draw(Shader3D& shader,
            const std::vector<bool>* attachmentOverride) const;
  void destroy();

  // context 已不可 current 时仅丢弃 CPU/GPU 侧跟踪，绝不发出 GL 调用。
  void abandonGpuResources();

  const std::string& lastError() const;
  std::size_t vertexCount() const;
  std::size_t indexCount() const;
  std::size_t jointCount() const;
  // 与蒙皮调色板同序的关节名（缺失为空串），供武器挂点按名查找。
  const std::vector<std::string>& jointNames() const;
  const std::vector<std::string>& clipNames() const;
  // 刚性装备挂件（KayKit 模块化装备：头盔/披风/盾牌/副手等）：
  // 无 skin 的网格节点，父链挂在皮肤关节上，加载时烘焙为单关节
  // 全权重蒙皮，随本体同一管线绘制（描边/受击闪白/卡通共用）。
  // 默认全部关闭（一个 GLB 内含全部变体），调用方按名启用所需挂件。
  std::vector<std::string> attachmentNames() const;
  void setAttachmentEnabled(const std::string& name, bool enabled);
  bool attachmentEnabled(const std::string& name) const;
  // 宿主验证接口：顶点位置只读副本（挂件烘焙断言用，不参与渲染）。
  std::vector<glm::vec3> vertexPositionsForVerification() const;
  bool hasTexture() const;
  std::size_t primitiveCount() const;
  bool primitiveHasTexture(std::size_t primitiveIndex) const;
  int primitiveTextureIndex(std::size_t primitiveIndex) const;
  std::size_t embeddedTextureCount() const;
  std::size_t gpuResourceCount() const;

 private:
  struct Impl;
  void drawInternal(Shader3D* shader,
                    const std::vector<bool>* attachmentOverride = nullptr) const;
  std::unique_ptr<Impl> impl_;
};
