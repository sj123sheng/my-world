#include "native/gameplay/player/player_controller.h"

#include <cassert>
#include <cmath>

int main() {
  Player player;
  PlayerController controller({2.0f, 8.0f});
  controller.update(player, {0, 1}, 0.0f, 0.5f);
  assert(std::abs(player.x - 0.5f) < 0.0001f);
  assert(std::abs(player.y - 1.0f) < 0.0001f);
  player = {};
  controller.update(player, {0, 1}, 1.5707963f, 0.25f);
  assert(player.x > 0.99f);
  assert(std::abs(player.y - 0.5f) < 0.0001f);
  controller.update(player, {}, 1.5707963f, 0.25f);
  assert(!player.moving);
}
