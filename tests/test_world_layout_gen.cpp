// 生成头 world_layout.gen.h 的数据自洽断言（Phase 0 数据管线）。
// 编译：c++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
//   tests/test_world_layout_gen.cpp -o /tmp/test_world_layout_gen
#include "native/generated/world_layout.gen.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <set>
#include <string_view>

namespace WL = WorldLayout;

namespace {

bool InBounds(float value) {
  return value >= WL::kCoordMin && value <= WL::kCoordMax;
}

int32_t ChunkOf(float value, int32_t count) {
  int32_t chunk = static_cast<int32_t>(std::floor(value * static_cast<float>(count)));
  if (chunk < 0) chunk = 0;
  if (chunk > count - 1) chunk = count - 1;
  return chunk;
}

// 判断坐标 (x, y) 落在给定 district 的分块范围内。
bool InDistrict(const WL::WorldDistrictDef& district, float x, float y) {
  const int32_t cx = ChunkOf(x, WL::kGridCountX);
  const int32_t cy = ChunkOf(y, WL::kGridCountY);
  return cx >= district.chunkXMin && cx <= district.chunkXMax &&
         cy >= district.chunkYMin && cy <= district.chunkYMax;
}

const WL::WorldDistrictDef& DistrictById(std::string_view districtId) {
  for (const auto& district : WL::kDistricts) {
    if (district.districtId == districtId) return district;
  }
  assert(false && "unknown districtId");
  return WL::kDistricts[0];
}

}  // namespace

