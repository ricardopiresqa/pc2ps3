#include "pc2ps3/physics.hpp"

#include <algorithm>
#include <cmath>

namespace pc2ps3 {
namespace {

bool valid(const Aabb& box) {
  if (!std::isfinite(box.left) || !std::isfinite(box.top) ||
      !std::isfinite(box.right) || !std::isfinite(box.bottom) ||
      box.right <= box.left || box.bottom <= box.top) {
    return false;
  }
  return std::isfinite(box.right - box.left) &&
         std::isfinite(box.bottom - box.top);
}

AabbContact no_contact() {
  return AabbContact{false, Vec2{0.0f, 0.0f}, 0.0f};
}

}  // namespace

AabbContact query_aabb(const Aabb& first, const Aabb& second) {
  if (!valid(first) || !valid(second)) {
    return no_contact();
  }

  if (first.right <= second.left || second.right <= first.left ||
      first.bottom <= second.top || second.bottom <= first.top) {
    return no_contact();
  }

  const double move_first_left = static_cast<double>(first.right) - second.left;
  const double move_first_right = static_cast<double>(second.right) - first.left;
  const double move_first_up = static_cast<double>(first.bottom) - second.top;
  const double move_first_down = static_cast<double>(second.bottom) - first.top;
  const double overlap_x = std::min(move_first_left, move_first_right);
  const double overlap_y = std::min(move_first_up, move_first_down);

  if (overlap_x <= overlap_y) {
    const float direction = move_first_left <= move_first_right ? 1.0f : -1.0f;
    return AabbContact{true, Vec2{direction, 0.0f}, static_cast<float>(overlap_x)};
  }

  const float direction = move_first_up <= move_first_down ? 1.0f : -1.0f;
  return AabbContact{true, Vec2{0.0f, direction}, static_cast<float>(overlap_y)};
}

}  // namespace pc2ps3
