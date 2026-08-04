#include "asset_profile.h"

AssetProfile AssetProfile::forModel(ModelKind kind) {
  switch (kind) {
    case ModelKind::Player:
      // 主角盔甲：高光最强最锐利，作为画面焦点。
      return {0.025f / 3.0f, 0.0f, {0.16f, 0.24f, 0.27f},
              {0.31f, 0.84f, 0.75f}, 0.75f, 0, 0.42f, 32.0f};
    case ModelKind::Enemy:
      // 敌人：哑光弱高光，退到背景不抢主角。
      return {0.022f / 3.0f, 0.0f, {0.24f, 0.20f, 0.25f},
              {0.45f, 0.30f, 0.48f}, 0.35f, 1, 0.14f, 12.0f};
    case ModelKind::Boss:
      // Boss：介于两者之间，宽而厚的高光强调体量。
      return {0.045f / 3.0f, 3.14159265f, {0.18f, 0.16f, 0.22f},
              {0.72f, 0.39f, 0.66f}, 0.65f, 3, 0.3f, 20.0f};
  }
  return {};
}
