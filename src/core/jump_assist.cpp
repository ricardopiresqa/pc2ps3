#include "pc2ps3/jump_assist.hpp"

namespace pc2ps3 {
namespace {

const unsigned int kJumpWindowTicks = 6u;

} // namespace

JumpAssist::JumpAssist() : jump_buffer_remaining_(0), coyote_remaining_(0) {}

void JumpAssist::reset() {
  jump_buffer_remaining_ = 0;
  coyote_remaining_ = 0;
}

bool JumpAssist::update(bool jump_pressed, bool grounded) {
  if (jump_pressed) jump_buffer_remaining_ = kJumpWindowTicks;
  if (grounded) coyote_remaining_ = kJumpWindowTicks;

  if (jump_buffer_remaining_ != 0 && (grounded || coyote_remaining_ != 0)) {
    jump_buffer_remaining_ = 0;
    coyote_remaining_ = 0;
    return true;
  }

  if (jump_buffer_remaining_ != 0) --jump_buffer_remaining_;
  if (!grounded && coyote_remaining_ != 0) --coyote_remaining_;
  return false;
}

} // namespace pc2ps3
