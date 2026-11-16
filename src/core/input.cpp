#include "pc2ps3/input.hpp"

namespace pc2ps3 {
namespace {
float normalize_axis(std::int16_t axis) {
  return axis < 0 ? static_cast<float>(axis) / 32768.0f
                  : static_cast<float>(axis) / 32767.0f;
}

int digital_axis(bool negative_a, bool positive_a,
                 bool negative_b, bool positive_b) {
  const bool negative = negative_a || negative_b;
  const bool positive = positive_a || positive_b;
  if (negative == positive) return 0;
  return positive ? 1 : -1;
}

bool pressed(bool held, bool was_held) { return held && !was_held; }
} // namespace

InputSnapshot::InputSnapshot()
  : keyboard_left(false), keyboard_right(false), keyboard_up(false),
    keyboard_down(false), keyboard_jump(false), keyboard_confirm(false),
    keyboard_attack(false), keyboard_cancel(false), keyboard_ability(false),
    keyboard_l1(false), keyboard_r1(false), keyboard_pause(false),
    keyboard_menu(false), dualshock3_left(false), dualshock3_right(false),
    dualshock3_up(false), dualshock3_down(false), dualshock3_jump(false),
    dualshock3_axis_x(0), dualshock3_axis_y(0), dualshock3_cross(false),
    dualshock3_square(false), dualshock3_circle(false), dualshock3_triangle(false),
    dualshock3_l1(false), dualshock3_r1(false), dualshock3_start(false),
    dualshock3_select(false) {}

InputMapper::InputMapper() : previous_() {}

InputFrame InputMapper::sample(const InputSnapshot& snapshot) {
  InputFrame frame = {};
  const int digital_x = digital_axis(
    snapshot.keyboard_left, snapshot.keyboard_right,
    snapshot.dualshock3_left, snapshot.dualshock3_right);
  const int digital_y = digital_axis(
    snapshot.keyboard_up, snapshot.keyboard_down,
    snapshot.dualshock3_up, snapshot.dualshock3_down);
  const bool digital_x_active = snapshot.keyboard_left || snapshot.keyboard_right ||
    snapshot.dualshock3_left || snapshot.dualshock3_right;
  const bool digital_y_active = snapshot.keyboard_up || snapshot.keyboard_down ||
    snapshot.dualshock3_up || snapshot.dualshock3_down;
  frame.move_x = digital_x_active ? static_cast<float>(digital_x)
                                  : normalize_axis(snapshot.dualshock3_axis_x);
  frame.move_y = digital_y_active ? static_cast<float>(digital_y)
                                  : normalize_axis(snapshot.dualshock3_axis_y);
  frame.left = frame.move_x < 0.0f;
  frame.right = frame.move_x > 0.0f;
  frame.up = frame.move_y < 0.0f;
  frame.down = frame.move_y > 0.0f;

  const bool confirm = snapshot.keyboard_confirm || snapshot.keyboard_jump ||
    snapshot.dualshock3_cross || snapshot.dualshock3_jump;
  const bool attack = snapshot.keyboard_attack || snapshot.dualshock3_square;
  const bool cancel = snapshot.keyboard_cancel || snapshot.dualshock3_circle;
  const bool ability = snapshot.keyboard_ability || snapshot.dualshock3_triangle;
  const bool l1 = snapshot.keyboard_l1 || snapshot.dualshock3_l1;
  const bool r1 = snapshot.keyboard_r1 || snapshot.dualshock3_r1;
  const bool pause = snapshot.keyboard_pause || snapshot.dualshock3_start;
  const bool menu = snapshot.keyboard_menu || snapshot.dualshock3_select;

  frame.confirm_held = confirm;
  frame.confirm_pressed = pressed(confirm, previous_.confirm);
  frame.jump_held = confirm;
  frame.jump_pressed = frame.confirm_pressed;
  frame.attack_held = attack;
  frame.attack_pressed = pressed(attack, previous_.attack);
  frame.cancel_held = cancel;
  frame.cancel_pressed = pressed(cancel, previous_.cancel);
  frame.ability_held = ability;
  frame.ability_pressed = pressed(ability, previous_.ability);
  frame.l1_held = l1;
  frame.l1_pressed = pressed(l1, previous_.l1);
  frame.r1_held = r1;
  frame.r1_pressed = pressed(r1, previous_.r1);
  frame.pause_held = pause;
  frame.pause_pressed = pressed(pause, previous_.pause);
  frame.menu_held = menu;
  frame.menu_pressed = pressed(menu, previous_.menu);

  previous_.confirm = confirm;
  previous_.attack = attack;
  previous_.cancel = cancel;
  previous_.ability = ability;
  previous_.l1 = l1;
  previous_.r1 = r1;
  previous_.pause = pause;
  previous_.menu = menu;
  return frame;
}

void InputMapper::reset() { previous_ = PreviousState(); }
} // namespace pc2ps3
