#include "pc2ps3/core.hpp"
#include "pc2ps3/input.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {
bool near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.001f;
}

void test_axes_and_conflicts() {
  pc2ps3::InputMapper mapper;
  pc2ps3::InputSnapshot snapshot;
  pc2ps3::InputFrame frame = mapper.sample(snapshot);
  assert(frame.move_x == 0.0f && frame.move_y == 0.0f);
  assert(!frame.left && !frame.right && !frame.up && !frame.down);

  snapshot.dualshock3_axis_x = 16384;
  snapshot.dualshock3_axis_y = -16384;
  frame = mapper.sample(snapshot);
  assert(near(frame.move_x, 0.5f) && near(frame.move_y, -0.5f));
  assert(frame.right && frame.up);

  snapshot.dualshock3_axis_x = static_cast<std::int16_t>(-32768);
  snapshot.dualshock3_axis_y = static_cast<std::int16_t>(32767);
  frame = mapper.sample(snapshot);
  assert(frame.move_x == -1.0f && frame.move_y == 1.0f);
  assert(frame.left && frame.down);

  snapshot.keyboard_left = true;
  snapshot.dualshock3_right = true;
  frame = mapper.sample(snapshot);
  assert(frame.move_x == 0.0f && !frame.left && !frame.right);

  snapshot.keyboard_right = true;
  frame = mapper.sample(snapshot);
  assert(frame.move_x == 0.0f && !frame.left && !frame.right);

  snapshot.dualshock3_down = true;
  frame = mapper.sample(snapshot);
  assert(frame.move_y == 1.0f && frame.down && !frame.up);
}

void test_action_edges_and_reset() {
  pc2ps3::InputMapper mapper;
  pc2ps3::InputSnapshot snapshot;
  snapshot.dualshock3_cross = true;
  snapshot.dualshock3_square = true;
  snapshot.dualshock3_circle = true;
  snapshot.dualshock3_triangle = true;
  snapshot.dualshock3_l1 = true;
  snapshot.dualshock3_r1 = true;
  snapshot.dualshock3_start = true;
  snapshot.dualshock3_select = true;

  pc2ps3::InputFrame frame = mapper.sample(snapshot);
  assert(frame.confirm_pressed && frame.confirm_held);
  assert(frame.jump_pressed && frame.jump_held);
  assert(frame.attack_pressed && frame.attack_held);
  assert(frame.cancel_pressed && frame.cancel_held);
  assert(frame.ability_pressed && frame.ability_held);
  assert(frame.l1_pressed && frame.l1_held);
  assert(frame.r1_pressed && frame.r1_held);
  assert(frame.pause_pressed && frame.pause_held);
  assert(frame.menu_pressed && frame.menu_held);

  frame = mapper.sample(snapshot);
  assert(!frame.confirm_pressed && frame.confirm_held);
  assert(!frame.attack_pressed && frame.attack_held);
  assert(!frame.cancel_pressed && frame.cancel_held);
  assert(!frame.ability_pressed && frame.ability_held);
  assert(!frame.l1_pressed && frame.l1_held);
  assert(!frame.r1_pressed && frame.r1_held);
  assert(!frame.pause_pressed && frame.pause_held);
  assert(!frame.menu_pressed && frame.menu_held);

  mapper.reset();
  frame = mapper.sample(snapshot);
  assert(frame.confirm_pressed && frame.attack_pressed && frame.pause_pressed);

  snapshot = pc2ps3::InputSnapshot();
  frame = mapper.sample(snapshot);
  assert(!frame.confirm_held && !frame.attack_held && !frame.menu_held);
}

void test_keyboard_mapping_and_world_motion() {
  pc2ps3::InputMapper mapper;
  pc2ps3::InputSnapshot snapshot;
  snapshot.keyboard_confirm = true;
  snapshot.keyboard_attack = true;
  snapshot.keyboard_cancel = true;
  snapshot.keyboard_ability = true;
  snapshot.keyboard_l1 = true;
  snapshot.keyboard_r1 = true;
  snapshot.keyboard_pause = true;
  snapshot.keyboard_menu = true;
  pc2ps3::InputFrame frame = mapper.sample(snapshot);
  assert(frame.confirm_pressed && frame.jump_pressed);
  assert(frame.attack_pressed && frame.cancel_pressed && frame.ability_pressed);
  assert(frame.l1_pressed && frame.r1_pressed);
  assert(frame.pause_pressed && frame.menu_pressed);

  const pc2ps3::WorldConfig config = {1.0f, 0.0f, 10.0f, 20.0f,
                                      1.0f, 0.0f};
  pc2ps3::World analog_world(config);
  frame = pc2ps3::InputFrame();
  frame.move_x = 0.5f;
  analog_world.step(frame);
  assert(near(analog_world.player().velocity.x, 5.0f));
  assert(near(analog_world.player().position.x, 69.0f));

  pc2ps3::World digital_world(config);
  frame = pc2ps3::InputFrame();
  frame.right = true;
  digital_world.step(frame);
  assert(near(digital_world.player().velocity.x, 10.0f));

  pc2ps3::World oversized_axis_world(config);
  frame = pc2ps3::InputFrame();
  frame.move_x = 2.0f;
  oversized_axis_world.step(frame);
  assert(near(oversized_axis_world.player().velocity.x, 10.0f));

  pc2ps3::World invalid_axis_world(config);
  frame = pc2ps3::InputFrame();
  frame.move_x = std::numeric_limits<float>::quiet_NaN();
  frame.left = true;
  invalid_axis_world.step(frame);
  assert(near(invalid_axis_world.player().velocity.x, -10.0f));
}
} // namespace

int main() {
  test_axes_and_conflicts();
  test_action_edges_and_reset();
  test_keyboard_mapping_and_world_motion();
  return 0;
}
