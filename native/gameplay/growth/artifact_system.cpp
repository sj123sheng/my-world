#include "native/gameplay/growth/artifact_system.h"

#include <algorithm>

namespace {

// 确定性 LCG 推进（与每日委托同模式）。
uint32_t nextRand(uint32_t& seed) {
  seed = seed * 1664525u + 1013904223u;
  return seed;
}

// 副属性基础值（按种类与稀有度）：1=固定生命 2=固定攻击 3=攻击% 4=暴击率%。
int32_t subStatBase(int32_t kind, int32_t rarity) {
  switch (kind) {
    case 1:
      return 40 + rarity * 20;
    case 2:
      return 8 + rarity * 4;
    case 3:
      return 2 + rarity;
    default:
      return 1 + rarity;
  }
}

}  // namespace

const std::vector<ArtifactDef>& ArtifactSystem::catalog() {
  static const std::vector<ArtifactDef> defs = {
      {11, "辉辉之芽", 1, ArtifactSlot::Flower},
      {12, "辉辉之羽", 1, ArtifactSlot::Plume},
      {13, "辉辉之时", 1, ArtifactSlot::Sands},
      {14, "辉辉之杯", 1, ArtifactSlot::Goblet},
      {15, "辉辉之冠", 1, ArtifactSlot::Circlet},
      {21, "潮鸣之花", 2, ArtifactSlot::Flower},
      {22, "潮鸣之羽", 2, ArtifactSlot::Plume},
      {23, "潮鸣之时", 2, ArtifactSlot::Sands},
      {24, "潮鸣之杯", 2, ArtifactSlot::Goblet},
      {25, "潮鸣之冠", 2, ArtifactSlot::Circlet},
      {31, "蚀痕之花", 3, ArtifactSlot::Flower},
      {32, "蚀痕之羽", 3, ArtifactSlot::Plume},
      {33, "蚀痕之时", 3, ArtifactSlot::Sands},
      {34, "蚀痕之杯", 3, ArtifactSlot::Goblet},
      {35, "蚀痕之冠", 3, ArtifactSlot::Circlet},
  };
  return defs;
}

const ArtifactDef* ArtifactSystem::artifactDef(int32_t defId) {
  for (const ArtifactDef& def : catalog()) {
    if (def.id == defId) return &def;
  }
  return nullptr;
}

std::string ArtifactSystem::setName(int32_t setId) {
  switch (setId) {
    case 1:
      return "辉辉骑士";
    case 2:
      return "潮鸣之诗";
    default:
      return "蚀痕残响";
  }
}

std::string ArtifactSystem::slotName(ArtifactSlot slot) {
  switch (slot) {
    case ArtifactSlot::Flower:
      return "生之花";
    case ArtifactSlot::Plume:
      return "死之羽";
    case ArtifactSlot::Sands:
      return "时之沙";
    case ArtifactSlot::Goblet:
      return "空之杯";
    default:
      return "理之冠";
  }
}

int32_t ArtifactSystem::maxLevelFor(int32_t rarity) {
  const int32_t clamped = std::clamp(rarity, kMinRarity, kMaxRarity);
  return clamped * 4;  // 3星12 / 4星16 / 5星20。
}

int32_t ArtifactSystem::mainStatKind(ArtifactSlot slot) {
  switch (slot) {
    case ArtifactSlot::Flower:
      return 1;
    case ArtifactSlot::Plume:
      return 2;
    case ArtifactSlot::Circlet:
      return 4;
    default:
      return 3;
  }
}

int32_t ArtifactSystem::mainStatValue(ArtifactSlot slot, int32_t rarity,
                                      int32_t level) {
  const int32_t r = std::clamp(rarity, kMinRarity, kMaxRarity);
  const int32_t l = std::max(level, 1);
  switch (slot) {
    case ArtifactSlot::Flower:
      return r * 100 + l * r * 20;
    case ArtifactSlot::Plume:
      return r * 15 + l * r * 3;
    case ArtifactSlot::Circlet:
      return r * 2 + l;
    default:
      return r * 3 + l;
  }
}

