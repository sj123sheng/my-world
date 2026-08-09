// shader_3d.cpp: 3D 着色器程序实现。
//
// 编译顶点/片段着色器（设计规格 §3.5）并链接为 Program。使用 #version 300 es
// 语法，与现有 2D 着色器保持一致，保证在 HarmonyOS GLES3 设备上原生编译。
// 属性通过 layout(location = N) 显式绑定 0–4；静态 Mesh 的 draw() 保持只绑定
// 0–2。所有 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作。

#include "native/engine/render/shader_3d.h"

#include <algorithm>

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#include <hilog/log.h>

#define LOGI_3D(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan3D", __VA_ARGS__)
#define LOGE_3D(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan3D", __VA_ARGS__)
#endif

namespace {

// 设计规格 §3.5 的顶点着色器，改写为 GLES3 #version 300 es 语法：
// attribute -> in，varying -> out，使用 layout(location) 显式绑定属性槽位。
// vWorldPos 输出世界空间位置，供片段着色器计算视线方向（高光/轮廓光）。
[[maybe_unused]] const char* kVertexShaderSrc =
    "#version 300 es\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "uniform bool uSkinned;\n"
    "uniform mat4 uJoints[64];\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec2 aUV;\n"
    "layout(location = 3) in uvec4 aJoints;\n"
    "layout(location = 4) in vec4 aWeights;\n"
    "uniform float uOutlineWidth;\n"
    "out vec3 vNormal;\n"
    "out vec2 vUV;\n"
    "out vec3 vWorldPos;\n"
    "void main() {\n"
    "  mat4 skin = uJoints[aJoints.x] * aWeights.x +\n"
    "              uJoints[aJoints.y] * aWeights.y +\n"
    "              uJoints[aJoints.z] * aWeights.z +\n"
    "              uJoints[aJoints.w] * aWeights.w;\n"
    "  vec4 localPosition = uSkinned ? skin * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "  vec3 localNormal = uSkinned ? mat3(skin) * aNormal : aNormal;\n"
    // 反向壳描边：沿（蒙皮后）法线外推背面顶点，宽度为模型局部空间，
    // 调用方按 世界宽度 / 模型缩放 换算；uOutlineWidth=0 时与升级前等价。
    "  if (uOutlineWidth > 0.0) {\n"
    "    localPosition.xyz += normalize(localNormal) * uOutlineWidth;\n"
    "  }\n"
    "  gl_Position = uMVP * localPosition;\n"
    "  vNormal = mat3(uModel) * localNormal;\n"
    "  vWorldPos = (uModel * localPosition).xyz;\n"
    "  vUV = aUV;\n"
    "}\n";

// 设计规格 §3.5 的片段着色器，改写为 GLES3 #version 300 es 语法：
// varying -> in，gl_FragColor -> 自定义 out，texture2D -> texture。
// 在方向光漫反射基础上叠加 Blinn-Phong 高光与菲涅尔轮廓光，两者强度
// uniform 默认 0，未配置时与升级前输出完全一致。
// uSurfaceMode 切换表面模式：0 普通 / 1 地形混色 / 2 水面 / 3 天空渐变，
// 默认 0 与升级前行为一致。
[[maybe_unused]] const char* kFragmentShaderSrc =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec3 uLightDir;\n"
    "uniform vec3 uLightColor;\n"
    "uniform vec3 uAmbient;\n"
    "uniform bool uHasTexture;\n"
    "uniform vec3 uEnvironmentTint;\n"
    "uniform float uEnvironmentTintStrength;\n"
    "uniform float uAlpha;\n"
    "uniform vec3 uCameraPos;\n"
    "uniform vec3 uRimColor;\n"
    "uniform float uRimStrength;\n"
    "uniform float uSpecularStrength;\n"
    "uniform float uShininess;\n"
    "uniform vec3 uFogColor;\n"
    "uniform float uFogDensity;\n"
    "uniform int uOutlinePass;\n"
    "uniform vec3 uOutlineColor;\n"
    "uniform int uToon;\n"
    "uniform vec3 uShadowColor;\n"
    "uniform float uToonEdge;\n"
    "uniform float uToonSoftness;\n"
    "uniform int uSurfaceMode;\n"
    "uniform vec3 uColorSand;\n"
    "uniform vec3 uColorGrass;\n"
    "uniform vec3 uColorRock;\n"
    "uniform vec4 uDistrictRects[6];\n"
    "uniform vec3 uDistrictGrass[6];\n"
    "uniform vec3 uDistrictSand[6];\n"
    "uniform vec3 uDistrictRock[6];\n"
    "uniform int uDistrictCount;\n"
    "uniform vec4 uRouteSegments[8];\n"
    "uniform int uRouteCount;\n"
    "uniform float uTerrainWaterLevel;\n"
    "uniform vec3 uWaterColor;\n"
    "uniform float uWaterAlpha;\n"
    "uniform float uTime;\n"
    "uniform vec3 uSkyTop;\n"
    "uniform vec3 uSkyHorizon;\n"
    "in vec3 vNormal;\n"
    "in vec2 vUV;\n"
    "in vec3 vWorldPos;\n"
    "out vec4 fragColor;\n"
    "float hash21(vec2 p) {\n"
    "  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
    "}\n"
    "float vnoise(vec2 p) {\n"
    "  vec2 i = floor(p);\n"
    "  vec2 f = fract(p);\n"
    "  f = f * f * (3.0 - 2.0 * f);\n"
    "  float a = hash21(i);\n"
    "  float b = hash21(i + vec2(1.0, 0.0));\n"
    "  float c = hash21(i + vec2(0.0, 1.0));\n"
    "  float d = hash21(i + vec2(1.0, 1.0));\n"
    "  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    // 描边 pass：输出纯色轮廓线，不参与光照/雾计算。
    "  if (uOutlinePass == 1) {\n"
    "    fragColor = vec4(uOutlineColor, uAlpha);\n"
    "    return;\n"
    "  }\n"
    "  vec3 N = normalize(vNormal);\n"
    "  vec3 V = normalize(uCameraPos - vWorldPos);\n"
    "  if (uSurfaceMode == 3) {\n"
    // 天空穹顶：视线仰角驱动天顶→地平线渐变，地平线色与雾色一致，
    "  // 远山融入天际时无接缝；不受光照与额外雾混合影响。\n"
    "  float h = clamp(V.y, 0.0, 1.0);\n"
    "  vec3 sky = mix(uSkyHorizon, uSkyTop, pow(h, 0.6));\n"
    "  fragColor = vec4(sky, uAlpha);\n"
    "  return;\n"
    "}\n"
    "  vec4 baseColor;\n"
    "  if (uSurfaceMode == 1) {\n"
    // 地形（原神式分区生态）：分区调色板按世界坐标加权混合出沙/草/岩，
    // 高度/坡度混色 + 值噪声打碎 + 大尺度明度调制 + 主干道压暗 +
    // 湿沙带 + 高海拔岩帽。
    "  vec3 sand = uColorSand;\n"
    "  vec3 grass = uColorGrass;\n"
    "  vec3 rock = uColorRock;\n"
    "  float biomeWeight = 0.0;\n"
    "  vec3 sandAcc = vec3(0.0);\n"
    "  vec3 grassAcc = vec3(0.0);\n"
    "  vec3 rockAcc = vec3(0.0);\n"
    "  for (int i = 0; i < 6; ++i) {\n"
    "    if (i >= uDistrictCount) { break; }\n"
    "    vec4 rect = uDistrictRects[i];\n"
    "    float outsideX = max(max(rect.x - vUV.x, vUV.x - rect.z), 0.0);\n"
    "    float outsideY = max(max(rect.y - vUV.y, vUV.y - rect.w), 0.0);\n"
    "    float outside = length(vec2(outsideX, outsideY));\n"
    "    float w = 1.0 - smoothstep(0.0, 0.035, outside);\n"
    "    sandAcc += uDistrictSand[i] * w;\n"
    "    grassAcc += uDistrictGrass[i] * w;\n"
    "    rockAcc += uDistrictRock[i] * w;\n"
    "    biomeWeight += w;\n"
    "  }\n"
    "  if (biomeWeight > 0.001) {\n"
    "    sand = sandAcc / biomeWeight;\n"
    "    grass = grassAcc / biomeWeight;\n"
    "    rock = rockAcc / biomeWeight;\n"
    "  }\n"
    "  float noise = (vnoise(vUV * 90.0) * 0.6 + vnoise(vUV * 23.0) * 0.4) - 0.5;\n"
    "  float macro = vnoise(vUV * 6.5) - 0.5;\n"
    "  float slope = 1.0 - clamp(N.y, 0.0, 1.0);\n"
    "  float shore = clamp((vWorldPos.y - uTerrainWaterLevel) / 0.022 +\n"
    "                      noise * 0.5, 0.0, 1.0);\n"
    "  float wet = 1.0 - smoothstep(0.0, 0.008,\n"
    "                               vWorldPos.y - uTerrainWaterLevel);\n"
    "  sand *= 1.0 - wet * 0.22;\n"
    "  float rockMix = clamp((slope - 0.28) / 0.22 +\n"
    "                        (vWorldPos.y - 0.055) / 0.05 +\n"
    "                        noise * 0.25, 0.0, 1.0);\n"
    "  float cap = smoothstep(0.062, 0.088, vWorldPos.y);\n"
    "  rock = mix(rock, vec3(0.60, 0.60, 0.64), cap * 0.55);\n"
    "  vec3 terrainColor = mix(mix(sand, grass, shore), rock, rockMix);\n"
    "  terrainColor *= 1.0 + macro * 0.09 + noise * 0.07;\n"
    "  float pathDist = 1000.0;\n"
    "  for (int i = 0; i < 8; ++i) {\n"
    "    if (i >= uRouteCount) { break; }\n"
    "    vec2 a = uRouteSegments[i].xy;\n"
    "    vec2 b = uRouteSegments[i].zw;\n"
    "    vec2 pa = vUV - a;\n"
    "    vec2 ba = b - a;\n"
    "    float t = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);\n"
    "    pathDist = min(pathDist, length(pa - ba * t));\n"
    "  }\n"
    "  float path = (1.0 - smoothstep(0.006, 0.013, pathDist)) * shore;\n"
    "  terrainColor = mix(terrainColor,\n"
    "                     terrainColor * vec3(0.82, 0.78, 0.70), path * 0.55);\n"
    "  baseColor = vec4(clamp(terrainColor, 0.0, 1.0), 1.0);\n"
    "} else if (uSurfaceMode == 2) {\n"
    // 水面：双频涟漪扰动颜色与法线，菲涅尔掠射增浓，日光高光闪点。
    "  float ripple = sin(vWorldPos.x * 120.0 + uTime * 1.6) *\n"
    "                 sin(vWorldPos.z * 105.0 - uTime * 1.2) * 0.5 + 0.5;\n"
    "  float ripple2 = sin(vWorldPos.x * 57.0 - uTime * 0.9 + 1.7) *\n"
    "                  sin(vWorldPos.z * 49.0 + uTime * 0.7) * 0.5 + 0.5;\n"
    "  float wave = ripple * 0.6 + ripple2 * 0.4;\n"
    "  N = normalize(N + vec3(sin(vWorldPos.z * 90.0 + uTime) * 0.06 +\n"
    "                           sin(vWorldPos.z * 41.0 - uTime * 0.6) * 0.03,\n"
    "                           0.0,\n"
    "                           cos(vWorldPos.x * 80.0 - uTime * 0.8) * 0.06 +\n"
    "                           cos(vWorldPos.x * 37.0 + uTime * 0.5) * 0.03));\n"
    "  float fresnel = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 2.0);\n"
    "  vec3 waterHalf = normalize(normalize(uLightDir) + V);\n"
    "  float glint = pow(max(dot(N, waterHalf), 0.0), 90.0) * 0.35;\n"
    "  baseColor = vec4(uWaterColor + wave * 0.07 + glint,\n"
    "                   mix(uWaterAlpha, 0.92, fresnel));\n"
    "} else {\n"
    "  baseColor = uHasTexture ? texture(uTexture, vUV) : vec4(1.0);\n"
    "}\n"
    "  vec3 L = normalize(uLightDir);\n"
    "  float diff = max(dot(N, L), 0.0);\n"
    "  vec3 lit;\n"
    "  if (uToon == 1) {\n"
    // 卡通着色：漫反射量化为明暗两段，暗部乘以角色专属阴影色，
    // 过渡宽度 uToonSoftness 控制阴影边缘软硬；亮部保持固有色全光照。
    "    float band = smoothstep(uToonEdge - uToonSoftness,\n"
    "                            uToonEdge + uToonSoftness, dot(N, L));\n"
    "    vec3 bright = baseColor.rgb * (uAmbient + uLightColor);\n"
    "    vec3 shadowed = baseColor.rgb * uShadowColor;\n"
    "    lit = mix(shadowed, bright, band);\n"
    "  } else {\n"
    "    lit = baseColor.rgb * (uAmbient + uLightColor * diff);\n"
    "  }\n"
    "  vec3 H = normalize(L + V);\n"
    "  float spec = pow(max(dot(N, H), 0.0), max(uShininess, 1.0)) *\n"
    "               uSpecularStrength;\n"
    "  lit += uLightColor * spec;\n"
    "  float rim = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 3.0) * uRimStrength;\n"
    "  lit += uRimColor * rim;\n"
    "  vec3 finalColor = mix(lit, uEnvironmentTint, uEnvironmentTintStrength);\n"
    "  float fogDistance = length(uCameraPos - vWorldPos);\n"
    "  float fogFactor = clamp(1.0 - exp(-uFogDensity * fogDistance), 0.0, 1.0);\n"
    "  finalColor = mix(finalColor, uFogColor, fogFactor);\n"
    "  fragColor = vec4(finalColor, baseColor.a * uAlpha);\n"
    "}\n";

}  // namespace

