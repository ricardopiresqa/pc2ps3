#pragma once

#include <cstdint>

namespace pc2ps3 {

// Bounded tick window. Input and parry rewards remain caller-owned.
class ParryWindow {
 public:
 enum { kMaxWindowTicks = 65535 };

 explicit ParryWindow(std::uint32_t window_ticks);

 bool valid() const;
 // The activation tick counts as the first active tick.
 bool step(bool activate);
 bool active() const;
 bool accepts_contact(bool parryable) const;
 std::uint32_t ticks_remaining() const;
 void reset();

 private:
 std::uint32_t window_ticks_;
 std::uint32_t ticks_remaining_;
 bool valid_;
};

} // namespace pc2ps3
