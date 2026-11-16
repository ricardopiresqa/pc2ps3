#pragma once

#include "pc2ps3/core.hpp"

#include <cstdint>

namespace pc2ps3 {

// Platform adapters sample hardware and fill logical button states. Analog
// axes are normalized to the signed 16-bit range with dead-zones applied.
struct InputSnapshot {
  bool keyboard_left;
  bool keyboard_right;
  bool keyboard_up;
  bool keyboard_down;
  bool keyboard_jump;
  bool keyboard_confirm;
  bool keyboard_attack;
  bool keyboard_cancel;
  bool keyboard_ability;
  bool keyboard_l1;
  bool keyboard_r1;
  bool keyboard_pause;
  bool keyboard_menu;
  bool dualshock3_left;
  bool dualshock3_right;
  bool dualshock3_up;
  bool dualshock3_down;
  bool dualshock3_jump;
  // Positive X is right; positive Y is down.
  std::int16_t dualshock3_axis_x;
  std::int16_t dualshock3_axis_y;
  bool dualshock3_cross;
  bool dualshock3_square;
  bool dualshock3_circle;
  bool dualshock3_triangle;
  bool dualshock3_l1;
  bool dualshock3_r1;
  bool dualshock3_start;
  bool dualshock3_select;

  InputSnapshot();
};

// Combines keyboard and controller states into platform-independent actions.
class InputMapper {
public:
  InputMapper();

  InputFrame sample(const InputSnapshot& snapshot);
  void reset();

private:
  struct PreviousState {
    bool confirm;
    bool attack;
    bool cancel;
    bool ability;
    bool l1;
    bool r1;
    bool pause;
    bool menu;
  };
  PreviousState previous_;
};

} // namespace pc2ps3