bool Shader3D::init() {
#ifdef OHOS_PLATFORM
  if (program_ != 0u) {
    return true;  // 已初始化，避免重复创建造成资源泄漏
  }

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  if (vs == 0u) {
    LOGE_3D("glCreateShader(GL_VERTEX_SHADER) failed");
    return false;
  }
  glShaderSource(vs, 1, &kVertexShaderSrc, nullptr);
  glCompileShader(vs);
  GLint compiled = 0;
  glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buf[512];
    glGetShaderInfoLog(vs, sizeof(buf), nullptr, buf);
    LOGE_3D("3D vertex shader compile failed: %{public}s", buf);
    glDeleteShader(vs);
    return false;
  }

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  if (fs == 0u) {
    LOGE_3D("glCreateShader(GL_FRAGMENT_SHADER) failed");
    glDeleteShader(vs);
    return false;
  }
  glShaderSource(fs, 1, &kFragmentShaderSrc, nullptr);
  glCompileShader(fs);
  glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buf[512];
    glGetShaderInfoLog(fs, sizeof(buf), nullptr, buf);
    LOGE_3D("3D fragment shader compile failed: %{public}s", buf);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return false;
  }

  GLuint prog = glCreateProgram();
  if (prog == 0u) {
    LOGE_3D("glCreateProgram failed for 3D shader");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return false;
  }
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  // 链接完成后即可删除 shader 对象（Program 保留引用）。
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buf[512];
    glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
    LOGE_3D("3D program link failed: %{public}s", buf);
    glDeleteProgram(prog);
    return false;
  }

  program_ = prog;
  // 缓存 uniform 位置，避免每帧查询。
  locMVP_ = glGetUniformLocation(program_, "uMVP");
  locModel_ = glGetUniformLocation(program_, "uModel");
  locLightDir_ = glGetUniformLocation(program_, "uLightDir");
  locLightColor_ = glGetUniformLocation(program_, "uLightColor");
  locAmbient_ = glGetUniformLocation(program_, "uAmbient");
  locHasTexture_ = glGetUniformLocation(program_, "uHasTexture");
  locTexture_ = glGetUniformLocation(program_, "uTexture");
  locEnvironmentTint_ = glGetUniformLocation(program_, "uEnvironmentTint");
  locEnvironmentTintStrength_ =
      glGetUniformLocation(program_, "uEnvironmentTintStrength");
  locSkinned_ = glGetUniformLocation(program_, "uSkinned");
  locJoints_ = glGetUniformLocation(program_, "uJoints");
  locAlpha_ = glGetUniformLocation(program_, "uAlpha");
  locCameraPos_ = glGetUniformLocation(program_, "uCameraPos");
  locRimColor_ = glGetUniformLocation(program_, "uRimColor");
  locRimStrength_ = glGetUniformLocation(program_, "uRimStrength");
  locSpecularStrength_ = glGetUniformLocation(program_, "uSpecularStrength");
  locShininess_ = glGetUniformLocation(program_, "uShininess");
  locFogColor_ = glGetUniformLocation(program_, "uFogColor");
  locFogDensity_ = glGetUniformLocation(program_, "uFogDensity");
  locOutlinePass_ = glGetUniformLocation(program_, "uOutlinePass");
  locOutlineColor_ = glGetUniformLocation(program_, "uOutlineColor");
  locOutlineWidth_ = glGetUniformLocation(program_, "uOutlineWidth");
  locToon_ = glGetUniformLocation(program_, "uToon");
  locShadowColor_ = glGetUniformLocation(program_, "uShadowColor");
  locToonEdge_ = glGetUniformLocation(program_, "uToonEdge");
  locToonSoftness_ = glGetUniformLocation(program_, "uToonSoftness");
  locSurfaceMode_ = glGetUniformLocation(program_, "uSurfaceMode");
  locColorSand_ = glGetUniformLocation(program_, "uColorSand");
  locColorGrass_ = glGetUniformLocation(program_, "uColorGrass");
  locColorRock_ = glGetUniformLocation(program_, "uColorRock");
  locDistrictRects_ = glGetUniformLocation(program_, "uDistrictRects");
  locDistrictGrass_ = glGetUniformLocation(program_, "uDistrictGrass");
  locDistrictSand_ = glGetUniformLocation(program_, "uDistrictSand");
  locDistrictRock_ = glGetUniformLocation(program_, "uDistrictRock");
  locDistrictCount_ = glGetUniformLocation(program_, "uDistrictCount");
  locRouteSegments_ = glGetUniformLocation(program_, "uRouteSegments");
  locRouteCount_ = glGetUniformLocation(program_, "uRouteCount");
  locTerrainWaterLevel_ = glGetUniformLocation(program_, "uTerrainWaterLevel");
  locWaterColor_ = glGetUniformLocation(program_, "uWaterColor");
  locWaterAlpha_ = glGetUniformLocation(program_, "uWaterAlpha");
  locTime_ = glGetUniformLocation(program_, "uTime");
  locSkyTop_ = glGetUniformLocation(program_, "uSkyTop");
  locSkyHorizon_ = glGetUniformLocation(program_, "uSkyHorizon");
  // uniform 默认值为 0：显式把 uAlpha 初置为 1，避免未调用 setAlpha
  // 的既有绘制路径被透明化；轮廓光/高光/雾强度置 0，保持未配置时与升级前等价。
  glUseProgram(program_);
  glUniform1f(locAlpha_, 1.0f);
  glUniform1f(locRimStrength_, 0.0f);
  glUniform1f(locSpecularStrength_, 0.0f);
  glUniform1f(locShininess_, 32.0f);
  glUniform1f(locFogDensity_, 0.0f);
  // 卡通着色与描边默认关闭，未显式配置时与升级前输出完全一致。
  glUniform1i(locOutlinePass_, 0);
  glUniform3f(locOutlineColor_, 0.0f, 0.0f, 0.0f);
  glUniform1f(locOutlineWidth_, 0.0f);
  glUniform1i(locToon_, 0);
  glUniform3f(locShadowColor_, 0.7f, 0.7f, 0.78f);
  glUniform1f(locToonEdge_, 0.1f);
  glUniform1f(locToonSoftness_, 0.08f);
  // 表面模式默认普通；地形/水面/天空配色预置默认值，未显式配置时
  // 也有合理观感。
  glUniform1i(locSurfaceMode_, 0);
  glUniform3f(locColorSand_, 0.76f, 0.68f, 0.50f);
  glUniform3f(locColorGrass_, 0.32f, 0.52f, 0.30f);
  glUniform3f(locColorRock_, 0.42f, 0.42f, 0.46f);
  glUniform1f(locTerrainWaterLevel_, -0.012f);
  glUniform3f(locWaterColor_, 0.16f, 0.38f, 0.47f);
  glUniform1f(locWaterAlpha_, 0.72f);
  glUniform1f(locTime_, 0.0f);
  glUniform3f(locSkyTop_, 0.20f, 0.32f, 0.52f);
  glUniform3f(locSkyHorizon_, 0.55f, 0.58f, 0.65f);
  glUseProgram(0);
  LOGI_3D("3D program linked: mvp=%{public}d model=%{public}d lightDir=%{public}d "
          "lightColor=%{public}d ambient=%{public}d hasTexture=%{public}d texture=%{public}d",
          locMVP_, locModel_, locLightDir_, locLightColor_, locAmbient_,
          locHasTexture_, locTexture_);
  return true;
