#include "native/gameplay/growth/artifact_system.h"

#include <cassert>

int main() {
  // 图鉴：3 套装 x 5 部位 = 15 条定义。
  const std::vector<ArtifactDef>& catalog = ArtifactSystem::catalog();
  assert(catalog.size() == 15);
  for (const ArtifactDef& def : catalog) {
    assert(ArtifactSystem::artifactDef(def.id) != nullptr);
    assert(def.setId >= 1 && def.setId <= 3);
    assert(!ArtifactSystem::setName(def.setId).empty());
    assert(!ArtifactSystem::slotName(def.slot).empty());
  }
  assert(ArtifactSystem::artifactDef(99) == nullptr);

  // 稀有度强化上限：3星12 / 4星16 / 5星20。
  assert(ArtifactSystem::maxLevelFor(3) == 12);
  assert(ArtifactSystem::maxLevelFor(4) == 16);
  assert(ArtifactSystem::maxLevelFor(5) == 20);

  // 部位主属性：花=生命、羽=攻击、沙/杯=攻击%、冠=暴击率%。
  assert(ArtifactSystem::mainStatKind(ArtifactSlot::Flower) == 1);
  assert(ArtifactSystem::mainStatKind(ArtifactSlot::Plume) == 2);
  assert(ArtifactSystem::mainStatKind(ArtifactSlot::Sands) == 3);
  assert(ArtifactSystem::mainStatKind(ArtifactSlot::Goblet) == 3);
  assert(ArtifactSystem::mainStatKind(ArtifactSlot::Circlet) == 4);
  // 主属性随等级成长。
  assert(ArtifactSystem::mainStatValue(ArtifactSlot::Flower, 5, 20) >
         ArtifactSystem::mainStatValue(ArtifactSlot::Flower, 5, 1));
  assert(ArtifactSystem::mainStatValue(ArtifactSlot::Plume, 5, 1) >
         ArtifactSystem::mainStatValue(ArtifactSlot::Plume, 3, 1));

  // 副属性：确定性（同种子同结果）、条数随稀有度与强化增长。
  const std::vector<ArtifactSubStat> sub3 = ArtifactSystem::subStats(3, 1, 42);
  assert(sub3.size() == 1);
  const std::vector<ArtifactSubStat> sub5 = ArtifactSystem::subStats(5, 1, 42);
  assert(sub5.size() == 3);
  const std::vector<ArtifactSubStat> sub5Max =
      ArtifactSystem::subStats(5, 20, 42);
  assert(sub5Max.size() == 4);
  // 前 3 条在升级追加后保持不变（稳定性）。
  for (size_t i = 0; i < sub5.size(); ++i) {
    assert(sub5[i].kind == sub5Max[i].kind);
    assert(sub5Max[i].value >= sub5[i].value);
  }
  assert(ArtifactSystem::subStats(5, 1, 42)[0].kind ==
         ArtifactSystem::subStats(5, 1, 42)[0].kind);
  const std::vector<ArtifactSubStat> other = ArtifactSystem::subStats(5, 1, 7);
  bool anyDifferent = false;
  for (size_t i = 0; i < other.size(); ++i) {
    if (other[i].kind != sub5[i].kind) anyDifferent = true;
  }
  assert(anyDifferent);

  // 入包与实例 id 递增。
  ArtifactSystem artifacts;
  assert(artifacts.addArtifact(11, 5, 123u));
  assert(artifacts.addArtifact(22, 4, 456u));
  assert(!artifacts.addArtifact(99, 5, 1u));
  assert(!artifacts.addArtifact(11, 2, 1u));
  assert(artifacts.owned().size() == 2);
  assert(artifacts.owned()[0].instanceId == 1);
  assert(artifacts.owned()[1].instanceId == 2);

  // 强化：经验逐级消耗，稀有度封顶。
  const int32_t levels = artifacts.addUpgradeExp(1, 100000);
  assert(artifacts.find(1)->level == 20);
  assert(levels == 19);
  assert(artifacts.addUpgradeExp(1, 10000) == 0);
  // 素材经验与金币费用。
  assert(ArtifactSystem::feedExpValue(3, 1) == 1500);
  assert(ArtifactSystem::feedExpValue(5, 1) == 2500);
  assert(ArtifactSystem::upgradeGoldCost(2500) == 250);

  // 喂食强化：素材被移除、经验入账；已装备素材被跳过。
  artifacts.addArtifact(12, 3, 11u);
  artifacts.addArtifact(13, 3, 12u);
  const int32_t before = artifacts.find(2)->level;
  const int32_t gained = artifacts.feedUpgrade(2, {3, 4});
  assert(gained == ArtifactSystem::feedExpValue(3, 1) * 2);
  assert(artifacts.owned().size() == 2);
  assert(artifacts.find(2)->level >= before);
  // 目标自身与未拥有素材被跳过。
  assert(artifacts.feedUpgrade(2, {2, 99}) == 0);

  // 装备：同角色同部位互斥，换装自动卸下。
  artifacts.addArtifact(21, 5, 77u);  // 花（套装2）→ 实例 3
  artifacts.addArtifact(14, 5, 78u);  // 杯（套装1）→ 实例 4
  assert(artifacts.equip(1, 1));      // 花（套装1）装到角色1
  assert(artifacts.equip(3, 1));      // 套装2花顶替套装1花
  assert(artifacts.find(1)->equippedBy == 0);
  assert(artifacts.find(3)->equippedBy == 1);
  // 不同部位不互斥。
  assert(artifacts.equip(4, 1));
  assert(artifacts.find(3)->equippedBy == 1);
  assert(artifacts.find(4)->equippedBy == 1);
  // 圣遗物改绑其他角色：原角色解绑。
  assert(artifacts.equip(4, 2));
  assert(artifacts.find(4)->equippedBy == 2);
  assert(!artifacts.equip(99, 1));
  assert(!artifacts.equip(1, 0));
  assert(artifacts.unequip(4));
  assert(!artifacts.unequip(4));

  // 套装效果：同套装 2 件 +10%，4 件 +25%（隔离副属性干扰）。
  ArtifactSystem sets;
  sets.addArtifact(11, 5, 1u);
  sets.addArtifact(12, 5, 2u);
  sets.equip(1, 1);
  assert(sets.setBonusPctFor(1) == 0);
  sets.equip(2, 1);
  assert(sets.setBonusPctFor(1) == 10);
  assert(sets.percentAtkFor(1) >= 10);
  sets.addArtifact(13, 5, 3u);
  sets.addArtifact(14, 5, 4u);
  sets.equip(3, 1);
  sets.equip(4, 1);
  assert(sets.setBonusPctFor(1) == 25);
  // 混装两套装：各自 2 件各 +10%（部位错开避免互斥）。
  ArtifactSystem mixed;
  mixed.addArtifact(11, 5, 1u);  // 花
  mixed.addArtifact(12, 5, 2u);  // 羽
  mixed.addArtifact(23, 5, 3u);  // 沙
  mixed.addArtifact(24, 5, 4u);  // 杯
  mixed.equip(1, 1);
  mixed.equip(2, 1);
  mixed.equip(3, 1);
  mixed.equip(4, 1);
  assert(mixed.setBonusPctFor(1) == 20);

  // 派生属性：花提供固定生命、羽提供固定攻击。
  ArtifactSystem derived;
  derived.addArtifact(11, 5, 1u);
  derived.addArtifact(12, 5, 2u);
  derived.equip(1, 3);
  derived.equip(2, 3);
  assert(derived.flatHpFor(3) >=
         ArtifactSystem::mainStatValue(ArtifactSlot::Flower, 5, 1));
  assert(derived.flatAtkFor(3) >=
         ArtifactSystem::mainStatValue(ArtifactSlot::Plume, 5, 1));
  assert(derived.flatHpFor(9) == 0);

  // Boss 掉落定义确定性。
  assert(ArtifactSystem::dropDefId(0) == 1);
  assert(ArtifactSystem::dropDefId(14) == 15);
  assert(ArtifactSystem::dropDefId(15) == 1);

  // 存档恢复：钳制与去重。
  ArtifactSystem restored;
  assert(restored.restoreArtifact(5, 21, 9, 99, -1, 7u));
  assert(restored.find(5)->rarity == 5);
  assert(restored.find(5)->level == 20);
  assert(restored.find(5)->equippedBy == 0);
  assert(!restored.restoreArtifact(5, 21, 5, 1, 0, 7u));
  assert(!restored.restoreArtifact(6, 99, 5, 1, 0, 7u));

  return 0;
}
