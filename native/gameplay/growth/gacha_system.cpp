#include "native/gameplay/growth/gacha_system.h"

#include "native/gameplay/growth/character_growth.h"

#include <algorithm>

namespace {

// LCG 伪随机：与项目命中火花同源的确定性序列。
float nextRandom(uint32_t& seed) {
  seed = seed * 1664525u + 1013904223u;
  return static_cast<float>((seed >> 8) & 0xFFFFu) / 65536.0f;
}

int32_t pickFromPool(const std::vector<CharacterDef>& pool, uint32_t& seed,
                     int32_t rarity) {
  std::vector<int32_t> candidates;
  for (const CharacterDef& def : pool) {
    if (def.rarity == rarity) candidates.push_back(def.id);
  }
  if (candidates.empty()) return 1;
  const float r = nextRandom(seed);
  const auto index = static_cast<size_t>(r * candidates.size()) %
                     candidates.size();
  return candidates[index];
}

}  // namespace

GachaSystem::GachaSystem(GachaConfig config) : config_(config) {
  if (!(config_.p5 > 0.0f)) config_.p5 = 0.006f;
  if (!(config_.p4 > 0.0f)) config_.p4 = 0.051f;
  if (config_.pity5At < 1) config_.pity5At = 90;
  if (config_.pity4At < 1) config_.pity4At = 10;
}

std::vector<GachaPull> GachaSystem::draw(GachaState& state,
                                         int32_t count) const {
  std::vector<GachaPull> results;
  if (count <= 0) return results;
  results.reserve(static_cast<size_t>(count));
  const std::vector<CharacterDef>& pool = CharacterGrowth::roster();
  for (int32_t i = 0; i < count; ++i) {
    state.since5 += 1;
    state.since4 += 1;
    GachaPull pull;
    const bool pity5 = state.since5 >= config_.pity5At;
    const float r5 = nextRandom(state.seed);
    if (pity5 || r5 < config_.p5) {
      pull.rarity = 5;
      pull.pity5 = pity5;
      pull.characterId = pickFromPool(pool, state.seed, 5);
      state.since5 = 0;
      state.since4 = 0;
    } else {
      const bool pity4 = state.since4 >= config_.pity4At;
      const float r4 = nextRandom(state.seed);
      // 四星及以上：保底、幸运或普通池均落入四星角色池。
      pull.rarity = 4;
      pull.pity5 = false;
      pull.characterId = pickFromPool(pool, state.seed, 4);
      if (pity4 || r4 < config_.p4) {
        state.since4 = 0;
      }
    }
    results.push_back(pull);
  }
  return results;
}

std::vector<GachaPull> GachaSystem::drawWeapon(GachaState& state,
                                               int32_t count) const {
  // 武器池：五星专武 1-3，四星通用 4-5；五星概率 0.75%，其余四星。
  std::vector<GachaPull> results;
  if (count <= 0) return results;
  results.reserve(static_cast<size_t>(count));
  constexpr float kWeaponP5 = 0.0075f;
  for (int32_t i = 0; i < count; ++i) {
    state.since5 += 1;
    state.since4 += 1;
    GachaPull pull;
    const bool pity5 = state.since5 >= config_.pity5At;
    const float r5 = nextRandom(state.seed);
    if (pity5 || r5 < kWeaponP5) {
      pull.rarity = 5;
      pull.pity5 = pity5;
      const float pick = nextRandom(state.seed);
      pull.characterId = 1 + static_cast<int32_t>(pick * 3.0f) % 3;
      state.since5 = 0;
      state.since4 = 0;
    } else {
      pull.rarity = 4;
      pull.pity5 = false;
      const float pick = nextRandom(state.seed);
      pull.characterId = 4 + static_cast<int32_t>(pick * 2.0f) % 2;
      const bool pity4 = state.since4 >= config_.pity4At;
      if (pity4) {
        state.since4 = 0;
      }
    }
    results.push_back(pull);
  }
  return results;
}