#else
  return false;
#endif
}

void Shader3D::destroy() {
#ifdef OHOS_PLATFORM
  if (program_ != 0u) {
    glDeleteProgram(program_);
    program_ = 0;
    locMVP_ = -1;
    locModel_ = -1;
    locLightDir_ = -1;
    locLightColor_ = -1;
    locAmbient_ = -1;
    locHasTexture_ = -1;
    locTexture_ = -1;
    locEnvironmentTint_ = -1;
    locEnvironmentTintStrength_ = -1;
    locSkinned_ = -1;
    locJoints_ = -1;
    locAlpha_ = -1;
    locCameraPos_ = -1;
    locRimColor_ = -1;
    locRimStrength_ = -1;
    locSpecularStrength_ = -1;
    locShininess_ = -1;
    locFogColor_ = -1;
    locFogDensity_ = -1;
    locOutlinePass_ = -1;
    locOutlineColor_ = -1;
    locOutlineWidth_ = -1;
    locToon_ = -1;
    locShadowColor_ = -1;
    locToonEdge_ = -1;
    locToonSoftness_ = -1;
    locSurfaceMode_ = -1;
    locColorSand_ = -1;
    locColorGrass_ = -1;
    locColorRock_ = -1;
    locTerrainWaterLevel_ = -1;
    locWaterColor_ = -1;
    locWaterAlpha_ = -1;
    locTime_ = -1;
    locSkyTop_ = -1;
    locSkyHorizon_ = -1;
  }
#endif
  skinPaletteValid_ = false;
  skinningEnabled_ = false;
  toonEnabled_ = false;
  outlineWidth_ = 0.0f;
}

