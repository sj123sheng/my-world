#include "asset_profile.h"

AssetProfile AssetProfile::forModel(ModelKind kind) {
  switch (kind) {
    case ModelKind::Player:
      return {0.025f / 3.0f, 0.0f, {0.16f, 0.24f, 0.27f},
              {0.31f, 0.84f, 0.75f}, 0.75f, 0};
    case ModelKind::Enemy:
      return {0.022f / 3.0f, 0.0f, {0.24f, 0.20f, 0.25f},
              {0.45f, 0.30f, 0.48f}, 0.35f, 1};
    case ModelKind::Boss:
      return {0.045f / 3.0f, 3.14159265f, {0.18f, 0.16f, 0.22f},
              {0.72f, 0.39f, 0.66f}, 0.65f, 3};
  }
  return {};
}
