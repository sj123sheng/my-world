#include "native/engine/render/asset_profile.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float left, float right) {
  return std::fabs(left - right) < 0.0001f;
}

void testProfilesProduceUsableActorTransforms() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);

  assert(nearlyEqual(player.scale, 0.025f / 3.0f));
  assert(nearlyEqual(enemy.scale, 0.022f / 3.0f));
  assert(nearlyEqual(boss.scale, 0.045f / 3.0f));
  assert(boss.scale > player.scale);
  assert(!nearlyEqual(boss.yawOffsetRadians, 0.0f));
}

void testProfilesKeepVisualRolesDistinct() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);

  assert(player.outlineStrength > enemy.outlineStrength);
  assert(boss.coreMountCount == 3);
  assert(player.materialTint != enemy.materialTint);
}

void testProfilesCarryDistinctSpecularMaterials() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);

  // 主角盔甲高光最强最锐利，是画面焦点；敌人压暗哑光退到背景；
  // Boss 介于两者之间，三者参数互不相同。
  assert(player.specularStrength > boss.specularStrength);
  assert(boss.specularStrength > enemy.specularStrength);
  assert(player.specularShininess > boss.specularShininess);
  assert(boss.specularShininess > enemy.specularShininess);
  assert(enemy.specularStrength > 0.0f);
}

void testActorRimUsesProfileOutlineWhenCalm() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const ActorRimLight rim = ActorRimLightFor(player, 0.0f);
  assert(rim.color == player.outlineColor);
  assert(nearlyEqual(rim.strength, player.outlineStrength));
}

void testActorRimBoostsAndWhitensDuringHitFlash() {
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const ActorRimLight calm = ActorRimLightFor(enemy, 0.0f);
  const ActorRimLight flashing = ActorRimLightFor(enemy, 0.15f);
  // 受击窗口内轮廓光强度提升、颜色向白色靠拢，强化“打中了”的轮廓反馈。
  assert(flashing.strength > calm.strength);
  const float calmLuma = calm.color.r + calm.color.g + calm.color.b;
  const float flashLuma = flashing.color.r + flashing.color.g + flashing.color.b;
  assert(flashLuma > calmLuma);
  // 闪白窗口封顶 0.15s，超出不再继续叠加。
  const ActorRimLight capped = ActorRimLightFor(enemy, 1.0f);
  assert(nearlyEqual(capped.strength, flashing.strength));
  assert(capped.color == flashing.color);
}

void testActorRimsStayDistinctPerRole() {
  const ActorRimLight player =
      ActorRimLightFor(AssetProfile::forModel(ModelKind::Player), 0.0f);
  const ActorRimLight enemy =
      ActorRimLightFor(AssetProfile::forModel(ModelKind::Enemy), 0.0f);
  const ActorRimLight boss =
      ActorRimLightFor(AssetProfile::forModel(ModelKind::Boss), 0.0f);
  assert(player.color != enemy.color);
  assert(enemy.color != boss.color);
  assert(player.color != boss.color);
}

void testLockedTargetRimStaysBoostedAndTinted() {
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const ActorRimLight calm = ActorRimLightFor(enemy, 0.0f);
  const ActorRimLight locked = ActorRimLightFor(enemy, 0.0f, true);
  // 锁定目标轮廓常亮：强度稳定抬升，颜色向锁定环青金色靠拢，
  // 与脚下锁定环共享同一套视觉语言。
  assert(locked.strength > calm.strength);
  assert(locked.color != calm.color);
  assert(locked.color.g > locked.color.r);
  // 受击闪白与锁定叠加：两种增强同时生效。
  const ActorRimLight lockedFlashing = ActorRimLightFor(enemy, 0.15f, true);
  const ActorRimLight flashing = ActorRimLightFor(enemy, 0.15f);
  assert(lockedFlashing.strength > flashing.strength);
  assert(lockedFlashing.strength > locked.strength);
}

void testBossEntranceRampsRimStrength() {
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);
  const ActorRimLight full = ActorRimLightFor(boss, 0.0f);
  // 出场进度线性抬升轮廓光强度，颜色保持不变：Boss 从黑暗中渐显。
  const ActorRimLight half = ActorRimLightFor(boss, 0.0f, false, 0.5f);
  const ActorRimLight none = ActorRimLightFor(boss, 0.0f, false, 0.0f);
  assert(nearlyEqual(half.strength, full.strength * 0.5f));
  assert(nearlyEqual(none.strength, 0.0f));
  assert(half.color == full.color);
  // 越界进度被夹取：超过 1 不再增强，负值等同未出场。
  const ActorRimLight over = ActorRimLightFor(boss, 0.0f, false, 2.0f);
  assert(nearlyEqual(over.strength, full.strength));
  const ActorRimLight negative = ActorRimLightFor(boss, 0.0f, false, -1.0f);
  assert(nearlyEqual(negative.strength, 0.0f));
  // 受击增强同样受出场进度缩放。
  const ActorRimLight flashingFull = ActorRimLightFor(boss, 0.15f);
  const ActorRimLight flashingHalf = ActorRimLightFor(boss, 0.15f, false, 0.5f);
  assert(nearlyEqual(flashingHalf.strength, flashingFull.strength * 0.5f));
}

void testBossEntranceRevealCurveReachesFull() {
  // 出场曲线：0.8s 内线性从 0 到 1，之后保持完全显现。
  assert(nearlyEqual(BossEntranceReveal(0.0f), 0.0f));
  assert(nearlyEqual(BossEntranceReveal(0.4f), 0.5f));
  assert(nearlyEqual(BossEntranceReveal(0.8f), 1.0f));
  assert(nearlyEqual(BossEntranceReveal(5.0f), 1.0f));
  assert(nearlyEqual(BossEntranceReveal(-1.0f), 0.0f));
}

}  // namespace

int main() {
  testProfilesProduceUsableActorTransforms();
  testProfilesKeepVisualRolesDistinct();
  testProfilesCarryDistinctSpecularMaterials();
  testActorRimUsesProfileOutlineWhenCalm();
  testActorRimBoostsAndWhitensDuringHitFlash();
  testActorRimsStayDistinctPerRole();
  testLockedTargetRimStaysBoostedAndTinted();
  testBossEntranceRampsRimStrength();
  testBossEntranceRevealCurveReachesFull();
  return 0;
}
