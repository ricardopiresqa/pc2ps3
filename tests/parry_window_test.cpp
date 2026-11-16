#include "pc2ps3/parry_window.hpp"

#include <cassert>
#include <cstdint>

int main() {
 pc2ps3::ParryWindow window(3);
 assert(window.valid());
 assert(!window.active());
 assert(!window.step(false));

 assert(window.step(true));
 assert(window.ticks_remaining() == 3);
 assert(window.accepts_contact(true));
 assert(!window.accepts_contact(false));
 assert(window.step(false));
 assert(window.ticks_remaining() == 2);
 assert(window.step(false));
 assert(window.ticks_remaining() == 1);
 assert(!window.step(false));
 assert(!window.active());

 pc2ps3::ParryWindow one_tick(1);
 assert(one_tick.step(true));
 assert(!one_tick.step(false));

 assert(window.step(true));
 assert(window.step(false));
 assert(window.step(true));
 assert(window.ticks_remaining() == 3);
 assert(window.step(false));
 assert(window.step(false));
 assert(!window.step(false));

 window.step(true);
 window.reset();
 assert(!window.active());
 assert(window.ticks_remaining() == 0);

 pc2ps3::ParryWindow invalid_zero(0);
 pc2ps3::ParryWindow invalid_large(
     static_cast<std::uint32_t>(pc2ps3::ParryWindow::kMaxWindowTicks) + 1);
 assert(!invalid_zero.valid() && !invalid_zero.step(true));
 assert(!invalid_large.valid() && !invalid_large.step(true));
 assert(!invalid_zero.accepts_contact(true));
 return 0;
}
