#include "surface.h"
#include "platform/harmony/fence_wait.h"
#include <hilog/log.h>
#include <unistd.h>
#include <cmath>
#include <cinttypes>
#include <vector>
#include <algorithm>
#include <cstring>

#ifdef OHOS_PLATFORM
#include <glm/gtc/matrix_transform.hpp>
#endif

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan", __VA_ARGS__)

// -----------------------------------------------------------------------------
// OpenGL ES shaders
// -----------------------------------------------------------------------------
static const char* kVertexShader300 =
  "#version 300 es\n"
  "in vec4 a_position;\n"
  "in vec4 a_color;\n"
  "out vec4 v_color;\n"
  "void main() {\n"
  "  gl_Position = a_position;\n"
  "  v_color = a_color;\n"
  "}\n";

static const char* kFragmentShader300 =
  "#version 300 es\n"
  "precision mediump float;\n"
  "in vec4 v_color;\n"
  "out vec4 fragColor;\n"
  "void main() {\n"
  "  fragColor = v_color;\n"
  "}\n";

static const char* kVertexShader100 =
  "attribute vec4 a_position;\n"
  "attribute vec4 a_color;\n"
  "varying vec4 v_color;\n"
  "void main() {\n"
  "  gl_Position = a_position;\n"
  "  v_color = a_color;\n"
  "}\n";

static const char* kFragmentShader100 =
  "precision mediump float;\n"
  "varying vec4 v_color;\n"
  "void main() {\n"
  "  gl_FragColor = v_color;\n"
  "}\n";

// -----------------------------------------------------------------------------
// Common math
// -----------------------------------------------------------------------------
static float aspect(const Surface& s) { return (float)s.height / (float)s.width; }
static float ndcX(float x) { return x * 2.0f - 1.0f; }
static float ndcY(float y) { return 1.0f - y * 2.0f; }
static Vec2 worldToNdc(const Surface& s, Vec2 world) {
  const Vec2 view = s.cameraRenderState.worldToView(world);
  return {ndcX(view.x), ndcY(view.y)};
}
static Vec2 cameraScale(const Surface& s) {
  return s.cameraRenderState.worldSizeToView({1.0f, 1.0f});
}

// -----------------------------------------------------------------------------
// OpenGL ES pipeline
// -----------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  if (!shader) return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buf[512];
    glGetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
    LOGE("Shader compile failed: %{public}s", buf);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

static bool createProgram(Surface& s) {
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  LOGI("GL_VERSION starts with: %{public}d", version ? version[0] : 0);

  GLuint vs = 0, fs = 0;
  LOGI("Trying ES 3.0 shaders");
  vs = compileShader(GL_VERTEX_SHADER, kVertexShader300);
  if (!vs) LOGI("ES 3.0 vertex shader compile failed");
  fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader300);
  if (!fs) LOGI("ES 3.0 fragment shader compile failed");
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    LOGI("Falling back to ES 2.0 shaders");
    vs = compileShader(GL_VERTEX_SHADER, kVertexShader100);
    fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader100);
  }
  if (!vs || !fs) {
    LOGE("Both shader versions failed");
    return false;
  }

  s.program = glCreateProgram();
  if (!s.program) {
    LOGE("glCreateProgram failed");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return false;
  }
  glAttachShader(s.program, vs);
  glAttachShader(s.program, fs);
  glLinkProgram(s.program);
  GLint linked = 0;
  glGetProgramiv(s.program, GL_LINK_STATUS, &linked);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!linked) {
    char buf[512];
    glGetProgramInfoLog(s.program, sizeof(buf), nullptr, buf);
    LOGE("Program link failed: %{public}s", buf);
    glDeleteProgram(s.program);
    s.program = 0;
    return false;
  }
  s.locPosition = glGetAttribLocation(s.program, "a_position");
  s.locColor = glGetAttribLocation(s.program, "a_color");
  LOGI("Program linked: pos=%{public}d color=%{public}d", s.locPosition, s.locColor);
  return true;
}

static void fillColor(std::vector<float>& colors, int count, float r, float g, float b, float a) {
  for (int i = 0; i < count; ++i) {
    colors.push_back(r);
    colors.push_back(g);
    colors.push_back(b);
    colors.push_back(a);
  }
}

static void drawArraysGL(const Surface& s, GLenum mode, const std::vector<float>& verts, const std::vector<float>& colors) {
  if (verts.empty() || s.program == 0 || s.locPosition < 0 || s.locColor < 0) return;
  glVertexAttribPointer(s.locPosition, 2, GL_FLOAT, GL_FALSE, 0, verts.data());
  glEnableVertexAttribArray(s.locPosition);
  glVertexAttribPointer(s.locColor, 4, GL_FLOAT, GL_FALSE, 0, colors.data());
  glEnableVertexAttribArray(s.locColor);
  glDrawArrays(mode, 0, static_cast<GLsizei>(verts.size() / 2));
  glDisableVertexAttribArray(s.locPosition);
  glDisableVertexAttribArray(s.locColor);
}

static void drawSolidRectGL(const Surface& s, float x, float y, float w, float h, float r, float g, float b, float a) {
  std::vector<float> verts = { x - w, y - h, x + w, y - h, x - w, y + h, x + w, y + h };
  std::vector<float> colors;
  fillColor(colors, 4, r, g, b, a);
  drawArraysGL(s, GL_TRIANGLE_STRIP, verts, colors);
}

static void drawSolidEllipseGL(const Surface& s, float cx, float cy,
                               Vec2 radii, int segs, float r, float g,
                               float b, float a) {
  std::vector<float> verts;
  verts.push_back(cx);
  verts.push_back(cy);
  for (int i = 0; i <= segs; ++i) {
    float theta = (float)i / (float)segs * 6.283185f;
    verts.push_back(cx + std::cos(theta) * radii.x);
    verts.push_back(cy + std::sin(theta) * radii.y);
  }
  std::vector<float> colors;
  fillColor(colors, verts.size() / 2, r, g, b, a);
  drawArraysGL(s, GL_TRIANGLE_FAN, verts, colors);
}

