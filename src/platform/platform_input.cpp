#include "pc2ps3/platform_input.hpp"

namespace pc2ps3 {

PlatformControllerSample::PlatformControllerSample()
    : dpad_left(false),
      dpad_right(false),
      dpad_up(false),
      dpad_down(false),
      axis_x(0),
      axis_y(0),
      jump(false),
      cross(false),
      square(false),
      circle(false),
      triangle(false),
      l1(false),
      r1(false),
      start(false),
      select(false) {}

void apply_platform_controller_sample(const PlatformControllerSample& sample,
                                      InputSnapshot& snapshot) {
  snapshot.dualshock3_left = sample.dpad_left;
  snapshot.dualshock3_right = sample.dpad_right;
  snapshot.dualshock3_up = sample.dpad_up;
  snapshot.dualshock3_down = sample.dpad_down;
  snapshot.dualshock3_axis_x = sample.axis_x;
  snapshot.dualshock3_axis_y = sample.axis_y;
  snapshot.dualshock3_cross = sample.cross;
  snapshot.dualshock3_square = sample.square;
  snapshot.dualshock3_circle = sample.circle;
  snapshot.dualshock3_triangle = sample.triangle;
  snapshot.dualshock3_l1 = sample.l1;
  snapshot.dualshock3_r1 = sample.r1;
  snapshot.dualshock3_start = sample.start;
  snapshot.dualshock3_select = sample.select;
  snapshot.dualshock3_jump = sample.jump;
}

}  // namespace pc2ps3
