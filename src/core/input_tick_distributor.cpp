#include "pc2ps3/input_tick_distributor.hpp"

namespace pc2ps3 {
namespace {

void clear_pressed(InputFrame& input) {
  input.jump_pressed = false;
  input.confirm_pressed = false;
  input.attack_pressed = false;
  input.cancel_pressed = false;
  input.ability_pressed = false;
  input.l1_pressed = false;
  input.r1_pressed = false;
  input.pause_pressed = false;
  input.menu_pressed = false;
}

void accumulate_pressed(InputFrame& pending, const InputFrame& input) {
  pending.jump_pressed = pending.jump_pressed || input.jump_pressed;
  pending.confirm_pressed = pending.confirm_pressed || input.confirm_pressed;
  pending.attack_pressed = pending.attack_pressed || input.attack_pressed;
  pending.cancel_pressed = pending.cancel_pressed || input.cancel_pressed;
  pending.ability_pressed = pending.ability_pressed || input.ability_pressed;
  pending.l1_pressed = pending.l1_pressed || input.l1_pressed;
  pending.r1_pressed = pending.r1_pressed || input.r1_pressed;
  pending.pause_pressed = pending.pause_pressed || input.pause_pressed;
  pending.menu_pressed = pending.menu_pressed || input.menu_pressed;
}

} // namespace

InputTickDistributor::InputTickDistributor()
    : latest_(), pending_pressed_(), has_input_(false) {}

void InputTickDistributor::submit(const InputFrame& input) {
  latest_ = input;
  accumulate_pressed(pending_pressed_, input);
  has_input_ = true;
}

bool InputTickDistributor::consume(InputFrame& output) {
  if (!has_input_) {
    return false;
  }

  output = latest_;
  output.jump_pressed = pending_pressed_.jump_pressed;
  output.confirm_pressed = pending_pressed_.confirm_pressed;
  output.attack_pressed = pending_pressed_.attack_pressed;
  output.cancel_pressed = pending_pressed_.cancel_pressed;
  output.ability_pressed = pending_pressed_.ability_pressed;
  output.l1_pressed = pending_pressed_.l1_pressed;
  output.r1_pressed = pending_pressed_.r1_pressed;
  output.pause_pressed = pending_pressed_.pause_pressed;
  output.menu_pressed = pending_pressed_.menu_pressed;
  clear_pressed(pending_pressed_);
  return true;
}

void InputTickDistributor::reset() {
  latest_ = InputFrame();
  pending_pressed_ = InputFrame();
  has_input_ = false;
}

} // namespace pc2ps3
