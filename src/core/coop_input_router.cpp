#include "pc2ps3/coop_input_router.hpp"

namespace pc2ps3 {

CoopInputRouter::CoopInputRouter() {
 for (std::size_t i = 0; i < kPlayerCount; ++i) {
  available_[i] = false;
 }
}

void CoopInputRouter::begin_tick() {
 for (std::size_t i = 0; i < kPlayerCount; ++i) {
  available_[i] = false;
 }
}

bool CoopInputRouter::submit(std::size_t player_slot,
                             const InputFrame& input) {
 if (player_slot >= kPlayerCount || available_[player_slot]) {
  return false;
 }
 frames_[player_slot] = input;
 available_[player_slot] = true;
 return true;
}

const InputFrame* CoopInputRouter::frame(std::size_t player_slot) const {
 if (!ready(player_slot)) {
  return 0;
 }
 return &frames_[player_slot];
}

bool CoopInputRouter::ready(std::size_t player_slot) const {
 return player_slot < kPlayerCount && available_[player_slot];
}

} // namespace pc2ps3