static void drawGridGL(const Surface& s) {
  std::vector<float> verts;
  const int lines = 10;
  const float step = 1.0f / lines;
  const auto appendLine = [&s, &verts](Vec2 start, Vec2 end) {
    const Vec2 viewStart = worldToNdc(s, start);
    const Vec2 viewEnd = worldToNdc(s, end);
    verts.push_back(viewStart.x); verts.push_back(viewStart.y);
    verts.push_back(viewEnd.x); verts.push_back(viewEnd.y);
  };
  for (int i = 0; i <= lines; ++i) {
    float p = i * step;
    appendLine({p, 0.0f}, {p, 1.0f});
    appendLine({0.0f, p}, {1.0f, p});
  }
  std::vector<float> colors;
  fillColor(colors, verts.size() / 2, 0.18f, 0.22f, 0.35f, 1.0f);
  drawArraysGL(s, GL_LINES, verts, colors);

  std::vector<float> border;
  const auto appendBorder = [&s, &border](Vec2 start, Vec2 end) {
    const Vec2 viewStart = worldToNdc(s, start);
    const Vec2 viewEnd = worldToNdc(s, end);
    border.push_back(viewStart.x); border.push_back(viewStart.y);
    border.push_back(viewEnd.x); border.push_back(viewEnd.y);
  };
  appendBorder({0.0f, 0.0f}, {1.0f, 0.0f});
  appendBorder({1.0f, 0.0f}, {1.0f, 1.0f});
  appendBorder({1.0f, 1.0f}, {0.0f, 1.0f});
  appendBorder({0.0f, 1.0f}, {0.0f, 0.0f});
  std::vector<float> borderColors;
  fillColor(borderColors, 8, 0.25f, 0.30f, 0.45f, 1.0f);
  drawArraysGL(s, GL_LINES, border, borderColors);
}

static void drawPropsGL(const Surface& s) {
  // Props remain projected world geometry, including pitch-dependent height.
  const float asp = aspect(s);
  const Vec2 scale = cameraScale(s);
  for (const auto& p : s.props) {
    const Vec2 view = worldToNdc(s, {p.x, p.y});
    float x = view.x;
    float y = view.y;
    float r = p.size * asp * scale.x;
    float rh = p.size * scale.y;
    if (p.kind == 0) {
      drawSolidRectGL(s, x, y + rh * 0.3f, r * 0.25f, rh * 0.4f, 0.45f, 0.30f, 0.18f, 1.0f);
      drawSolidEllipseGL(s, x, y - rh * 0.2f, {r * 0.6f, rh * 0.6f}, 16, 0.15f, 0.55f, 0.25f, 1.0f);
      drawSolidEllipseGL(s, x, y - rh * 0.45f, {r * 0.4f, rh * 0.4f}, 14, 0.20f, 0.65f, 0.30f, 1.0f);
    } else {
      drawSolidEllipseGL(s, x, y, {r * 0.55f, rh * 0.55f}, 12, 0.42f, 0.42f, 0.46f, 1.0f);
      drawSolidEllipseGL(s, x - r * 0.3f, y + rh * 0.1f, {r * 0.35f, rh * 0.35f}, 10, 0.50f, 0.50f, 0.54f, 1.0f);
    }
  }
}

static void drawParticlesGL(const Surface& s) {
  // Particles and the player are screen-facing billboards in both pipelines.
  const float asp = aspect(s);
  for (const auto& p : s.particles) {
    float a = p.life / p.maxLife;
    const Vec2 view = worldToNdc(s, {p.x, p.y});
    float x = view.x;
    float y = view.y;
    const Vec2 radii =
        s.cameraRenderState.billboardNdcRadii(0.012f * a, asp);
    drawSolidEllipseGL(s, x, y, radii, 10, 0.9f, 0.9f, 1.0f,
                       a * 0.7f);
  }
}

static void drawPlayerGL(const Surface& s) {
  const float asp = aspect(s);
  const Vec2 view = worldToNdc(s, {s.player.x, s.player.y});
  float x = view.x;
  float y = view.y;
  const Vec2 radii =
      s.cameraRenderState.billboardNdcRadii(s.player.size, asp);
  drawSolidEllipseGL(s, x, y - radii.y * 0.1f, radii * 1.1f, 20,
                     0.0f, 0.0f, 0.0f, 0.35f);
  drawSolidEllipseGL(s, x, y, radii, 24, 0.18f, 0.65f, 0.95f, 1.0f);
  drawSolidEllipseGL(s, x, y, radii * 0.75f, 20, 0.25f, 0.75f, 1.0f,
                     1.0f);
  const Vec2 worldFacing{std::cos(s.player.angle), std::sin(s.player.angle)};
  const Vec2 viewFacing =
      s.cameraRenderState.worldVectorToView(worldFacing);
  const float viewAngle = std::atan2(viewFacing.y, viewFacing.x);
  float ax = x + std::cos(viewAngle) * radii.x * 0.6f;
  float ay = y - std::sin(viewAngle) * radii.y * 0.6f;
  drawSolidEllipseGL(s, ax, ay, radii * 0.28f, 12, 1.0f, 1.0f, 1.0f,
                     0.95f);
  drawSolidEllipseGL(s, ax, ay, radii * 0.14f, 8, 0.95f, 0.35f, 0.35f,
                     1.0f);
}

static void drawTrainingTargetGL(const Surface& s) {
  if (!s.trainingTarget.alive) return;
  const Vec2 view = worldToNdc(s, {s.trainingTarget.x, s.trainingTarget.y});
  const Vec2 radii = s.cameraRenderState.billboardNdcRadii(
      s.trainingTarget.size, aspect(s));
  drawSolidEllipseGL(s, view.x, view.y, radii, 20, 0.85f, 0.32f, 0.22f, 1.0f);
}

