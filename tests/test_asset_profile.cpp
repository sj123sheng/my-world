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

  assert(nearlyEqual(player.scale, 0.05f / 3.0f));
  assert(nearlyEqual(enemy.scale, 0.044f / 3.0f));
  assert(nearlyEqual(boss.scale, 0.09f / 3.0f));
  assert(boss.scale > player.scale);
  // boss3d.angle 已按 boss→player 方向计算，模型局部 +Z 为前方，
  // yawOffset 必须为 0，否则首领永远背对玩家。
  assert(nearlyEqual(boss.yawOffsetRadians, 0.0f));
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

void testProfilesCarryDistinctToonShading() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);
  const AssetProfile npc = AssetProfile::forModel(ModelKind::Npc);
  // 所有角色都启用卡通着色，且阴影色互不相同（角色识别度）。
  assert(player.toonShading && enemy.toonShading && boss.toonShading &&
         npc.toonShading);
  assert(player.shadowColor != enemy.shadowColor);
  assert(enemy.shadowColor != boss.shadowColor);
  assert(player.shadowColor != boss.shadowColor);
  // 阴影色是乘性暗部系数：各通道必须落在 (0,1)，否则暗部过曝或死黑。
  const AssetProfile all[] = {player, enemy, boss, npc};
  for (const AssetProfile& profile : all) {
    assert(profile.shadowColor.r > 0.0f && profile.shadowColor.r < 1.0f);
    assert(profile.shadowColor.g > 0.0f && profile.shadowColor.g < 1.0f);
    assert(profile.shadowColor.b > 0.0f && profile.shadowColor.b < 1.0f);
    assert(profile.toonSoftness > 0.0f);
  }
  // 敌人阴影阈值最低（暗部占比最大），主角最高（面部受光最多）。
  assert(enemy.toonShadowEdge < boss.toonShadowEdge);
  assert(boss.toonShadowEdge < player.toonShadowEdge);
}

void testOutlineWidthFollowsRoleHierarchyAndFlash() {
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);
  // 体量层级：Boss 描边最粗 > 主角 > 敌人。
  assert(boss.outlineWidth > player.outlineWidth);
  assert(player.outlineWidth > enemy.outlineWidth);
  assert(enemy.outlineWidth > 0.0f);
  // 受击闪白窗口内描边加宽，强化打击感；窗口外回落。
  const float calm = ActorOutlineWidthFor(player, 0.0f);
  const float flashing = ActorOutlineWidthFor(player, 0.15f);
  assert(flashing > calm);
  assert(nearlyEqual(ActorOutlineWidthFor(player, 1.0f), flashing));
  // 锁定目标描边略增；出场进度线性缩放（0 时不出现描边）。
  assert(ActorOutlineWidthFor(enemy, 0.0f, true) >
         ActorOutlineWidthFor(enemy, 0.0f, false));
  assert(nearlyEqual(ActorOutlineWidthFor(boss, 0.0f, false, 0.0f), 0.0f));
  assert(nearlyEqual(ActorOutlineWidthFor(boss, 0.0f, false, 0.5f),
                     ActorOutlineWidthFor(boss, 0.0f) * 0.5f));
}

void testOutlineColorDerivesFromRimAndWhitensOnFlash() {
  const AssetProfile enemy = AssetProfile::forModel(ModelKind::Enemy);
  const glm::vec3 calm = ActorOutlineColorFor(enemy, 0.0f);
  const glm::vec3 flashing = ActorOutlineColorFor(enemy, 0.15f);
  // 线色保持色相但整体压暗（低于轮廓光原色亮度）。
  assert(calm.r < enemy.outlineColor.r);
  assert(calm.g < enemy.outlineColor.g);
  assert(calm.b < enemy.outlineColor.b);
  // 受击闪白时线色向白靠拢（各通道抬升）。
  assert(flashing.r > calm.r);
  assert(flashing.g > calm.g);
  assert(flashing.b > calm.b);
  // 三角色线色互不相同，与轮廓光同源的视觉语言保持一致。
  const AssetProfile player = AssetProfile::forModel(ModelKind::Player);
  const AssetProfile boss = AssetProfile::forModel(ModelKind::Boss);
  assert(ActorOutlineColorFor(player, 0.0f) != calm);
  assert(ActorOutlineColorFor(boss, 0.0f) != calm);
}

void testEnemyArchetypeScaleMatchesVisualRoles() {
  assert(nearlyEqual(EnemyArchetypeScale(0), 1.0f));
  assert(nearlyEqual(EnemyArchetypeScale(3), 1.3f));
  assert(nearlyEqual(EnemyArchetypeScale(5), 1.3f));
  assert(nearlyEqual(EnemyArchetypeScale(4), 0.95f));
  // 未知原型回退默认体型，不产生 0/负缩放。
  assert(nearlyEqual(EnemyArchetypeScale(99), 1.0f));
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
  testProfilesCarryDistinctToonShading();
  testOutlineWidthFollowsRoleHierarchyAndFlash();
  testOutlineColorDerivesFromRimAndWhitensOnFlash();
  testEnemyArchetypeScaleMatchesVisualRoles();
  return 0;
}
