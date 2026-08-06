#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 武器系统（原神式深化）：武器通过任务奖励与卡池获取，
// 消耗矿石类材料获得武器经验强化，到达突破阶段上限后消耗
// 突破材料进阶（等级上限 20/40/50/60/70/80/90），
// 重复武器转化为精炼素材提升精炼阶（1..5，每阶 +10% 攻击）。
// 全部规则确定性、无随机，可独立测试。
struct WeaponDef {
  int32_t id = 0;
  std::string name;
  int32_t rarity = 4;      // 4 或 5 星
  int32_t baseAtk = 40;    // 1 级基础攻击
};

struct OwnedWeapon {
  int32_t weaponId = 0;
  int32_t level = 1;
  int32_t ascension = 0;   // 突破阶段 0..6。
  int32_t refine = 1;      // 精炼阶 1..5。
  int32_t refineStock = 0; // 同名武器精炼素材存量（重复获取累积）。
  int32_t exp = 0;         // 当前等级内累计武器经验。
  // 装备者角色 id；0 表示未装备。
  int32_t equippedBy = 0;
};

class WeaponSystem {
 public:
  static constexpr int32_t kMaxLevel = 90;
  static constexpr int32_t kMaxAscension = 6;
  static constexpr int32_t kMaxRefine = 5;

  // 武器图鉴：三源专武 + 通用武器。
  static const std::vector<WeaponDef>& catalog();
  static const WeaponDef* weaponDef(int32_t weaponId);

  // 金币强化路径：升到下一级所需金币（保留旧接口）。
  static int32_t upgradeCost(int32_t level);
  // 武器经验曲线：level -> level+1 所需武器经验。
  static int32_t expRequired(int32_t level);
  // 突破阶段等级上限：20/40/50/60/70/80/90。
  static int32_t levelCap(int32_t ascension);
  // 突破消耗：突破材料份数与金币（随阶段递增）。
  static int32_t ascensionMaterialCost(int32_t ascension);
  static int32_t ascensionGoldCost(int32_t ascension);
  // 武器当前攻击（基础 + 等级成长，五星成长更高；精炼每阶 +10%）。
  static int32_t atkOf(int32_t weaponId, int32_t level, int32_t refine = 1);

  // 获得武器：已拥有返回 false（重复获取由调用方折算补偿）。
  bool addWeapon(int32_t weaponId);
  // 重复获取累计精炼素材（抽卡重复武器调用）。
  bool addRefineStock(int32_t weaponId);
  bool owns(int32_t weaponId) const;
  const OwnedWeapon* find(int32_t weaponId) const;

  // 金币强化（旧路径）：未达当前突破上限时等级 +1，消耗由调用方扣除。
  bool upgrade(int32_t weaponId);
  // 矿石强化：注入武器经验并级联升级，到达突破上限后停止积累。
  // 返回升了几级。
  int32_t addWeaponExp(int32_t weaponId, int32_t expAmount);
  // 突破：等级达到当前上限且未达最高阶段时成功。
  bool ascend(int32_t weaponId);
  // 精炼：消耗 1 份同名素材，精炼阶 +1（上限 5）。
  bool refine(int32_t weaponId);

  // 装备：武器改绑到指定角色（自动从原角色解绑）；
  // 角色原装备武器自动卸下。weaponId 未拥有返回 false。
  bool equip(int32_t weaponId, int32_t characterId);

  // 角色当前装备武器的攻击加成（含精炼）；未装备返回 0。
  int32_t equippedBonusFor(int32_t characterId) const;

  // 存档恢复（旧三元组兼容）：等级/装备者，突破 0、精炼 1。
  bool restoreWeapon(int32_t weaponId, int32_t level, int32_t equippedBy);
  // 存档恢复（V7 全字段）：非法值钳制。
  bool restoreWeapon(int32_t weaponId, int32_t level, int32_t ascension,
                     int32_t refine, int32_t refineStock, int32_t exp,
                     int32_t equippedBy);

  const std::vector<OwnedWeapon>& owned() const { return owned_; }

 private:
  std::vector<OwnedWeapon> owned_;
};
