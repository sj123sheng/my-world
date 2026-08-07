// test_shader_3d.cpp: 验证 Shader3D 的骨骼蒙皮状态机。

#include "native/engine/render/shader_3d.h"

#include <cassert>

namespace {

SkinPalette paletteWithJointCount(std::size_t count) {
  SkinPalette palette;
  palette.matrices.assign(count, glm::mat4(1.0f));
  return palette;
}

void testSkinPaletteBoundariesAndInvalidation() {
  Shader3D shader;

  shader.setSkinPalette(paletteWithJointCount(1));
  assert(shader.skinPaletteValid());
  shader.setSkinned(true);
  assert(shader.skinningEnabled());

  shader.setSkinPalette(paletteWithJointCount(kMaxSkinJoints));
  assert(shader.skinPaletteValid());
  shader.setSkinned(true);
  assert(shader.skinningEnabled());

  shader.setSkinPalette(paletteWithJointCount(0));
  assert(!shader.skinPaletteValid());
  assert(!shader.skinningEnabled());

  shader.setSkinPalette(paletteWithJointCount(kMaxSkinJoints + 1));
  assert(!shader.skinPaletteValid());
  assert(!shader.skinningEnabled());
}

void testEnvironmentTintSetterIsSafeWithoutGlContext() {
  Shader3D shader;
  shader.setEnvironmentTint({0.35f, 0.03f, 0.02f}, 0.5f);
}

void testPresentationLightingSettersAreSafeWithoutGlContext() {
  Shader3D shader;
  shader.setCameraPosition({0.5f, 0.4f, 0.2f});
  shader.setRim({0.62f, 0.72f, 0.85f}, 0.45f);
  shader.setSpecular(0.28f, 24.0f);
  shader.setAlpha(1.0f);
}

void testToonAndOutlineStateTrackWithoutGlContext() {
  Shader3D shader;
  // 默认关闭：未配置时与升级前行为等价。
  assert(!shader.toonShadingEnabled());
  assert(shader.outlineWidth() == 0.0f);

  shader.setToonShading(true, {0.74f, 0.70f, 0.86f}, 0.16f, 0.09f);
  assert(shader.toonShadingEnabled());
  shader.setToonShading(false, {0.7f, 0.7f, 0.78f}, 0.1f, 0.08f);
  assert(!shader.toonShadingEnabled());

  shader.setOutlinePass(0.02f, {0.1f, 0.2f, 0.3f});
  assert(shader.outlineWidth() > 0.0f);
  // 负宽度被夹取为 0：等价关闭描边 pass。
  shader.setOutlinePass(-1.0f, {0.0f, 0.0f, 0.0f});
  assert(shader.outlineWidth() == 0.0f);
  // destroy/abandon 后状态复位，宿主机侧不残留开启标记。
  shader.setToonShading(true, {0.7f, 0.7f, 0.78f}, 0.1f, 0.08f);
  shader.setOutlinePass(0.01f, {0.1f, 0.1f, 0.1f});
  shader.abandonGpuResources();
  assert(!shader.toonShadingEnabled());
  assert(shader.outlineWidth() == 0.0f);
}

}  // namespace

int main() {
  testSkinPaletteBoundariesAndInvalidation();
  testEnvironmentTintSetterIsSafeWithoutGlContext();
  testPresentationLightingSettersAreSafeWithoutGlContext();
  testToonAndOutlineStateTrackWithoutGlContext();
  return 0;
}
