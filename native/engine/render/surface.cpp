#include "surface.h"
#include "native/engine/render/combat_vfx.h"
#include "native/engine/render/digit_atlas.h"
#include "native/engine/render/terrain_mesh.h"
#include "native/engine/world/stream_scheduler.h"
#include "platform/harmony/fence_wait.h"
#include <hilog/log.h>
#include <unistd.h>
#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstring>
#include <array>

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

static void drawGradientSkyGL(const Surface& s) {
  const std::vector<float> vertices = {
      -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
  // Bottom horizon #46515d, top #18243d.
  const std::vector<float> colors = {
      70.0f / 255.0f, 81.0f / 255.0f, 93.0f / 255.0f, 1.0f,
      70.0f / 255.0f, 81.0f / 255.0f, 93.0f / 255.0f, 1.0f,
      24.0f / 255.0f, 36.0f / 255.0f, 61.0f / 255.0f, 1.0f,
      24.0f / 255.0f, 36.0f / 255.0f, 61.0f / 255.0f, 1.0f};
  drawArraysGL(s, GL_TRIANGLE_STRIP, vertices, colors);
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
  const Vec2 worldFacing{std::sin(s.player.angle), std::cos(s.player.angle)};
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

static void drawVfxOverlayGL(const Surface& s) {
  if (s.program == 0) return;
  glUseProgram(s.program);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (s.vfxHitFlash > 0.0f) {
    drawSolidRectGL(s, 0.0f, 0.0f, 2.0f, 2.0f, 0.8f, 0.15f, 0.1f, s.vfxHitFlash * 0.3f);
  }
  if (s.vfxDodgeFlash > 0.0f) {
    drawSolidRectGL(s, 0.0f, 0.0f, 2.0f, 2.0f, 0.1f, 0.3f, 0.8f, s.vfxDodgeFlash * 0.25f);
  }
  if (s.vfxResonanceBurst > 0.0f) {
    drawSolidRectGL(s, 0.0f, 0.0f, 2.0f, 2.0f, 0.85f, 0.63f, 0.16f, s.vfxResonanceBurst * 0.2f);
  }
  // 低血量警示：血量低于 35% 时屏幕四边红色脉冲，越低越强烈。
  if (s.playerHpRatio < 0.35f) {
    const float phase = s.windupPulseSeconds / 0.8f * 6.2831853f;
    const float pulse = 0.5f + 0.5f * std::sin(phase);
    const float urgency = (0.35f - s.playerHpRatio) / 0.35f;
    const float alpha = urgency * (0.12f + 0.14f * pulse);
    constexpr float kEdge = 0.28f;
    drawSolidRectGL(s, 0.0f, 0.0f, kEdge, 2.0f, 0.75f, 0.08f, 0.05f, alpha);
    drawSolidRectGL(s, 2.0f - kEdge, 0.0f, kEdge, 2.0f, 0.75f, 0.08f, 0.05f,
                    alpha);
    drawSolidRectGL(s, 0.0f, 0.0f, 2.0f, kEdge, 0.75f, 0.08f, 0.05f, alpha);
    drawSolidRectGL(s, 0.0f, 2.0f - kEdge, 2.0f, kEdge, 0.75f, 0.08f, 0.05f,
                    alpha);
  }
  glDisable(GL_BLEND);
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
    case 3:  // Bruiser：暗红重装
      return {0.52f, 0.24f, 0.24f};
    case 4:  // Caster：青蓝法袍
      return {0.30f, 0.46f, 0.62f};
    case 5:  // Elite：紫黑精英
      return {0.46f, 0.28f, 0.52f};
    case 0:  // RiftClaw
    default:
      return {0.60f, 0.30f, 0.20f};
  }
}

// 野外敌人按原型缩放（第一版共用单 enemy.glb，体型差异靠缩放区分；
// 色调已由 enemyColorByArchetype 覆盖 0-5，留待美术出独立模型）。
static float enemyScaleByArchetype(int archetype) {
  return EnemyArchetypeScale(archetype);
}

static glm::vec3 bossColorByPhase(int phase) {
  switch (phase) {
    case 2:  // CurrentStorm
      return VisualTokens::sourceColor(SourceType::Current);
    case 3:  // CorruptionCollapse
      return VisualTokens::sourceColor(SourceType::Corruption);
    case 1:  // RadianceLockdown
    default:
      return VisualTokens::sourceColor(SourceType::Radiance);
  }
}

static glm::vec3 bossCoreColor(uint8_t sourceColor) {
  switch (sourceColor % 3u) {
    case 1:
      return VisualTokens::sourceColor(SourceType::Current);
    case 2:
      return VisualTokens::sourceColor(SourceType::Corruption);
    default:
      return VisualTokens::sourceColor(SourceType::Radiance);
  }
}

static void applyEntityTint(const Surface& s, const glm::vec3& base) {
  // ambient = base*0.3 保证背光面仍有基色可见，lightColor = base*0.7 让受光面
  // 保留基色并随方向光产生明暗。lightDir 保持场景统一方向。
  s.shader3d.setLight(s.lightDir, base * 0.7f, base * 0.3f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
}

// 受击闪白：按剩余闪白计时把基色向白色插值，给出“打中了”的即时反馈；
// timer<=0 时返回原色，与升级前等价。
static glm::vec3 hitFlashTint(const glm::vec3& base, float flashSeconds) {
  if (flashSeconds <= 0.0f) return base;
  const float factor = std::min(flashSeconds / 0.15f, 1.0f) * 0.7f;
  return base + (glm::vec3(1.0f) - base) * factor;
}

// 查询实体的剩余闪白时间（无记录返回 0）。
static float hitFlashRemaining(const Surface& s, uint32_t id) {
  const auto flash = s.enemyHitFlash.find(id);
  return flash != s.enemyHitFlash.end() ? flash->second : 0.0f;
}

// 地形贴合：采样逻辑层同一高度场取地面高度，让角色/阴影/预警环等
// 贴随地形起伏。未注入高度场时退化为平面世界（高度 0）。
static float groundYAt(const Surface& s, float x, float y) {
  if (s.terrain == nullptr) return 0.0f;
  return s.terrain->heightAt(x, y);
}

// 攻击前摇预警 pass：在前摇敌人/吟唱首领脚下绘制呼吸闪烁的红色警示环，
// 给玩家精确闪避/应对机制的时机窗口。深度只读 + 混合，先于角色绘制。
static void drawWindupWarnings(Surface& s, const glm::mat4& vp) {
  if (s.targetRingMesh.vbo == 0u) return;
  bool any = s.boss3d.active && !s.boss3d.defeated && s.boss3d.windingUp;
  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    if (enemy.alive && enemy.windingUp) {
      any = true;
      break;
    }
  }
  if (!any) {
    for (const WildEnemy3DRenderState& enemy : s.wildEnemies3d) {
      if (enemy.alive && enemy.windingUp) {
        any = true;
        break;
      }
    }
  }
  if (!any) return;

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);

  // 0.8s 呼吸周期：环体缩放与透明度同步脉冲，营造紧迫感。
  const float phase = s.windupPulseSeconds / 0.8f * 6.2831853f;
  const float pulse = 0.5f + 0.5f * std::sin(phase);
  const glm::vec3 warnColor{1.0f, 0.32f, 0.22f};
  s.shader3d.setLight(s.lightDir, warnColor * 0.7f, warnColor * 0.5f);
  s.shader3d.setAlpha(0.35f + 0.4f * pulse);

  // 环半径略大于接地阴影（scale*0.36）；单位环外半径 0.075+0.014/2=0.082。
  const auto drawRing = [&](float x, float z, float profileScale,
                            float radiusFactor) {
    const float ringScale = profileScale * radiusFactor / 0.082f *
                            (1.0f + 0.08f * pulse);
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(x, groundYAt(s, x, z) + 0.006f, z)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(ringScale));
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.shader3d.setSkinned(false);
    s.shader3d.setHasTexture(false);
    s.targetRingMesh.draw();
  };

  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    if (!enemy.alive || !enemy.windingUp) continue;
    drawRing(enemy.x, enemy.y, s.enemyAssetProfile.scale, 0.44f);
  }
  for (const WildEnemy3DRenderState& enemy : s.wildEnemies3d) {
    if (!enemy.alive || !enemy.windingUp) continue;
    // 预警环半径随原型缩放同步，覆盖大体型敌人受击范围。
    drawRing(enemy.x, enemy.y,
             s.enemyAssetProfile.scale * enemyScaleByArchetype(enemy.archetype),
             0.44f);
  }
  if (s.boss3d.active && !s.boss3d.defeated && s.boss3d.windingUp) {
    // 首领体型更大，预警环半径系数略增，覆盖其受击范围。
    drawRing(s.boss3d.x, s.boss3d.y, s.bossAssetProfile.scale, 0.5f);
  }

  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

// 元素附着光环：附着源质的目标脚下元素色呼吸光环（加法混合，
// 每个附着源质一环、多环同心错峰），原神式元素附着指示；
// 半径随模型缩放同步，绘制结束恢复中性状态。
static void drawAuraRings(Surface& s, const glm::mat4& vp) {
  if (s.targetRingMesh.vbo == 0u) return;
  bool any = s.trainingTarget.alive && s.trainingTargetAuraMask != 0;
  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    if (enemy.alive && enemy.auraMask != 0) {
      any = true;
      break;
    }
  }
  if (!any) return;

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // 加法混合：元素光晕在暗处更醒目
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);

  // 单位环外半径 0.082（createRing(0.075, 0.014, 40)），与预警环一致。
  constexpr float kRingOuterRadius = 0.082f;
  const auto drawAura = [&](float x, float z, float profileScale,
                            int auraMask) {
    int ringIndex = 0;
    for (int source = 0; source < 3; ++source) {
      if ((auraMask & (1 << source)) == 0) continue;
      const AuraRingPose pose = AuraRingPoseAt(s.auraPulseSeconds, ringIndex);
      // 多源质同心分层：每多一环外扩一档，避免同半径重叠。
      const float radius =
          profileScale * (0.46f + 0.08f * static_cast<float>(ringIndex)) *
          pose.radiusScale;
      const glm::vec3 color = AuraColorFor(source);
      const glm::mat4 model =
          glm::translate(glm::mat4(1.0f),
                         glm::vec3(x, groundYAt(s, x, z) + 0.008f, z)) *
          glm::scale(glm::mat4(1.0f),
                     glm::vec3(radius / kRingOuterRadius));
      s.shader3d.setLight(glm::vec3(0.0f, 1.0f, 0.0f), color * 0.8f,
                          color * 0.6f);
      s.shader3d.setAlpha(pose.alpha);
      s.shader3d.setMVP(vp * model);
      s.shader3d.setModel(model);
      s.targetRingMesh.draw();
      ++ringIndex;
    }
  };

  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    if (!enemy.alive || enemy.auraMask == 0) continue;
    drawAura(enemy.x, enemy.y, s.enemyAssetProfile.scale, enemy.auraMask);
  }
  if (s.trainingTarget.alive && s.trainingTargetAuraMask != 0) {
    drawAura(s.trainingTarget.x, s.trainingTarget.y,
             s.enemyAssetProfile.scale, s.trainingTargetAuraMask);
  }

  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  // 中性轮廓光/高光字面量与 kNeutral* 常量一致（常量定义在本函数之后）。
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);
}

// 审判光束轨迹预演：吟唱期间在地面铺设从首领指向玩家的脉冲红条，
// 提前暴露光束路径，让闪避躲避有明确的空间参考。
static void drawJudgmentBeam(Surface& s, const glm::mat4& vp) {
  if (!s.boss3d.active || s.boss3d.defeated || !s.boss3d.windingUp) return;
  if (s.boss3d.mechanic != 1) return;  // 1 = JudgmentBeam
  if (s.hpBarQuadMesh.vbo == 0u) return;

  const glm::vec3 bossPos(s.boss3d.x, groundYAt(s, s.boss3d.x, s.boss3d.y) + 0.005f,
                          s.boss3d.y);
  const glm::vec3 playerPos(s.player.x,
                            groundYAt(s, s.player.x, s.player.y) + 0.005f,
                            s.player.y);
  const glm::vec3 dir = playerPos - bossPos;
  const float length = glm::length(dir);
  if (length < 0.001f) return;

  const float yaw = std::atan2(dir.x, dir.z);
  const float pulse =
      0.5f + 0.5f * std::sin(s.windupPulseSeconds / 0.8f * 6.2831853f);

  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  const glm::vec3 beamColor{1.0f, 0.30f, 0.20f};
  s.shader3d.setLight(s.lightDir, beamColor * 0.7f, beamColor * 0.5f);
  s.shader3d.setAlpha(0.22f + 0.28f * pulse);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);

  // 单位四边形（XY 平面）：绕 X 转 90° 铺到地面，长轴对齐光束方向。
  const glm::mat4 model =
      glm::translate(glm::mat4(1.0f), bossPos + dir * 0.5f) *
      glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
      glm::rotate(glm::mat4(1.0f), 1.5707963f, glm::vec3(1.0f, 0.0f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(0.03f, length, 1.0f));
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.hpBarQuadMesh.draw();

  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

// 受击后仰：命中闪白窗口内沿朝向反方向微位移，
// 用身位反应把“被打实了”可视化；位移平方衰减，前强后弱更干脆。
static glm::vec2 hitKnockback(const Surface& s, uint32_t id, float angle) {
  const float remaining = hitFlashRemaining(s, id);
  if (remaining <= 0.0f) return glm::vec2(0.0f);
  constexpr float kFlashDuration = 0.15f;
  const float strength = std::min(remaining / kFlashDuration, 1.0f);
  const float amount = 0.004f * strength * strength;
  // 世界前向 (sin, cos)，后仰取反方向。
  return glm::vec2(-std::sin(angle) * amount, -std::cos(angle) * amount);
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

static void drawBossCinematicGeometry(Surface& s, const glm::mat4& vp) {
  if (!s.boss3d.active) return;
  const glm::vec3 center{s.boss3d.x, 0.31f, s.boss3d.y};
  const glm::vec3 coreColor = bossCoreColor(s.boss3d.sourceColor);
  const float reveal = std::max(0.5f, s.boss3d.cinematicProgress);

  auto drawRing = [&](float yaw, float roll, float scale) {
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), center) *
        glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), 1.5707963f,
                    glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.shader3d.setSkinned(false);
    s.shader3d.setHasTexture(false);
    applyEntityTint(s, coreColor);
    s.bossRingMesh.draw();
  };

  drawRing(s.boss3d.ringBroken ? -0.62f : -0.28f,
           s.boss3d.ringBroken ? 0.38f : 0.0f, reveal);
  drawRing(s.boss3d.ringBroken ? 0.70f : 0.34f,
           s.boss3d.ringBroken ? -0.42f : 0.0f, reveal * 0.82f);

  drawMeshAt(s, s.bossMesh, vp, center, 0.15f + reveal * 0.05f,
             coreColor);
  for (uint8_t i = 0; i < s.boss3d.shardCount; ++i) {
    constexpr float kTau = 6.2831853071795864769f;
    const float angle = kTau * static_cast<float>(i) / 3.0f +
                        s.boss3d.cinematicProgress * 2.2f;
    const float radius = s.boss3d.ringBroken ? 0.27f : 0.20f;
    const glm::vec3 shardPosition{
        center.x + std::cos(angle) * radius,
        center.y + std::sin(angle * 2.0f) * 0.045f,
        center.z + std::sin(angle) * radius};
    drawMeshAt(s, s.bossMesh, vp, shardPosition, 0.08f, coreColor * 0.75f);
  }
}