// -----------------------------------------------------------------------------
// 3D 渲染阶段（M3-1）
// -----------------------------------------------------------------------------
// 2D 位置 (x, y)（0-1 范围）映射到 3D 世界坐标 (x, 0, y)，角色立方体半高
// 贴地放置。3D 着色器无独立 base color uniform，因此通过 setLight 把每个
// 实体的基色写入 ambient 与 lightColor（ambient = base*0.3，lightColor = base*0.7），
// 既保留方向光照的明暗变化，又实现按实体/阶段配色，且不修改 Task 3 的着色器。
#ifdef OHOS_PLATFORM
static glm::vec3 enemyColorByArchetype(int archetype) {
  switch (archetype) {
    case 1:  // Priest
      return {0.70f, 0.60f, 0.30f};
    case 2:  // Guard
      return {0.40f, 0.42f, 0.52f};
    case 0:  // RiftClaw
    default:
      return {0.60f, 0.30f, 0.20f};
  }
}

static glm::vec3 bossColorByPhase(int phase) {
  switch (phase) {
    case 2:  // CurrentStorm
      return {0.30f, 0.60f, 0.95f};
    case 3:  // CorruptionCollapse
      return {0.62f, 0.22f, 0.62f};
    case 1:  // RadianceLockdown
    default:
      return {0.92f, 0.80f, 0.32f};
  }
}

static void applyEntityTint(const Surface& s, const glm::vec3& base) {
  // ambient = base*0.3 保证背光面仍有基色可见，lightColor = base*0.7 让受光面
  // 保留基色并随方向光产生明暗。lightDir 保持场景统一方向。
  s.shader3d.setLight(s.lightDir, base * 0.7f, base * 0.3f);
}

static void drawMeshAt(Surface& s, const Mesh& mesh,
                       const glm::mat4& vp, const glm::vec3& position,
                       float scale, const glm::vec3& base) {
  // 单位网格（createCube(1.0)/createPlane）经 translate+scale 落到世界坐标，
  // 立方体底面贴 y=0：translate.y = scale*0.5。
  glm::mat4 model = glm::translate(glm::mat4(1.0f), position) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(scale));
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(mesh.texture != 0u);
  applyEntityTint(s, base);
  mesh.draw();
}

static glm::mat4 actorModelMatrix(const glm::vec3& position, float scale,
                                  float yaw = 0.0f) {
  return glm::translate(glm::mat4(1.0f), position) *
         glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
         glm::scale(glm::mat4(1.0f), glm::vec3(scale));
}

static void drawActor(Surface& s, SkinnedModel& model, const Mesh& fallback,
                      SkinnedAnimationState& animationState,
                      const ActorRenderState& actor, const glm::mat4& matrix,
                      const glm::mat4& vp, const glm::vec3& base) {
  s.shader3d.setMVP(vp * matrix);
  s.shader3d.setModel(matrix);
  applyEntityTint(s, base);

  if (model.ready()) {
    s.shader3d.setSkinPalette(
        model.update(animationState, actor, 1.0f / 60.0f));
#ifdef OHOS_PLATFORM
    LOGI("drawActor clip=%{public}s attacking=%{public}d moving=%{public}d",
         RenderAnimationName(ChooseAnimation(actor)),
         static_cast<int>(actor.attacking),
         static_cast<int>(actor.moving));
#endif
    s.shader3d.setSkinned(true);
    if (s.shader3d.skinningEnabled()) {
      model.draw(s.shader3d);
      return;
    }
  }

  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(fallback.texture != 0u);
  // 静态 Mesh 没有死亡姿态；死亡实体保持隐藏，而可用的骨骼模型可播放 death。
  if (actor.alive) fallback.draw();
}

static bool takePendingModelAsset(Surface& s, ModelKind kind,
                                  std::vector<uint8_t>& bytes) {
  std::lock_guard<std::mutex> lock(s.modelAssetMutex);
  PendingModelAsset* asset = nullptr;
  switch (kind) {
    case ModelKind::Player:
      asset = &s.playerModelAsset;
      break;
    case ModelKind::Enemy:
      asset = &s.enemyModelAsset;
      break;
    case ModelKind::Boss:
      asset = &s.bossModelAsset;
      break;
  }
  return asset != nullptr && asset->take(bytes);
}

static void tryInitializeModelAsset(Surface& s, ModelKind kind,
                                    SkinnedModel& model,
                                    const char* assetName) {
  std::vector<uint8_t> bytes;
  if (!takePendingModelAsset(s, kind, bytes)) return;

  // 替换和清空都必须先在 current context 下释放旧 GPU 资源。
  model.destroy();
  if (bytes.empty()) {
    LOGI("%{public}s cleared; static Mesh fallback remains active", assetName);
    return;
  }
  if (!model.tryInitialize(bytes, assetName)) {
    LOGE("%{public}s; static Mesh fallback remains active",
         model.lastError().c_str());
  }
}

static void tryInitializePendingModelAssets(Surface& s) {
  tryInitializeModelAsset(s, ModelKind::Player, s.playerModel, "player.glb");
  tryInitializeModelAsset(s, ModelKind::Enemy, s.enemyModel, "enemy.glb");
  tryInitializeModelAsset(s, ModelKind::Boss, s.bossModel, "boss.glb");
}

