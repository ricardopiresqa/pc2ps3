#include "pc2ps3/camera.hpp"

#include <cassert>
#include <limits>

int main() {
  pc2ps3::Camera2D camera(100, 50);
  assert(camera.valid());
  assert(camera.set_world_bounds(pc2ps3::Aabb{0.0f, 0.0f, 400.0f, 200.0f}));
  assert(camera.follow(pc2ps3::Vec2{200.0f, 100.0f}));
  assert(camera.x() == 150);
  assert(camera.y() == 75);
  assert(camera.follow(pc2ps3::Vec2{0.0f, 0.0f}));
  assert(camera.x() == 0 && camera.y() == 0);
  assert(camera.follow(pc2ps3::Vec2{400.0f, 200.0f}));
  assert(camera.x() == 300 && camera.y() == 150);

  assert(camera.set_world_bounds(
      pc2ps3::Aabb{-10.0f, -20.0f, 50.0f, 20.0f}));
  assert(camera.x() == -30 && camera.y() == -25);
  assert(camera.follow(pc2ps3::Vec2{10000.0f, -10000.0f}));
  assert(camera.x() == -30 && camera.y() == -25);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!camera.set_world_bounds(pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}));
  assert(!camera.follow(pc2ps3::Vec2{nan, 0.0f}));
  assert(camera.x() == -30 && camera.y() == -25);
  pc2ps3::Camera2D invalid(0, 50);
  assert(!invalid.valid());
  assert(!invalid.set_world_bounds(pc2ps3::Aabb{0.0f, 0.0f, 10.0f, 10.0f}));
  return 0;
}
