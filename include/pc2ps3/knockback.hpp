#pragma once

#include "pc2ps3/core.hpp"

namespace pc2ps3 {

// Adds a caller-specified velocity impulse atomically; no game tuning is implied.
bool apply_knockback_impulse(Actor& actor, Vec2 impulse);

}  // namespace pc2ps3