static void draw3DPhase(Surface& s) {
  // bridge 可能晚于 Surface 创建；surface_draw 已成功 makeCurrent，因此只在这里
  // 消费一次标脏字节，解析失败后保持静态 Mesh，不在每帧反复尝试。
  tryInitializePendingModelAssets(s);
  if (!s.shader3dReady || s.shader3d.program() == 0u) return;

  // 3D 阶段需要深度测试；2D 阶段未写深度，故在此单独清深度并开启深度测试，
  // 绘制结束后关闭，避免影响下一帧 2D 绘制。
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  s.shader3d.use();
  s.shader3d.setHasTexture(false);

  if (s.width > 0 && s.height > 0) {
    s.camera3d.aspectRatio =
        static_cast<float>(s.width) / static_cast<float>(s.height);
  }

  const glm::mat4 vp = s.camera3d.projectionMatrix() * s.camera3d.viewMatrix();

  // 地面：大平面覆盖可玩区域，中心放在 (0.5, 0, 0.5)。
  drawMeshAt(s, s.groundMesh, vp, glm::vec3(0.5f, 0.0f, 0.5f), 3.0f,
             {0.30f, 0.32f, 0.36f});

  // M3-1 地面索引按双面占位使用；角色模型阶段启用背面剔除。
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // 玩家：模型可用时走蒙皮，否则保留 M3-1 立方体。
  drawActor(s, s.playerModel, s.playerMesh, s.playerAnimationState,
            s.player3dAnimation,
            actorModelMatrix(glm::vec3(s.player.x, 0.012f, s.player.y), 0.025f,
                             s.player.angle),
            vp, {0.18f, 0.65f, 0.95f});

  // 训练假人立方体（按 alive 跳过）。
  drawActor(s, s.enemyModel, s.enemyMesh, s.trainingTargetAnimationState,
            s.trainingTarget3dAnimation,
            actorModelMatrix(
                glm::vec3(s.trainingTarget.x, 0.011f, s.trainingTarget.y),
                0.022f),
            vp, {0.85f, 0.32f, 0.22f});

  // 敌人立方体（按存活状态跳过）。
  s.pruneEnemyAnimationStates();
  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    SkinnedAnimationState& animationState = s.enemyAnimationStates[enemy.id];
    drawActor(s, s.enemyModel, s.enemyMesh, animationState, enemy.animation,
              actorModelMatrix(glm::vec3(enemy.x, 0.011f, enemy.y), 0.022f,
                               enemy.angle),
              vp, enemyColorByArchetype(enemy.archetype));
  }

  // 首领立方体（按阶段配色，击败后跳过）。
  if (s.boss3d.active) {
    drawActor(s, s.bossModel, s.bossMesh, s.bossAnimationState,
              s.boss3d.animation,
              actorModelMatrix(glm::vec3(s.boss3d.x, 0.02f, s.boss3d.y),
                               0.04f, s.boss3d.angle),
              vp, bossColorByPhase(s.boss3d.phase));
  }

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
}
#endif  // OHOS_PLATFORM

// -----------------------------------------------------------------------------
// Software rasterizer fallback (used when OpenGL ES is unavailable on simulators)
// -----------------------------------------------------------------------------
struct Canvas {
  uint32_t* pixels = nullptr;
  int32_t width = 0;
  int32_t height = 0;
  int32_t stride = 0;
  bool swapRedBlue = false;
};

static uint32_t packColor(float r, float g, float b, float a, bool swapRedBlue) {
  auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
  uint32_t R = static_cast<uint32_t>(clampF(r) * 255.0f);
  uint32_t G = static_cast<uint32_t>(clampF(g) * 255.0f);
  uint32_t B = static_cast<uint32_t>(clampF(b) * 255.0f);
  uint32_t A = static_cast<uint32_t>(clampF(a) * 255.0f);
  if (swapRedBlue) std::swap(R, B);
  return (A << 24) | (B << 16) | (G << 8) | R;
}

static void blendPixel(Canvas& c, int x, int y, uint32_t src) {
  if (x < 0 || y < 0 || x >= c.width || y >= c.height) return;
  uint8_t* row = reinterpret_cast<uint8_t*>(c.pixels) + y * c.stride;
  uint32_t* dst = reinterpret_cast<uint32_t*>(row) + x;
  uint8_t sa = src >> 24;
  if (sa == 0) return;
  if (sa == 255) { *dst = src; return; }
  uint8_t da = 255 - sa;
  uint32_t d = *dst;
  uint8_t r = ((src & 0xFF) * sa + (d & 0xFF) * da) / 255;
  uint8_t g = (((src >> 8) & 0xFF) * sa + ((d >> 8) & 0xFF) * da) / 255;
  uint8_t b = (((src >> 16) & 0xFF) * sa + ((d >> 16) & 0xFF) * da) / 255;
  uint8_t a = sa + ((d >> 24) * da) / 255;
  *dst = (a << 24) | (b << 16) | (g << 8) | r;
}

static void clearCanvas(Canvas& c, uint32_t color) {
  for (int y = 0; y < c.height; ++y) {
    uint8_t* row = reinterpret_cast<uint8_t*>(c.pixels) + y * c.stride;
    uint32_t* dst = reinterpret_cast<uint32_t*>(row);
    for (int x = 0; x < c.width; ++x) dst[x] = color;
  }
}

static void drawLine(Canvas& c, int x0, int y0, int x1, int y1, uint32_t color) {
  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  while (true) {
    blendPixel(c, x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx) { err += dx; y0 += sy; }
  }
}

static void drawRect(Canvas& c, int x, int y, int w, int h, uint32_t color) {
  int x0 = std::max(0, x);
  int y0 = std::max(0, y);
  int x1 = std::min(c.width, x + w);
  int y1 = std::min(c.height, y + h);
  for (int py = y0; py < y1; ++py) {
    uint8_t* row = reinterpret_cast<uint8_t*>(c.pixels) + py * c.stride;
    uint32_t* dst = reinterpret_cast<uint32_t*>(row);
    for (int px = x0; px < x1; ++px) dst[px] = color;
  }
}

static void drawSolidEllipse(Canvas& c, int cx, int cy, int rx, int ry, uint32_t color) {
  if (rx <= 0 || ry <= 0) return;
  int x0 = std::max(0, cx - rx);
  int y0 = std::max(0, cy - ry);
  int x1 = std::min(c.width, cx + rx + 1);
  int y1 = std::min(c.height, cy + ry + 1);
  for (int y = y0; y < y1; ++y) {
    int dy = y - cy;
    float dyNorm = (float)dy / ry;
    float dyNorm2 = dyNorm * dyNorm;
    uint8_t* row = reinterpret_cast<uint8_t*>(c.pixels) + y * c.stride;
    uint32_t* dst = reinterpret_cast<uint32_t*>(row);
    for (int x = x0; x < x1; ++x) {
      int dx = x - cx;
      float dxNorm = (float)dx / rx;
      if (dxNorm * dxNorm + dyNorm2 <= 1.0f) dst[x] = color;
    }
  }
}