// 角色阶段默认的中性轮廓光：环境/预警环等非角色几何体共用。
static const glm::vec3 kNeutralRimColor{0.62f, 0.72f, 0.85f};
static constexpr float kNeutralRimStrength = 0.45f;
// 角色阶段默认的中性高光：与逐帧全局状态一致，drawActor 结束后恢复。
static constexpr float kNeutralSpecularStrength = 0.28f;
static constexpr float kNeutralSpecularShininess = 24.0f;
// 主角佩剑配色：冷银蓝，经 applyEntityTint 派生刃面光照。
static const glm::vec3 kBladeTint{0.72f, 0.78f, 0.88f};

static void drawActor(Surface& s, SkinnedModel& model, const Mesh& fallback,
                      SkinnedAnimationState& animationState,
                      const ActorRenderState& actor, const glm::mat4& matrix,
                      const glm::mat4& vp, const glm::vec3& base,
                      const AssetProfile& profile, float hitFlashSeconds,
                      bool targeted, float fadeAlpha, float appearance,
                      const char* actorName, const Mesh* weapon = nullptr,
                      int weaponJoint = -1,
                      const std::vector<bool>* attachmentOverride = nullptr) {
  if (fadeAlpha <= 0.0f) return;  // 尸体淡出完毕：整体跳过绘制。
  // 逐角色轮廓光：玩家青绿/敌人紫/Boss 品红，受击窗口内增强，
  // 被软锁定时常亮抬升，出场进度驱动渐入；绘制结束后恢复中性轮廓光，
  // 避免泄漏到后续非角色绘制。
  const ActorRimLight rim =
      ActorRimLightFor(profile, hitFlashSeconds, targeted, appearance);
  s.shader3d.setRim(rim.color, rim.strength);
  // 逐角色高光分档：主角盔甲强锐、敌人哑光、Boss 宽厚，
  // 绘制结束后与轮廓光一并恢复中性值。
  s.shader3d.setSpecular(profile.specularStrength, profile.specularShininess);
  // 卡通着色（原神式赛璐璐）：漫反射量化为明暗两段，暗部乘以角色
  // 专属阴影色；绘制结束后恢复关闭，不泄漏到地形/水面/天空。
  s.shader3d.setToonShading(profile.toonShading, profile.shadowColor,
                            profile.toonShadowEdge, profile.toonSoftness);
  s.shader3d.setMVP(vp * matrix);
  s.shader3d.setModel(matrix);
  applyEntityTint(s, base);

  // 尸体淡出：alpha 通道线性淡出，两条绘制路径结束后统一恢复状态。
  const bool fading = fadeAlpha < 1.0f;
  if (fading) {
#ifdef OHOS_PLATFORM
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
    s.shader3d.setAlpha(fadeAlpha);
  }
  const auto restoreFade = [&s, fading] {
    if (!fading) return;
    s.shader3d.setAlpha(1.0f);
#ifdef OHOS_PLATFORM
    glDisable(GL_BLEND);
#endif
  };

  // 反向壳描边 pass：剔除正面、把背面沿法线外推后绘制纯色轮廓线。
  // 线宽/颜色与轮廓光同源决策（受击闪白增宽变白、锁定染色、出场渐入），
  // 世界宽度需换算到模型局部空间（顶点着色器在模型矩阵前完成外推）。
  const auto drawOutline = [&] {
    const float width =
        ActorOutlineWidthFor(profile, hitFlashSeconds, targeted, appearance);
    if (width <= 0.0f) return;
    s.shader3d.setOutlinePass(
        width / std::max(profile.scale, 1e-6f),
        ActorOutlineColorFor(profile, hitFlashSeconds, targeted, appearance));
#ifdef OHOS_PLATFORM
    glCullFace(GL_FRONT);
#endif
    if (s.shader3d.skinningEnabled()) {
      model.draw(s.shader3d, attachmentOverride);
    } else if (actor.alive) {
      fallback.draw();
    }
#ifdef OHOS_PLATFORM
    glCullFace(GL_BACK);
#endif
    s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  };

  const auto restoreActorState = [&] {
    restoreFade();
    s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
    s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
    s.shader3d.setSpecular(kNeutralSpecularStrength,
                           kNeutralSpecularShininess);
  };

  if (model.ready()) {
    const SkinPalette palette =
        model.update(animationState, actor, 1.0f / 60.0f);
    s.shader3d.setSkinPalette(palette);
#ifdef OHOS_PLATFORM
    const RenderAnimation animation = ChooseAnimation(actor);
    const std::string clip = ResolveClip(model.clipNames(), animation,
                                         actor.variant, actor.moveRatio);
    if (animationState.shouldReport(animation, clip)) {
      LOGI("animation actor=%{public}s action=%{public}s clip=%{public}s",
           actorName, RenderAnimationName(animation), clip.c_str());
    }
#endif
    s.shader3d.setSkinned(true);
    if (s.shader3d.skinningEnabled()) {
      model.draw(s.shader3d, attachmentOverride);
      // 武器挂载：关节矩阵（globalTransform * inverseBind）把剑网格放进
      // handslot 绑定姿态，严格跟随手部动画；受击闪白同步染白剑身。
      const bool hasWeapon = weapon != nullptr && weapon->vbo != 0u &&
                             weaponJoint >= 0 &&
                             weaponJoint <
                                 static_cast<int>(palette.matrices.size());
      if (hasWeapon) {
        const glm::mat4 weaponMatrix = matrix * palette.matrices[weaponJoint];
        s.shader3d.setMVP(vp * weaponMatrix);
        s.shader3d.setModel(weaponMatrix);
        s.shader3d.setSkinned(false);
        s.shader3d.setHasTexture(false);
        applyEntityTint(s, hitFlashTint(kBladeTint, hitFlashSeconds));
        weapon->draw();
      }
      drawOutline();
      if (hasWeapon) {
        // 武器与本体共用同一线宽/线色描边，保持轮廓语言一致。
        const float weaponWidth = ActorOutlineWidthFor(
            profile, hitFlashSeconds, targeted, appearance);
        if (weaponWidth > 0.0f) {
          const glm::mat4 weaponMatrix =
              matrix * palette.matrices[weaponJoint];
          s.shader3d.setOutlinePass(
              weaponWidth / std::max(profile.scale, 1e-6f),
              ActorOutlineColorFor(profile, hitFlashSeconds, targeted,
                                   appearance));
          s.shader3d.setMVP(vp * weaponMatrix);
          s.shader3d.setModel(weaponMatrix);
          s.shader3d.setSkinned(false);
          s.shader3d.setHasTexture(false);
#ifdef OHOS_PLATFORM
          glCullFace(GL_FRONT);
#endif
          weapon->draw();
#ifdef OHOS_PLATFORM
          glCullFace(GL_BACK);
#endif
          s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
        }
      }
      restoreActorState();
      return;
    }
  }

  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(fallback.texture != 0u);
  // 静态 Mesh 没有死亡姿态；死亡实体保持隐藏，而可用的骨骼模型可播放 death。
  if (actor.alive) fallback.draw();
  drawOutline();
  restoreActorState();
}

// 接地接触阴影：角色脚下平铺半透明黑色圆盘，提供接地感，
// 代价远低于阴影贴图。调用方需已开启混合、关闭深度写入，
// 并把轮廓光/高光/alpha 设为阴影状态。
static void drawContactShadow(Surface& s, const glm::mat4& vp, float x, float z,
                              float radius) {
  if (s.shadowMesh.vbo == 0u) return;
  // 略高于地面（y=0）避免 z-fighting，又低于角色基座（0.011+）。
  const glm::mat4 model =
      glm::translate(glm::mat4(1.0f),
                     glm::vec3(x, groundYAt(s, x, z) + 0.004f, z)) *
      glm::scale(glm::mat4(1.0f),
                 glm::vec3(radius * 2.0f, 1.0f, radius * 2.0f));
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  // 纯黑无光照圆盘；光照/轮廓光/高光由调用方统一置为阴影状态。
  s.shader3d.setLight(s.lightDir, glm::vec3(0.0f), glm::vec3(0.0f));
  s.shadowMesh.draw();
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
    case ModelKind::Npc:
      asset = &s.npcModelAsset;
      break;
  }
  return asset != nullptr && asset->take(bytes);
}

// 按名构建挂件启用表（与 attachmentNames() 同序）：未列出的挂件关闭。
static std::vector<bool> buildAttachmentOverride(
    const SkinnedModel& model,
    std::initializer_list<const char*> enabledNames) {
  const std::vector<std::string> names = model.attachmentNames();
  std::vector<bool> overrideFlags(names.size(), false);
  for (const char* name : enabledNames) {
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (names[i] == name) overrideFlags[i] = true;
    }
  }
  return overrideFlags;
}

// 敌人原型装备覆盖指针：越界原型返回 nullptr（回退全局开关）。
static const std::vector<bool>* enemyAttachmentOverride(const Surface& s,
                                                        int archetype) {
  if (archetype < 0 ||
      archetype >= static_cast<int>(s.enemyArchetypeAttachments.size())) {
    return nullptr;
  }
  return &s.enemyArchetypeAttachments[static_cast<std::size_t>(archetype)];
}

// NPC 市民装备覆盖指针：按 id 取模分配变体（NpcAttachmentVariantFor），
// 变体表未构建（模型未加载）时返回 nullptr 回退全局开关。
static const std::vector<bool>* npcAttachmentOverride(const Surface& s,
                                                      uint32_t id) {
  const int variantCount = static_cast<int>(s.npcAttachmentVariants.size());
  const int index = NpcAttachmentVariantFor(id, variantCount);
  if (index < 0 || index >= variantCount) return nullptr;
  const std::vector<bool>& variant =
      s.npcAttachmentVariants[static_cast<std::size_t>(index)];
  if (variant.empty()) return nullptr;
  return &variant;
}

static void tryInitializeModelAsset(Surface& s, ModelKind kind,
                                    SkinnedModel& model,
                                    const char* assetName) {
  std::vector<uint8_t> bytes;
  if (!takePendingModelAsset(s, kind, bytes)) return;

  // 替换和清空都必须先在 current context 下释放旧 GPU 资源。
  model.destroy();
  if (kind == ModelKind::Player) {
    s.playerWeaponJoint = -1;
  }
  if (kind == ModelKind::Enemy) {
    s.enemyWeaponJoint = -1;
  }
  if (kind == ModelKind::Boss) {
    s.bossWeaponJoint = -1;
  }
  if (bytes.empty()) {
    LOGI("%{public}s cleared; static Mesh fallback remains active", assetName);
    return;
  }
  if (!model.tryInitialize(bytes, assetName)) {
    LOGE("%{public}s; static Mesh fallback remains active",
         model.lastError().c_str());
    return;
  }
  if (kind == ModelKind::Player) {
    // KayKit 角色右手的武器挂点；缺失时保持 -1，武器不挂载。
    s.playerWeaponJoint = FindJointIndex(model.jointNames(), "handslot.r");
    LOGI("player weapon joint=%{public}d", s.playerWeaponJoint);
    // 骑士模块化装备：头盔 + 披风 + 左手圆盾（右手保留程序化佩剑）。
    model.setAttachmentEnabled("Knight_Helmet", true);
    model.setAttachmentEnabled("Knight_Cape", true);
    model.setAttachmentEnabled("Round_Shield", true);
  }
  if (kind == ModelKind::Enemy) {
    s.enemyWeaponJoint = FindJointIndex(model.jointNames(), "handslot.r");
    LOGI("enemy weapon joint=%{public}d", s.enemyWeaponJoint);
    // 法师模块化装备：帽子 + 披风 + 左手法术书（右手保留程序化法杖）。
    model.setAttachmentEnabled("Mage_Hat", true);
    model.setAttachmentEnabled("Mage_Cape", true);
    model.setAttachmentEnabled("Spellbook", true);
    // 原型装备变体（下标 = EnemyArchetype）：6 类原型共享法师模型，
    // 用装备组合差异化剪影；训练假人走上面的全局默认组合。
    s.enemyArchetypeAttachments[0] =
        buildAttachmentOverride(model, {"Mage_Cape"});  // RiftClaw
    s.enemyArchetypeAttachments[1] = buildAttachmentOverride(
        model, {"Mage_Hat", "Spellbook_open"});  // Priest
    s.enemyArchetypeAttachments[2] = buildAttachmentOverride(
        model, {"Mage_Hat", "Mage_Cape", "Spellbook"});  // Guard
    s.enemyArchetypeAttachments[3] =
        buildAttachmentOverride(model, {"Mage_Cape"});  // Bruiser
    s.enemyArchetypeAttachments[4] = buildAttachmentOverride(
        model, {"Mage_Hat", "Mage_Cape", "Spellbook_open"});  // Caster
    s.enemyArchetypeAttachments[5] = buildAttachmentOverride(
        model, {"Mage_Hat", "Mage_Cape", "Spellbook"});  // Elite
  }
  if (kind == ModelKind::Boss) {
    s.bossWeaponJoint = FindJointIndex(model.jointNames(), "handslot.r");
    LOGI("boss weapon joint=%{public}d", s.bossWeaponJoint);
    // 野蛮人模块化装备：帽子 + 披风 + 左手圆盾（右手保留程序化重棍）。
    model.setAttachmentEnabled("Barbarian_Hat", true);
    model.setAttachmentEnabled("Barbarian_Cape", true);
    model.setAttachmentEnabled("Barbarian_Round_Shield", true);
  }
  if (kind == ModelKind::Npc) {
    // 市民模块化装备（NPC 复用玩家 KayKit 模型）：披风为基础着装，
    // 与全副武装的主角（头盔+披风+盾牌）区分；按 id 分配三种变体
    // 差异化剪影——披风市民 / 头盔民兵 / 披风+盾卫兵。
    model.setAttachmentEnabled("Knight_Cape", true);
    s.npcAttachmentVariants[0] =
        buildAttachmentOverride(model, {"Knight_Cape"});
    s.npcAttachmentVariants[1] =
        buildAttachmentOverride(model, {"Knight_Helmet", "Knight_Cape"});
    s.npcAttachmentVariants[2] = buildAttachmentOverride(
        model, {"Knight_Cape", "Round_Shield"});
  }
}

