#include "pc2ps3/core.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {
pc2ps3::WorldConfig config() {
  return pc2ps3::WorldConfig{1.0f, 0.0f, 0.0f, 100.0f, 20.0f, 300.0f};
}

pc2ps3::InputFrame no_input() {
  return pc2ps3::InputFrame{false, false, false, false};
}

void test_player_rides_and_reset_restores_platform() {
  pc2ps3::World world(config());
  assert(world.add_moving_platform(pc2ps3::Aabb{0.0f, 300.0f, 200.0f, 320.0f},
                                  pc2ps3::Vec2{2.0f, 0.0f}));
  world.step(no_input());
  assert(world.player().grounded);
  assert(world.player().position.x == 66.0f);
  world.step(no_input());
  assert(world.player().position.x == 68.0f);
  world.reset();
  assert(world.player().position.x == 64.0f);
  world.step(no_input());
  assert(world.player().position.x == 66.0f);
}

void test_vertical_carry_and_no_carry_without_support() {
  pc2ps3::WorldConfig vertical_config = config();
  vertical_config.floor_y = 300.0f;
  pc2ps3::World vertical(vertical_config);
  assert(vertical.add_moving_platform(
      pc2ps3::Aabb{0.0f, 300.0f, 200.0f, 320.0f},
      pc2ps3::Vec2{0.0f, -2.0f}));
  vertical.step(no_input());
  assert(vertical.player().position.y == 298.0f);
  assert(vertical.player().grounded);

  pc2ps3::World unsupported(config());
  assert(unsupported.add_moving_platform(
      pc2ps3::Aabb{150.0f, 300.0f, 200.0f, 320.0f},
      pc2ps3::Vec2{2.0f, 0.0f}));
  unsupported.step(no_input());
  assert(unsupported.player().position.x == 64.0f);
}

void test_walking_off_platform_stops_carry() {
  const pc2ps3::WorldConfig walking_config =
      {1.0f, 0.0f, 10.0f, 100.0f, 20.0f, 300.0f};
  pc2ps3::World world(walking_config);
  assert(world.add_moving_platform(pc2ps3::Aabb{0.0f, 300.0f, 100.0f, 320.0f},
                                  pc2ps3::Vec2{2.0f, 0.0f}));
  const pc2ps3::InputFrame right = {false, true, false, false};
  for (int i = 0; i < 4; ++i) world.step(right);
  assert(world.player().grounded);
  assert(world.player().position.x == 170.0f);
  world.step(right);
  assert(world.player().position.x == 220.0f);
}

void test_invalid_inputs_capacity_and_determinism() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  pc2ps3::World first(config());
  pc2ps3::World second(config());
  assert(!first.add_moving_platform(
      pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}, pc2ps3::Vec2{1.0f, 0.0f}));
  assert(!first.add_moving_platform(
      pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}, pc2ps3::Vec2{infinity, 0.0f}));

  const pc2ps3::Aabb offscreen = {500.0f, 300.0f, 520.0f, 320.0f};
  pc2ps3::World full(config());
  for (std::size_t i = 0; i < pc2ps3::World::kMaxMovingPlatforms; ++i) {
    assert(full.add_moving_platform(offscreen, pc2ps3::Vec2{1.0f, 0.0f}));
  }
  assert(!full.add_moving_platform(offscreen, pc2ps3::Vec2{1.0f, 0.0f}));
  assert(first.add_moving_platform(offscreen, pc2ps3::Vec2{1.0f, 0.0f}));
  assert(second.add_moving_platform(offscreen, pc2ps3::Vec2{1.0f, 0.0f}));
  assert(first.add_moving_platform(
      pc2ps3::Aabb{0.0f, 300.0f, 200.0f, 320.0f},
      pc2ps3::Vec2{2.0f, 0.0f}));
  assert(second.add_moving_platform(
      pc2ps3::Aabb{0.0f, 300.0f, 200.0f, 320.0f},
      pc2ps3::Vec2{2.0f, 0.0f}));
  first.reset();
  for (int i = 0; i < 5; ++i) {
    first.step(no_input());
    second.step(no_input());
    assert(std::fabs(first.player().position.x -
                     second.player().position.x) < 0.0001f);
    assert(std::fabs(first.player().position.y -
                     second.player().position.y) < 0.0001f);
  }
}
}  // namespace

int main() {
  test_player_rides_and_reset_restores_platform();
  test_vertical_carry_and_no_carry_without_support();
  test_walking_off_platform_stops_carry();
  test_invalid_inputs_capacity_and_determinism();
  return 0;
}
