#include "pc2ps3/platform_input.hpp"

#include <cassert>

int main() {
  pc2ps3::InputSnapshot snapshot;
  snapshot.keyboard_left = true;
  snapshot.keyboard_confirm = true;
  snapshot.dualshock3_jump = true;

  pc2ps3::PlatformControllerSample sample;
  sample.dpad_right = true;
  sample.dpad_down = true;
  sample.axis_x = 12345;
  sample.axis_y = -23456;
  sample.jump = true;
  sample.cross = true;
  sample.square = true;
  sample.circle = true;
  sample.triangle = true;
  sample.l1 = true;
  sample.r1 = true;
  sample.start = true;
  sample.select = true;

  pc2ps3::apply_platform_controller_sample(sample, snapshot);
  assert(snapshot.keyboard_left);
  assert(snapshot.keyboard_confirm);
  assert(snapshot.dualshock3_jump);
  assert(snapshot.dualshock3_right && snapshot.dualshock3_down);
  assert(!snapshot.dualshock3_left && !snapshot.dualshock3_up);
  assert(snapshot.dualshock3_axis_x == 12345);
  assert(snapshot.dualshock3_axis_y == -23456);
  assert(snapshot.dualshock3_cross && snapshot.dualshock3_square);
  assert(snapshot.dualshock3_circle && snapshot.dualshock3_triangle);
  assert(snapshot.dualshock3_l1 && snapshot.dualshock3_r1);
  assert(snapshot.dualshock3_start && snapshot.dualshock3_select);

  pc2ps3::PlatformControllerSample released;
  pc2ps3::apply_platform_controller_sample(released, snapshot);
  assert(!snapshot.dualshock3_jump && !snapshot.dualshock3_cross);
  assert(snapshot.keyboard_left && snapshot.keyboard_confirm);
  return 0;
}