void Shader3D::abandonGpuResources() {
  program_ = 0;
  skinPaletteValid_ = false;
  skinningEnabled_ = false;
  toonEnabled_ = false;
  outlineWidth_ = 0.0f;
#ifdef OHOS_PLATFORM
  locMVP_ = -1;
  locModel_ = -1;
  locLightDir_ = -1;
  locLightColor_ = -1;
  locAmbient_ = -1;
  locHasTexture_ = -1;
  locTexture_ = -1;
  locEnvironmentTint_ = -1;
  locEnvironmentTintStrength_ = -1;
  locSkinned_ = -1;
  locJoints_ = -1;
  locAlpha_ = -1;
  locCameraPos_ = -1;
  locRimColor_ = -1;
  locRimStrength_ = -1;
  locSpecularStrength_ = -1;
  locShininess_ = -1;
  locFogColor_ = -1;
  locFogDensity_ = -1;
  locOutlinePass_ = -1;
  locOutlineColor_ = -1;
  locOutlineWidth_ = -1;
  locToon_ = -1;
  locShadowColor_ = -1;
  locToonEdge_ = -1;
  locToonSoftness_ = -1;
  locSurfaceMode_ = -1;
  locColorSand_ = -1;
  locColorGrass_ = -1;
  locColorRock_ = -1;
  locTerrainWaterLevel_ = -1;
  locWaterColor_ = -1;
  locWaterAlpha_ = -1;
  locTime_ = -1;
  locSkyTop_ = -1;
  locSkyHorizon_ = -1;
#endif
}

