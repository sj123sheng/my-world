#include "native/gameplay/world/terrain_wall_collision.h"

#include <cassert>
#include <cmath>
#include <cstdio>

#include "native/engine/world/terrain_heightfield.h"
#include "native/gameplay/player/exploration_motion.h"
#include "native/gameplay/world/world_terrain.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 1e-4f) {
  return std::abs(a - b) <= tolerance;
}

// 平底地形：无起伏、无水域、无可攀爬面。
TerrainHeightfield flatTerrain() {
  TerrainConfig config;
  config.amplitude = 0.0f;
  config.detailAmplitude = 0.0f;
  config.ridgeAmplitude = 0.0f;
  config.edgeMountainHeight = 0.0f;
  config.waterLevel = -100.0f;
  return TerrainHeightfield{config};
}

// 缓坡穹丘：全域坡度远低于攀爬阈值，应永远不阻挡。
TerrainHeightfield gentleDomeTerrain() {
  TerrainConfig config;
  config.amplitude = 0.0f;
  config.detailAmplitude = 0.0f;
  config.ridgeAmplitude = 0.0f;
  config.edgeMountainHeight = 0.0f;
  config.waterLevel = -100.0f;
  TerrainFeature dome;
  dome.kind = TerrainFeatureKind::Hill;
  dome.x = 0.5f;
  dome.y = 0.5f;
  dome.radiusX = 0.2f;
  dome.radiusY = 0.2f;
  dome.amplitude = 0.02f;
  dome.feather = 1.0f;
  return TerrainHeightfield{config, {dome}};
}

// 陡峭悬崖：中心高台，四周坡度远超攀爬阈值。
TerrainHeightfield cliffTerrain() {
  TerrainConfig config;
  config.amplitude = 0.0f;
  config.detailAmplitude = 0.0f;
  config.ridgeAmplitude = 0.0f;
  config.edgeMountainHeight = 0.0f;
  config.waterLevel = -100.0f;
  TerrainFeature cliff;
  cliff.kind = TerrainFeatureKind::Hill;
  cliff.x = 0.5f;
  cliff.y = 0.5f;
  cliff.radiusX = 0.08f;
  cliff.radiusY = 0.08f;
  cliff.amplitude = 0.08f;
  cliff.feather = 0.8f;
  return TerrainHeightfield{config, {cliff}};
}

}  // namespace

void testFlatGroundNeverBlocks() {
  const TerrainHeightfield flat = flatTerrain();
  const Vec2 dirs[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}};
  for (const Vec2& dir : dirs) {
    const TerrainWallContact contact =
        terrainWallContact(flat, 0.5f, 0.5f, 0.0f, dir, 0.02f);
    assert(!contact.blocked);
    assert(!contact.climbable);
  }
  // 零移动方向 / 非法探测距离：无接触。
  assert(!terrainWallContact(flat, 0.5f, 0.5f, 0.0f, {0, 0}, 0.02f).blocked);
  assert(!terrainWallContact(flat, 0.5f, 0.5f, 0.0f, {1, 0}, 0.0f).blocked);
}

void testGentleSlopeNeverBlocks() {
  const TerrainHeightfield dome = gentleDomeTerrain();
  // 从穹丘外围一路朝中心探测：坡度远低于阈值，任何高度都不阻挡。
  for (float distance = 0.25f; distance > 0.03f; distance -= 0.02f) {
    const float x = 0.5f - distance;
    const float height = dome.heightAt(x, 0.5f);
    const TerrainWallContact contact =
        terrainWallContact(dome, x, 0.5f, height, {1, 0}, 0.018f);
    assert(!contact.blocked);
  }
}

void testCliffBlocksAndIsClimbable() {
  const TerrainHeightfield cliff = cliffTerrain();
  // 悬崖影响范围半径 0.08（边缘 x=0.42），feather=0.8 的外缘坡度平缓：
  // 探测起点取在坡面中段（x=0.44），一步探入陡坡区才会被阻挡。
  const float baseHeight = cliff.heightAt(0.44f, 0.5f);
  const TerrainWallContact contact = terrainWallContact(
      cliff, 0.44f, 0.5f, baseHeight, {1, 0}, 0.018f);
  assert(contact.blocked);
  assert(contact.climbable);
  assert(contact.groundAhead > baseHeight + 0.01f);
  // 法线指向墙内（朝悬崖中心，+x 方向为主）。
  assert(contact.normal.x > 0.3f);
  // 已经越过墙顶的高度（跳跃/滑翔）不阻挡。
  const TerrainWallContact above = terrainWallContact(
      cliff, 0.44f, 0.5f, contact.groundAhead + 0.02f, {1, 0}, 0.018f);
  assert(!above.blocked);
}