static void tryInitializePendingModelAssets(Surface& s) {
  tryInitializeModelAsset(s, ModelKind::Player, s.playerModel, "player.glb");
  tryInitializeModelAsset(s, ModelKind::Enemy, s.enemyModel, "enemy.glb");
  tryInitializeModelAsset(s, ModelKind::Boss, s.bossModel, "boss.glb");
  tryInitializeModelAsset(s, ModelKind::Npc, s.npcModel, "npc.glb");
}

static const char* environmentAssetName(size_t index) {
  static constexpr std::array<const char*, 4> kNames = {
      "outer_ring.glb", "center_rift.glb", "backdrop.glb", "decoration.glb"};
  return kNames[index];
}

static void tryInitializePendingEnvironmentAssets(Surface& s) {
  for (size_t index = 0; index < s.environmentAssets.size(); ++index) {
    std::vector<uint8_t> bytes;
    {
      std::lock_guard<std::mutex> lock(s.modelAssetMutex);
      if (!s.environmentAssets[index].take(bytes)) continue;
    }
    StaticModel& model = s.environmentModels[index];
    model.destroy();
    if (bytes.empty()) {
      s.environmentStatuses[index] = EnvironmentBatchStatus::Empty;
      continue;
    }
    if (model.tryInitialize(bytes, environmentAssetName(index))) {
      s.environmentStatuses[index] = EnvironmentBatchStatus::Ready;
      LOGI("environment batch ready: %{public}s", environmentAssetName(index));
    } else {
      s.environmentStatuses[index] = EnvironmentBatchStatus::Failed;
      LOGE("%{public}s; procedural fallback remains active",
           model.lastError().c_str());
    }
  }
}

// Phase 2 区块批次上传：每帧最多消费一个待上传区块，避免一次性注入
// 多个大 GLB 造成帧尖峰；解析失败仅把该区块置为 Failed（跳过绘制），
// 不回退全局批次。
static void tryInitializePendingBlockEnvironmentAssets(Surface& s) {
  int32_t blockId = -1;
  std::vector<uint8_t> bytes;
  {
    std::lock_guard<std::mutex> lock(s.modelAssetMutex);
    for (auto& entry : s.blockEnvironmentAssets) {
      if (entry.second.take(bytes)) {
        blockId = entry.first;
        break;
      }
    }
  }
  if (blockId < 0) return;
  StaticModel& model = s.blockEnvironmentModels[blockId];
  model.destroy();
  if (bytes.empty()) {
    s.blockEnvironmentStatuses[blockId] = EnvironmentBatchStatus::Empty;
    return;
  }
  char assetName[32];
  std::snprintf(assetName, sizeof(assetName), "block_%d.glb",
                static_cast<int>(blockId));
  if (model.tryInitialize(bytes, assetName)) {
    s.blockEnvironmentStatuses[blockId] = EnvironmentBatchStatus::Ready;
    LOGI("environment block ready: %{public}s", assetName);
  } else {
    s.blockEnvironmentStatuses[blockId] = EnvironmentBatchStatus::Failed;
    LOGE("%{public}s; block batch skipped", model.lastError().c_str());
  }
}

// 环境模型世界适配变换：layout.json 以米制描述布局（-34..+20），而世界为
// [0,1] 归一化坐标；参数与碰撞层共用 environmentWorldFitForRegion，
// 保证可见建筑与碰撞体严格对齐（见 environment.h）。
static glm::mat4 environmentWorldFit(const Surface& s, size_t index) {
  return environmentWorldFitMatrix(index, s.environmentComposition);
}

// ---- 视锥剔除（Phase 5）：环境批次绘制前可见性判断 ----
// 单个布局条目的世界空间包围球：fit 相似变换作用于布局空间
// translation 与 halfExtents，旋转不改变球半径，直接取保守球。
static bool placementInFrustum(const EnvironmentPlacement& placement,
                               const EnvironmentWorldFit& fit,
                               const FrustumPlanes& frustum) {
  const glm::vec3 center{
      fit.centerX + fit.scale * placement.translation[0],
      fit.yBias + fit.scale * placement.translation[1],
      fit.centerZ + fit.scale * placement.translation[2]};
  const float radius = fit.scale * glm::length(glm::vec3(
      placement.scale[0] * placement.halfExtents[0],
      placement.scale[1] * placement.halfExtents[1],
      placement.scale[2] * placement.halfExtents[2]));
  return FrustumContainsSphere(frustum, center, radius);
}

// 全局批次（blockId=-1 且 region=index）可见性：任一条目通过平面
// 测试即保留；批次无布局条目时保守保留，避免误剔除。
static bool environmentBatchInFrustum(const Surface& s, size_t index,
                                      const FrustumPlanes& frustum) {
  const EnvironmentWorldFit fit =
      environmentWorldFitParams(index, s.environmentComposition);
  bool anyPlacement = false;
  const size_t count = environmentLayoutPlacementCount();
  const EnvironmentPlacement* placements = environmentLayoutPlacements();
  for (size_t i = 0; i < count; ++i) {
    const EnvironmentPlacement& placement = placements[i];
    if (placement.blockId != kEnvironmentGlobalBlockId ||
        placement.region != static_cast<int>(index)) {
      continue;
    }
    anyPlacement = true;
    if (placementInFrustum(placement, fit, frustum)) return true;
  }
  return !anyPlacement;
}

// 区块批次（blockId≥0）可见性：与全局批次同理，fit 统一取 OuterRing
// 参数（与 environmentBlockWorldFitMatrix 一致）。
static bool environmentBlockInFrustum(const Surface& s, int32_t blockId,
                                      const FrustumPlanes& frustum) {
  const EnvironmentWorldFit fit = environmentWorldFitParams(
      static_cast<size_t>(EnvironmentBatchKind::OuterRing),
      s.environmentComposition);
  bool anyPlacement = false;
  const size_t count = environmentLayoutPlacementCount();
  const EnvironmentPlacement* placements = environmentLayoutPlacements();
  for (size_t i = 0; i < count; ++i) {
    const EnvironmentPlacement& placement = placements[i];
    if (placement.blockId != blockId) continue;
    anyPlacement = true;
    if (placementInFrustum(placement, fit, frustum)) return true;
  }
  return !anyPlacement;
}

static void drawEnvironmentModel(Surface& s, size_t index,
                                 const glm::mat4& vp,
                                 const FrustumPlanes& frustum,
                                 const glm::vec3& tint, float tintStrength) {
  StaticModel& model = s.environmentModels[index];
  if (s.environmentStatuses[index] != EnvironmentBatchStatus::Ready ||
      !model.ready()) return;
  // 视锥剔除（Phase 5）：布局条目全部在视锥外时跳过整个批次。
  if (!environmentBatchInFrustum(s, index, frustum)) return;
  model.setTextureTier(s.environmentPlan.textureTier);
  const glm::mat4 fit = environmentWorldFit(s, index);
  s.shader3d.setMVP(vp * fit);
  s.shader3d.setModel(fit);
  s.shader3d.setSkinned(false);
  s.shader3d.setLight(glm::normalize(s.lightDir), {0.8f, 0.8f, 0.75f},
                      {0.18f, 0.20f, 0.24f});
  s.shader3d.setEnvironmentTint(tint, tintStrength);
  model.draw(s.shader3d);
  s.environmentDrawCalls += static_cast<uint32_t>(model.stats().primitiveCount);
  s.environmentTriangles += static_cast<uint32_t>(model.stats().triangleCount);
}

// 区块批次绘制：仅绘制 Ready 且所属分块在 environmentPlan.activeBlocks
// 中的区块；fit 矩阵统一取 OuterRing 参数，与碰撞层一致。
static void drawBlockEnvironmentModels(Surface& s, const glm::mat4& vp,
                                       const FrustumPlanes& frustum) {
  if (s.blockEnvironmentModels.empty()) return;
  const glm::mat4 fit = environmentBlockWorldFitMatrix(s.environmentComposition);
  for (auto& entry : s.blockEnvironmentModels) {
    const int32_t blockId = entry.first;
    const auto status = s.blockEnvironmentStatuses.find(blockId);
    if (status == s.blockEnvironmentStatuses.end() ||
        status->second != EnvironmentBatchStatus::Ready) continue;
    if (!s.environmentPlan.blockActive(blockId)) continue;
    // 视锥剔除（Phase 5）：区块内布局条目全部在视锥外时跳过。
    if (!environmentBlockInFrustum(s, blockId, frustum)) continue;
    StaticModel& model = entry.second;
    if (!model.ready()) continue;
    model.setTextureTier(s.environmentPlan.textureTier);
    s.shader3d.setMVP(vp * fit);
    s.shader3d.setModel(fit);
    s.shader3d.setSkinned(false);
    s.shader3d.setLight(glm::normalize(s.lightDir), {0.8f, 0.8f, 0.75f},
                        {0.18f, 0.20f, 0.24f});
    s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
    model.draw(s.shader3d);
    s.environmentDrawCalls +=
        static_cast<uint32_t>(model.stats().primitiveCount);
    s.environmentTriangles +=
        static_cast<uint32_t>(model.stats().triangleCount);
  }
}

static void drawFallbackMesh(Surface& s, const Mesh& mesh,
                             const glm::mat4& vp, const glm::mat4& model,
                             const glm::vec3& color) {
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setLight(glm::normalize(s.lightDir), color * 0.75f, color * 0.25f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  mesh.draw();
  ++s.environmentDrawCalls;
  s.environmentTriangles += static_cast<uint32_t>(mesh.indices.size() / 3u);
}

static void drawEnvironmentFallback(Surface& s, const glm::mat4& vp) {
  constexpr int kPillars = 12;
  for (int index = 0; index < kPillars; ++index) {
    const float angle = static_cast<float>(index) * 6.2831853f / kPillars;
    const float pillarX = 0.5f + std::cos(angle) * 0.42f;
    const float pillarZ = 0.65f + std::sin(angle) * 0.42f;
    const glm::vec3 position{pillarX,
                             groundYAt(s, pillarX, pillarZ) + 0.06f, pillarZ};
    const glm::mat4 pillar = glm::translate(glm::mat4(1.0f), position) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.0f, 0.55f));
    drawFallbackMesh(s, s.fallbackPillarMesh, vp, pillar,
                     {0.42f, 0.45f, 0.50f});
  }
  constexpr int kWalls = 8;
  for (int index = 0; index < kWalls; ++index) {
    const float angle = static_cast<float>(index) * 6.2831853f / kWalls;
    const float wallX = 0.5f + std::cos(angle) * 0.46f;
    const float wallZ = 0.65f + std::sin(angle) * 0.46f;
    const glm::vec3 position{wallX, groundYAt(s, wallX, wallZ) + 0.035f,
                             wallZ};
    const glm::mat4 wall = glm::translate(glm::mat4(1.0f), position) *
        glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.20f, 0.07f, 0.025f));
    drawFallbackMesh(s, s.fallbackWallMesh, vp, wall, {0.34f, 0.37f, 0.42f});
  }
}

static void drawCenterFallback(Surface& s, const glm::mat4& vp) {
  constexpr int kMarkers = 4;
  for (int index = 0; index < kMarkers; ++index) {
    const float angle = static_cast<float>(index) * 6.2831853f / kMarkers;
    const float markerX = 0.5f + std::cos(angle) * 0.16f;
    const float markerZ = 0.75f + std::sin(angle) * 0.09f;
    const glm::vec3 position{markerX,
                             groundYAt(s, markerX, markerZ) + 0.018f, markerZ};
    const glm::mat4 marker =
        glm::translate(glm::mat4(1.0f), position) *
        glm::rotate(glm::mat4(1.0f), -angle,
                    glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.07f, 0.036f, 0.035f));
    drawFallbackMesh(s, s.fallbackWallMesh, vp, marker,
                     {0.31f, 0.25f, 0.25f});
  }
}