void Shader3D::use() const {
#ifdef OHOS_PLATFORM
  if (program_ != 0u) {
    glUseProgram(program_);
  }
#endif
}

void Shader3D::setMVP(const glm::mat4& mvp) const {
#ifdef OHOS_PLATFORM
  if (locMVP_ != -1) {
    glUniformMatrix4fv(locMVP_, 1, GL_FALSE, &mvp[0][0]);
  }
#else
  (void)mvp;
#endif
}

void Shader3D::setModel(const glm::mat4& model) const {
#ifdef OHOS_PLATFORM
  if (locModel_ != -1) {
    glUniformMatrix4fv(locModel_, 1, GL_FALSE, &model[0][0]);
  }
#else
  (void)model;
#endif
}

void Shader3D::setLight(const glm::vec3& dir, const glm::vec3& color,
                        const glm::vec3& ambient) const {
#ifdef OHOS_PLATFORM
  if (locLightDir_ != -1) {
    glUniform3fv(locLightDir_, 1, &dir[0]);
  }
  if (locLightColor_ != -1) {
    glUniform3fv(locLightColor_, 1, &color[0]);
  }
  if (locAmbient_ != -1) {
    glUniform3fv(locAmbient_, 1, &ambient[0]);
  }
#else
  (void)dir;
  (void)color;
  (void)ambient;
#endif
}

