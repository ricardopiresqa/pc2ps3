#pragma once

#include "pc2ps3/physics.hpp"

namespace pc2ps3 {

// Integer-scroll camera: centers the target and clamps to finite world bounds.
class Camera2D {
 public:
  enum { kMaxViewportDimension = 4096 };

  Camera2D(int viewport_width, int viewport_height);

  bool valid() const;
  bool set_world_bounds(const Aabb& bounds);
  bool follow(Vec2 target);
  int x() const;
  int y() const;
  int viewport_width() const;
  int viewport_height() const;

 private:
  bool calculate_scroll(const Aabb& bounds, Vec2 target, int& x, int& y) const;
  static bool to_integer_scroll(double value, int& output);

  int viewport_width_;
  int viewport_height_;
  Aabb world_bounds_;
  Vec2 target_;
  int x_;
  int y_;
  bool valid_;
  bool has_world_bounds_;
  bool has_target_;
};

}  // namespace pc2ps3
