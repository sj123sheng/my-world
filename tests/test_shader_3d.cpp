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

}  // namespace

int main() {
  testSkinPaletteBoundariesAndInvalidation();
  return 0;
}