std::vector<ArtifactSubStat> ArtifactSystem::subStats(int32_t rarity,
                                                      int32_t level,
                                                      uint32_t seed) {
  const int32_t r = std::clamp(rarity, kMinRarity, kMaxRarity);
  const int32_t enhancements = (std::max(level, 1) - 1) / 4;
  const int32_t count =
      std::min(r - 2 + enhancements, kMaxSubStats);
  // 先生成完整 4 条的种类序列，取前 count 条，保证升级追加稳定。
  uint32_t state = seed;
  int32_t kinds[kMaxSubStats] = {0, 0, 0, 0};
  for (int32_t i = 0; i < kMaxSubStats; ++i) {
    kinds[i] = 1 + static_cast<int32_t>(nextRand(state) % 4u);
  }
  std::vector<ArtifactSubStat> stats;
  for (int32_t i = 0; i < count; ++i) {
    const int32_t base = subStatBase(kinds[i], r);
    stats.push_back(
        {kinds[i], base * (100 + 25 * enhancements) / 100});
  }
  return stats;
}

int32_t ArtifactSystem::feedExpValue(int32_t rarity, int32_t level) {
  const int32_t r = std::clamp(rarity, kMinRarity, kMaxRarity);
  return r * 500 + (std::max(level, 1) - 1) * 200;
}

int32_t ArtifactSystem::upgradeGoldCost(int32_t expGain) {
  return std::max(expGain, 0) / 10;
}

int32_t ArtifactSystem::dropDefId(uint32_t seed) {
  return 1 + static_cast<int32_t>(seed % 15u);
}

bool ArtifactSystem::addArtifact(int32_t defId, int32_t rarity,
                                 uint32_t substatSeed) {
  if (artifactDef(defId) == nullptr || rarity < kMinRarity ||
      rarity > kMaxRarity) {
    return false;
  }
  int32_t nextId = 0;
  for (const OwnedArtifact& artifact : owned_) {
    nextId = std::max(nextId, artifact.instanceId);
  }
  owned_.push_back({nextId + 1, defId, rarity, 1, 0, substatSeed});
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedArtifact& left, const OwnedArtifact& right) {
              return left.instanceId < right.instanceId;
            });
  return true;
}

const OwnedArtifact* ArtifactSystem::find(int32_t instanceId) const {
  for (const OwnedArtifact& artifact : owned_) {
    if (artifact.instanceId == instanceId) return &artifact;
  }
  return nullptr;
}

int32_t ArtifactSystem::addUpgradeExp(int32_t instanceId, int32_t expGain) {
  if (expGain <= 0) return 0;
  OwnedArtifact* target = nullptr;
  for (OwnedArtifact& artifact : owned_) {
    if (artifact.instanceId == instanceId) {
      target = &artifact;
      break;
    }
  }
  if (target == nullptr) return 0;
  const int32_t maxLevel = maxLevelFor(target->rarity);
  // 每级强化消耗与等级挂钩的确定性经验：500 + (level-1) * 250。
  int32_t levelsGained = 0;
  int32_t remaining = expGain;
  while (target->level < maxLevel) {
    const int32_t cost = 500 + (target->level - 1) * 250;
    if (remaining < cost) break;
    remaining -= cost;
    target->level += 1;
    levelsGained += 1;
  }
  return levelsGained;
}

int32_t ArtifactSystem::feedUpgrade(int32_t targetInstanceId,
                                    const std::vector<int32_t>& feedInstanceIds) {
  const OwnedArtifact* target = find(targetInstanceId);
  if (target == nullptr) return 0;
  // 先统计经验：跳过未拥有、已装备与目标自身。
  int32_t expGain = 0;
  std::vector<int32_t> validFeeds;
  for (const int32_t feedId : feedInstanceIds) {
    if (feedId == targetInstanceId) continue;
    const OwnedArtifact* feed = find(feedId);
    if (feed == nullptr || feed->equippedBy != 0) continue;
    expGain += feedExpValue(feed->rarity, feed->level);
    validFeeds.push_back(feedId);
  }
  if (expGain <= 0) return 0;
  for (const int32_t feedId : validFeeds) {
    auto it = std::find_if(
        owned_.begin(), owned_.end(),
        [feedId](const OwnedArtifact& candidate) {
          return candidate.instanceId == feedId;
        });
    if (it != owned_.end()) {
      owned_.erase(it);
    }
  }
  (void)addUpgradeExp(targetInstanceId, expGain);
  return expGain;
}

bool ArtifactSystem::equip(int32_t instanceId, int32_t characterId) {
  const OwnedArtifact* target = find(instanceId);
  if (characterId <= 0 || target == nullptr) return false;
  const ArtifactDef* def = artifactDef(target->defId);
  if (def == nullptr) return false;
  // 同角色同部位原圣遗物卸下；该圣遗物从原装备者解绑。
  for (OwnedArtifact& artifact : owned_) {
    if (artifact.equippedBy == characterId) {
      const ArtifactDef* other = artifactDef(artifact.defId);
      if (other != nullptr && other->slot == def->slot) {
        artifact.equippedBy = 0;
      }
    }
    if (artifact.instanceId == instanceId) {
      artifact.equippedBy = 0;
    }
  }
  for (OwnedArtifact& artifact : owned_) {
    if (artifact.instanceId == instanceId) {
      artifact.equippedBy = characterId;
      return true;
    }
  }
  return false;
}