static int ndcToScreenX(const Surface& s, float x) {
  return static_cast<int>((x + 1.0f) * 0.5f * (s.width - 1));
}
static int ndcToScreenY(const Surface& s, float y) {
  return static_cast<int>((1.0f - y) * 0.5f * (s.height - 1));
}
static int ndcToPixelRadiusX(const Surface& s, float r) {
  return static_cast<int>(r * 0.5f * (s.width - 1));
}
static int ndcToPixelRadiusY(const Surface& s, float r) {
  return static_cast<int>(r * 0.5f * (s.height - 1));
}

static void drawSolidRectSW(const Surface& s, Canvas& c, float x, float y, float w, float h, float r, float g, float b, float a) {
  int cx = ndcToScreenX(s, x);
  int cy = ndcToScreenY(s, y);
  int rw = ndcToPixelRadiusX(s, w);
  int rh = ndcToPixelRadiusY(s, h);
  drawRect(c, cx - rw, cy - rh, rw * 2 + 1, rh * 2 + 1, packColor(r, g, b, a, c.swapRedBlue));
}

static void drawSolidEllipseSW(const Surface& s, Canvas& c, float cx, float cy,
                               Vec2 radii, float r, float g, float b,
                               float a) {
  int scx = ndcToScreenX(s, cx);
  int scy = ndcToScreenY(s, cy);
  int srx = ndcToPixelRadiusX(s, radii.x);
  int sry = ndcToPixelRadiusY(s, radii.y);
  drawSolidEllipse(c, scx, scy, srx, sry, packColor(r, g, b, a, c.swapRedBlue));
}

static void drawGridSW(const Surface& s, Canvas& c) {
  const uint32_t gridColor = packColor(0.18f, 0.22f, 0.35f, 1.0f, c.swapRedBlue);
  const uint32_t borderColor = packColor(0.25f, 0.30f, 0.45f, 1.0f, c.swapRedBlue);
  const int lines = 10;
  const auto drawWorldLine = [&s, &c](Vec2 start, Vec2 end, uint32_t color) {
    const Vec2 viewStart = worldToNdc(s, start);
    const Vec2 viewEnd = worldToNdc(s, end);
    drawLine(c, ndcToScreenX(s, viewStart.x), ndcToScreenY(s, viewStart.y),
             ndcToScreenX(s, viewEnd.x), ndcToScreenY(s, viewEnd.y), color);
  };
  for (int i = 0; i <= lines; ++i) {
    float p = (float)i / lines;
    drawWorldLine({p, 0.0f}, {p, 1.0f}, gridColor);
    drawWorldLine({0.0f, p}, {1.0f, p}, gridColor);
  }
  drawWorldLine({0.0f, 0.0f}, {1.0f, 0.0f}, borderColor);
  drawWorldLine({1.0f, 0.0f}, {1.0f, 1.0f}, borderColor);
  drawWorldLine({1.0f, 1.0f}, {0.0f, 1.0f}, borderColor);
  drawWorldLine({0.0f, 1.0f}, {0.0f, 0.0f}, borderColor);
}

static void drawPropsSW(const Surface& s, Canvas& c) {
  // Keep the same projected world-geometry radii used by the GL path.
  const float asp = aspect(s);
  const Vec2 scale = cameraScale(s);
  for (const auto& p : s.props) {
    const Vec2 view = worldToNdc(s, {p.x, p.y});
    float x = view.x;
    float y = view.y;
    float r = p.size * asp * scale.x;
    float rh = p.size * scale.y;
    if (p.kind == 0) {
      drawSolidRectSW(s, c, x, y + rh * 0.3f, r * 0.25f, rh * 0.4f, 0.45f, 0.30f, 0.18f, 1.0f);
      drawSolidEllipseSW(s, c, x, y - rh * 0.2f, {r * 0.6f, rh * 0.6f}, 0.15f, 0.55f, 0.25f, 1.0f);
      drawSolidEllipseSW(s, c, x, y - rh * 0.45f, {r * 0.4f, rh * 0.4f}, 0.20f, 0.65f, 0.30f, 1.0f);
    } else {
      drawSolidEllipseSW(s, c, x, y, {r * 0.55f, rh * 0.55f}, 0.42f, 0.42f, 0.46f, 1.0f);
      drawSolidEllipseSW(s, c, x - r * 0.3f, y + rh * 0.1f, {r * 0.35f, rh * 0.35f}, 0.50f, 0.50f, 0.54f, 1.0f);
    }
  }
}

static void drawParticlesSW(const Surface& s, Canvas& c) {
  // Billboard radii are shared with GL and map to circular pixel geometry.
  const float asp = aspect(s);
  for (const auto& p : s.particles) {
    float a = p.life / p.maxLife;
    const Vec2 view = worldToNdc(s, {p.x, p.y});
    float x = view.x;
    float y = view.y;
    const Vec2 radii =
        s.cameraRenderState.billboardNdcRadii(0.012f * a, asp);
    drawSolidEllipseSW(s, c, x, y, radii, 0.9f, 0.9f, 1.0f,
                       a * 0.7f);
  }
}

static void drawPlayerSW(const Surface& s, Canvas& c) {
  const float asp = aspect(s);
  const Vec2 view = worldToNdc(s, {s.player.x, s.player.y});
  float x = view.x;
  float y = view.y;
  const Vec2 radii =
      s.cameraRenderState.billboardNdcRadii(s.player.size, asp);
  drawSolidEllipseSW(s, c, x, y - radii.y * 0.1f, radii * 1.1f, 0.0f,
                     0.0f, 0.0f, 0.35f);
  drawSolidEllipseSW(s, c, x, y, radii, 0.18f, 0.65f, 0.95f, 1.0f);
  drawSolidEllipseSW(s, c, x, y, radii * 0.75f, 0.25f, 0.75f, 1.0f,
                     1.0f);
  const Vec2 worldFacing{std::cos(s.player.angle), std::sin(s.player.angle)};
  const Vec2 viewFacing =
      s.cameraRenderState.worldVectorToView(worldFacing);
  const float viewAngle = std::atan2(viewFacing.y, viewFacing.x);
  float ax = x + std::cos(viewAngle) * radii.x * 0.6f;
  float ay = y - std::sin(viewAngle) * radii.y * 0.6f;
  drawSolidEllipseSW(s, c, ax, ay, radii * 0.28f, 1.0f, 1.0f, 1.0f,
                     0.95f);
  drawSolidEllipseSW(s, c, ax, ay, radii * 0.14f, 0.95f, 0.35f, 0.35f,
                     1.0f);
}

