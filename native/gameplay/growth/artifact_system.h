#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 圣遗物系统（原神式）：5 部位 x 3 套装，稀有度 3..5 星；
// 主属性按部位固定，副属性由种子确定性生成；
// 强化消耗金币与低星圣遗物素材，装备到角色叠加派生属性，
// 同套装 2/4 件套触发攻击加成。全部规则确定性、无随机。
enum class ArtifactSlot : int32_t {
  Flower = 1,  // 生之花（主属性：固定生命）
  Plume = 2,   // 死之羽（主属性：固定攻击）
  Sands = 3,   // 时之沙（主属性：攻击%）
  Goblet = 4,  // 空之杯（主属性：攻击%）
  Circlet = 5  // 理之冠（主属性：暴击率%）
};

// 主/副属性种类：1=固定生命 2=固定攻击 3=攻击% 4=暴击率%。
struct ArtifactDef {
  int32_t id = 0;         // setId * 10 + slot
  std::string name;
  int32_t setId = 0;
  ArtifactSlot slot = ArtifactSlot::Flower;
};

struct ArtifactSubStat {
  int32_t kind = 0;
  int32_t value = 0;
};

struct OwnedArtifact {
  int32_t instanceId = 0;
  int32_t defId = 0;
  int32_t rarity = 3;
  int32_t level = 1;
  int32_t equippedBy = 0;   // 角色 id；0 表示未装备。
  uint32_t substatSeed = 0; // 副属性确定性种子。
};

class ArtifactSystem {
 public:
  static constexpr int32_t kMinRarity = 3;
  static constexpr int32_t kMaxRarity = 5;
  static constexpr int32_t kMaxSubStats = 4;

  // 图鉴：3 套装 x 5 部位 = 15 条定义。
  static const std::vector<ArtifactDef>& catalog();
  static const ArtifactDef* artifactDef(int32_t defId);
  static std::string setName(int32_t setId);
  static std::string slotName(ArtifactSlot slot);

  // 稀有度决定强化上限：3星12级 / 4星16级 / 5星20级。
  static int32_t maxLevelFor(int32_t rarity);
  // 部位主属性种类。
  static int32_t mainStatKind(ArtifactSlot slot);
  // 主属性数值（随稀有度与等级成长）。
  static int32_t mainStatValue(ArtifactSlot slot, int32_t rarity,
                               int32_t level);
  // 副属性列表（确定性）：初始条数 = 稀有度-2，每 +4 级追加一条（上限 4）。
  static std::vector<ArtifactSubStat> subStats(int32_t rarity, int32_t level,
                                               uint32_t seed);
  // 作为强化素材提供的经验。
  static int32_t feedExpValue(int32_t rarity, int32_t level);
  // 强化金币费用：经验的十分之一（整数折算）。
  static int32_t upgradeGoldCost(int32_t expGain);
  // Boss 掉落圣遗物的定义 id（确定性）。
  static int32_t dropDefId(uint32_t seed);

  // 掉落/奖励入包：实例 id 自动递增；非法返回 false。
  bool addArtifact(int32_t defId, int32_t rarity, uint32_t substatSeed);
  const OwnedArtifact* find(int32_t instanceId) const;
  bool owns(int32_t instanceId) const { return find(instanceId) != nullptr; }

  // 注入强化经验并级联升级（上限由稀有度决定）。返回升了几级。
  int32_t addUpgradeExp(int32_t instanceId, int32_t expGain);
  // 喂食强化：移除未装备的素材圣遗物并累加其经验。
  // 返回获得经验；素材含已装备或目标自身时跳过该素材。
  int32_t feedUpgrade(int32_t targetInstanceId,
                      const std::vector<int32_t>& feedInstanceIds);

  // 装备：同角色同部位原圣遗物自动卸下；圣遗物原装备者解绑。
  bool equip(int32_t instanceId, int32_t characterId);
  bool unequip(int32_t instanceId);

  // 派生加成：角色已装备圣遗物的固定生命/固定攻击/攻击百分比
  //（含主属性、副属性与套装 2/4 件套加成）。
  int32_t flatHpFor(int32_t characterId) const;
  int32_t flatAtkFor(int32_t characterId) const;
  int32_t percentAtkFor(int32_t characterId) const;
  // 仅套装 2/4 件套提供的攻击百分比（供 UI 展示与测试隔离）。
  int32_t setBonusPctFor(int32_t characterId) const;

  // 存档恢复：非法值钳制；实例 id 重复返回 false。
  bool restoreArtifact(int32_t instanceId, int32_t defId, int32_t rarity,
                       int32_t level, int32_t equippedBy, uint32_t seed);

  const std::vector<OwnedArtifact>& owned() const { return owned_; }

 private:
  std::vector<OwnedArtifact> owned_;
};