void Shader3D::setEnvironmentTint(const glm::vec3& tint,
                                  float strength) const {
#ifdef OHOS_PLATFORM
  if (locEnvironmentTint_ != -1) {
    glUniform3fv(locEnvironmentTint_, 1, &tint[0]);
  }
  if (locEnvironmentTintStrength_ != -1) {
    glUniform1f(locEnvironmentTintStrength_, strength);
  }
#else
  (void)tint;
  (void)strength;
#endif
}

void Shader3D::setHasTexture(bool hasTexture) const {
#ifdef OHOS_PLATFORM
  if (locHasTexture_ != -1) {
    glUniform1i(locHasTexture_, hasTexture ? 1 : 0);
  }
#else
  (void)hasTexture;
#endif
}

void Shader3D::setAlpha(float alpha) const {
#ifdef OHOS_PLATFORM
  if (locAlpha_ != -1) {
    glUniform1f(locAlpha_, alpha);
  }
#else
  (void)alpha;
#endif
}

void Shader3D::setCameraPosition(const glm::vec3& position) const {
#ifdef OHOS_PLATFORM
  if (locCameraPos_ != -1) {
    glUniform3fv(locCameraPos_, 1, &position[0]);
  }
#else
  (void)position;
#endif
}

void Shader3D::setRim(const glm::vec3& color, float strength) const {
#ifdef OHOS_PLATFORM
  if (locRimColor_ != -1) {
    glUniform3fv(locRimColor_, 1, &color[0]);
  }
  if (locRimStrength_ != -1) {
    glUniform1f(locRimStrength_, strength);
  }
#else
  (void)color;
  (void)strength;
#endif
}

