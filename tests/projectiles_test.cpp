#include "pc2ps3/projectiles.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;

#define CHECK(expression)                                                     \
  do {                                                                        \
    if (!(expression)) {                                                      \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "            \
                << #expression << '\n';                                      \
      ++failures;                                                             \
    }                                                                         \
  } while (false)

pc2ps3::ProjectileSpawn projectile(std::uint32_t id, int x, int y,
                                   std::int32_t vx, std::int32_t vy,
                                   std::uint32_t lifetime, int width,
                                   int height, pc2ps3::Color color) {
  return pc2ps3::ProjectileSpawn{id, x, y, width, height, vx, vy, lifetime,
                                 color};
}

bool pixel_is(const pc2ps3::Framebuffer& framebuffer, int x, int y,
              std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  pc2ps3::Color color = {0, 0, 0, 0};
  return framebuffer.read_pixel(x, y, color) && color.r == r && color.g == g &&
         color.b == b && color.a == 255;
}

void test_capacity_movement_and_collision_order() {
  pc2ps3::ProjectileSystem system(2);
  const pc2ps3::Color red = {255, 0, 0, 255};
  CHECK(system.valid());
  CHECK(system.capacity() == 2);
  CHECK(system.spawn(projectile(10, 0, 0, 100, 0, 1000, 2, 2, red)));
  CHECK(system.spawn(projectile(11, 0, 0, 100, 0, 1000, 2, 2, red)));
  CHECK(!system.spawn(projectile(12, 0, 0, 0, 0, 100, 1, 1, red)));
  CHECK(!system.spawn(projectile(10, 2, 2, 0, 0, 100, 1, 1, red)));
  CHECK(system.update(500));
  CHECK(!system.update(1001));
  CHECK(system.active_count() == 2);
  CHECK(system.collide(pc2ps3::Aabb{50.0f, 0.0f, 52.0f, 2.0f}));
  CHECK(system.hits().size() == 2);
  CHECK(system.hits()[0].projectile_id == 10);
  CHECK(system.hits()[1].projectile_id == 11);
  CHECK(system.active_count() == 0);
  CHECK(system.spawn(projectile(12, 0, 0, 0, 0, 100, 1, 1, red)));
}

void test_invalid_collision_preserves_state_and_expiry() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const pc2ps3::Color green = {0, 255, 0, 255};
  pc2ps3::ProjectileSystem system(1);
  CHECK(system.spawn(projectile(3, 4, 5, 0, 0, 10, 1, 1, green)));
  CHECK(!system.collide(pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}));
  CHECK(system.active_count() == 1);
  CHECK(system.hits().empty());
  CHECK(system.update(9));
  CHECK(system.active_count() == 1);
  CHECK(system.update(1));
  CHECK(system.active_count() == 0);
  CHECK(!system.spawn(projectile(4, 0, 0, 0, 0, 0, 1, 1, green)));
  CHECK(!system.spawn(projectile(4, 0, 0, 0, 0, 100, 0, 1, green)));
  CHECK(!system.spawn(projectile(4, 0, 0,
                                pc2ps3::ProjectileSystem::
                                    kMaxVelocityPixelsPerSecond + 1,
                                0, 100, 1, 1, green)));
  pc2ps3::ProjectileSystem invalid(0);
  CHECK(!invalid.valid());
  CHECK(!invalid.update(0));
}

void test_draw_clipping_alpha_and_clear() {
  pc2ps3::ProjectileSystem system(2);
  const pc2ps3::Color half_red = {255, 0, 0, 128};
  CHECK(system.spawn(projectile(1, 0, 0, 0, 0, 100, 2, 2, half_red)));
  pc2ps3::Framebuffer framebuffer(2, 2);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer, 1, 1);
  CHECK(pixel_is(framebuffer, 0, 0, 128, 0, 0));
  CHECK(pixel_is(framebuffer, 1, 0, 0, 0, 0));
  system.clear();
  CHECK(system.active_count() == 0);
}
}  // namespace

int main() {
  test_capacity_movement_and_collision_order();
  test_invalid_collision_preserves_state_and_expiry();
  test_draw_clipping_alpha_and_clear();
  if (failures != 0) {
    std::cerr << failures << " projectile assertion(s) failed\n";
    return 1;
  }
  std::cout << "projectile tests passed\n";
  return 0;
}