int main() {
  // ---- districts：数量、范围合法、覆盖全图且互不重叠 ----
  static_assert(WL::kDistrictCount >= 4 && WL::kDistrictCount <= 6,
                "district count out of design range");
  std::set<std::string_view> districtIds;
  std::array<int32_t, WL::kGridCountX * WL::kGridCountY> coverage{};
  for (const auto& district : WL::kDistricts) {
    assert(!district.districtId.empty());
    assert(!district.name.empty());
    assert(districtIds.insert(district.districtId).second && "duplicate districtId");
    assert(district.chunkXMin >= 0 && district.chunkXMin <= district.chunkXMax &&
           district.chunkXMax < WL::kGridCountX);
    assert(district.chunkYMin >= 0 && district.chunkYMin <= district.chunkYMax &&
           district.chunkYMax < WL::kGridCountY);
    for (int32_t cy = district.chunkYMin; cy <= district.chunkYMax; cy += 1) {
      for (int32_t cx = district.chunkXMin; cx <= district.chunkXMax; cx += 1) {
        coverage[static_cast<size_t>(cy * WL::kGridCountX + cx)] += 1;
      }
    }
  }
  for (const int32_t times : coverage) {
    assert(times == 1 && "each chunk must belong to exactly one district");
  }

  // ---- anchors：id≥8、坐标界内、落在声明 district ----
  static_assert(WL::kAnchorCount >= 4 && WL::kAnchorCount <= 6,
                "anchor count out of design range");
  std::set<int32_t> numericIds;
  for (const auto& anchor : WL::kAnchors) {
    assert(anchor.id >= 8 && "new anchors must not collide with legacy ids 1-7");
    assert(numericIds.insert(anchor.id).second && "duplicate entity id");
    assert(InBounds(anchor.x) && InBounds(anchor.y));
    assert(!anchor.label.empty());
    assert(InDistrict(DistrictById(anchor.districtId), anchor.x, anchor.y));
  }

  // ---- npcs：id≥32、dialogId 占位段≥100、行为与巡逻点位自洽 ----
  static_assert(WL::kNpcCount >= 5 && WL::kNpcCount <= 8,
                "npc count out of design range");
  std::set<int32_t> dialogIds;
  for (const auto& npc : WL::kNpcs) {
    assert(npc.id >= 32 && "new NPCs must not collide with legacy ids");
    assert(numericIds.insert(npc.id).second && "duplicate entity id");
    assert(dialogIds.insert(npc.dialogId).second && "duplicate dialogId");
    assert(npc.dialogId >= 100);
    assert(InBounds(npc.x) && InBounds(npc.y));
    assert(!npc.label.empty());
    assert(InDistrict(DistrictById(npc.districtId), npc.x, npc.y));
    if (npc.behavior == WL::NpcBehavior::Idle) {
      assert(npc.patrolCount == 0);
    } else {
      assert(npc.patrolCount >= 2 && npc.patrolCount <= 4);
    }
    assert(npc.patrolCount <= WL::kMaxNpcPatrolPoints);
    for (int32_t i = 0; i < npc.patrolCount; i += 1) {
      assert(InBounds(npc.patrolX[i]) && InBounds(npc.patrolY[i]));
      assert(InDistrict(DistrictById(npc.districtId), npc.patrolX[i], npc.patrolY[i]));
    }
  }

  // ---- spawnZones：archetype 合法、count/respawn 范围、点位界内 ----
  static_assert(WL::kSpawnZoneCount >= 6 && WL::kSpawnZoneCount <= 10,
                "spawn zone count out of design range");
  std::set<std::string_view> zoneIds;
  for (const auto& zone : WL::kSpawnZones) {
    assert(!zone.zoneId.empty());
    assert(zoneIds.insert(zone.zoneId).second && "duplicate zoneId");
    assert(!zone.aggroGroup.empty());
    assert(!WL::ArchetypeName(zone.archetype).empty());
    assert(WL::ArchetypeName(zone.archetype) != "Unknown" && "illegal archetype");
    assert(zone.count >= 1 && zone.count <= 3);
    assert(zone.count <= WL::kMaxSpawnPositions);
    assert(zone.respawnMs >= 30000 && zone.respawnMs <= 90000);
    assert(InBounds(zone.patrolCenterX) && InBounds(zone.patrolCenterY));
    assert(InDistrict(DistrictById(zone.districtId),
                      zone.patrolCenterX, zone.patrolCenterY));
    for (int32_t i = 0; i < zone.count; i += 1) {
      assert(InBounds(zone.positionX[i]) && InBounds(zone.positionY[i]));
      assert(InDistrict(DistrictById(zone.districtId),
                        zone.positionX[i], zone.positionY[i]));
    }
  }

  // ---- chests / collectibles：id≥32、坐标界内、落在声明 district ----
  static_assert(WL::kChestCount >= 1, "needs chests");
  static_assert(WL::kCollectibleCount >= 1, "needs collectibles");
  for (const auto& chest : WL::kChests) {
    assert(chest.id >= 32);
    assert(numericIds.insert(chest.id).second && "duplicate entity id");
    assert(InBounds(chest.x) && InBounds(chest.y));
    assert(!chest.label.empty());
    assert(InDistrict(DistrictById(chest.districtId), chest.x, chest.y));
  }
  for (const auto& collectible : WL::kCollectibles) {
    assert(collectible.id >= 32);
    assert(numericIds.insert(collectible.id).second && "duplicate entity id");
    assert(InBounds(collectible.x) && InBounds(collectible.y));
    assert(!collectible.kind.empty());
    assert(InDistrict(DistrictById(collectible.districtId),
                      collectible.x, collectible.y));
  }

  // ---- 垂直切片探索内容：POI、机关、路径门与奖励 ----
  static_assert(WL::kPointOfInterestCount >= 4 &&
                    WL::kPointOfInterestCount <= 6,
                "vertical slice needs 4-6 points of interest");
  static_assert(WL::kPuzzleNodeCount >= 3 && WL::kPuzzleNodeCount <= 5,
                "vertical slice needs 3-5 puzzles");
  static_assert(WL::kTraversalGateCount >= 2 && WL::kTraversalGateCount <= 5,
                "vertical slice needs 2-5 traversal gates");
  static_assert(WL::kExplorationRewardCount >= 4 &&
                    WL::kExplorationRewardCount <= 6,
                "vertical slice needs 4-6 exploration rewards");
  std::set<int32_t> explorationIds;
  for (const auto& poi : WL::kPointsOfInterest) {
    assert(poi.id >= 60);
    assert(explorationIds.insert(poi.id).second);
    assert(InBounds(poi.x) && InBounds(poi.y));
    assert(!poi.label.empty() && !poi.districtId.empty());
  }
  for (const auto& puzzle : WL::kPuzzleNodes) {
    assert(puzzle.id >= 70);
    assert(explorationIds.insert(puzzle.id).second);
    assert(InBounds(puzzle.x) && InBounds(puzzle.y));
    assert(puzzle.opensGateId >= 80);
    assert(puzzle.rewardId >= 90);
  }
  for (const auto& gate : WL::kTraversalGates) {
    assert(gate.id >= 80);
    assert(explorationIds.insert(gate.id).second);
    assert(InBounds(gate.x) && InBounds(gate.y));
    assert(gate.halfExtents[0] > 0.0f && gate.halfExtents[0] < 0.15f);
    assert(gate.halfExtents[1] > 0.0f && gate.halfExtents[1] < 0.15f);
    assert(std::isfinite(gate.yaw));
    assert(gate.top > 0.0f && gate.top < 0.5f);
  }
  for (const auto& reward : WL::kExplorationRewards) {
    assert(reward.id >= 90);
    assert(explorationIds.insert(reward.id).second);
    assert(!reward.label.empty());
    assert(reward.sourceTraces >= 0 && reward.gold >= 0 && reward.fate >= 0);
  }

  return 0;
}
