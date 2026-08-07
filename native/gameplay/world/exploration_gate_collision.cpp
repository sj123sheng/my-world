#include "native/gameplay/world/exploration_gate_collision.h"

#include "native/gameplay/world/exploration_content.h"

#include <cmath>

ExplorationGateCollision ExplorationGateCollision::fromContent(
    const ExplorationContent& content) {
  std::vector<BuildingBox> boxes;
  std::vector<int32_t> gateIds;
  for (const TraversalGate& gate : content.gates()) {
    if (content.isGateOpen(gate.id)) continue;
    BuildingBox box;
    box.cx = gate.x;
    box.cz = gate.y;
    box.hx = gate.halfExtents[0];
    box.hz = gate.halfExtents[1];
    box.yaw = gate.yaw;
    box.top = gate.top;
    boxes.push_back(box);
    gateIds.push_back(gate.id);
  }
  return ExplorationGateCollision(std::move(boxes), std::move(gateIds));
}

ExplorationGateCollision::ExplorationGateCollision(
    std::vector<BuildingBox> boxes, std::vector<int32_t> gateIds)
    : collision_(boxes), gateIds_(std::move(gateIds)) {}

BuildingContact ExplorationGateCollision::resolve(float& x, float& y,
                                                  float radius,
                                                  float height) const {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) ||
      !std::isfinite(height) || radius < 0.0f || height < 0.0f) {
    return {};
  }
  if (collision_.boxes().empty()) return {};
  return collision_.resolve(x, y, radius, height);
}

bool ExplorationGateCollision::blocks(int32_t gateId) const {
  for (const int32_t id : gateIds_) {
    if (id == gateId) return true;
  }
  return false;
}
