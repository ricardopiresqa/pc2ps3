#include "pc2ps3/core.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool near(float left, float right) {
  return std::fabs(left - right) < 0.0001f;
}

pc2ps3::World make_world() {
  const pc2ps3::WorldConfig config = {1.0f, 0.0f, 1.0f, 10.0f, 0.0f, 100.0f};
  return pc2ps3::World(config);
}

void test_interpolates_only_between_fixed_states() {
  pc2ps3::World world = make_world();
  pc2ps3::Vec2 output = {-1.0f, -1.0f};
  assert(world.interpolated_player_position(0.0, output));
  assert(near(output.x, 64.0f) && near(output.y, 100.0f));

  pc2ps3::InputFrame input = {};
  input.move_x = 1.0f;
  world.step(input);
  const pc2ps3::Actor after_step = world.player();
  const std::uint64_t tick_after_step = world.tick();
  assert(near(after_step.position.x, 65.0f));

  assert(world.interpolated_player_position(0.0, output));
  assert(near(output.x, 64.0f));
  assert(world.interpolated_player_position(0.5, output));
  assert(near(output.x, 64.5f) && near(output.y, 100.0f));
  assert(world.interpolated_player_position(
      std::nextafter(1.0, 0.0), output));
  assert(near(output.x, after_step.position.x));

  assert(world.player().position.x == after_step.position.x);
  assert(world.player().position.y == after_step.position.y);
  assert(world.player().velocity.x == after_step.velocity.x);
  assert(world.player().velocity.y == after_step.velocity.y);
  assert(world.player().grounded == after_step.grounded);
  assert(world.tick() == tick_after_step);
}

void test_invalid_alpha_preserves_output_and_reset_syncs_history() {
  pc2ps3::World world = make_world();
  pc2ps3::InputFrame input = {};
  input.move_x = 1.0f;
  world.step(input);

  const double invalid_alphas[] = {
      -0.01, 1.0, std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN()};
  for (std::size_t i = 0;
       i < sizeof(invalid_alphas) / sizeof(invalid_alphas[0]); ++i) {
    pc2ps3::Vec2 output = {123.0f, 456.0f};
    assert(!world.interpolated_player_position(invalid_alphas[i], output));
    assert(output.x == 123.0f && output.y == 456.0f);
  }

  world.reset();
  pc2ps3::Vec2 output = {-1.0f, -1.0f};
  assert(world.interpolated_player_position(0.5, output));
  assert(near(output.x, world.player().position.x));
  assert(near(output.y, world.player().position.y));
}

} // namespace

int main() {
  test_interpolates_only_between_fixed_states();
  test_invalid_alpha_preserves_output_and_reset_syncs_history();
  return 0;
}
