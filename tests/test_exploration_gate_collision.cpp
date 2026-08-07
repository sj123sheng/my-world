#include "native/gameplay/world/exploration_content.h"
#include "native/gameplay/world/exploration_gate_collision.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
  ExplorationContent content = ExplorationContent::verticalSlice();
  ExplorationGateCollision closed =
      ExplorationGateCollision::fromContent(content);
  assert(closed.boxes().size() == content.gates().size());
  assert(closed.blocks(81));

  float x = 0.78f;
  float y = 0.28f;
  BuildingContact contact = closed.resolve(x, y, 0.012f, 0.0f);
  assert(contact.touching);
  assert(std::abs(x - 0.78f) > 1e-5f || std::abs(y - 0.28f) > 1e-5f);

  // 高度越过门顶时不阻挡，语义与静态建筑盒一致。
  x = 0.78f;
  y = 0.28f;
  contact = closed.resolve(x, y, 0.012f, 0.13f);
  assert(!contact.touching);
  assert(x == 0.78f && y == 0.28f);

  assert(content.activatePuzzle(71, MotionState::Swimming));
  ExplorationGateCollision open = ExplorationGateCollision::fromContent(content);
  assert(!open.blocks(81));
  assert(open.boxes().size() + 1 == closed.boxes().size());
  x = 0.78f;
  y = 0.28f;
  contact = open.resolve(x, y, 0.012f, 0.0f);
  assert(!contact.touching);
  assert(x == 0.78f && y == 0.28f);

  // 非有限输入不进入碰撞数学路径，也不污染坐标。
  x = std::numeric_limits<float>::quiet_NaN();
  y = 0.28f;
  contact = closed.resolve(x, y, 0.012f, 0.0f);
  assert(!contact.touching);
  assert(std::isnan(x) && y == 0.28f);

  assert(content.gateById(81) != nullptr);
  assert(content.gateById(999) == nullptr);
  assert(content.puzzleById(71) != nullptr);
  assert(content.puzzleById(999) == nullptr);
  return 0;
}
