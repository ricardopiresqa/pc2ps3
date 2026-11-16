#include "pc2ps3/core.hpp"

#include <cassert>
#include <cmath>

int main() {
  const pc2ps3::WorldConfig config = {1.0f / 60.0f, 1500.0f, 2200.0f,
                                      280.0f, 560.0f, 360.0f};
  pc2ps3::World first(config);
  pc2ps3::World second(config);

  for (int frame = 0; frame < 180; ++frame) {
    const pc2ps3::InputFrame input = {
        frame < 90, frame >= 90 && frame < 150, frame == 30, frame >= 30 && frame < 55};
    first.step(input);
    second.step(input);
  }

  const pc2ps3::Actor& a = first.player();
  const pc2ps3::Actor& b = second.player();
  assert(first.tick() == 180);
  assert(std::fabs(a.position.x - b.position.x) < 0.0001f);
  assert(std::fabs(a.position.y - b.position.y) < 0.0001f);
  assert(a.grounded);
  assert(a.position.y == config.floor_y);

  // Checkpoint respawn: teleport with zero velocity and no interpolated streak.
  assert(first.respawn(pc2ps3::Vec2{500.0f, config.floor_y}));
  pc2ps3::Vec2 drawn;
  assert(first.interpolated_player_position(0.5, drawn));
  assert(drawn.x == 500.0f && drawn.y == config.floor_y);
  assert(first.player().velocity.x == 0.0f && first.player().grounded);
  const pc2ps3::InputFrame idle = {false, false, false, false};
  first.step(idle);
  assert(first.player().position.x == 500.0f);
  assert(!first.respawn(pc2ps3::Vec2{NAN, 0.0f}));
  assert(first.player().position.x == 500.0f);
  return 0;
}
