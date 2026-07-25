#include "native/engine/presentation/visual_tokens.h"

#include <cassert>

namespace {

void testSourceColorsRemainDistinct() {
  const glm::vec3 radiance = VisualTokens::sourceColor(SourceType::Radiance);
  const glm::vec3 current = VisualTokens::sourceColor(SourceType::Current);
  const glm::vec3 corruption = VisualTokens::sourceColor(SourceType::Corruption);

  assert(radiance != current);
  assert(current != corruption);
  assert(radiance != corruption);
}

void testEnvironmentPaletteKeepsAltarAsBrightestFocus() {
  const EnvironmentPalette palette = VisualTokens::environmentPalette();
  const float ambientLuma = palette.ambient.r + palette.ambient.g + palette.ambient.b;
  const float altarLuma = palette.altarGlow.r + palette.altarGlow.g + palette.altarGlow.b;

  assert(altarLuma > ambientLuma);
  assert(palette.fogDensity > 0.0f);
  assert(palette.fogDensity < 1.0f);
}

}  // namespace

int main() {
  testSourceColorsRemainDistinct();
  testEnvironmentPaletteKeepsAltarAsBrightestFocus();
  return 0;
}
