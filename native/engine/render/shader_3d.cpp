// shader_3d.cpp: 3D 着色器程序实现。
//
// 编译顶点/片段着色器（设计规格 §3.5）并链接为 Program。使用 #version 300 es
// 语法，与现有 2D 着色器保持一致，保证在 HarmonyOS GLES3 设备上原生编译。
// 属性通过 layout(location = N) 显式绑定 0–4；静态 Mesh 的 draw() 保持只绑定
// 0–2。所有 GL 调用在 #ifdef OHOS_PLATFORM 内，非平台侧为空操作。

#include "native/engine/render/shader_3d.h"

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#include <hilog/log.h>

#define LOGI_3D(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan3D", __VA_ARGS__)
#define LOGE_3D(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan3D", __VA_ARGS__)
#endif

namespace {

// 设计规格 §3.5 的顶点着色器，改写为 GLES3 #version 300 es 语法：
// attribute -> in，varying -> out，使用 layout(location) 显式绑定属性槽位。
const char* kVertexShaderSrc =
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
    "out vec3 vNormal;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "  mat4 skin = uJoints[aJoints.x] * aWeights.x +\n"
    "              uJoints[aJoints.y] * aWeights.y +\n"
    "              uJoints[aJoints.z] * aWeights.z +\n"
    "              uJoints[aJoints.w] * aWeights.w;\n"
    "  vec4 localPosition = uSkinned ? skin * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "  vec3 localNormal = uSkinned ? mat3(skin) * aNormal : aNormal;\n"
    "  gl_Position = uMVP * localPosition;\n"
    "  vNormal = mat3(uModel) * localNormal;\n"
    "  vUV = aUV;\n"
    "}\n";

// 设计规格 §3.5 的片段着色器，改写为 GLES3 #version 300 es 语法：
// varying -> in，gl_FragColor -> 自定义 out，texture2D -> texture。
const char* kFragmentShaderSrc =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uTexture;\n"
    "uniform vec3 uLightDir;\n"
    "uniform vec3 uLightColor;\n"
    "uniform vec3 uAmbient;\n"
    "uniform bool uHasTexture;\n"
    "in vec3 vNormal;\n"
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec3 N = normalize(vNormal);\n"
    "  float diff = max(dot(N, normalize(uLightDir)), 0.0);\n"
    "  vec3 color = uAmbient + diff * uLightColor;\n"
    "  if (uHasTexture) {\n"
    "    fragColor = vec4(color, 1.0) * texture(uTexture, vUV);\n"
    "  } else {\n"
    "    fragColor = vec4(color, 1.0);\n"
    "  }\n"
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
  locSkinned_ = glGetUniformLocation(program_, "uSkinned");
  locJoints_ = glGetUniformLocation(program_, "uJoints");
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
    locSkinned_ = -1;
    locJoints_ = -1;
  }
#endif
  skinPaletteValid_ = false;
  skinningEnabled_ = false;
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

void Shader3D::setHasTexture(bool hasTexture) const {
#ifdef OHOS_PLATFORM
  if (locHasTexture_ != -1) {
    glUniform1i(locHasTexture_, hasTexture ? 1 : 0);
  }
#else
  (void)hasTexture;
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