bool ArtifactSystem::unequip(int32_t instanceId) {
  for (OwnedArtifact& artifact : owned_) {
    if (artifact.instanceId == instanceId) {
      if (artifact.equippedBy == 0) return false;
      artifact.equippedBy = 0;
      return true;
    }
  }
  return false;
}

int32_t ArtifactSystem::flatHpFor(int32_t characterId) const {
  int32_t total = 0;
  for (const OwnedArtifact& artifact : owned_) {
    if (artifact.equippedBy != characterId) continue;
    const ArtifactDef* def = artifactDef(artifact.defId);
    if (def == nullptr) continue;
    if (mainStatKind(def->slot) == 1) {
      total += mainStatValue(def->slot, artifact.rarity, artifact.level);
    }
    for (const ArtifactSubStat& sub :
         subStats(artifact.rarity, artifact.level, artifact.substatSeed)) {
      if (sub.kind == 1) total += sub.value;
    }
  }
  return total;
}

int32_t ArtifactSystem::flatAtkFor(int32_t characterId) const {
  int32_t total = 0;
  for (const OwnedArtifact& artifact : owned_) {
    if (artifact.equippedBy != characterId) continue;
    const ArtifactDef* def = artifactDef(artifact.defId);
    if (def == nullptr) continue;
    if (mainStatKind(def->slot) == 2) {
      total += mainStatValue(def->slot, artifact.rarity, artifact.level);
    }
    for (const ArtifactSubStat& sub :
         subStats(artifact.rarity, artifact.level, artifact.substatSeed)) {
      if (sub.kind == 2) total += sub.value;
    }
  }
  return total;
}

int32_t ArtifactSystem::percentAtkFor(int32_t characterId) const {
  int32_t percent = 0;
  for (const OwnedArtifact& artifact : owned_) {
    if (artifact.equippedBy != characterId) continue;
    const ArtifactDef* def = artifactDef(artifact.defId);
    if (def == nullptr) continue;
    if (mainStatKind(def->slot) == 3) {
      percent += mainStatValue(def->slot, artifact.rarity, artifact.level);
    }
    for (const ArtifactSubStat& sub :
         subStats(artifact.rarity, artifact.level, artifact.substatSeed)) {
      if (sub.kind == 3) percent += sub.value;
    }
  }
  return percent + setBonusPctFor(characterId);
}

int32_t ArtifactSystem::setBonusPctFor(int32_t characterId) const {
  int32_t setCounts[4] = {0, 0, 0, 0};
  for (const OwnedArtifact& artifact : owned_) {
    if (artifact.equippedBy != characterId) continue;
    const ArtifactDef* def = artifactDef(artifact.defId);
    if (def == nullptr) continue;
    if (def->setId >= 1 && def->setId <= 3) {
      setCounts[def->setId] += 1;
    }
  }
  int32_t percent = 0;
  // 套装效果：2 件套攻击 +10%，4 件套攻击 +25%。
  for (int32_t setId = 1; setId <= 3; ++setId) {
    if (setCounts[setId] >= 4) {
      percent += 25;
    } else if (setCounts[setId] >= 2) {
      percent += 10;
    }
  }
  return percent;
}

bool ArtifactSystem::restoreArtifact(int32_t instanceId, int32_t defId,
                                     int32_t rarity, int32_t level,
                                     int32_t equippedBy, uint32_t seed) {
  if (instanceId <= 0 || owns(instanceId) || artifactDef(defId) == nullptr) {
    return false;
  }
  const int32_t clampedRarity = std::clamp(rarity, kMinRarity, kMaxRarity);
  OwnedArtifact artifact;
  artifact.instanceId = instanceId;
  artifact.defId = defId;
  artifact.rarity = clampedRarity;
  artifact.level = std::clamp(level, 1, maxLevelFor(clampedRarity));
  artifact.equippedBy = equippedBy < 0 ? 0 : equippedBy;
  artifact.substatSeed = seed;
  owned_.push_back(artifact);
  std::sort(owned_.begin(), owned_.end(),
            [](const OwnedArtifact& left, const OwnedArtifact& right) {
              return left.instanceId < right.instanceId;
            });
  return true;
}