void Shader3D::setSpecular(float strength, float shininess) const {
#ifdef OHOS_PLATFORM
  if (locSpecularStrength_ != -1) {
    glUniform1f(locSpecularStrength_, strength);
  }
  if (locShininess_ != -1) {
    glUniform1f(locShininess_, shininess);
  }
#else
  (void)strength;
  (void)shininess;
#endif
}

void Shader3D::setToonShading(bool enabled, const glm::vec3& shadowColor,
                              float edge, float softness) {
  toonEnabled_ = enabled;
#ifdef OHOS_PLATFORM
  if (locToon_ != -1) {
    glUniform1i(locToon_, enabled ? 1 : 0);
  }
  if (enabled) {
    if (locShadowColor_ != -1) {
      glUniform3fv(locShadowColor_, 1, &shadowColor[0]);
    }
    if (locToonEdge_ != -1) {
      glUniform1f(locToonEdge_, edge);
    }
    if (locToonSoftness_ != -1) {
      glUniform1f(locToonSoftness_, std::max(softness, 0.001f));
    }
  }
#else
  (void)shadowColor;
  (void)edge;
  (void)softness;
#endif
}

void Shader3D::setOutlinePass(float width, const glm::vec3& color) {
  outlineWidth_ = std::max(width, 0.0f);
#ifdef OHOS_PLATFORM
  if (locOutlineWidth_ != -1) {
    glUniform1f(locOutlineWidth_, outlineWidth_);
  }
  if (locOutlinePass_ != -1) {
    glUniform1i(locOutlinePass_, outlineWidth_ > 0.0f ? 1 : 0);
  }
  if (outlineWidth_ > 0.0f && locOutlineColor_ != -1) {
    glUniform3fv(locOutlineColor_, 1, &color[0]);
  }
#else
  (void)color;
#endif
}

void Shader3D::setFog(const glm::vec3& color, float density) const {
#ifdef OHOS_PLATFORM
  if (locFogColor_ != -1) {
    glUniform3fv(locFogColor_, 1, &color[0]);
  }
  if (locFogDensity_ != -1) {
    glUniform1f(locFogDensity_, density);
  }
#else
  (void)color;
  (void)density;
#endif
}

void Shader3D::setSurfaceMode(SurfaceMode mode) const {
#ifdef OHOS_PLATFORM
  if (locSurfaceMode_ != -1) {
    glUniform1i(locSurfaceMode_, static_cast<int>(mode));
  }
#else
  (void)mode;
#endif
}

