#pragma once

#include "native/engine/render/render_animation.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cstdint>

struct AssetProfile {
  float scale = 1.0f;
  float yawOffsetRadians = 0.0f;
  glm::vec3 materialTint{1.0f};
  glm::vec3 outlineColor{0.0f};
  float outlineStrength = 0.0f;
  uint8_t coreMountCount = 0;
  // Blinn-Phong 高光分档：主角盔甲强而锐利，敌人哑光退后，
  // Boss 居中；默认值与升级前的全局高光一致。
  float specularStrength = 0.28f;
  float specularShininess = 24.0f;
  // 卡通（cel）着色分档：阴影带颜色为 baseColor 的乘色，
  // edge 是 NdotL 阴影阈值，softness 是带边过渡宽度。
  bool toonShading = false;
  glm::vec3 shadowColor{0.7f, 0.7f, 0.78f};
  float toonShadowEdge = 0.1f;
  float toonSoftness = 0.08f;
  // 反向壳描边宽度（世界单位）：0 关闭描边。
  float outlineWidth = 0.0f;

  static AssetProfile forModel(ModelKind kind);
};

// VFX 粒子尺寸的参考基准：取特效调参时的原始模型缩放。
// 火花/投射物等特效按“当前模型缩放 / 基准”的比例动态放大，
// 之后模型尺寸再调整时特效自动同步跟随，无需修改粒子代码。
inline float VfxSizeRatio(const AssetProfile& profile, ModelKind kind) {
  const float reference = kind == ModelKind::Player  ? 0.025f / 3.0f
                          : kind == ModelKind::Enemy ? 0.022f / 3.0f
                                                     : 0.045f / 3.0f;
  return reference > 0.0f ? profile.scale / reference : 1.0f;
}

struct ActorRimLight {
  glm::vec3 color{0.0f};
  float strength = 0.0f;
};

// 角色轮廓光决策：从档案的 outlineColor/outlineStrength 派生逐角色
// 个性化轮廓光；受击闪白窗口（0.15s 封顶）内同时增强并向白色靠拢，
// 把“打中了”从材质闪白扩展到轮廓层。被软锁定时轮廓常亮并向
// 锁定环青金色靠拢，与脚下锁定指示环共享视觉语言。
// appearance（0..1）是出场显现进度：线性缩放最终强度，让 Boss
// 登场时轮廓光从黑暗中渐入；颜色不受影响。
// 档案未配置轮廓光时退回中性轮廓光。
inline ActorRimLight ActorRimLightFor(const AssetProfile& profile,
                                      float hitFlashSeconds,
                                      bool targeted = false,
                                      float appearance = 1.0f) {
  if (profile.outlineStrength <= 0.0f) {
    return {{0.62f, 0.72f, 0.85f}, 0.45f};
  }
  const float flash = std::min(std::max(hitFlashSeconds, 0.0f) / 0.15f, 1.0f);
  ActorRimLight rim;
  rim.color = profile.outlineColor +
              (glm::vec3(1.0f) - profile.outlineColor) * (flash * 0.5f);
  rim.strength = profile.outlineStrength + flash * 0.9f;
  if (targeted) {
    // 锁定环同色（surface 锁定指示环 markerColor），按 0.45 占比混合。
    constexpr glm::vec3 kLockColor{0.35f, 0.85f, 0.80f};
    rim.color = rim.color * 0.55f + kLockColor * 0.45f;
    rim.strength += 0.55f;
  }
  rim.strength *= std::clamp(appearance, 0.0f, 1.0f);
  return rim;
}

// 卡通着色参数决策：直接透传档案分档；toonShading=false 时调用方
// 不得启用着色器卡通路径（地形/水面/天空保持原光照）。
struct ActorToonShading {
  glm::vec3 shadowColor{0.7f, 0.7f, 0.78f};
  float edge = 0.1f;
  float softness = 0.08f;
};

inline ActorToonShading ActorToonShadingFor(const AssetProfile& profile) {
  return {profile.shadowColor, profile.toonShadowEdge, profile.toonSoftness};
}

// 反向壳描边宽度决策（世界单位）：受击闪白窗口内最多加宽 75%
// 强化打击感；被锁定时 +12% 与轮廓光常亮同步；出场进度线性缩放，
// 避免 Boss 模型未显形就先出现描边。
inline float ActorOutlineWidthFor(const AssetProfile& profile,
                                  float hitFlashSeconds, bool targeted = false,
                                  float appearance = 1.0f) {
  if (profile.outlineWidth <= 0.0f) return 0.0f;
  const float flash = std::min(std::max(hitFlashSeconds, 0.0f) / 0.15f, 1.0f);
  float width = profile.outlineWidth * (1.0f + 0.75f * flash);
  if (targeted) width *= 1.12f;
  return width * std::clamp(appearance, 0.0f, 1.0f);
}

// 描边线色决策：与轮廓光同一决策派生后整体压暗，保留色相，
// 天然继承受击闪白变白与锁定青金混色，视觉语言与轮廓光统一。
inline glm::vec3 ActorOutlineColorFor(const AssetProfile& profile,
                                      float hitFlashSeconds,
                                      bool targeted = false,
                                      float appearance = 1.0f) {
  const ActorRimLight rim =
      ActorRimLightFor(profile, hitFlashSeconds, targeted, appearance);
  return rim.color * 0.40f + glm::vec3(0.02f);
}

// 野外敌人按原型缩放（第一版共用单 enemy.glb，体型差异靠缩放区分；
// 色调由 surface 的 enemyColorByArchetype 覆盖 0-5，留待美术出独立模型）。
inline float EnemyArchetypeScale(int archetype) {
  switch (archetype) {
    case 3:  // Bruiser
    case 5:  // Elite
      return 1.3f;
    case 4:  // Caster
      return 0.95f;
    default:
      return 1.0f;
  }
}
