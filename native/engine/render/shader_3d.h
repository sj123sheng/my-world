// shader_3d.h: 3D 渲染着色器程序。
//
// Shader3D 编译顶点/片段着色器源码（设计规格 §3.5），链接为独立 Program，
// 与 2D 单色着色器不共享。提供 MVP、方向光照和纹理开关 uniform 设置接口。
// 所有 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作，便于 macOS 语法检查。

#pragma once

#include "native/engine/render/skinned_model.h"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

class Shader3D {
 public:
  // 编译并链接着色器程序。成功返回 true，失败返回 false（已清理中间资源）。
  // 非平台侧直接返回 false，调用方应跳过 3D 绘制。
  bool init();

  // 释放 Program。非平台侧为空操作。
  void destroy();

  // 启用本程序（glUseProgram）。非平台侧为空操作。
  void use() const;

  // 设置 uMVP（projection * view * model）。非平台侧为空操作。
  void setMVP(const glm::mat4& mvp) const;

  // 设置 uModel（用于法线变换）。非平台侧为空操作。
  void setModel(const glm::mat4& model) const;

  // 设置方向光照 uniform：uLightDir、uLightColor、uAmbient。非平台侧为空操作。
  void setLight(const glm::vec3& dir, const glm::vec3& color,
                const glm::vec3& ambient) const;

  // 设置 uHasTexture：true 时片段着色器采样 uTexture。非平台侧为空操作。
  void setHasTexture(bool hasTexture) const;

  // 上传骨骼调色板。空调色板或超过 64 个矩阵时拒绝启用蒙皮绘制。
  void setSkinPalette(const SkinPalette& palette);

  // 设置 uSkinned。未接受有效调色板时，true 会退化为 false，防止非法骨骼绘制。
  void setSkinned(bool skinned);

  // 返回最近一次调色板上传是否有效，供宿主机状态测试使用。
  bool skinPaletteValid() const { return skinPaletteValid_; }

  // 返回最近一次写入 uSkinned 的状态，供宿主机状态测试使用。
  bool skinningEnabled() const { return skinningEnabled_; }

  // 返回 Program 句柄（非平台侧恒为 0）。
  unsigned int program() const { return program_; }

 private:
  unsigned int program_ = 0;
  bool skinPaletteValid_ = false;
  bool skinningEnabled_ = false;

#ifdef OHOS_PLATFORM
  GLint locMVP_ = -1;
  GLint locModel_ = -1;
  GLint locLightDir_ = -1;
  GLint locLightColor_ = -1;
  GLint locAmbient_ = -1;
  GLint locHasTexture_ = -1;
  GLint locTexture_ = -1;
  GLint locSkinned_ = -1;
  GLint locJoints_ = -1;
#endif
};