// -----------------------------------------------------------------------------
// 面向相机的广告牌旋转：把四边形法线 (0,0,1) 转到相机观察方向，
// 与 2D 相机约定一致（屏幕上 = 世界 {sin yaw, cos yaw}）。伤害飘字与
// 敌人血条共用。
// -----------------------------------------------------------------------------
static glm::mat4 cameraBillboard(const Surface& s) {
  const float yaw = s.cameraRenderState.yaw();
  const float pitch = s.cameraRenderState.pitch();
  return glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
         glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f)) *
         glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(0.0f, 1.0f, 0.0f));
}

// -----------------------------------------------------------------------------
// 锁定目标指示器：软瞄准目标脚下的脉冲环，提示当前攻击对象。
// -----------------------------------------------------------------------------
static void drawTargetMarker(Surface& s, const glm::mat4& vp) {
  if (!s.targetMarker3d.active) return;
  if (s.targetRingMesh.vbo == 0u) return;

  const float phase = s.targetMarker3d.pulsePhase;
  const float scalePulse = 1.0f + 0.10f * std::sin(phase * 2.0f);
  // 环体旋转对称，无需旋转；仅做呼吸缩放脉冲。
  const glm::mat4 model =
      glm::translate(glm::mat4(1.0f),
                     glm::vec3(s.targetMarker3d.x,
                               groundYAt(s, s.targetMarker3d.x,
                                         s.targetMarker3d.z) + 0.016f,
                               s.targetMarker3d.z)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(scalePulse));
  // 青金色锁定环，背光面仍保持可见。
  const glm::vec3 markerColor{0.35f, 0.85f, 0.80f};
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setLight(s.lightDir, markerColor * 0.7f, markerColor * 0.5f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  s.targetRingMesh.draw();
}

// -----------------------------------------------------------------------------
// 伤害飘字：程序化数字图集 + 面向相机的广告牌绘制
// -----------------------------------------------------------------------------
static void ensureDigitAssets(Surface& s) {
  if (s.digitAssetsReady) return;
  s.digitAssetsReady = true;

  const DigitAtlas atlas = DigitAtlas::build();
  glGenTextures(1, &s.digitAtlasTexture);
  glBindTexture(GL_TEXTURE_2D, s.digitAtlasTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas.width, atlas.height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, atlas.pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  // 每个数字一个单位四边形，UV 预烘焙到对应图集单元。
  const float aspect = static_cast<float>(atlas.cellWidth) /
                       static_cast<float>(atlas.cellHeight);
  for (int digit = 0; digit < 10; ++digit) {
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    atlas.uvRect(static_cast<char>('0' + digit), u0, v0, u1, v1);
    Mesh quad;
    const float halfWidth = 0.5f * aspect;
    quad.vertices = {
        {{-halfWidth, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v0}},
        {{halfWidth, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v0}},
        {{halfWidth, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v1}},
        {{-halfWidth, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v1}},
    };
    quad.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    quad.texture = s.digitAtlasTexture;
    quad.upload();
    s.digitMeshes[digit] = quad;
  }
}

// 技能释放冲击波：施法者脚下加法混合的扩散光环，双面无剔除，
// 深度只读不写；结束后恢复混合/深度/剔除与中性光照状态。
static void drawShockwaveRings(Surface& s, const glm::mat4& vp) {
  if (s.shockwaveRings.empty() || s.targetRingMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  // 与 init3DResources 的 createRing(0.075, 0.014, 40) 基准半径一致。
  constexpr float kRingBaseRadius = 0.075f;
  for (const Surface::ShockwaveRing& ring : s.shockwaveRings) {
    const ShockwavePose pose = ShockwavePoseAt(ring.seconds);
    if (!pose.visible) continue;
    const float radius = ring.maxRadius * (0.25f + 0.75f * pose.radiusScale);
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(ring.x,
                                 groundYAt(s, ring.x, ring.z) + 0.012f,
                                 ring.z)) *
        glm::scale(glm::mat4(1.0f),
                   glm::vec3(radius / kRingBaseRadius));
    s.shader3d.setLight(glm::vec3(0.0f, 1.0f, 0.0f), ring.color * 0.8f,
                        ring.color * 0.6f);
    s.shader3d.setAlpha(pose.alpha * 0.8f);
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.targetRingMesh.draw();
  }
  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
  s.shader3d.setSpecular(kNeutralSpecularStrength, kNeutralSpecularShininess);
}

// 命中贴地冲击贴花：加法混合的地面光斑（单位圆盘 shadowMesh，
// 半径 0.5），快速扩张后淡出；深度只读不写，双面可见。
static void drawImpactDecals(Surface& s, const glm::mat4& vp) {
  if (s.impactDecals.empty() || s.shadowMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  constexpr float kDiskBaseRadius = 0.5f;  // createDisk(0.5) 基准半径
  for (const Surface::ImpactDecal& decal : s.impactDecals) {
    const ImpactDecalPose pose = ImpactDecalPoseAt(decal.seconds);
    if (!pose.visible) continue;
    const float radius =
        decal.maxRadius * (0.35f + 0.65f * pose.radiusScale);
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(decal.x,
                                 groundYAt(s, decal.x, decal.z) + 0.010f,
                                 decal.z)) *
        glm::scale(glm::mat4(1.0f),
                   glm::vec3(radius / kDiskBaseRadius, 1.0f,
                             radius / kDiskBaseRadius));
    s.shader3d.setLight(glm::vec3(0.0f, 1.0f, 0.0f), decal.color * 0.8f,
                        decal.color * 0.6f);
    s.shader3d.setAlpha(pose.alpha * 0.6f);
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.shadowMesh.draw();
  }
  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
  s.shader3d.setSpecular(kNeutralSpecularStrength, kNeutralSpecularShininess);
}

// 共鸣爆发光柱：元素反应触发点的垂直元素光柱（加法混合，双层
// 外柔晕 + 内亮芯），绕 Y 轴 billboard 始终面向相机；画在角色
// 层之上让光柱包裹受击实体，深度只读不写。结束后恢复状态。
static void drawLightPillars(Surface& s, const glm::mat4& vp) {
  if (s.lightPillars.empty() || s.hpBarQuadMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  // 绕 Y 轴 billboard：只取相机 yaw，光柱保持垂直不随俯仰倾倒。
  const float yaw = s.cameraRenderState.yaw();
  for (const Surface::LightPillar& pillar : s.lightPillars) {
    const LightPillarPose pose = LightPillarPoseAt(pillar.seconds);
    if (!pose.visible) continue;
    const float height = pillar.maxHeight * pose.heightScale;
    if (height <= 0.0f) continue;
    const float groundY = groundYAt(s, pillar.x, pillar.z);
    for (int layer = 0; layer < 2; ++layer) {
      // 外层柔晕宽而暗、内层亮芯窄而亮，宽度随高度联动。
      const float width =
          height * (layer == 0 ? 0.22f : 0.10f) * pose.widthScale;
      const float layerAlpha =
          layer == 0 ? pose.alpha * 0.35f : pose.alpha * 0.8f;
      const glm::mat4 model =
          glm::translate(glm::mat4(1.0f),
                         glm::vec3(pillar.x, groundY + height * 0.5f,
                                   pillar.z)) *
          glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
          glm::scale(glm::mat4(1.0f), glm::vec3(width, height, 1.0f));
      s.shader3d.setLight(glm::vec3(0.0f, 1.0f, 0.0f), pillar.color * 0.8f,
                          pillar.color * 0.6f);
      s.shader3d.setAlpha(layerAlpha);
      s.shader3d.setMVP(vp * model);
      s.shader3d.setModel(model);
      s.hpBarQuadMesh.draw();
    }
  }
  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
  s.shader3d.setSpecular(kNeutralSpecularStrength, kNeutralSpecularShininess);
}

// 元素技能符文环：技能释放瞬间施法者脚下的旋转双新月符阵
// （复用刀光新月网格，两弧相差 180°），加法混合贴地，缓出旋转 +
// 淡入淡出，原神技能法阵语言。结束后恢复状态。
static void drawSkillRunes(Surface& s, const glm::mat4& vp) {
  if (s.skillRunes.empty() || s.slashArcMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  const glm::vec3 up{0.0f, 1.0f, 0.0f};
  for (const Surface::SkillRune& rune : s.skillRunes) {
    const SkillRunePose pose = SkillRunePoseAt(rune.seconds);
    if (!pose.visible) continue;
    const float radius = rune.maxRadius * pose.scale;
    const glm::vec3 center{rune.x, groundYAt(s, rune.x, rune.z) + 0.012f,
                           rune.z};
    // 双新月符阵：两弧相差 180°，外层柔晕 + 内层亮芯各画一遍。
    for (int arc = 0; arc < 2; ++arc) {
      const float rotation =
          pose.rotationRadians + static_cast<float>(arc) * 3.14159265f;
      for (int layer = 0; layer < 2; ++layer) {
        const float layerRadius =
            layer == 0 ? radius * 1.15f : radius * 0.82f;
        const float layerAlpha =
            layer == 0 ? pose.alpha * 0.35f : pose.alpha * 0.75f;
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), center) *
            glm::rotate(glm::mat4(1.0f), rotation, up) *
            glm::scale(glm::mat4(1.0f),
                       glm::vec3(layerRadius, 1.0f, layerRadius));
        s.shader3d.setLight(up, rune.color * 0.8f, rune.color * 0.6f);
        s.shader3d.setAlpha(layerAlpha);
        s.shader3d.setMVP(vp * model);
        s.shader3d.setModel(model);
        s.slashArcMesh.draw();
      }
    }
  }
  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
  s.shader3d.setSpecular(kNeutralSpecularStrength, kNeutralSpecularShininess);
}

// 普攻刀光：加法混合的新月弧线，双层叠加（外层柔晕 + 内层亮芯）
// 模拟渐变刀光；双面可见，深度只读不写。主角为金白刀光、终结段
// 更亮更大；敌人为红色刀光，尺寸随原型缩放。结束后恢复状态。
static void drawSlashArcs(Surface& s, const glm::mat4& vp) {
  const bool playerActive = s.playerSlashSeconds >= 0.0f;
  if (!playerActive && s.enemySlashArcs.empty()) return;
  if (s.slashArcMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setToonShading(false, glm::vec3(0.7f), 0.1f, 0.08f);
  s.shader3d.setOutlinePass(0.0f, glm::vec3(0.0f));
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  const glm::vec3 up{0.0f, 1.0f, 0.0f};

  const auto drawArc = [&](const glm::vec3& center, float yaw, float sweep,
                           float radius, const glm::vec3& color, float alpha) {
    if (alpha <= 0.0f || radius <= 0.0f) return;
    for (int layer = 0; layer < 2; ++layer) {
      const float layerRadius = layer == 0 ? radius * 1.18f : radius * 0.8f;
      const float layerAlpha = layer == 0 ? alpha * 0.4f : alpha;
      const glm::mat4 model =
          glm::translate(glm::mat4(1.0f), center) *
          glm::rotate(glm::mat4(1.0f), yaw + sweep,
                      glm::vec3(0.0f, 1.0f, 0.0f)) *
          glm::scale(glm::mat4(1.0f),
                     glm::vec3(layerRadius, 1.0f, layerRadius));
      s.shader3d.setLight(up, color * 0.8f, color * 0.6f);
      s.shader3d.setAlpha(layerAlpha);
      s.shader3d.setMVP(vp * model);
      s.shader3d.setModel(model);
      s.slashArcMesh.draw();
    }
  };

  if (playerActive) {
    const SlashArcPose pose =
        SlashArcPoseAt(s.playerSlashSeconds, s.playerSlashCombo);
    if (pose.visible) {
      // 元素附魔染色：刀光颜色跟随最近施放的源质（终结段固定金橙）。
      const glm::vec3 color =
          SlashArcColorFor(s.playerSlashCombo, s.playerSlashSource);
      const float radius = s.playerAssetProfile.scale * 2.4f * pose.scale;
      const glm::vec3 center{s.player.x,
                             groundYAt(s, s.player.x, s.player.y) +
                                 s.playerAssetProfile.scale * 1.15f,
                             s.player.y};
      drawArc(center, s.playerSlashYaw, pose.sweepRadians, radius, color,
              pose.alpha * 0.85f);
    }
  }
  for (const Surface::EnemySlashArc& arc : s.enemySlashArcs) {
    const SlashArcPose pose = SlashArcPoseAt(arc.seconds, 0);
    if (!pose.visible) continue;
    // 原型元素色刀光：物理红 / Priest 金白 / Caster 青蓝 / Elite 暗紫。
    const glm::vec3 color = arc.color;
    const float radius =
        s.enemyAssetProfile.scale * arc.scale * 2.2f * pose.scale;
    const glm::vec3 center{arc.x,
                           groundYAt(s, arc.x, arc.y) +
                               s.enemyAssetProfile.scale * arc.scale * 1.1f,
                           arc.y};
    drawArc(center, arc.yaw, pose.sweepRadians, radius, color,
            pose.alpha * 0.7f);
  }

  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  s.shader3d.setRim(kNeutralRimColor, kNeutralRimStrength);
  s.shader3d.setSpecular(kNeutralSpecularStrength, kNeutralSpecularShininess);
}

// 命中火花：加法混合的广告牌四边形，尺寸与透明度随寿命衰减。
// 深度只读不写，被前景实体正确遮挡。调用后需恢复轮廓光/高光。
static void drawHitSparks(Surface& s, const glm::mat4& vp) {
  if (s.hitSparks3d.empty() || s.hpBarQuadMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // 加法混合：火花在暗场景上更醒目
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  const glm::mat4 billboard = cameraBillboard(s);
  const glm::vec3 billboardNormal =
      glm::normalize(glm::vec3(billboard * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
  for (const HitSpark3D& spark : s.hitSparks3d) {
    const float t = std::clamp(spark.life / spark.maxLife, 0.0f, 1.0f);
    glm::vec3 color{1.0f, 0.78f, 0.32f};      // 0 = 命中金橙
    if (spark.kind == 1) {
      color = {1.0f, 0.35f, 0.30f};            // 1 = 玩家受击红
    } else if (spark.kind == 2) {
      color = {1.0f, 0.92f, 0.62f};            // 2 = 击杀亮金
    } else if (spark.kind == 3) {
      color = {0.55f, 0.78f, 0.95f};           // 3 = 移动尾迹淡蓝
    } else if (spark.kind == 4) {
      color = {1.0f, 0.96f, 0.72f};            // 4 = 辉印金白
    } else if (spark.kind == 5) {
      color = {0.45f, 0.85f, 1.0f};            // 5 = 脉流青蓝
    } else if (spark.kind == 6) {
      color = {0.72f, 0.45f, 0.95f};           // 6 = 蚀质暗紫
    } else if (spark.kind == 7) {
      color = {1.0f, 0.94f, 0.66f};            // 7 = 主角武器拖尾金白
    } else if (spark.kind == 8) {
      color = {1.0f, 0.45f, 0.38f};            // 8 = 敌方武器拖尾红
    } else if (spark.kind == 9) {
      color = {0.50f, 0.87f, 1.0f};            // 9 = 脉流附魔青蓝拖尾
    } else if (spark.kind == 10) {
      color = {0.78f, 0.48f, 0.98f};           // 10 = 蚀质附魔暗紫拖尾
    }
    s.shader3d.setLight(billboardNormal, color * 0.8f, color * 0.6f);
    // 尾迹粒子低透明度小尺寸，不与战斗火花争夺注意力。
    const float alpha = spark.kind == 3    ? t * 0.45f
                        : spark.kind >= 7 ? t * 0.55f
                                          : t;
    s.shader3d.setAlpha(alpha);
    const float baseSize = spark.kind == 2  ? 0.005f
                           : spark.kind == 3 ? 0.0022f
                           : spark.kind >= 7 ? 0.003f
                           : spark.kind >= 4 ? 0.0042f
                                             : 0.0035f;
    // sizeScale 随归属实体的模型缩放同步，模型放大后特效等大跟随。
    const float size = (baseSize + 0.004f * t) * std::max(0.0f, spark.sizeScale);
    // 速度对齐拉伸：把火花沿飞行方向拉成流光（尾迹 kind 速度近 0
    // 时自动保持圆形），与原神命中/投射物特效的拖尾语言一致。
    const SparkStretch stretch = SparkStretchFor(
        spark.vx, spark.vy, spark.vz, s.cameraRenderState.yaw(),
        s.cameraRenderState.pitch());
    const float sizeX = size * stretch.stretch;
    const float sizeY = stretch.stretch > 1.0f
                            ? size / std::sqrt(stretch.stretch)
                            : size;
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(spark.x, spark.y, spark.z)) *
        billboard *
        glm::rotate(glm::mat4(1.0f), stretch.angleRadians,
                    glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(sizeX, sizeY, 1.0f));
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.hpBarQuadMesh.draw();
  }
  s.shader3d.setAlpha(1.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

static void drawDamageNumbers(Surface& s, const glm::mat4& vp) {
  if (s.damageNumbers3d.empty()) return;
  ensureDigitAssets(s);
  if (s.digitAtlasTexture == 0u) return;

  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(true);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);

  // 广告牌旋转：把四边形法线 (0,0,1) 转到相机观察方向。
  const glm::mat4 billboard = cameraBillboard(s);
  // 光向取广告牌法线：漫反射恒为 1，数字亮度不随相机旋转变化。
  const glm::vec3 billboardNormal =
      glm::normalize(glm::vec3(billboard * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));

  // 字号缩小约 40%（0.055 → 0.034）：原版飘字过大遮挡敌人，
  // 大额伤害略大但仍显著小于旧尺寸，保持层级感。
  constexpr float kCharHeight = 0.034f;
  constexpr float kHeavyHeight = 0.044f;
  constexpr float kCharAspect = 0.8f;  // 数字图集单元 16x20
  char buffer[16];
  for (const DamageNumberRenderState& number : s.damageNumbers3d) {
    glm::vec3 tint(0.92f, 0.95f, 0.94f);  // Normal：近白
    if (number.kind == 1) {
      tint = {1.0f, 0.84f, 0.40f};        // Heavy：金色
    } else if (number.kind == 2) {
      tint = {1.0f, 0.45f, 0.40f};        // PlayerHit：红色
    }
    s.shader3d.setLight(billboardNormal, tint * 0.7f, tint * 0.3f);
    s.shader3d.setAlpha(number.alpha);

    const int length = snprintf(buffer, sizeof(buffer), "%d", number.value);
    // 入场弹出缩放：生成瞬间从 0.6 倍放大到常尺寸。
    const float charHeight =
        (number.kind == 1 ? kHeavyHeight : kCharHeight) *
        std::clamp(number.scale, 0.1f, 1.5f);
    const float charWidth = charHeight * kCharAspect;
    const float totalWidth = charWidth * static_cast<float>(length);
    const glm::vec3 basePosition(number.x + number.driftX,
                                 0.145f + number.rise, number.z);
    for (int index = 0; index < length && index < 15; ++index) {
      if (buffer[index] < '0' || buffer[index] > '9') continue;
      const int digit = buffer[index] - '0';
      const float localX = -totalWidth * 0.5f +
                           charWidth * (static_cast<float>(index) + 0.5f);
      const glm::mat4 model =
          glm::translate(glm::mat4(1.0f), basePosition) * billboard *
          glm::translate(glm::mat4(1.0f), glm::vec3(localX, 0.0f, 0.0f)) *
          glm::scale(glm::mat4(1.0f), glm::vec3(charHeight));
      s.shader3d.setMVP(vp * model);
      s.shader3d.setModel(model);
      s.digitMeshes[digit].draw();
    }
  }

  s.shader3d.setAlpha(1.0f);
  s.shader3d.setHasTexture(false);
  glDepthMask(GL_TRUE);
}

// -----------------------------------------------------------------------------
// 敌人头顶血条：背景条 + 按血量比例缩短的前景条，颜色随血量分档。
// -----------------------------------------------------------------------------
static glm::vec3 hpBarFillColor(float ratio) {
  if (ratio > 0.5f) return {0.31f, 0.83f, 0.73f};   // 高血量：青绿
  if (ratio > 0.25f) return {0.85f, 0.63f, 0.27f};  // 中血量：琥珀
  return {0.88f, 0.42f, 0.37f};                     // 低血量：警示红
}

static void drawEnemyHpBars(Surface& s, const glm::mat4& vp) {
  if (s.enemyHpBars3d.empty()) return;
  if (s.hpBarQuadMesh.vbo == 0u) return;

  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  s.shader3d.setAlpha(1.0f);

  const glm::mat4 billboard = cameraBillboard(s);
  const glm::vec3 billboardNormal =
      glm::normalize(glm::vec3(billboard * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
  constexpr float kBarWidth = 0.09f;
  constexpr float kBarHeight = 0.012f;
  constexpr float kBarY = 0.185f;  // 敌人头顶上方

  for (const EnemyHpBarRenderState& bar : s.enemyHpBars3d) {
    const float ratio = std::clamp(bar.ratio, 0.0f, 1.0f);
    const glm::vec3 basePosition(bar.x, kBarY, bar.z);

    // 背景条（深色底）。
    const glm::vec3 backColor{0.10f, 0.12f, 0.16f};
    glm::mat4 model =
        glm::translate(glm::mat4(1.0f), basePosition) * billboard *
        glm::scale(glm::mat4(1.0f), glm::vec3(kBarWidth, kBarHeight, 1.0f));
    s.shader3d.setMVP(vp * model);
    s.shader3d.setModel(model);
    s.shader3d.setLight(billboardNormal, backColor * 0.7f, backColor * 0.3f);
    s.hpBarQuadMesh.draw();

    // 滞后条（扣血追赶动效）：受击后短暂停留再向实际血量收缩，
    // 暖红底衬托前景条，让单次扣血量清晰可读。
    const float trail = std::clamp(bar.trailRatio, ratio, 1.0f);
    if (trail > ratio) {
      const glm::vec3 trailColor{0.85f, 0.42f, 0.30f};
      const float trailWidth = kBarWidth * trail;
      const float trailX = -kBarWidth * 0.5f + trailWidth * 0.5f;
      const glm::mat4 trailModel =
          glm::translate(glm::mat4(1.0f), basePosition) * billboard *
          glm::translate(glm::mat4(1.0f), glm::vec3(trailX, 0.0f, 0.0002f)) *
          glm::scale(glm::mat4(1.0f),
                     glm::vec3(trailWidth, kBarHeight * 0.72f, 1.0f));
      s.shader3d.setMVP(vp * trailModel);
      s.shader3d.setModel(trailModel);
      s.shader3d.setLight(billboardNormal, trailColor * 0.7f,
                          trailColor * 0.3f);
      s.hpBarQuadMesh.draw();
    }

    // 前景条：左对齐，宽度按血量比例缩放。
    if (ratio > 0.0f) {
      const glm::vec3 fillColor = hpBarFillColor(ratio);
      const float fillWidth = kBarWidth * ratio;
      const float localX = -kBarWidth * 0.5f + fillWidth * 0.5f;
      const glm::mat4 fillModel =
          glm::translate(glm::mat4(1.0f), basePosition) * billboard *
          glm::translate(glm::mat4(1.0f), glm::vec3(localX, 0.0f, 0.0005f)) *
          glm::scale(glm::mat4(1.0f),
                     glm::vec3(fillWidth, kBarHeight * 0.72f, 1.0f));
      s.shader3d.setMVP(vp * fillModel);
      s.shader3d.setModel(fillModel);
      s.shader3d.setLight(billboardNormal, fillColor * 0.7f, fillColor * 0.3f);
      s.hpBarQuadMesh.draw();
    }
  }

  glDepthMask(GL_TRUE);
}

// 天空穹顶：以相机为心的大球体内壁，天顶→地平线渐变，地平线色取雾色
// 保证与深度雾无缝衔接。深度只读不写，不遮挡后续几何。
static void drawSkyDome(Surface& s, const glm::mat4& vp) {
  if (s.skyMesh.vbo == 0u) return;
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  s.shader3d.setSurfaceMode(SurfaceMode::Sky);
  s.shader3d.setSkyColors({0.16f, 0.30f, 0.54f}, s.environmentPalette.fogColor);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  const glm::mat4 model =
      glm::translate(glm::mat4(1.0f), s.camera3d.position) *
      glm::scale(glm::mat4(1.0f), glm::vec3(40.0f));
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.skyMesh.draw();
  s.shader3d.setSurfaceMode(SurfaceMode::Normal);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);
  glDepthMask(GL_TRUE);
}

// 整世界地形网格回退：分块尚未就绪时保证启动不黑屏。
// 采样与逻辑层同一高度场，按高度/坡度混合沙地/草地/岩石色。
static void drawTerrainFallback(Surface& s, const glm::mat4& vp) {
#ifdef OHOS_PLATFORM
  if (s.terrainMesh.vbo == 0u && s.terrain != nullptr &&
      s.terrainMesh.vertices.empty()) {
    s.terrainMesh = createTerrainMesh(*s.terrain, 96u);
    s.terrainMesh.upload();
  }
#endif
  if (s.terrainMesh.vbo == 0u) return;
  s.shader3d.setSurfaceMode(SurfaceMode::Terrain);
  s.shader3d.setTerrainColors({0.72f, 0.64f, 0.46f}, {0.30f, 0.48f, 0.27f},
                              {0.44f, 0.44f, 0.48f});
  s.shader3d.setTerrainWaterLevel(-0.012f);
  const glm::mat4 model(1.0f);
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setLight(glm::normalize(s.lightDir), {0.8f, 0.8f, 0.75f},
                      s.environmentPalette.ambient * 0.8f +
                          glm::vec3(0.08f));
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  s.terrainMesh.draw();
  s.shader3d.setSurfaceMode(SurfaceMode::Normal);
}

// 分块地形 AABB 高度范围估计（Phase 5 视锥剔除）：3×3 网格采样
// 高度场取 min/max，再叠加全幅度保守余量（主起伏+褶皱+山脊
// 总幅宽）覆盖采样间隙；下缘额外含侧裙深度。
static void terrainChunkHeightRange(const Surface& s,
                                    const TerrainChunkRect& rect,
                                    float& outMin, float& outMax) {
  // 高度场正弦组合总幅宽上限（amplitude+detail+ridge），边缘山脊
  // 只抬升不下降，上缘余量同样覆盖。
  constexpr float kAmplitudeMargin = 0.09f;
  constexpr float kSkirtMargin = 0.06f;
  float minHeight = 0.0f;
  float maxHeight = 0.0f;
  bool sampled = false;
  for (int iy = 0; iy <= 2; ++iy) {
    for (int ix = 0; ix <= 2; ++ix) {
      const float x = rect.x0 +
                      (rect.x1 - rect.x0) * static_cast<float>(ix) * 0.5f;
      const float y = rect.y0 +
                      (rect.y1 - rect.y0) * static_cast<float>(iy) * 0.5f;
      const float height = s.terrain->heightAt(x, y);
      if (!sampled) {
        minHeight = height;
        maxHeight = height;
        sampled = true;
      } else {
        minHeight = std::min(minHeight, height);
        maxHeight = std::max(maxHeight, height);
      }
    }
  }
  outMin = minHeight - kAmplitudeMargin - kSkirtMargin;
  outMax = maxHeight + kAmplitudeMargin;
}

// 分块地形流式绘制：每帧先执行卸载回调（渲染线程释放退出滞后带
// 分块的 GPU 资源），再从调度器取最多 1 个就绪分块上传（默认 2ms
// 预算，超时推下帧），随后绘制全部已上传分块。无任何分块就绪时
// 回退整世界网格，保证启动不黑屏。
static void drawTerrainChunks(Surface& s, const glm::mat4& vp,
                              const FrustumPlanes& frustum) {
#ifdef OHOS_PLATFORM
  if (s.streamScheduler != nullptr) {
    for (const int32_t chunkId : s.streamScheduler->applyUnloads()) {
      const auto found = s.terrainChunkMeshes.find(chunkId);
      if (found != s.terrainChunkMeshes.end()) {
        found->second.destroy();
        s.terrainChunkMeshes.erase(found);
      }
    }
    // 按配额上传就绪分块（正常每帧 1 块，传送 burst 窗口内放宽）。
    for (const int32_t readyChunk : s.streamScheduler->drainReady()) {
      if (s.terrainChunkMeshes.count(readyChunk) != 0) continue;
      const TerrainChunkCpuMesh* cpuMesh =
          s.streamScheduler->activeChunkMesh(readyChunk);
      if (cpuMesh != nullptr && !cpuMesh->mesh.vertices.empty()) {
        Mesh uploaded = cpuMesh->mesh;
        uploaded.upload();
        s.terrainChunkMeshes.emplace(readyChunk, std::move(uploaded));
      }
    }
  }
#endif
  if (s.terrainChunkMeshes.empty()) {
    drawTerrainFallback(s, vp);
    return;
  }
  s.shader3d.setSurfaceMode(SurfaceMode::Terrain);
  s.shader3d.setTerrainColors({0.72f, 0.64f, 0.46f}, {0.30f, 0.48f, 0.27f},
                              {0.44f, 0.44f, 0.48f});
  s.shader3d.setTerrainWaterLevel(-0.012f);
  const glm::mat4 model(1.0f);
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.shader3d.setLight(glm::normalize(s.lightDir), {0.8f, 0.8f, 0.75f},
                      s.environmentPalette.ambient * 0.8f +
                          glm::vec3(0.08f));
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  for (auto& entry : s.terrainChunkMeshes) {
    // 视锥剔除（Phase 5）：按分块矩形 + 估计高度范围构造 AABB，
    // 整体在视锥外时跳过绘制；无调度器（理论上不会发生）时全绘。
    if (s.streamScheduler != nullptr && s.terrain != nullptr) {
      const TerrainChunkRect rect =
          s.streamScheduler->chunkedTerrain().chunkRect(entry.first);
      float minHeight = 0.0f;
      float maxHeight = 0.0f;
      terrainChunkHeightRange(s, rect, minHeight, maxHeight);
      if (!FrustumContainsAabb(frustum,
                               glm::vec3(rect.x0, minHeight, rect.y0),
                               glm::vec3(rect.x1, maxHeight, rect.y1))) {
        continue;
      }
    }
    entry.second.draw();
  }
  s.shader3d.setSurfaceMode(SurfaceMode::Normal);
}

// 水面：半透明平面抬升到水面高度，流动正弦涟漪；深度只读不写，
// 在不透明地形/环境之后、角色之前绘制。
static void drawWater(Surface& s, const glm::mat4& vp) {
  if (s.waterMesh.vbo == 0u) return;
  const float waterLevel =
      s.terrain != nullptr ? s.terrain->config().waterLevel : -0.012f;
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setSurfaceMode(SurfaceMode::Water);
  s.shader3d.setWaterColor({0.16f, 0.38f, 0.47f}, 0.72f);
  s.shader3d.setTime(s.renderSeconds);
  s.shader3d.setEnvironmentTint(glm::vec3(0.0f), 0.0f);
  // 略超出世界边界，避免边缘露缝；边缘山体遮挡外围。
  const glm::mat4 model =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, waterLevel, 0.5f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(1.3f, 1.0f, 1.3f));
  s.shader3d.setMVP(vp * model);
  s.shader3d.setModel(model);
  s.shader3d.setSkinned(false);
  s.shader3d.setHasTexture(false);
  s.waterMesh.draw();
  s.shader3d.setSurfaceMode(SurfaceMode::Normal);
  s.shader3d.setTime(0.0f);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

// 角色视锥剔除（Phase 5）：按模型缩放的保守包围球——模型局部
// 高约 3 单位，球心取半高处、半径覆盖头脚，仅用于绘制前判断。
static bool actorInFrustum(const FrustumPlanes& frustum, const glm::vec3& feet,
                           float scale) {
  const float radius = scale * 2.2f;
  return FrustumContainsSphere(frustum,
                               feet + glm::vec3(0.0f, scale * 1.5f, 0.0f),
                               radius);
}

// -----------------------------------------------------------------------------
// bloom 后处理（原神式技能发光）：场景先渲染入全分辨率 FBO，
// 亮通提取 → 半分辨率双向高斯 ping-pong → 与场景加法合成。
// 刀光/冲击波/火花/光环等加法混合特效因此获得溢出光晕。
// -----------------------------------------------------------------------------

// 全屏三角形由 gl_VertexID 生成，无需 VBO。
static const char* kBloomVertexShader =
    "#version 300 es\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "  vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "  vUV = pos;\n"
    "  gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

// uMode：0=亮通提取（软膝阈值） 1=9-tap 方向高斯模糊 2=场景+bloom 合成。
// kW 权重与 bloom_pass.h 的 BloomGaussianWeight 一致。
static const char* kBloomFragmentShader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uScene;\n"
    "uniform sampler2D uBloomTex;\n"
    "uniform int uMode;\n"
    "uniform vec2 uTexel;\n"
    "uniform vec2 uDirection;\n"
    "uniform float uThreshold;\n"
    "uniform float uIntensity;\n"
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "const float kW[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);\n"
    "void main() {\n"
    "  if (uMode == 0) {\n"
    "    vec3 c = texture(uScene, vUV).rgb;\n"
    "    float luma = dot(c, vec3(0.299, 0.587, 0.114));\n"
    "    float knee = clamp((luma - uThreshold) / max(uThreshold, 0.001), 0.0, 1.0);\n"
    "    fragColor = vec4(c * knee, 1.0);\n"
    "  } else if (uMode == 1) {\n"
    "    vec3 c = texture(uScene, vUV).rgb * kW[0];\n"
    "    for (int i = 1; i < 5; ++i) {\n"
    "      vec2 off = uDirection * uTexel * (float(i) * 1.6);\n"
    "      c += texture(uScene, vUV + off).rgb * kW[i];\n"
    "      c += texture(uScene, vUV - off).rgb * kW[i];\n"
    "    }\n"
    "    fragColor = vec4(c, 1.0);\n"
    "  } else {\n"
    "    vec3 scene = texture(uScene, vUV).rgb;\n"
    "    vec3 bloom = texture(uBloomTex, vUV).rgb;\n"
    "    fragColor = vec4(scene + bloom * uIntensity, 1.0);\n"
    "  }\n"
    "}\n";

static bool initBloomProgram(Surface& s) {
  const GLuint vs = compileShader(GL_VERTEX_SHADER, kBloomVertexShader);
  const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kBloomFragmentShader);
  if (!vs || !fs) {
    if (vs != 0u) glDeleteShader(vs);
    if (fs != 0u) glDeleteShader(fs);
    LOGE("bloom shader compile failed");
    return false;
  }
  s.bloomProgram = glCreateProgram();
  if (s.bloomProgram == 0u) {
    glDeleteShader(vs);
    glDeleteShader(fs);
    return false;
  }
  glAttachShader(s.bloomProgram, vs);
  glAttachShader(s.bloomProgram, fs);
  glLinkProgram(s.bloomProgram);
  GLint linked = 0;
  glGetProgramiv(s.bloomProgram, GL_LINK_STATUS, &linked);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!linked) {
    char buf[512];
    glGetProgramInfoLog(s.bloomProgram, sizeof(buf), nullptr, buf);
    LOGE("bloom program link failed: %{public}s", buf);
    glDeleteProgram(s.bloomProgram);
    s.bloomProgram = 0;
    return false;
  }
  return true;
}

static void deleteBloomTargets(Surface& s) {
  if (s.bloomSceneFbo != 0u) glDeleteFramebuffers(1, &s.bloomSceneFbo);
  if (s.bloomPingFbo != 0u) glDeleteFramebuffers(1, &s.bloomPingFbo);
  if (s.bloomPongFbo != 0u) glDeleteFramebuffers(1, &s.bloomPongFbo);
  if (s.bloomSceneTex != 0u) glDeleteTextures(1, &s.bloomSceneTex);
  if (s.bloomPingTex != 0u) glDeleteTextures(1, &s.bloomPingTex);
  if (s.bloomPongTex != 0u) glDeleteTextures(1, &s.bloomPongTex);
  if (s.bloomDepthRbo != 0u) glDeleteRenderbuffers(1, &s.bloomDepthRbo);
  s.bloomSceneFbo = 0;
  s.bloomPingFbo = 0;
  s.bloomPongFbo = 0;
  s.bloomSceneTex = 0;
  s.bloomPingTex = 0;
  s.bloomPongTex = 0;
  s.bloomDepthRbo = 0;
  s.bloomFboWidth = 0;
  s.bloomFboHeight = 0;
}

static GLuint createBloomTexture(int width, int height) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

// 按当前窗口尺寸惰性（重）建 bloom 目标；尺寸不变则直接复用。
static bool ensureBloomTargets(Surface& s) {
  if (s.width <= 0 || s.height <= 0) return false;
  if (s.bloomSceneFbo != 0u && s.bloomFboWidth == s.width &&
      s.bloomFboHeight == s.height) {
    return true;
  }
  deleteBloomTargets(s);
  const int w = s.width;
  const int h = s.height;
  const int hw = BloomDownsampleSize(w);
  const int hh = BloomDownsampleSize(h);
  // 场景 FBO：全分辨率 RGBA8 颜色 + DEPTH24 renderbuffer。
  glGenFramebuffers(1, &s.bloomSceneFbo);
  s.bloomSceneTex = createBloomTexture(w, h);
  glGenRenderbuffers(1, &s.bloomDepthRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, s.bloomDepthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glBindFramebuffer(GL_FRAMEBUFFER, s.bloomSceneFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         s.bloomSceneTex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, s.bloomDepthRbo);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOGE("bloom scene FBO incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    deleteBloomTargets(s);
    return false;
  }
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // 半分辨率 ping-pong：模糊在 1/4 像素量上做，移动端开销可控。
  s.bloomPingTex = createBloomTexture(hw, hh);
  s.bloomPongTex = createBloomTexture(hw, hh);
  glGenFramebuffers(1, &s.bloomPingFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, s.bloomPingFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         s.bloomPingTex, 0);
  glGenFramebuffers(1, &s.bloomPongFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, s.bloomPongFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         s.bloomPongTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  s.bloomFboWidth = w;
  s.bloomFboHeight = h;
  LOGI("bloom targets ready: %{public}dx%{public}d blur %{public}dx%{public}d",
       w, h, hw, hh);
  return true;
}

// 亮通提取 → 双向高斯 ping-pong → 加法合成回默认帧缓冲。
// 调用前提：场景已渲染入 bloomSceneFbo，bloomProgram 已链接。
static void runBloomPasses(Surface& s, const BloomParams& params) {
  const int hw = BloomDownsampleSize(s.bloomFboWidth);
  const int hh = BloomDownsampleSize(s.bloomFboHeight);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glUseProgram(s.bloomProgram);
  const GLint locScene = glGetUniformLocation(s.bloomProgram, "uScene");
  const GLint locBloomTex = glGetUniformLocation(s.bloomProgram, "uBloomTex");
  const GLint locMode = glGetUniformLocation(s.bloomProgram, "uMode");
  const GLint locTexel = glGetUniformLocation(s.bloomProgram, "uTexel");
  const GLint locDirection = glGetUniformLocation(s.bloomProgram, "uDirection");
  const GLint locThreshold = glGetUniformLocation(s.bloomProgram, "uThreshold");
  const GLint locIntensity = glGetUniformLocation(s.bloomProgram, "uIntensity");
  glUniform1i(locScene, 0);
  glUniform1i(locBloomTex, 1);
  // Pass 1：亮通提取（全分辨率场景 → 半分辨率 ping，线性采样降采样）。
  glBindFramebuffer(GL_FRAMEBUFFER, s.bloomPingFbo);
  glViewport(0, 0, hw, hh);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s.bloomSceneTex);
  glUniform1i(locMode, 0);
  glUniform1f(locThreshold, params.threshold);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // Pass 2..N：方向高斯 ping-pong（H→V 为一轮）。
  glUniform1i(locMode, 1);
  glUniform2f(locTexel, 1.0f / static_cast<float>(hw),
              1.0f / static_cast<float>(hh));
  GLuint srcTex = s.bloomPingTex;
  GLuint dstFbo = s.bloomPongFbo;
  for (int iteration = 0; iteration < params.blurIterations; ++iteration) {
    for (int dir = 0; dir < 2; ++dir) {
      glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
      glBindTexture(GL_TEXTURE_2D, srcTex);
      glUniform2f(locDirection, dir == 0 ? 1.0f : 0.0f,
                  dir == 0 ? 0.0f : 1.0f);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      srcTex = dstFbo == s.bloomPongFbo ? s.bloomPongTex : s.bloomPingTex;
      dstFbo = dstFbo == s.bloomPongFbo ? s.bloomPingFbo : s.bloomPongFbo;
    }
  }
  // 合成：场景 + bloom*intensity 写回默认帧缓冲（全屏三角形覆盖）。
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, s.bloomFboWidth, s.bloomFboHeight);
  glUniform1i(locMode, 2);
  glUniform1f(locIntensity, params.intensity);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, s.bloomSceneTex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, srcTex);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void draw3DPhase(Surface& s) {
  // bridge 可能晚于 Surface 创建；surface_draw 已成功 makeCurrent，因此只在这里
  // 消费一次标脏字节，解析失败后保持静态 Mesh，不在每帧反复尝试。
  tryInitializePendingModelAssets(s);
  tryInitializePendingEnvironmentAssets(s);
  tryInitializePendingBlockEnvironmentAssets(s);
  if (!s.shader3dReady || s.shader3d.program() == 0u) return;

  // bloom（原神式技能发光）：高画质档场景先渲染入 FBO，3D 阶段末尾
  // 做亮通提取/半分辨率模糊/加法合成；资源失败自动回退直渲。
  const BloomParams bloomParams = BloomParamsFor(s.bloomEnabled ? 0 : 1);
  const bool bloomActive = BloomEnabled(bloomParams) &&
                           s.bloomProgram != 0u && ensureBloomTargets(s);
  if (bloomActive) {
    glBindFramebuffer(GL_FRAMEBUFFER, s.bloomSceneFbo);
    glViewport(0, 0, s.bloomFboWidth, s.bloomFboHeight);
    glClearColor(0.06f, 0.08f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  // 3D 阶段需要深度测试；2D 阶段未写深度，故在此单独清深度并开启深度测试，
  // 绘制结束后关闭，避免影响下一帧 2D 绘制。
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  s.shader3d.use();
  s.shader3d.setHasTexture(false);
  // 相机位置与轮廓光/高光参数逐帧写入：轮廓光把角色从暗色背景中勾出，
  // 高光提升盔甲/皮肤材质观感；骨骼模型与回退几何体共享同一套光照。
  s.shader3d.setCameraPosition(s.camera3d.position);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);
  // 指数深度雾：世界为归一化坐标（相机距离约 0.7~2），把调色板密度
  // 缩到 0.55 使远处环境融入雾色而近处角色保持清晰。
  s.shader3d.setFog(s.environmentPalette.fogColor,
                    s.environmentPalette.fogDensity * 0.55f);

  if (s.width > 0 && s.height > 0) {
    s.camera3d.aspectRatio =
        static_cast<float>(s.width) / static_cast<float>(s.height);
  }

  const glm::mat4 vp = s.camera3d.projectionMatrix() * s.camera3d.viewMatrix();
  // 视锥剔除平面（Phase 5）：地形分块/环境批次/角色绘制前统一消费。
  const FrustumPlanes frustum = s.camera3d.frustumPlanes();

  s.environmentDrawCalls = 0;
  s.environmentTriangles = 0;
  s.environmentPlan = s.environmentController.evaluate(
      {s.player.x, s.player.y}, s.environmentPerfLevel);
  // 区块批次启停：激活分块集合直接来自流式调度器（Loop 已在推进它），
  // 无需在 loop.cpp 挂接任何回调；binary_search 要求升序。
  if (s.streamScheduler != nullptr) {
    s.environmentPlan.activeBlocks = s.streamScheduler->activeChunkIds();
    std::sort(s.environmentPlan.activeBlocks.begin(),
              s.environmentPlan.activeBlocks.end());
  } else {
    s.environmentPlan.activeBlocks.clear();
  }
  if (s.environmentPlan.textureTier != s.loggedEnvironmentTextureTier) {
    s.loggedEnvironmentTextureTier = s.environmentPlan.textureTier;
    LOGI("environment texture tier: %{public}s",
         s.loggedEnvironmentTextureTier == StaticTextureTier::Half ? "half"
                                                                    : "full");
  }

  // 天空穹顶 → 地形网格 → 环境模型 → 水面：地形采样与逻辑层同一
  // 高度场，起伏/水域与贴地判定严格一致。
  drawSkyDome(s, vp);
  drawTerrainChunks(s, vp, frustum);

  if (s.environmentPlan.backdrop) {
    drawEnvironmentModel(s, 2, vp, frustum, glm::vec3(0.0f), 0.0f);
  }
  drawEnvironmentModel(s, 0, vp, frustum, glm::vec3(0.0f), 0.0f);
  if (s.environmentStatuses[0] != EnvironmentBatchStatus::Ready) {
    drawEnvironmentFallback(s, vp);
  }
  if (s.environmentPlan.decoration) {
    drawEnvironmentModel(s, 3, vp, frustum, glm::vec3(0.0f), 0.0f);
  }
  drawEnvironmentModel(s, 1, vp, frustum, s.environmentPalette.fogColor,
                       0.22f);
  if (s.environmentStatuses[1] != EnvironmentBatchStatus::Ready) {
    drawCenterFallback(s, vp);
  }
  // Phase 2 区块批次：仅激活分块的 block_<id>.glb 参与绘制。
  drawBlockEnvironmentModels(s, vp, frustum);
  const float altarGround =
      groundYAt(s, s.environmentComposition.altarAnchor.x,
                s.environmentComposition.altarAnchor.z);
  const glm::mat4 rift =
      glm::translate(glm::mat4(1.0f),
                     s.environmentComposition.altarAnchor +
                         glm::vec3(0.0f, altarGround + 0.004f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), {0.22f, 1.0f, 0.08f});
  drawFallbackMesh(s, s.riftPlaneMesh, vp, rift,
                   s.environmentPalette.altarGlow);
  drawWater(s, vp);
  const bool fallbackMeshesReady = s.fallbackPillarMesh.vbo != 0u &&
                                   s.fallbackWallMesh.vbo != 0u;
  const bool outerCovered =
      s.environmentStatuses[0] == EnvironmentBatchStatus::Ready ||
      fallbackMeshesReady;
  const bool centerCovered =
      s.environmentStatuses[1] == EnvironmentBatchStatus::Ready ||
      (s.fallbackWallMesh.vbo != 0u && s.riftPlaneMesh.vbo != 0u);
  s.environmentReady = s.shader3dReady && outerCovered && centerCovered;

  // M3-1 地面索引按双面占位使用；角色模型阶段启用背面剔除。
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // 接地接触阴影 pass：先于角色绘制半透明黑色圆盘，深度只读不写；
  // 结束后恢复轮廓光/高光/alpha 为角色阶段统一状态。
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  s.shader3d.setRim(glm::vec3(0.0f), 0.0f);
  s.shader3d.setSpecular(0.0f, 1.0f);
  s.shader3d.setAlpha(0.38f);
  drawContactShadow(s, vp, s.player.x, s.player.y,
                    s.playerAssetProfile.scale * 0.36f);
  if (s.trainingTarget.alive) {
    drawContactShadow(s, vp, s.trainingTarget.x, s.trainingTarget.y,
                      s.enemyAssetProfile.scale * 0.36f);
  }
  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    if (!enemy.alive) continue;
    drawContactShadow(s, vp, enemy.x, enemy.y,
                      s.enemyAssetProfile.scale * 0.36f);
  }
  for (const WildEnemy3DRenderState& enemy : s.wildEnemies3d) {
    if (!enemy.alive) continue;
    drawContactShadow(s, vp, enemy.x, enemy.y,
                      s.enemyAssetProfile.scale *
                          enemyScaleByArchetype(enemy.archetype) * 0.36f);
  }
  for (const Npc3DRenderState& npc : s.npcs3d) {
    if (!npc.visible) continue;
    drawContactShadow(s, vp, npc.x, npc.y,
                      s.npcAssetProfile.scale * 0.36f);
  }
  if (s.boss3d.active && !s.boss3d.defeated) {
    drawContactShadow(s, vp, s.boss3d.x, s.boss3d.y,
                      s.bossAssetProfile.scale * 0.36f);
  }
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);
  s.shader3d.setAlpha(1.0f);

  // 攻击前摇预警环：先于角色绘制，结束后函数内部恢复状态。
  drawWindupWarnings(s, vp);
  // 审判光束地面轨迹预演：同样先于角色，结束后恢复状态。
  drawJudgmentBeam(s, vp);
  // 元素附着光环：附着源质的目标脚下持续元素色环，与预警环同层。
  drawAuraRings(s, vp);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);
  // 技能释放冲击波：地面层光环，先于角色绘制，结束后恢复状态。
  drawShockwaveRings(s, vp);
  // 命中贴地冲击贴花：与冲击波同层，先于角色绘制。
  drawImpactDecals(s, vp);
  // 元素技能符文环：与冲击波同层，先于角色绘制。
  drawSkillRunes(s, vp);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);

  // 玩家：模型可用时走蒙皮，否则保留 M3-1 立方体。
  // 闪避无敌帧：半透明化给出清晰的免伤窗口反馈。
  const glm::vec3 playerFeet{s.player.x, s.playerGroundHeight + 0.012f,
                             s.player.y};
  if (actorInFrustum(frustum, playerFeet, s.playerAssetProfile.scale)) {
    if (s.playerInvulnerable) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      s.shader3d.setAlpha(0.55f);
    }
    drawActor(s, s.playerModel, s.playerMesh, s.playerAnimationState,
              s.player3dAnimation,
              actorModelMatrix(playerFeet,
                               s.playerAssetProfile.scale,
                               s.player.angle +
                                   s.playerAssetProfile.yawOffsetRadians),
              vp, hitFlashTint(s.playerAssetProfile.materialTint,
                               s.playerHitAnimationSeconds),
              s.playerAssetProfile, s.playerHitAnimationSeconds,
              false, 1.0f, 1.0f, "player", &s.swordMesh,
              s.playerWeaponJoint);
    if (s.playerInvulnerable) {
      s.shader3d.setAlpha(1.0f);
      glDisable(GL_BLEND);
    }
  }

  // 训练假人立方体（按 alive 跳过）。
  {
    const glm::vec3 dummyFeet{
        s.trainingTarget.x,
        groundYAt(s, s.trainingTarget.x, s.trainingTarget.y) + 0.011f,
        s.trainingTarget.y};
    if (actorInFrustum(frustum, dummyFeet, s.enemyAssetProfile.scale)) {
      drawActor(s, s.enemyModel, s.enemyMesh, s.trainingTargetAnimationState,
                s.trainingTarget3dAnimation,
                actorModelMatrix(dummyFeet,
                                 s.enemyAssetProfile.scale,
                                 s.enemyAssetProfile.yawOffsetRadians),
                vp, hitFlashTint(s.enemyAssetProfile.materialTint,
                                 hitFlashRemaining(s, s.trainingTarget.id)),
                s.enemyAssetProfile, hitFlashRemaining(s, s.trainingTarget.id),
                false, 1.0f, 1.0f, "training-target", &s.staffMesh,
                s.enemyWeaponJoint);
    }
  }

  // 敌人立方体（按存活状态跳过）。
  s.pruneEnemyAnimationStates();
  for (const Enemy3DRenderState& enemy : s.enemies3d) {
    SkinnedAnimationState& animationState = s.enemyAnimationStates[enemy.id];
    const glm::vec2 knock = hitKnockback(s, enemy.id, enemy.angle);
    const glm::vec3 enemyFeet{enemy.x + knock.x,
                              groundYAt(s, enemy.x, enemy.y) + 0.011f,
                              enemy.y + knock.y};
    // 视锥剔除（Phase 5）：视锥外敌人跳过绘制（动画状态表不受影响）。
    if (!actorInFrustum(frustum, enemyFeet, s.enemyAssetProfile.scale)) {
      continue;
    }
    drawActor(s, s.enemyModel, s.enemyMesh, animationState, enemy.animation,
              actorModelMatrix(enemyFeet,
                               s.enemyAssetProfile.scale,
                               enemy.angle +
                                   s.enemyAssetProfile.yawOffsetRadians),
              vp, hitFlashTint(enemyColorByArchetype(enemy.archetype),
                               hitFlashRemaining(s, enemy.id)),
              s.enemyAssetProfile, hitFlashRemaining(s, enemy.id),
              enemy.id == s.targetMarker3d.targetId,
              DeathFadeAlpha(enemy.deathSeconds), 1.0f,
              "enemy", &s.staffMesh, s.enemyWeaponJoint,
              enemyAttachmentOverride(s, enemy.archetype));
  }
  // 野外敌人（Phase 3.2/3.3）：复用同一 drawActor 路径与动画状态表
  // （id 从 5000 起无冲突），按原型缩放与色调区分。
  for (const WildEnemy3DRenderState& enemy : s.wildEnemies3d) {
    SkinnedAnimationState& animationState = s.enemyAnimationStates[enemy.id];
    const glm::vec2 knock = hitKnockback(s, enemy.id, enemy.angle);
    const float archetypeScale = enemyScaleByArchetype(enemy.archetype);
    const glm::vec3 enemyFeet{enemy.x + knock.x,
                              groundYAt(s, enemy.x, enemy.y) + 0.011f,
                              enemy.y + knock.y};
    // 视锥剔除（Phase 5）：按原型缩放后的保守包围球测试。
    if (!actorInFrustum(frustum, enemyFeet,
                        s.enemyAssetProfile.scale * archetypeScale)) {
      continue;
    }
    drawActor(s, s.enemyModel, s.enemyMesh, animationState, enemy.animation,
              actorModelMatrix(enemyFeet,
                               s.enemyAssetProfile.scale * archetypeScale,
                               enemy.angle +
                                   s.enemyAssetProfile.yawOffsetRadians),
              vp, hitFlashTint(enemyColorByArchetype(enemy.archetype),
                               hitFlashRemaining(s, enemy.id)),
              s.enemyAssetProfile, hitFlashRemaining(s, enemy.id),
              enemy.id == s.targetMarker3d.targetId,
              DeathFadeAlpha(enemy.deathSeconds), 1.0f,
              "enemy", &s.staffMesh, s.enemyWeaponJoint,
              enemyAttachmentOverride(s, enemy.archetype));
  }

  // NPC（Phase 4）：复用骨骼角色绘制路径，仅 idle/walk 动画，无血条。
  // 第一版模型复用 player.glb 占位，未注入时回退玩家静态 Mesh。
  s.pruneNpcAnimationStates();
  for (const Npc3DRenderState& npc : s.npcs3d) {
    if (!npc.visible) continue;
    SkinnedAnimationState& animationState = s.npcAnimationStates[npc.id];
    const glm::vec3 npcFeet{npc.x, groundYAt(s, npc.x, npc.y) + 0.011f,
                            npc.y};
    // 视锥剔除（Phase 5）：视锥外 NPC 跳过绘制。
    if (!actorInFrustum(frustum, npcFeet, s.npcAssetProfile.scale)) continue;
    drawActor(s, s.npcModel, s.playerMesh, animationState, npc.animation,
              actorModelMatrix(npcFeet,
                               s.npcAssetProfile.scale,
                               npc.angle + s.npcAssetProfile.yawOffsetRadians),
              vp, hitFlashTint(s.npcAssetProfile.materialTint, 0.0f),
              s.npcAssetProfile, 0.0f, false, 1.0f, 1.0f, "npc", nullptr, -1,
              npcAttachmentOverride(s, npc.id));
  }

  // 首领立方体（按阶段配色，击败后跳过）。
  if (s.boss3d.active) {
    // 首领受击后仰：体型更大，位移幅度略增。
    glm::vec2 bossKnock(0.0f);
    const float bossStrength =
        std::min(s.boss3d.hitAnimationSeconds / 0.2f, 1.0f);
    if (bossStrength > 0.0f) {
      const float amount = 0.006f * bossStrength * bossStrength;
      bossKnock = glm::vec2(-std::sin(s.boss3d.angle) * amount,
                            -std::cos(s.boss3d.angle) * amount);
    }
    const glm::vec3 bossFeet{s.boss3d.x + bossKnock.x,
                             groundYAt(s, s.boss3d.x, s.boss3d.y) + 0.02f,
                             s.boss3d.y + bossKnock.y};
    // 视锥剔除（Phase 5）：仅剔除首领本体绘制；出场演出几何
    // （cinematic 期间）保持无条件绘制，避免关键演出被误剔除。
    if (actorInFrustum(frustum, bossFeet, s.bossAssetProfile.scale)) {
      drawActor(s, s.bossModel, s.bossMesh, s.bossAnimationState,
                s.boss3d.animation,
                actorModelMatrix(bossFeet,
                                 s.bossAssetProfile.scale,
                                 s.boss3d.angle +
                                     s.bossAssetProfile.yawOffsetRadians),
               vp, hitFlashTint(bossColorByPhase(s.boss3d.phase),
                                s.boss3d.hitAnimationSeconds),
               s.bossAssetProfile, s.boss3d.hitAnimationSeconds,
               s.boss3d.targeted, 1.0f,
               BossEntranceReveal(s.boss3d.entranceSeconds), "boss",
               &s.clubMesh, s.bossWeaponJoint);
    }
    drawBossCinematicGeometry(s, vp);
  }

  // 普攻刀光：角色层之上、锁定环之下，结束后恢复状态。
  // 共鸣爆发光柱：先于刀光绘制，光柱包裹受击实体。
  drawLightPillars(s, vp);
  drawSlashArcs(s, vp);

  // 锁定目标指示器：绘制在飘字之下、实体之上。
  drawTargetMarker(s, vp);

  // 敌人头顶血条。
  drawEnemyHpBars(s, vp);

  // 命中火花：实体与飘字之间，结束后恢复轮廓光/高光状态。
  drawHitSparks(s, vp);
  s.shader3d.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  s.shader3d.setSpecular(0.28f, 24.0f);

  // 伤害飘字：最后绘制，深度只读不写，被前景实体正确遮挡。
  drawDamageNumbers(s, vp);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  // bloom 合成：亮通提取 → 半分辨率 ping-pong 模糊 → 场景+bloom 加法
  // 合成回默认帧缓冲；viewport 恢复为窗口尺寸（引擎无全局 viewport）。
  if (bloomActive) {
    runBloomPasses(s, bloomParams);
  }
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
  const Vec2 worldFacing{std::sin(s.player.angle), std::cos(s.player.angle)};
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

static void drawBossCinematicSW(const Surface& s, Canvas& c) {
  if (!s.boss3d.active) return;
  const Vec2 view = worldToNdc(s, {s.boss3d.x, s.boss3d.y});
  const Vec2 base = s.cameraRenderState.billboardNdcRadii(0.15f, aspect(s));
  const glm::vec3 color = bossCoreColor(s.boss3d.sourceColor);
  const uint32_t packed = packColor(color.r, color.g, color.b, 0.95f,
                                    c.swapRedBlue);
  const int cx = ndcToScreenX(s, view.x);
  const int cy = ndcToScreenY(s, view.y);
  const int rx = std::max(30, ndcToPixelRadiusX(s, base.x));
  const int ry = std::max(40, ndcToPixelRadiusY(s, base.y));
  for (int step = 0; step < 48; ++step) {
    if (s.boss3d.ringBroken && (step == 5 || step == 6 || step == 29)) continue;
    const float angle = 6.2831853f * static_cast<float>(step) / 48.0f;
    drawSolidEllipse(c, cx + static_cast<int>(std::cos(angle) * rx),
                     cy + static_cast<int>(std::sin(angle) * ry),
                     5, 5, packed);
  }
  drawSolidEllipse(c, cx, cy, std::max(10, rx / 4), std::max(10, ry / 4),
                   packed);
  for (uint8_t i = 0; i < s.boss3d.shardCount; ++i) {
    const float angle = 6.2831853f * static_cast<float>(i) / 3.0f +
                        s.boss3d.cinematicProgress * 2.2f;
    drawSolidEllipse(c, cx + static_cast<int>(std::cos(angle) * rx * 0.72f),
                     cy + static_cast<int>(std::sin(angle) * ry * 0.72f),
                     8, 8, packed);
  }
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
  drawBossCinematicSW(s, c);
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
  // 使用单位包络网格，通过 model 矩阵在 draw3DPhase 中缩放定位，
  // 避免为每种实体单独生成不同尺寸的几何体。角色回退使用多部件
  // 风格化人形（玩家英雄/敌人壮汉/Boss巨兽），保持与立方体同样的单位包络。
  s.playerMesh = createHumanoid();
  s.groundMesh = createPlane(1.0f, 1.0f);
  s.enemyMesh = createBrute();
  s.bossMesh = createBeast();
  s.bossRingMesh = createRing(0.42f, 0.055f, 24);
  s.targetRingMesh = createRing(0.075f, 0.014f, 40);
  // 普攻刀光新月弧线：单位外径（1.0），绘制时按角色缩放放大。
  s.slashArcMesh = createSlashArc(0.55f, 1.0f, 2.4f, 20);
  // 主角佩剑：按 handslot.r 关节矩阵挂载，随动画挥舞。
  s.swordMesh = createSword();
  // 敌方法杖与首领重棍：同一挂点机制。
  s.staffMesh = createStaff();
  s.clubMesh = createClub();
  // 地形/水面/天空：地形网格需要高度场，未注入时由 drawTerrain 惰性生成；
  // 水面为单位平面（绘制时抬升到水面高度），天空为单位球体（绘制时
  // 以相机为心放大成穹顶）。
  s.waterMesh = createPlane(1.0f, 1.0f);
  s.skyMesh = createSphere(1.0f, 24, 12);
  if (s.terrain != nullptr && s.terrainMesh.vertices.empty()) {
    s.terrainMesh = createTerrainMesh(*s.terrain, 96u);
  }
  // 接地接触阴影单位圆盘（半径 0.5，法线 +Y）。
  s.shadowMesh = createDisk(0.5f, 24);
  // 血条单位四边形（XY 平面，法线 +Z，无纹理）。
  s.hpBarQuadMesh.vertices = {
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
  };
  s.hpBarQuadMesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
  s.fallbackPillarMesh = createCylinder(0.025f, 0.12f, 16);
  s.fallbackWallMesh = createCube(1.0f);
  s.riftPlaneMesh = createPlane(1.0f, 1.0f);
  s.playerMesh.upload();
  s.groundMesh.upload();
  s.enemyMesh.upload();
  s.bossMesh.upload();
  s.bossRingMesh.upload();
  s.targetRingMesh.upload();
  s.slashArcMesh.upload();
  s.swordMesh.upload();
  s.staffMesh.upload();
  s.clubMesh.upload();
  s.hpBarQuadMesh.upload();
  s.shadowMesh.upload();
  s.fallbackPillarMesh.upload();
  s.fallbackWallMesh.upload();
  s.riftPlaneMesh.upload();
  s.terrainMesh.upload();
  s.waterMesh.upload();
  s.skyMesh.upload();
  s.shader3dReady = s.shader3d.init();
  if (!s.shader3dReady) {
    LOGE("3D shader init failed, 3D phase will be skipped");
  } else {
    LOGI("3D resources ready: shader=%{public}u", s.shader3d.program());
  }
  // bloom 程序与场景着色器独立：编译失败时 bloomReady 保持 false，
  // draw3DPhase 自动回退直绘（FBO 目标在首帧按需惰性创建）。
  s.bloomReady = initBloomProgram(s);
  if (!s.bloomReady) {
    LOGE("bloom program init failed, bloom disabled");
  }
  tryInitializePendingModelAssets(s);
  tryInitializePendingEnvironmentAssets(s);
  tryInitializePendingBlockEnvironmentAssets(s);
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
      s.npcModel.destroy();
      break;
    case SurfaceGlResource::StaticEnvironmentModels:
      for (StaticModel& model : s.environmentModels) model.destroy();
      for (auto& entry : s.blockEnvironmentModels) entry.second.destroy();
      break;
    case SurfaceGlResource::StaticMeshes:
      s.playerMesh.destroy();
      s.groundMesh.destroy();
      s.enemyMesh.destroy();
      s.bossMesh.destroy();
      s.fallbackPillarMesh.destroy();
      s.fallbackWallMesh.destroy();
      s.riftPlaneMesh.destroy();
      s.shadowMesh.destroy();
      s.terrainMesh.destroy();
      s.waterMesh.destroy();
      s.skyMesh.destroy();
      s.slashArcMesh.destroy();
      s.swordMesh.destroy();
      s.staffMesh.destroy();
      s.clubMesh.destroy();
      for (auto& entry : s.terrainChunkMeshes) entry.second.destroy();
      s.terrainChunkMeshes.clear();
      break;
    case SurfaceGlResource::BloomPipeline:
      // bloom 程序与 FBO/纹理：先于 Shader3D 销毁，互不依赖。
      if (s.bloomProgram != 0u) glDeleteProgram(s.bloomProgram);
      s.bloomProgram = 0;
      deleteBloomTargets(s);
      s.bloomReady = false;
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
        s.npcModelAsset.markDirtyForContextRebuild();
        for (PendingModelAsset& asset : s.environmentAssets) {
          asset.markDirtyForContextRebuild();
        }
        for (auto& entry : s.blockEnvironmentAssets) {
          entry.second.markDirtyForContextRebuild();
        }
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
  s.npcModel.abandonGpuResources();
  for (StaticModel& model : s.environmentModels) model.abandonGpuResources();
  for (auto& entry : s.blockEnvironmentModels) {
    entry.second.abandonGpuResources();
  }
  s.playerMesh.abandonGpuResources();
  s.groundMesh.abandonGpuResources();
  s.enemyMesh.abandonGpuResources();
  s.bossMesh.abandonGpuResources();
  s.fallbackPillarMesh.abandonGpuResources();
  s.fallbackWallMesh.abandonGpuResources();
  s.riftPlaneMesh.abandonGpuResources();
  s.targetRingMesh.abandonGpuResources();
  s.hpBarQuadMesh.abandonGpuResources();
  s.shadowMesh.abandonGpuResources();
  s.slashArcMesh.abandonGpuResources();
  s.swordMesh.abandonGpuResources();
  s.staffMesh.abandonGpuResources();
  s.clubMesh.abandonGpuResources();
  s.terrainMesh.abandonGpuResources();
  s.waterMesh.abandonGpuResources();
  s.skyMesh.abandonGpuResources();
  for (auto& entry : s.terrainChunkMeshes) entry.second.abandonGpuResources();
  s.terrainChunkMeshes.clear();
  for (Mesh& digitMesh : s.digitMeshes) digitMesh.abandonGpuResources();
  s.digitAtlasTexture = 0;
  s.digitAssetsReady = false;
  // bloom 句柄随 context 失效：仅清 CPU 跟踪，不调 GL 删除。
  s.bloomProgram = 0;
  s.bloomSceneFbo = 0;
  s.bloomSceneTex = 0;
  s.bloomDepthRbo = 0;
  s.bloomPingFbo = 0;
  s.bloomPongFbo = 0;
  s.bloomPingTex = 0;
  s.bloomPongTex = 0;
  s.bloomFboWidth = 0;
  s.bloomFboHeight = 0;
  s.bloomReady = false;
  s.shader3d.abandonGpuResources();
  s.shader3dReady = false;
  {
    std::lock_guard<std::mutex> lock(s.modelAssetMutex);
    s.playerModelAsset.markDirtyForContextRebuild();
    s.enemyModelAsset.markDirtyForContextRebuild();
    s.bossModelAsset.markDirtyForContextRebuild();
    s.npcModelAsset.markDirtyForContextRebuild();
    for (PendingModelAsset& asset : s.environmentAssets) {
      asset.markDirtyForContextRebuild();
    }
    for (auto& entry : s.blockEnvironmentAssets) {
      entry.second.markDirtyForContextRebuild();
    }
  }
#else
  (void)s;
#endif
}

static void clearModelAssets(Surface& s) {
  std::lock_guard<std::mutex> lock(s.modelAssetMutex);
  s.playerModelAsset.clear();
  s.enemyModelAsset.clear();
  s.bossModelAsset.clear();
  s.npcModelAsset.clear();
  for (size_t index = 0; index < s.environmentAssets.size(); ++index) {
    s.environmentAssets[index].clear();
    s.environmentStatuses[index] = EnvironmentBatchStatus::Empty;
  }
  s.environmentReady = false;
  s.environmentDrawCalls = 0;
  s.environmentTriangles = 0;
  s.loggedEnvironmentTextureTier = StaticTextureTier::Full;
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

  drawGradientSkyGL(s);
  drawGridGL(s);
  drawPropsGL(s);
  drawTrainingTargetGL(s);
  drawParticlesGL(s);
  drawPlayerGL(s);
#ifdef OHOS_PLATFORM
  draw3DPhase(s);
#endif
  drawVfxOverlayGL(s);
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
  s.wildEnemies3d.clear();
  s.enemyAnimationStates.clear();
  s.npcs3d.clear();
  s.npcAnimationStates.clear();
  s.playerAnimationState.reset();
  s.trainingTargetAnimationState.reset();
  s.bossAnimationState.reset();
  LOGI("Surface destroyed");
}
