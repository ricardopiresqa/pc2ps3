#pragma once

#include "pc2ps3/core.hpp"

#include <cstddef>

namespace pc2ps3 {

// Holds two already-mapped player frames independently for one simulation tick.
class CoopInputRouter {
 public:
 enum { kPlayerCount = 2 };

 CoopInputRouter();

 void begin_tick();
 bool submit(std::size_t player_slot, const InputFrame& frame);
 const InputFrame* frame(std::size_t player_slot) const;
 bool ready(std::size_t player_slot) const;

 private:
 InputFrame frames_[kPlayerCount];
 bool available_[kPlayerCount];
};

} // namespace pc2ps3
