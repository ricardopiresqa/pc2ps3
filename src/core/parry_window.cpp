#include "pc2ps3/parry_window.hpp"

namespace pc2ps3 {

ParryWindow::ParryWindow(std::uint32_t window_ticks)
    : window_ticks_(window_ticks), ticks_remaining_(0),
      valid_(window_ticks > 0 && window_ticks <= kMaxWindowTicks) {}

bool ParryWindow::valid() const { return valid_; }

bool ParryWindow::step(bool activate) {
 if (!valid_) {
  return false;
 }
 if (activate) {
  ticks_remaining_ = window_ticks_;
 } else if (ticks_remaining_ > 0) {
  --ticks_remaining_;
 }
 return ticks_remaining_ > 0;
}

bool ParryWindow::active() const {
 return valid_ && ticks_remaining_ > 0;
}

bool ParryWindow::accepts_contact(bool parryable) const {
 return active() && parryable;
}

std::uint32_t ParryWindow::ticks_remaining() const {
 return valid_ ? ticks_remaining_ : 0;
}

void ParryWindow::reset() { ticks_remaining_ = 0; }

} // namespace pc2ps3
