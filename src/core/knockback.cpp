#include "pc2ps3/knockback.hpp"

#include <cmath>
#include <limits>

namespace pc2ps3 {

bool apply_knockback_impulse(Actor& actor, Vec2 impulse) {
  if (!std::isfinite(actor.velocity.x) || !std::isfinite(actor.velocity.y) ||
      !std::isfinite(impulse.x) || !std::isfinite(impulse.y)) {
    return false;
  }
  const double velocity_x = static_cast<double>(actor.velocity.x) + impulse.x;
  const double velocity_y = static_cast<double>(actor.velocity.y) + impulse.y;
  const double max_float = std::numeric_limits<float>::max();
  if (!std::isfinite(velocity_x) || !std::isfinite(velocity_y) ||
      std::fabs(velocity_x) > max_float || std::fabs(velocity_y) > max_float) {
    return false;
  }
  actor.velocity = Vec2{static_cast<float>(velocity_x),
                        static_cast<float>(velocity_y)};
  if (impulse.y != 0.0f) actor.grounded = false;
  return true;
}

}  // namespace pc2ps3