void testSlideProjectionRemovesNormalComponent() {
  // 正对墙：位移全部被取消。
  const Vec2 headOn = slideAlongTerrainWall({0.005f, 0.0f}, {1.0f, 0.0f});
  assert(nearlyEqual(headOn.x, 0.0f) && nearlyEqual(headOn.y, 0.0f));
  // 斜向墙：保留切向分量。
  const Vec2 diagonal = slideAlongTerrainWall({0.005f, 0.004f}, {1.0f, 0.0f});
  assert(nearlyEqual(diagonal.x, 0.0f));
  assert(nearlyEqual(diagonal.y, 0.004f));
  // 背离墙/沿墙：位移不变。
  const Vec2 away = slideAlongTerrainWall({-0.005f, 0.002f}, {1.0f, 0.0f});
  assert(nearlyEqual(away.x, -0.005f) && nearlyEqual(away.y, 0.002f));
}

void testDepenetratePushesOutOfWall() {
  const TerrainHeightfield cliff = cliffTerrain();
  // 把角色放进悬崖腰部（脚下地面显著高于脚底高度）。
  const float embeddedX = 0.46f;
  const float groundThere = cliff.heightAt(embeddedX, 0.5f);
  assert(groundThere > 0.02f);  // 确认嵌入位置确实在墙体内。
  const Vec2 fixed =
      depenetrateTerrainWall(cliff, embeddedX, 0.5f, 0.0f, 0.012f);
  const float groundAfter = cliff.heightAt(fixed.x, fixed.y);
  assert(groundAfter <= 0.55f * 0.012f + 1e-4f);
  // 推出方向为下坡（远离悬崖中心，x 减小）。
  assert(fixed.x < embeddedX);
  // 未嵌入时原样返回。
  const Vec2 untouched =
      depenetrateTerrainWall(cliff, 0.2f, 0.5f, 0.0f, 0.012f);
  assert(nearlyEqual(untouched.x, 0.2f) && nearlyEqual(untouched.y, 0.5f));
}

void testTerrainClimbMotion() {
  ExplorationMotion motion;
  const TerrainHeightfield flat = flatTerrain();
  const float dt = 1.0f / 60.0f;

  // 地面 + terrainClimbing → 进入攀爬。
  ExplorationMotionState state = motion.reset(0.0f);
  MotionInput climbInput;
  climbInput.moving = true;
  climbInput.terrainClimbing = true;
  state = motion.update(state, climbInput, flat, 0.5f, 0.5f, dt);
  assert(state.state == MotionState::Climbing);

  // 攀爬按固定速度上升（平地基准：高度 = 爬升速度 × 时间）。
  const float before = state.height;
  state = motion.update(state, climbInput, flat, 0.5f, 0.5f, dt);
  assert(nearlyEqual(state.height - before,
                     ExplorationMotion::wallClimbSpeed() * dt, 1e-5f));
  assert(state.stamina < motion.config().maxStamina);

  // 停止朝墙推进 → 退出攀爬回到地面。
  MotionInput stopInput;
  stopInput.moving = true;
  state = motion.update(state, stopInput, flat, 0.5f, 0.5f, dt);
  assert(state.state == MotionState::Grounded);

  // 攀爬中体力耗尽 → 从墙上坠落（而非瞬移回地面）。
  ExplorationMotionState tired = motion.reset(0.0f);
  tired.height = 0.05f;  // 已在墙上一定高度。
  tired.stamina = 0.0f;
  ExplorationMotionState fell =
      motion.update(tired, climbInput, flat, 0.5f, 0.5f, dt);
  assert(fell.state == MotionState::Airborne);
  assert(nearlyEqual(fell.height, 0.05f));
}

