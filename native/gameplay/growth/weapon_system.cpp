#include "native/gameplay/growth/weapon_system.h"

#include <algorithm>

const std::vector<WeaponDef>& WeaponSystem::catalog() {
  static const std::vector<WeaponDef> weapons = {
      {1, "辉印之章", 5, 48},
      {2, "涟漪之刃", 5, 48},
      {3, "蚀灭之镰", 5, 48},
      {4, "雾猎之弓", 4, 40},
      {5, "守望大剑", 4, 40},
  };
  return weapons;
}

const WeaponDef* WeaponSystem::weaponDef(int32_t weaponId) {
  for (const WeaponDef& def : catalog()) {
    if (def.id == weaponId) return &def;
  }
  return nullptr;
}

int32_t WeaponSystem::upgradeCost(int32_t level) {
  if (level < 1) return 50;
  return 50 + level * 20;
}

int32_t WeaponSystem::expRequired(int32_t level) {
  if (level < 1) return 200;
  if (level >= kMaxLevel) return 0;
  // 武器经验曲线：200 起步，每级 +100（矿石 400/2000/10000 可整除段）。
  return 200 + (level - 1) * 100;
}

int32_t WeaponSystem::levelCap(int32_t ascension) {
  static const int32_t kCaps[] = {20, 40, 50, 60, 70, 80, 90};
  const int32_t clamped = std::clamp(ascension, 0, kMaxAscension);
  return kCaps[clamped];
}

int32_t WeaponSystem::ascensionMaterialCost(int32_t ascension) {
  const int32_t clamped = std::clamp(ascension, 0, kMaxAscension - 1);
  return 2 + clamped;
}

int32_t WeaponSystem::ascensionGoldCost(int32_t ascension) {
  const int32_t clamped = std::clamp(ascension, 0, kMaxAscension - 1);
  return 100 + clamped * 100;
}

int32_t WeaponSystem::atkOf(int32_t weaponId, int32_t level, int32_t refine) {
  const WeaponDef* def = weaponDef(weaponId);
  if (def == nullptr) return 0;
  const int32_t clampedLevel = std::clamp(level, 1, kMaxLevel);
  const int32_t clampedRefine = std::clamp(refine, 1, kMaxRefine);
  const int32_t growth = def->rarity == 5 ? 8 : 5;
  const int32_t base = def->baseAtk + (clampedLevel - 1) * growth;
  // 精炼：每阶 +10%（整数折算）。
  return base * (100 + (clampedRefine - 1) * 10) / 100;
}

bool WeaponSystem::addWeapon(int32_t weaponId) {
  if (weaponDef(weaponId) == nullptr || owns(weaponId)) {
    return false;
  }
  owned_.push_back({weaponId, 1, 0, 1, 0, 0, 0});
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedWeapon& left, const OwnedWeapon& right) {
              return left.weaponId < right.weaponId;
            });
  return true;
}

bool WeaponSystem::addRefineStock(int32_t weaponId) {
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId == weaponId) {
      weapon.refineStock += 1;
      return true;
    }
  }
  return false;
}

bool WeaponSystem::owns(int32_t weaponId) const {
  return find(weaponId) != nullptr;
}

const OwnedWeapon* WeaponSystem::find(int32_t weaponId) const {
  for (const OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId == weaponId) return &weapon;
  }
  return nullptr;
}

bool WeaponSystem::upgrade(int32_t weaponId) {
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId != weaponId) continue;
    if (weapon.level >= levelCap(weapon.ascension)) return false;
    weapon.level += 1;
    return true;
  }
  return false;
}

int32_t WeaponSystem::addWeaponExp(int32_t weaponId, int32_t expAmount) {
  if (expAmount <= 0) return 0;
  OwnedWeapon* target = nullptr;
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId == weaponId) {
      target = &weapon;
      break;
    }
  }
  if (target == nullptr) return 0;
  int32_t levelsGained = 0;
  target->exp += expAmount;
  // 级联升级：到达当前突破上限后停止积累。
  while (target->level < levelCap(target->ascension) &&
         target->level < kMaxLevel) {
    const int32_t required = expRequired(target->level);
    if (target->exp < required) break;
    target->exp -= required;
    target->level += 1;
    levelsGained += 1;
  }
  if (target->level >= levelCap(target->ascension)) {
    // 封顶后不再积累经验，等待突破。
    target->exp = 0;
  }
  return levelsGained;
}

bool WeaponSystem::ascend(int32_t weaponId) {
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId != weaponId) continue;
    if (weapon.ascension >= kMaxAscension ||
        weapon.level < levelCap(weapon.ascension)) {
      return false;
    }
    weapon.ascension += 1;
    weapon.exp = 0;
    return true;
  }
  return false;
}

bool WeaponSystem::refine(int32_t weaponId) {
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId != weaponId) continue;
    if (weapon.refine >= kMaxRefine || weapon.refineStock <= 0) {
      return false;
    }
    weapon.refineStock -= 1;
    weapon.refine += 1;
    return true;
  }
  return false;
}

bool WeaponSystem::equip(int32_t weaponId, int32_t characterId) {
  if (characterId <= 0 || find(weaponId) == nullptr) return false;
  // 先卸下：目标角色原武器与该武器的原装备者。
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.equippedBy == characterId || weapon.weaponId == weaponId) {
      weapon.equippedBy = 0;
    }
  }
  for (OwnedWeapon& weapon : owned_) {
    if (weapon.weaponId == weaponId) {
      weapon.equippedBy = characterId;
      return true;
    }
  }
  return false;
}

int32_t WeaponSystem::equippedBonusFor(int32_t characterId) const {
  for (const OwnedWeapon& weapon : owned_) {
    if (weapon.equippedBy == characterId) {
      return atkOf(weapon.weaponId, weapon.level, weapon.refine);
    }
  }
  return 0;
}

bool WeaponSystem::restoreWeapon(int32_t weaponId, int32_t level,
                                 int32_t equippedBy) {
  return restoreWeapon(weaponId, level, 0, 1, 0, 0, equippedBy);
}

bool WeaponSystem::restoreWeapon(int32_t weaponId, int32_t level,
                                 int32_t ascension, int32_t refine,
                                 int32_t refineStock, int32_t exp,
                                 int32_t equippedBy) {
  if (weaponDef(weaponId) == nullptr || owns(weaponId)) {
    return false;
  }
  const int32_t clampedAscension = std::clamp(ascension, 0, kMaxAscension);
  const int32_t clampedLevel = std::clamp(level, 1, levelCap(clampedAscension));
  OwnedWeapon weapon;
  weapon.weaponId = weaponId;
  weapon.level = clampedLevel;
  weapon.ascension = clampedAscension;
  weapon.refine = std::clamp(refine, 1, kMaxRefine);
  weapon.refineStock = std::max(refineStock, 0);
  weapon.exp = clampedLevel >= levelCap(clampedAscension)
                   ? 0
                   : std::clamp(exp, 0, expRequired(clampedLevel) - 1);
  weapon.equippedBy = equippedBy < 0 ? 0 : equippedBy;
  owned_.push_back(weapon);
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedWeapon& left, const OwnedWeapon& right) {
              return left.weaponId < right.weaponId;
            });
  return true;
}