static void drawTrainingTargetSW(const Surface& s, Canvas& c) {
  if (!s.trainingTarget.alive) return;
  const Vec2 view = worldToNdc(s, {s.trainingTarget.x, s.trainingTarget.y});
  const Vec2 radii = s.cameraRenderState.billboardNdcRadii(
      s.trainingTarget.size, aspect(s));
  drawSolidEllipseSW(s, c, view.x, view.y, radii, 0.85f, 0.32f, 0.22f, 1.0f);
}

static void softwareDrawFrame(Surface& s) {
  std::lock_guard<std::mutex> lock(s.windowMutex);
  if (!s.ready || !s.window) return;

  OHNativeWindowBuffer* windowBuffer = nullptr;
  int fenceFd = -1;
  int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(s.window, &windowBuffer, &fenceFd);
  if (ret != 0) {
    LOGE("RequestBuffer failed: %{public}d", ret);
    if (fenceFd >= 0) close(fenceFd);
    return;
  }
  if (!waitAndCloseFence(fenceFd, 3000)) {
    LOGE("Wait buffer fence timed out or failed");
    OH_NativeWindow_NativeWindowAbortBuffer(s.window, windowBuffer);
    return;
  }
  fenceFd = -1;

  OH_NativeBuffer* nativeBuffer = nullptr;
  ret = OH_NativeBuffer_FromNativeWindowBuffer(windowBuffer, &nativeBuffer);
  if (ret != 0) {
    LOGE("FromNativeWindowBuffer failed: %{public}d", ret);
    OH_NativeWindow_NativeWindowAbortBuffer(s.window, windowBuffer);
    return;
  }

  OH_NativeBuffer_Config config;
  void* addr = nullptr;
  ret = OH_NativeBuffer_MapAndGetConfig(nativeBuffer, &addr, &config);
  if (ret != 0 || !addr) {
    LOGE("MapAndGetConfig failed: %{public}d", ret);
    OH_NativeBuffer_Unreference(nativeBuffer);
    OH_NativeWindow_NativeWindowAbortBuffer(s.window, windowBuffer);
    return;
  }

  static int frameCount = 0;
  frameCount++;
  if (frameCount <= 5 || frameCount % 60 == 0) {
    LOGI("SW frame %{public}d: buffer %{public}d x %{public}d format=%{public}d stride=%{public}d",
         frameCount, config.width, config.height, config.format, config.stride);
  }

  s.stride = config.stride;
  s.bufferFormat = config.format;
  Canvas c;
  c.pixels = static_cast<uint32_t*>(addr);
  c.width = config.width;
  c.height = config.height;
  c.stride = config.stride;
  c.swapRedBlue = (config.format == NATIVEBUFFER_PIXEL_FMT_BGRA_8888);

  clearCanvas(c, packColor(0.06f, 0.08f, 0.14f, 1.0f, c.swapRedBlue));
  drawGridSW(s, c);
  drawPropsSW(s, c);
  drawTrainingTargetSW(s, c);
  drawParticlesSW(s, c);
  drawPlayerSW(s, c);

  OH_NativeBuffer_Unmap(nativeBuffer);

  Region::Rect rect = {0, 0, (uint32_t)config.width, (uint32_t)config.height};
  Region region = {&rect, 1};
  ret = OH_NativeWindow_NativeWindowFlushBuffer(s.window, windowBuffer, -1, region);
  if (ret != 0) {
    LOGE("FlushBuffer failed: %{public}d", ret);
    OH_NativeWindow_NativeWindowAbortBuffer(s.window, windowBuffer);
  }
  OH_NativeBuffer_Unreference(nativeBuffer);
}

// -----------------------------------------------------------------------------
// World generation
// -----------------------------------------------------------------------------
static void generateWorld(Surface& s) {
  s.props.clear();
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.1f, 0.9f);
  std::uniform_int_distribution<int> kind(0, 1);
  for (int i = 0; i < 18; ++i) {
    float px = dist(rng);
    float py = dist(rng);
    if (std::abs(px - 0.5f) < 0.15f && std::abs(py - 0.5f) < 0.15f) continue;
    Prop p;
    p.x = px;
    p.y = py;
    p.size = 0.04f + (rng() / (float)rng.max()) * 0.03f;
    p.kind = kind(rng);
    s.props.push_back(p);
  }
}

// -----------------------------------------------------------------------------
// Surface lifecycle
// -----------------------------------------------------------------------------
static bool testGLFunctionality() {
  GLuint testShader = glCreateShader(GL_VERTEX_SHADER);
  if (!testShader) {
    LOGE("glCreateShader not functional");
    return false;
  }
  static const char* testSource =
    "attribute vec4 a_position;\n"
    "void main() { gl_Position = a_position; }\n";
  glShaderSource(testShader, 1, &testSource, nullptr);
  glCompileShader(testShader);
  GLint compiled = 0;
  glGetShaderiv(testShader, GL_COMPILE_STATUS, &compiled);
  glDeleteShader(testShader);
  if (!compiled) {
    LOGE("Test shader compile failed");
    return false;
  }
  return true;
}

