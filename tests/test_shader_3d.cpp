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

}  // namespace

int main() {
  testSkinPaletteBoundariesAndInvalidation();
  testEnvironmentTintSetterIsSafeWithoutGlContext();
  testPresentationLightingSettersAreSafeWithoutGlContext();
  return 0;
}
