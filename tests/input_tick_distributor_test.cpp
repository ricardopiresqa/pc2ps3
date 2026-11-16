#include "pc2ps3/input_tick_distributor.hpp"

#include <cassert>

namespace {

void test_empty_consume_preserves_output() {
  pc2ps3::InputTickDistributor distributor;
  pc2ps3::InputFrame output = {};
  output.left = true;
  output.move_x = 0.75f;
  assert(!distributor.consume(output));
  assert(output.left && output.move_x == 0.75f);
}

void test_short_press_survives_release_until_tick() {
  pc2ps3::InputTickDistributor distributor;
  pc2ps3::InputFrame pressed = {};
  pressed.confirm_pressed = true;
  pressed.jump_pressed = true;
  pressed.confirm_held = true;
  pressed.jump_held = true;
  pressed.move_x = 0.5f;
  distributor.submit(pressed);

  pc2ps3::InputFrame released = {};
  released.move_x = -0.25f;
  released.move_y = 0.75f;
  distributor.submit(released);

  pc2ps3::InputFrame output = {};
  assert(distributor.consume(output));
  assert(output.confirm_pressed && output.jump_pressed);
  assert(!output.confirm_held && !output.jump_held);
  assert(output.move_x == -0.25f && output.move_y == 0.75f);

  assert(distributor.consume(output));
  assert(!output.confirm_pressed && !output.jump_pressed);
  assert(!output.confirm_held && !output.jump_held);
  assert(output.move_x == -0.25f && output.move_y == 0.75f);
}

void test_pressed_flags_accumulate_and_continuous_state_repeats() {
  pc2ps3::InputTickDistributor distributor;
  pc2ps3::InputFrame first = {};
  first.attack_pressed = true;
  first.attack_held = true;
  first.left = true;
  first.move_x = -1.0f;
  distributor.submit(first);

  pc2ps3::InputFrame latest = {};
  latest.cancel_pressed = true;
  latest.attack_held = true;
  latest.right = true;
  latest.move_x = 0.25f;
  distributor.submit(latest);

  pc2ps3::InputFrame output = {};
  assert(distributor.consume(output));
  assert(output.attack_pressed && output.cancel_pressed);
  assert(output.attack_held && output.right && !output.left);
  assert(output.move_x == 0.25f);

  assert(distributor.consume(output));
  assert(!output.attack_pressed && !output.cancel_pressed);
  assert(output.attack_held && output.right && output.move_x == 0.25f);
}

void test_reset_clears_pending_state() {
  pc2ps3::InputTickDistributor distributor;
  pc2ps3::InputFrame input = {};
  input.menu_pressed = true;
  input.menu_held = true;
  input.up = true;
  distributor.submit(input);
  distributor.reset();

  pc2ps3::InputFrame output = {};
  output.down = true;
  assert(!distributor.consume(output));
  assert(output.down);

  input = pc2ps3::InputFrame();
  distributor.submit(input);
  assert(distributor.consume(output));
  assert(!output.menu_pressed && !output.menu_held && !output.up);
}

} // namespace

int main() {
  test_empty_consume_preserves_output();
  test_short_press_survives_release_until_tick();
  test_pressed_flags_accumulate_and_continuous_state_repeats();
  test_reset_clears_pending_state();
  return 0;
}