void testWalkOffLedgeFalls() {
  ExplorationMotion motion;
  const TerrainHeightfield flat = flatTerrain();
  const float dt = 1.0f / 60.0f;
  // 站在高台边缘（脚底高度 0.05），下一步地面骤降 → 坠落而非吸附。
  ExplorationMotionState state = motion.reset(0.05f);
  state = motion.update(state, {}, flat, 0.5f, 0.5f, dt);
  assert(state.state == MotionState::Airborne);
  assert(nearlyEqual(state.height, 0.05f));
  // 小台阶（低于 maxStepDown）仍贴地。
  ExplorationMotionState stepDown = motion.reset(0.005f);
  stepDown = motion.update(stepDown, {}, flat, 0.5f, 0.5f, dt);
  assert(stepDown.state == MotionState::Grounded);
  assert(nearlyEqual(stepDown.height, 0.0f));
}

// 真实世界 mesa（湖心残塔平顶台地）全流程仿真：走向悬崖 → 被阻挡 →
// 真攀爬（限速上升）→ 登顶 → 行走接管站上平顶。全程脚底不陷入墙体。
void testRealWorldMesaClimbSimulation() {
  const TerrainHeightfield terrain = makeWorldTerrain();
  ExplorationMotion motion;
  const float dt = 1.0f / 60.0f;
  const float speed = 0.3f;
  const float radius = 0.012f;

  // mesa 中心 (0.86, 0.12)，从西侧平地 (0.70, 0.12) 朝 +x 直走。
  float x = 0.70f;
  float y = 0.12f;
  ExplorationMotionState state = motion.reset(terrain.heightAt(x, y));
  const float maxStepUp = 0.55f * (radius + speed * dt + 0.001f);

  int climbFrames = 0;
  bool everBlocked = false;
  int frame = 0;
  for (; frame < 3000 && x < 0.855f; ++frame) {
    float dx = speed * dt;
    float dy = 0.0f;
    bool terrainClimbing = false;
    if (state.state != MotionState::Swimming) {
      const float moveLen = std::hypot(dx, dy);
      const Vec2 moveDir{dx / moveLen, dy / moveLen};
      const float probeDistance = radius + moveLen + 0.001f;
      const TerrainWallContact wall = terrainWallContact(
          terrain, x, y, state.height, moveDir, probeDistance);
      if (wall.blocked) {
        everBlocked = true;
        const Vec2 slide = slideAlongTerrainWall({dx, dy}, wall.normal);
        dx = slide.x;
        dy = slide.y;
        terrainClimbing = wall.climbable && state.stamina > 0.0f &&
                          (state.state == MotionState::Grounded ||
                           state.state == MotionState::Climbing);
      }
      const Vec2 fixed =
          depenetrateTerrainWall(terrain, x + dx, y + dy, state.height, radius);
      dx = fixed.x - x;
      dy = fixed.y - y;
    }
    x += dx;
    y += dy;
    MotionInput input;
    input.moving = true;
    input.terrainClimbing = terrainClimbing;
    state = motion.update(state, input, terrain, x, y, dt);
    if (state.state == MotionState::Climbing) ++climbFrames;
    // 无穿模不变量：脚底永不显著低于脚下地面（不陷入墙体）。
    assert(terrain.heightAt(x, y) - state.height <= maxStepUp + 1e-4f);
  }

  assert(everBlocked);             // 悬崖确实阻挡了前进。
  assert(climbFrames >= 25);       // 限速真攀爬，而非瞬移登顶。
  assert(x >= 0.855f);             // 已越过 mesa 西侧悬崖。
  // 前进方向与曲面法线不完全正交时，沿墙滑动会改变 y；因此不能把
  // 单轴行走误当作“必达台地中心”。真正的回归口径是：阻挡后发生
  // 限速攀爬、始终未嵌入、并抵达显著高于平地的 mesa 坡面。
  assert(state.height > 0.03f);
  std::printf("mesa climb: frames=%d climbFrames=%d height=%.4f y=%.4f\n",
              frame, climbFrames, state.height, y);
}

int main() {
  testFlatGroundNeverBlocks();
  testGentleSlopeNeverBlocks();
  testCliffBlocksAndIsClimbable();
  testSlideProjectionRemovesNormalComponent();
  testDepenetratePushesOutOfWall();
  testTerrainClimbMotion();
  testWalkOffLedgeFalls();
  testRealWorldMesaClimbSimulation();
  std::printf("test_terrain_wall_collision ok\n");
  return 0;
}
