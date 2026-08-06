#include "native/gameplay/growth/gacha_system.h"

#include "native/gameplay/growth/character_growth.h"

#include <cassert>

int main() {
  GachaSystem gacha;

  // 确定性：相同初始状态两次抽取结果完全一致。
  GachaState stateA;
  GachaState stateB;
  const std::vector<GachaPull> sequenceA = gacha.draw(stateA, 30);
  const std::vector<GachaPull> sequenceB = gacha.draw(stateB, 30);
  assert(sequenceA.size() == 30);
  for (size_t i = 0; i < sequenceA.size(); ++i) {
    assert(sequenceA[i].characterId == sequenceB[i].characterId);
    assert(sequenceA[i].rarity == sequenceB[i].rarity);
    assert(sequenceA[i].pity5 == sequenceB[i].pity5);
  }
  assert(stateA.seed == stateB.seed);
  assert(stateA.since5 == stateB.since5);
  assert(stateA.since4 == stateB.since4);

  // 结果有效性：角色 id 在图鉴内且稀有度与星级池一致。
  for (const GachaPull& pull : sequenceA) {
    const CharacterDef* def = CharacterGrowth::characterDef(pull.characterId);
    assert(def != nullptr);
    assert(def->rarity == pull.rarity);
    assert(pull.rarity >= 4);
  }

  // 五星硬保底：连续 90 抽内必然出现五星。
  GachaState pityState;
  const std::vector<GachaPull> pitySequence = gacha.draw(pityState, 90);
  bool sawFiveStar = false;
  bool sawPityFlag = false;
  for (const GachaPull& pull : pitySequence) {
    if (pull.rarity == 5) sawFiveStar = true;
    if (pull.pity5) sawPityFlag = true;
  }
  assert(sawFiveStar);
  assert(sawPityFlag);
  // 出五星后保底计数清零。
  assert(pityState.since5 < 90);

  // 十连内必出四星及以上（本样板全池四星以上，恒成立）。
  GachaState fourState;
  for (const GachaPull& pull : gacha.draw(fourState, 10)) {
    assert(pull.rarity >= 4);
  }

  // 非法配置被规范化。
  GachaConfig broken;
  broken.p5 = -1.0f;
  broken.p4 = 0.0f;
  broken.pity5At = 0;
  broken.pity4At = -3;
  GachaSystem guarded(broken);
  assert(guarded.config().p5 > 0.0f);
  assert(guarded.config().p4 > 0.0f);
  assert(guarded.config().pity5At == 90);
  assert(guarded.config().pity4At == 10);

  // 大规模抽取不产生非法结果。
  GachaState longState;
  const std::vector<GachaPull> longSequence = gacha.draw(longState, 1000);
  int fiveStarCount = 0;
  for (const GachaPull& pull : longSequence) {
    assert(CharacterGrowth::characterDef(pull.characterId) != nullptr);
    if (pull.rarity == 5) ++fiveStarCount;
  }
  // 1000 抽至少 11 次五星（90 硬保底上限保证）。
  assert(fiveStarCount >= 11);

  // count <= 0 返回空。
  GachaState idleState;
  assert(gacha.draw(idleState, 0).empty());
  assert(gacha.draw(idleState, -5).empty());

  // 武器卡池（抽卡优化）：确定性、五星落 1-3、四星落 4-5、硬保底生效。
  GachaState weaponStateA;
  GachaState weaponStateB;
  const std::vector<GachaPull> weaponSeqA = gacha.drawWeapon(weaponStateA, 20);
  const std::vector<GachaPull> weaponSeqB = gacha.drawWeapon(weaponStateB, 20);
  assert(weaponSeqA.size() == 20);
  for (size_t i = 0; i < weaponSeqA.size(); ++i) {
    assert(weaponSeqA[i].characterId == weaponSeqB[i].characterId);
    assert(weaponSeqA[i].rarity == weaponSeqB[i].rarity);
  }
  GachaState weaponLongState;
  const std::vector<GachaPull> weaponLong =
      gacha.drawWeapon(weaponLongState, 1000);
  int weaponFiveStars = 0;
  for (const GachaPull& pull : weaponLong) {
    assert(pull.characterId >= 1 && pull.characterId <= 5);
    if (pull.rarity == 5) {
      assert(pull.characterId >= 1 && pull.characterId <= 3);
      ++weaponFiveStars;
    } else {
      assert(pull.rarity == 4);
      assert(pull.characterId >= 4 && pull.characterId <= 5);
    }
  }
  // 90 抽硬保底：1000 抽至少 11 次五星。
  assert(weaponFiveStars >= 11);
  GachaState weaponIdle;
  assert(gacha.drawWeapon(weaponIdle, 0).empty());
  assert(gacha.drawWeapon(weaponIdle, -2).empty());
  return 0;
}
