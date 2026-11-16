#include "pc2ps3/camera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pc2ps3 {

Camera2D::Camera2D(int viewport_width, int viewport_height)
    : viewport_width_(viewport_width),
      viewport_height_(viewport_height),
      world_bounds_{0.0f, 0.0f, 0.0f, 0.0f},
      target_{0.0f, 0.0f},
      x_(0),
      y_(0),
      valid_(viewport_width > 0 && viewport_width <= kMaxViewportDimension &&
             viewport_height > 0 &&
             viewport_height <= kMaxViewportDimension),
      has_world_bounds_(false),
      has_target_(false) {}

bool Camera2D::valid() const { return valid_; }

bool Camera2D::set_world_bounds(const Aabb& bounds) {
  if (!valid_ || !query_aabb(bounds, bounds).overlapping) return false;
  int new_x = x_;
  int new_y = y_;
  if (has_target_ && !calculate_scroll(bounds, target_, new_x, new_y)) {
    return false;
  }
  world_bounds_ = bounds;
  has_world_bounds_ = true;
  if (has_target_) {
    x_ = new_x;
    y_ = new_y;
  }
  return true;
}

bool Camera2D::follow(Vec2 target) {
  if (!valid_ || !has_world_bounds_ || !std::isfinite(target.x) ||
      !std::isfinite(target.y)) {
    return false;
  }
  int new_x = x_;
  int new_y = y_;
  if (!calculate_scroll(world_bounds_, target, new_x, new_y)) return false;
  target_ = target;
  x_ = new_x;
  y_ = new_y;
  has_target_ = true;
  return true;
}

int Camera2D::x() const { return x_; }
int Camera2D::y() const { return y_; }
int Camera2D::viewport_width() const { return viewport_width_; }
int Camera2D::viewport_height() const { return viewport_height_; }

bool Camera2D::calculate_scroll(const Aabb& bounds, Vec2 target, int& x,
                                int& y) const {
  const double world_width = static_cast<double>(bounds.right) - bounds.left;
  const double world_height = static_cast<double>(bounds.bottom) - bounds.top;
  double left = static_cast<double>(target.x) - viewport_width_ / 2.0;
  double top = static_cast<double>(target.y) - viewport_height_ / 2.0;
  if (world_width <= viewport_width_) {
    left = bounds.left - (viewport_width_ - world_width) / 2.0;
  } else {
    left = std::max(static_cast<double>(bounds.left),
                    std::min(left, static_cast<double>(bounds.right) -
                                       viewport_width_));
  }
  if (world_height <= viewport_height_) {
    top = bounds.top - (viewport_height_ - world_height) / 2.0;
  } else {
    top = std::max(static_cast<double>(bounds.top),
                   std::min(top, static_cast<double>(bounds.bottom) -
                                      viewport_height_));
  }
  return to_integer_scroll(left, x) && to_integer_scroll(top, y);
}

bool Camera2D::to_integer_scroll(double value, int& output) {
  if (!std::isfinite(value)) return false;
  const double floored = std::floor(value);
  if (floored < std::numeric_limits<int>::min() ||
      floored > std::numeric_limits<int>::max()) {
    return false;
  }
  output = static_cast<int>(floored);
  return true;
}

}  // namespace pc2ps3
