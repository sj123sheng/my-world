// shader_3d.h: 3D 渲染着色器程序。
//
// Shader3D 编译顶点/片段着色器源码（设计规格 §3.5），链接为独立 Program，
// 与 2D 单色着色器不共享。提供 MVP、方向光照和纹理开关 uniform 设置接口。
// 另提供地表/水面/天空三种表面模式（uSurfaceMode）：地形按高度/坡度
// 混色，水面半透明带流动涟漪，天空输出天顶→地平线渐变。
// 所有 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作，便于 macOS 语法检查。

#pragma once

#include "native/engine/render/skinned_model.h"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

// 片段着色器表面模式：与着色器内 uSurfaceMode 取值一一对应。
enum class SurfaceMode : int {
  Normal = 0,   // 普通光照（纹理/单色），与升级前行为一致。
  Terrain = 1,  // 地形：按高度/坡度混合沙地/草地/岩石色。
  Water = 2,    // 水面：半透明 + 流动涟漪。
  Sky = 3,      // 天空穹顶：天顶→地平线渐变，不受光照影响。
};

class Shader3D {
 public:
  // 编译并链接着色器程序。成功返回 true，失败返回 false（已清理中间资源）。
  // 非平台侧直接返回 false，调用方应跳过 3D 绘制。
  bool init();

  // 释放 Program。非平台侧为空操作。
  void destroy();

  // context 已不可 current 时仅清除 CPU 句柄跟踪，绝不调用 GL。
  void abandonGpuResources();

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

  // 设置 uAlpha：整体透明度缩放（默认 1.0），供伤害飘字淡出使用。
  // 调用后由调用方负责恢复 1.0。非平台侧为空操作。
  void setAlpha(float alpha) const;

  // 设置 uCameraPos：世界空间相机位置，供高光与菲涅尔轮廓光计算视线方向。
  // 非平台侧为空操作。
  void setCameraPosition(const glm::vec3& position) const;

  // 设置菲涅尔轮廓光：uRimColor/uRimStrength。strength=0 时与升级前等价。
  // 非平台侧为空操作。
  void setRim(const glm::vec3& color, float strength) const;

  // 设置 Blinn-Phong 高光：uSpecularStrength/uShininess。strength=0 时关闭。
  // 非平台侧为空操作。
  void setSpecular(float strength, float shininess) const;

  // 设置指数深度雾：uFogColor/uFogDensity，随片段到相机距离混合雾色。
  // density=0 时与升级前等价。非平台侧为空操作。
  void setFog(const glm::vec3& color, float density) const;

  void setEnvironmentTint(const glm::vec3& tint, float strength) const;

  // 设置表面模式（普通/地形/水面/天空），默认 Normal 与升级前等价。
  // 模式互斥：切换绘制对象前由调用方显式设置并负责恢复 Normal。
  void setSurfaceMode(SurfaceMode mode) const;

  // 地形模式配色：沙地（低洼/水岸）、草地（平原）、岩石（陡坡/高山）。
  void setTerrainColors(const glm::vec3& sand, const glm::vec3& grass,
                        const glm::vec3& rock) const;

  // 地形模式的水岸过渡高度：低于该值的区域向沙地色过渡。
  void setTerrainWaterLevel(float level) const;

  // 水面模式基础色与透明度（默认 0.72）。
  void setWaterColor(const glm::vec3& color, float alpha) const;

  // 动画时钟（秒）：驱动水面流动涟漪。
  void setTime(float seconds) const;

  // 天空模式配色：天顶色与地平线色（后者建议取雾色保证无缝衔接）。
  void setSkyColors(const glm::vec3& top, const glm::vec3& horizon) const;

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
  GLint locEnvironmentTint_ = -1;
  GLint locEnvironmentTintStrength_ = -1;
  GLint locSkinned_ = -1;
  GLint locJoints_ = -1;
  GLint locAlpha_ = -1;
  GLint locCameraPos_ = -1;
  GLint locRimColor_ = -1;
  GLint locRimStrength_ = -1;
  GLint locSpecularStrength_ = -1;
  GLint locShininess_ = -1;
  GLint locFogColor_ = -1;
  GLint locFogDensity_ = -1;
  GLint locSurfaceMode_ = -1;
  GLint locColorSand_ = -1;
  GLint locColorGrass_ = -1;
  GLint locColorRock_ = -1;
  GLint locTerrainWaterLevel_ = -1;
  GLint locWaterColor_ = -1;
  GLint locWaterAlpha_ = -1;
  GLint locTime_ = -1;
  GLint locSkyTop_ = -1;
  GLint locSkyHorizon_ = -1;
#endif
};
