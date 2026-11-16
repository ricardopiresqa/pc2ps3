#pragma once

#include "pc2ps3/input.hpp"
#include <cstdint>

namespace pc2ps3 {

// Canonical controller state after the device backend has applied its own
// axis orientation, normalization, and dead-zone.
struct PlatformControllerSample {
  bool dpad_left;
  bool dpad_right;
  bool dpad_up;
  bool dpad_down;
  std::int16_t axis_x;
  std::int16_t axis_y;
  bool jump;
  bool cross;
  bool square;
  bool circle;
  bool triangle;
  bool l1;
  bool r1;
  bool start;
  bool select;

  PlatformControllerSample();
};

// Replaces only the controller portion of snapshot. Keyboard state and
// game-specific policy remain owned by the caller.
void apply_platform_controller_sample(const PlatformControllerSample& sample,
                                      InputSnapshot& snapshot);

}  // namespace pc2ps3