void Shader3D::setTerrainColors(const glm::vec3& sand, const glm::vec3& grass,
                                const glm::vec3& rock) const {
#ifdef OHOS_PLATFORM
  if (locColorSand_ != -1) {
    glUniform3fv(locColorSand_, 1, &sand[0]);
  }
  if (locColorGrass_ != -1) {
    glUniform3fv(locColorGrass_, 1, &grass[0]);
  }
  if (locColorRock_ != -1) {
    glUniform3fv(locColorRock_, 1, &rock[0]);
  }
#else
  (void)sand;
  (void)grass;
  (void)rock;
#endif
}

void Shader3D::setTerrainBiomes(const TerrainBiomeUniforms& biomes) const {
#ifdef OHOS_PLATFORM
  const int count = std::clamp(biomes.count, 0,
                               TerrainBiomeUniforms::kMaxDistricts);
  if (locDistrictCount_ != -1) {
    glUniform1i(locDistrictCount_, count);
  }
  if (count <= 0) return;
  if (locDistrictRects_ != -1) {
    glUniform4fv(locDistrictRects_, count, &biomes.rects[0][0]);
  }
  if (locDistrictGrass_ != -1) {
    glUniform3fv(locDistrictGrass_, count, &biomes.grass[0][0]);
  }
  if (locDistrictSand_ != -1) {
    glUniform3fv(locDistrictSand_, count, &biomes.sand[0][0]);
  }
  if (locDistrictRock_ != -1) {
    glUniform3fv(locDistrictRock_, count, &biomes.rock[0][0]);
  }
#else
  (void)biomes;
#endif
}

void Shader3D::setTerrainRoutes(const TerrainRouteUniforms& routes) const {
#ifdef OHOS_PLATFORM
  const int count = std::clamp(routes.count, 0,
                               TerrainRouteUniforms::kMaxRoutes);
  if (locRouteCount_ != -1) {
    glUniform1i(locRouteCount_, count);
  }
  if (count > 0 && locRouteSegments_ != -1) {
    glUniform4fv(locRouteSegments_, count, &routes.segments[0][0]);
  }
#else
  (void)routes;
#endif
}

void Shader3D::setTerrainWaterLevel(float level) const {
#ifdef OHOS_PLATFORM
  if (locTerrainWaterLevel_ != -1) {
    glUniform1f(locTerrainWaterLevel_, level);
  }
#else
  (void)level;
#endif
}

void Shader3D::setWaterColor(const glm::vec3& color, float alpha) const {
#ifdef OHOS_PLATFORM
  if (locWaterColor_ != -1) {
    glUniform3fv(locWaterColor_, 1, &color[0]);
  }
  if (locWaterAlpha_ != -1) {
    glUniform1f(locWaterAlpha_, alpha);
  }
#else
  (void)color;
  (void)alpha;
#endif
}

void Shader3D::setTime(float seconds) const {
#ifdef OHOS_PLATFORM
  if (locTime_ != -1) {
    glUniform1f(locTime_, seconds);
  }
#else
  (void)seconds;
#endif
}

void Shader3D::setSkyColors(const glm::vec3& top,
                            const glm::vec3& horizon) const {
#ifdef OHOS_PLATFORM
  if (locSkyTop_ != -1) {
    glUniform3fv(locSkyTop_, 1, &top[0]);
  }
  if (locSkyHorizon_ != -1) {
    glUniform3fv(locSkyHorizon_, 1, &horizon[0]);
  }
#else
  (void)top;
  (void)horizon;
#endif
}

void Shader3D::setSkinPalette(const SkinPalette& palette) {
  skinPaletteValid_ = !palette.matrices.empty() &&
                      palette.matrices.size() <= kMaxSkinJoints;
  if (!skinPaletteValid_) {
    setSkinned(false);
    return;
  }
#ifdef OHOS_PLATFORM
  if (program_ == 0u || locJoints_ == -1) {
    skinPaletteValid_ = false;
    setSkinned(false);
    return;
  }
  glUniformMatrix4fv(locJoints_, static_cast<GLsizei>(palette.matrices.size()),
                     GL_FALSE, &palette.matrices.front()[0][0]);
#else
  (void)palette;
#endif
}

void Shader3D::setSkinned(bool skinned) {
  skinningEnabled_ = skinned && skinPaletteValid_;
#ifdef OHOS_PLATFORM
  if (program_ != 0u && locSkinned_ != -1) {
    glUniform1i(locSkinned_, skinningEnabled_ ? 1 : 0);
  }
#else
  (void)skinned;
#endif
}