// 创建 3D 渲染层资源（网格 VBO/IBO 与 3D 着色器 Program）。必须在 GL 上下文
// current 时调用，因此放在 tryInitGL 成功路径内、eglMakeCurrent 解绑之前。
static void init3DResources(Surface& s) {
#ifdef OHOS_PLATFORM
  // 使用单位网格（size=1.0），通过 model 矩阵在 draw3DPhase 中缩放定位，
  // 避免为每种实体单独生成不同尺寸的几何体。
  s.playerMesh = createCube(1.0f);
  s.groundMesh = createPlane(1.0f, 1.0f);
  s.enemyMesh = createCube(1.0f);
  s.bossMesh = createCube(1.0f);
  s.playerMesh.upload();
  s.groundMesh.upload();
  s.enemyMesh.upload();
  s.bossMesh.upload();
  s.shader3dReady = s.shader3d.init();
  if (!s.shader3dReady) {
    LOGE("3D shader init failed, 3D phase will be skipped");
  } else {
    LOGI("3D resources ready: shader=%{public}u", s.shader3d.program());
  }
  tryInitializePendingModelAssets(s);
#else
  (void)s;
#endif
}

// 释放一个 3D 渲染层资源组。必须在 GL 上下文 current 时调用。
static void destroy3DResource(Surface& s, SurfaceGlResource resource) {
#ifdef OHOS_PLATFORM
  switch (resource) {
    case SurfaceGlResource::SkinnedModels:
      // SkinnedModel 可能拥有 VBO/IBO/纹理；必须先于共享 shader 销毁。
      s.playerModel.destroy();
      s.enemyModel.destroy();
      s.bossModel.destroy();
      break;
    case SurfaceGlResource::StaticMeshes:
      s.playerMesh.destroy();
      s.groundMesh.destroy();
      s.enemyMesh.destroy();
      s.bossMesh.destroy();
      break;
    case SurfaceGlResource::Shader3D:
      s.shader3d.destroy();
      s.shader3dReady = false;
      // EGL context 重建后 GPU 对象必须重传；已有 CPU 字节重新标脏。
      {
        std::lock_guard<std::mutex> lock(s.modelAssetMutex);
        s.playerModelAsset.markDirtyForContextRebuild();
        s.enemyModelAsset.markDirtyForContextRebuild();
        s.bossModelAsset.markDirtyForContextRebuild();
      }
      break;
    case SurfaceGlResource::Program2D:
      break;
  }
#else
  (void)s;
  (void)resource;
#endif
}

// EGL current 绑定失败时不能调用 destroy3DResource：其中包含 GL 删除。context
// 随后由 eglDestroyContext 回收实际驱动对象；这里只丢弃 CPU 中已经无效的句柄跟踪，
// 既不会跨 context 删除，也不会伪装成已逐项释放。
static void abandon3DResources(Surface& s) {
#ifdef OHOS_PLATFORM
  s.playerModel.abandonGpuResources();
  s.enemyModel.abandonGpuResources();
  s.bossModel.abandonGpuResources();
  s.playerMesh.abandonGpuResources();
  s.groundMesh.abandonGpuResources();
  s.enemyMesh.abandonGpuResources();
  s.bossMesh.abandonGpuResources();
  s.shader3d.abandonGpuResources();
  s.shader3dReady = false;
#else
  (void)s;
#endif
}

static void clearModelAssets(Surface& s) {
  std::lock_guard<std::mutex> lock(s.modelAssetMutex);
  s.playerModelAsset.clear();
  s.enemyModelAsset.clear();
  s.bossModelAsset.clear();
}

static bool tryInitGL(Surface& s) {
  s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (s.display == EGL_NO_DISPLAY) {
    LOGE("eglGetDisplay failed");
    return false;
  }
  EGLint major, minor;
  if (!eglInitialize(s.display, &major, &minor)) {
    LOGE("eglInitialize failed: %{public}d", eglGetError());
    s.display = EGL_NO_DISPLAY;
    return false;
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    LOGE("eglBindAPI OpenGL ES failed: %{public}d", eglGetError());
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    return false;
  }

  const EGLint attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 16,
    EGL_NONE
  };
  EGLint numConfigs;
  if (!eglChooseConfig(s.display, attribs, &s.config, 1, &numConfigs) || numConfigs < 1) {
    LOGE("eglChooseConfig ES3 failed: %{public}d", eglGetError());
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    return false;
  }

  s.surface = eglCreateWindowSurface(
      s.display, s.config, reinterpret_cast<EGLNativeWindowType>(s.window), nullptr);
  if (s.surface == EGL_NO_SURFACE) {
    LOGE("eglCreateWindowSurface failed: %{public}d", eglGetError());
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    return false;
  }
  s.glWindowCreated = true;

  const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  s.context = eglCreateContext(s.display, s.config, EGL_NO_CONTEXT, contextAttribs);
  if (s.context == EGL_NO_CONTEXT) {
    LOGE("eglCreateContext failed: %{public}d", eglGetError());
    eglDestroySurface(s.display, s.surface);
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    return false;
  }

  if (!eglMakeCurrent(s.display, s.surface, s.surface, s.context)) {
    LOGE("eglMakeCurrent window failed: %{public}d", eglGetError());
    eglDestroyContext(s.display, s.context);
    eglDestroySurface(s.display, s.surface);
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
    return false;
  }

  const GLubyte* version = glGetString(GL_VERSION);
  LOGI("EGL initialized: %{public}d.%{public}d", major, minor);
  LOGI("GL_VERSION: %{public}s", version ? reinterpret_cast<const char*>(version) : "null");
  if (!version || !testGLFunctionality()) {
    LOGE("OpenGL ES validation failed, glError=%{public}u", glGetError());
    eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(s.display, s.context);
    eglDestroySurface(s.display, s.surface);
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
    return false;
  }

  if (!createProgram(s)) {
    LOGE("createProgram failed");
    eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(s.display, s.surface);
    eglDestroyContext(s.display, s.context);
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
    return false;
  }
  generateWorld(s);
  init3DResources(s);

  eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  LOGI("EGL surface ready, props=%{public}zu", s.props.size());
  return true;
}

