#include "native/gameplay/growth/character_growth.h"

#include <algorithm>

const std::vector<CharacterDef>& CharacterGrowth::roster() {
  static const std::vector<CharacterDef> characters = {
      {1, "辉印·莉拉", 5},
      {2, "脉流·凯恩", 5},
      {3, "蚀质·莫拉", 5},
      {4, "雾谷猎手·希娅", 4},
      {5, "遗迹守卫·奥尔丁", 4},
      {6, "辉光祭司·努恩", 4},
  };
  return characters;
}

const CharacterDef* CharacterGrowth::characterDef(int32_t characterId) {
  for (const CharacterDef& def : roster()) {
    if (def.id == characterId) return &def;
  }
  return nullptr;
}

int32_t CharacterGrowth::expRequired(int32_t level) {
  if (level < 1) return 20;
  return 20 + level * 10;
}

int32_t CharacterGrowth::levelCap(int32_t ascension) {
  const int32_t clamped = std::clamp(ascension, 0, kMaxAscension);
  return 20 * (clamped + 1);
}

int32_t CharacterGrowth::hpFor(int32_t characterId, int32_t level,
                               int32_t ascension, int32_t constellation) {
  const CharacterDef* def = characterDef(characterId);
  const int32_t rarityBonus = (def != nullptr && def->rarity == 5) ? 200 : 0;
  const int32_t clampedConstellation =
      std::clamp(constellation, 0, kMaxConstellation);
  return 800 + level * 40 + std::clamp(ascension, 0, kMaxAscension) * 300 +
         rarityBonus + clampedConstellation * 60;
}

int32_t CharacterGrowth::atkFor(int32_t characterId, int32_t level,
                                int32_t ascension, int32_t constellation) {
  const CharacterDef* def = characterDef(characterId);
  const int32_t rarityBonus = (def != nullptr && def->rarity == 5) ? 15 : 0;
  const int32_t clampedConstellation =
      std::clamp(constellation, 0, kMaxConstellation);
  return 60 + level * 6 + std::clamp(ascension, 0, kMaxAscension) * 25 +
         rarityBonus + clampedConstellation * 8;
}

bool CharacterGrowth::addCharacter(int32_t characterId) {
  if (characterDef(characterId) == nullptr || owns(characterId)) {
    return false;
  }
  owned_.push_back({characterId, 1, 0, 0, 0});
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedCharacter& left, const OwnedCharacter& right) {
              return left.characterId < right.characterId;
            });
  return true;
}

bool CharacterGrowth::restoreCharacter(int32_t characterId, int32_t level,
                                       int32_t ascension) {
  if (characterDef(characterId) == nullptr || owns(characterId)) {
    return false;
  }
  const int32_t clampedAscension = std::clamp(ascension, 0, kMaxAscension);
  const int32_t clampedLevel =
      std::clamp(level, 1, levelCap(clampedAscension));
  owned_.push_back({characterId, clampedLevel, clampedAscension, 0, 0});
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedCharacter& left, const OwnedCharacter& right) {
              return left.characterId < right.characterId;
            });
  return true;
}

bool CharacterGrowth::owns(int32_t characterId) const {
  return find(characterId) != nullptr;
}

const OwnedCharacter* CharacterGrowth::find(int32_t characterId) const {
  for (const OwnedCharacter& character : owned_) {
    if (character.characterId == characterId) return &character;
  }
  return nullptr;
}

int32_t CharacterGrowth::addExp(int32_t characterId, int32_t expAmount) {
  if (expAmount <= 0) return 0;
  auto character = std::find_if(
      owned_.begin(), owned_.end(),
      [characterId](const OwnedCharacter& candidate) {
        return candidate.characterId == characterId;
      });
  if (character == owned_.end()) return 0;
  int32_t levelsGained = 0;
  character->exp += expAmount;
  // 级联升级：到达当前突破上限后停止。
  while (character->level < levelCap(character->ascension)) {
    const int32_t required = expRequired(character->level);
    if (character->exp < required) break;
    character->exp -= required;
    character->level += 1;
    levelsGained += 1;
  }
  if (character->level >= levelCap(character->ascension)) {
    // 封顶后不再积累经验，避免无限堆积。
    character->exp = 0;
  }
  return levelsGained;
}

bool CharacterGrowth::ascend(int32_t characterId) {
  auto character = std::find_if(
      owned_.begin(), owned_.end(),
      [characterId](const OwnedCharacter& candidate) {
        return candidate.characterId == characterId;
      });
  if (character == owned_.end() ||
      character->ascension >= kMaxAscension ||
      character->level < levelCap(character->ascension)) {
    return false;
  }
  character->ascension += 1;
  character->exp = 0;
  return true;
}

bool CharacterGrowth::boostConstellation(int32_t characterId) {
  auto character = std::find_if(
      owned_.begin(), owned_.end(),
      [characterId](const OwnedCharacter& candidate) {
        return candidate.characterId == characterId;
      });
  if (character == owned_.end() ||
      character->constellation >= kMaxConstellation) {
    return false;
  }
  character->constellation += 1;
  return true;
}
