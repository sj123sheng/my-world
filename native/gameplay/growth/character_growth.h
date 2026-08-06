#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 角色图鉴与养成（阶段三）：
// 角色通过抽卡或开局赠送获得；等级消耗经验材料提升，
// 到达当前突破阶段上限后可突破解锁更高等级上限。
// 全部规则确定性、无随机，可被独立测试覆盖。
struct CharacterDef {
  int32_t id = 0;
  std::string name;
  int32_t rarity = 4;  // 4 或 5 星
};

struct OwnedCharacter {
  int32_t characterId = 0;
  int32_t level = 1;
  int32_t ascension = 0;
  int32_t exp = 0;  // 当前等级内累计经验
  int32_t constellation = 0;  // 命之座层数（重复抽取转化，0..6）。
};

class CharacterGrowth {
 public:
  static constexpr int32_t kMaxAscension = 3;
  static constexpr int32_t kMaxConstellation = 6;

  // 角色图鉴：三源主角 + 三名四星角色，供抽卡卡池使用。
  static const std::vector<CharacterDef>& roster();
  static const CharacterDef* characterDef(int32_t characterId);

  // 升到下一级所需经验（经验材料单位）。
  static int32_t expRequired(int32_t level);
  // 当前突破阶段的等级上限：20/40/60/80。
  static int32_t levelCap(int32_t ascension);
  // 派生属性（确定性公式）：等级/突破/稀有度/命之座驱动生命与攻击。
  static int32_t hpFor(int32_t characterId, int32_t level, int32_t ascension,
                       int32_t constellation = 0);
  static int32_t atkFor(int32_t characterId, int32_t level, int32_t ascension,
                        int32_t constellation = 0);

  // 获得角色：已拥有返回 false（重复获取由调用方折算补偿）。
  bool addCharacter(int32_t characterId);
  // 存档恢复：直接以指定等级/突破写入角色；非法值被钳制。
  bool restoreCharacter(int32_t characterId, int32_t level,
                        int32_t ascension);
  bool owns(int32_t characterId) const;
  const OwnedCharacter* find(int32_t characterId) const;

  // 注入经验并级联升级；到达突破上限后停止积累。返回升了几级。
  int32_t addExp(int32_t characterId, int32_t expAmount);
  // 突破：等级达到上限且未达最高突破阶段时成功。
  bool ascend(int32_t characterId);
  // 命之座提升（重复抽取转化）：未达 6 层时 +1，返回是否成功。
  bool boostConstellation(int32_t characterId);

  // 按角色 id 升序的已拥有角色（确定性）。
  const std::vector<OwnedCharacter>& owned() const { return owned_; }

 private:
  std::vector<OwnedCharacter> owned_;
};