bool surface_init(Surface& s, OHNativeWindow* window) {
  if (!window) return false;

  if (OH_NativeWindow_NativeObjectReference(window) != 0) {
    LOGE("Failed to retain native window");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(s.windowMutex);
    if (s.window) {
      LOGE("Surface already owns a native window");
      OH_NativeWindow_NativeObjectUnreference(window);
      return false;
    }
    s.window = window;

    OH_NativeWindow_NativeWindowHandleOpt(window, GET_BUFFER_GEOMETRY, &s.height, &s.width);
    if (s.width <= 0) s.width = 1080;
    if (s.height <= 0) s.height = 1920;
    OH_NativeWindow_NativeWindowHandleOpt(window, SET_BUFFER_GEOMETRY, s.width, s.height);
  }
  LOGI("Surface init: %{public}d x %{public}d", s.width, s.height);

  if (tryInitGL(s)) {
    s.useSoftware = false;
    s.ready = true;
    return true;
  }

  // The HarmonyOS 6.1 emulator can crash inside ProducerSurface after several
  // CPU-buffer frames when its GLES runtime is unavailable. Do not enter the
  // NativeWindow software path after GL probing fails; keep ArkUI alive and let
  // the page present a recoverable unsupported-renderer state instead.
  LOGE("Native software rendering disabled because GLES initialization failed");
  {
    std::lock_guard<std::mutex> lock(s.windowMutex);
    if (s.window) {
      OH_NativeWindow_NativeObjectUnreference(s.window);
      s.window = nullptr;
    }
  }
  s.ready = false;
  s.useSoftware = false;
  return false;
}

bool surface_resize(Surface& s, OHNativeWindow* window) {
  if (!window) return false;
  std::lock_guard<std::mutex> lock(s.windowMutex);
  if (!s.ready || s.window != window) {
    LOGE("surface_resize rejected: ready=%{public}d sameWindow=%{public}d",
         static_cast<int>(s.ready), static_cast<int>(s.window == window));
    return false;
  }

  int32_t width = 0;
  int32_t height = 0;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(window, GET_BUFFER_GEOMETRY, &height, &width);
  if (ret != 0 || width <= 0 || height <= 0) {
    LOGE("GET_BUFFER_GEOMETRY failed: %{public}d, %{public}d x %{public}d", ret, width, height);
    return false;
  }
  ret = OH_NativeWindow_NativeWindowHandleOpt(window, SET_BUFFER_GEOMETRY, width, height);
  if (ret != 0) {
    LOGE("SET_BUFFER_GEOMETRY failed: %{public}d", ret);
    return false;
  }
  s.width = width;
  s.height = height;
  LOGI("Surface resized: %{public}d x %{public}d", width, height);
  return true;
}

void surface_draw(Surface& s) {
  if (!s.ready) return;

  if (s.useSoftware) {
    softwareDrawFrame(s);
    return;
  }

  if (!eglMakeCurrent(s.display, s.surface, s.surface, s.context)) {
    LOGE("surface_draw eglMakeCurrent failed: %{public}d", eglGetError());
    return;
  }

  glClearColor(0.06f, 0.08f, 0.14f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (s.program != 0) glUseProgram(s.program);

  drawGridGL(s);
  drawPropsGL(s);
  drawTrainingTargetGL(s);
  drawParticlesGL(s);
  drawPlayerGL(s);
#ifdef OHOS_PLATFORM
  draw3DPhase(s);
#endif
  glFlush();
}

void surface_swap(Surface& s) {
  if (!s.ready || s.useSoftware) return;
  if (!eglMakeCurrent(s.display, s.surface, s.surface, s.context)) {
    LOGE("surface_swap eglMakeCurrent failed: %{public}d", eglGetError());
    return;
  }
  eglSwapBuffers(s.display, s.surface);
}

void surface_destroy(Surface& s) {
  std::lock_guard<std::mutex> lock(s.windowMutex);
  if (!s.ready && !s.window) return;
  if (!s.useSoftware) {
    SurfaceDestroyOperations operations;
    operations.makeCurrent = [&s] {
      return eglMakeCurrent(s.display, s.surface, s.surface, s.context);
    };
    operations.destroyGlResource = [&s](SurfaceGlResource resource) {
      if (resource == SurfaceGlResource::Program2D) {
        if (s.program != 0) glDeleteProgram(s.program);
        s.program = 0;
        return;
      }
      destroy3DResource(s, resource);
    };
    operations.abandonGpuResources = [&s] {
      LOGE("surface_destroy eglMakeCurrent failed: %{public}d; skipping GL "
           "deletes and relying on eglDestroyContext", eglGetError());
      abandon3DResources(s);
      // 2D program 与 3D 资源同属即将销毁的 EGL context；清除 CPU 跟踪但不发 GL。
      s.program = 0;
    };
    operations.unbindCurrent = [&s] {
      eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    };
    operations.destroyEglSurface = [&s] {
      if (s.display != EGL_NO_DISPLAY && s.surface != EGL_NO_SURFACE) {
        eglDestroySurface(s.display, s.surface);
        s.surface = EGL_NO_SURFACE;
      }
    };
    operations.destroyEglContext = [&s] {
      if (s.display != EGL_NO_DISPLAY && s.context != EGL_NO_CONTEXT) {
        eglDestroyContext(s.display, s.context);
        s.context = EGL_NO_CONTEXT;
      }
    };
    operations.terminateEglDisplay = [&s] {
      if (s.display != EGL_NO_DISPLAY) eglTerminate(s.display);
      s.display = EGL_NO_DISPLAY;
    };
    ExecuteSurfaceDestroy(operations);
  }
  clearModelAssets(s);
  if (s.window) {
    OH_NativeWindow_NativeObjectUnreference(s.window);
    s.window = nullptr;
  }
  s.ready = false;
  s.useSoftware = false;
  s.glWindowCreated = false;
  s.props.clear();
  s.particles.clear();
  s.enemies3d.clear();
  s.enemyAnimationStates.clear();
  s.playerAnimationState.reset();
  s.trainingTargetAnimationState.reset();
  s.bossAnimationState.reset();
  LOGI("Surface destroyed");
}
