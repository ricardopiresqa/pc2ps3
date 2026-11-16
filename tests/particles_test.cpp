#include "pc2ps3/particles.hpp"

#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "             \
                << #expression << '\n';                                       \
      ++failures;                                                              \
    }                                                                          \
  } while (false)

pc2ps3::ParticleSpawn particle(int x, int y, std::int32_t vx, std::int32_t vy,
                               std::uint32_t lifetime, int size,
                               pc2ps3::Color color) {
  pc2ps3::ParticleSpawn result = {x, y, vx, vy, lifetime, size, color};
  return result;
}

bool pixel_is(const pc2ps3::Framebuffer& framebuffer, int x, int y,
              std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  pc2ps3::Color color = {0, 0, 0, 0};
  if (!framebuffer.read_pixel(x, y, color)) return false;
  const bool matches = color.r == r && color.g == g && color.b == b &&
                       color.a == 255;
  if (!matches) {
    std::cerr << "pixel(" << x << ',' << y << ") was " << +color.r << ','
              << +color.g << ',' << +color.b << ',' << +color.a << '\n';
  }
  return matches;
}

void test_capacity_spawn_and_reuse() {
  pc2ps3::ParticleSystem system(2);
  CHECK(system.valid());
  CHECK(system.capacity() == 2);
  CHECK(system.active_count() == 0);

  const pc2ps3::Color red = {255, 0, 0, 255};
  const pc2ps3::Color green = {0, 255, 0, 255};
  const pc2ps3::Color blue = {0, 0, 255, 255};
  CHECK(system.spawn(particle(1, 0, 100, 0, 100, 1, red)));
  CHECK(system.spawn(particle(3, 0, 0, 0, 50, 1, green)));
  CHECK(!system.spawn(particle(4, 0, 0, 0, 10, 1, blue)));
  CHECK(system.active_count() == 2);
  CHECK(system.update(0));

  pc2ps3::Framebuffer framebuffer(5, 1);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer);
  CHECK(pixel_is(framebuffer, 1, 0, 255, 0, 0));
  CHECK(pixel_is(framebuffer, 3, 0, 0, 255, 0));

  CHECK(system.update(10));
  CHECK(!system.update(1001));
  CHECK(system.active_count() == 2);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer);
  CHECK(pixel_is(framebuffer, 2, 0, 255, 0, 0));
  CHECK(pixel_is(framebuffer, 3, 0, 0, 255, 0));

  CHECK(system.update(40));  // green particle reaches its exact lifetime
  CHECK(system.active_count() == 1);
  CHECK(system.spawn(particle(4, 0, 0, 0, 100, 1, blue)));
  CHECK(system.active_count() == 2);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer);
  CHECK(pixel_is(framebuffer, 2, 0, 0, 0, 0));
  CHECK(pixel_is(framebuffer, 4, 0, 0, 0, 255));

  CHECK(system.update(50));
  CHECK(system.active_count() == 1);
  CHECK(system.update(50));
  CHECK(system.active_count() == 0);
}

void test_rejects_invalid_spawn_and_capacity() {
  pc2ps3::ParticleSystem invalid_zero(0);
  pc2ps3::ParticleSystem invalid_large(1025);
  CHECK(!invalid_zero.valid());
  CHECK(!invalid_large.valid());
  CHECK(!invalid_zero.spawn(particle(0, 0, 0, 0, 1, 1,
                                     pc2ps3::Color{1, 2, 3, 255})));

  pc2ps3::ParticleSystem system(1);
  const pc2ps3::Color color = {1, 2, 3, 255};
  CHECK(!system.spawn(particle(0, 0, 0, 0, 0, 1, color)));
  CHECK(!system.spawn(particle(0, 0, 0, 0,
                               pc2ps3::ParticleSystem::kMaxLifetimeMs + 1u,
                               1, color)));
  CHECK(!system.spawn(particle(0, 0, 0, 0, 1, 0, color)));
  CHECK(!system.spawn(particle(0, 0, 0, 0, 1,
                               pc2ps3::ParticleSystem::kMaxSize + 1, color)));
  CHECK(!system.spawn(particle(
      0, 0, pc2ps3::ParticleSystem::kMaxVelocityPixelsPerSecond + 1, 0, 1, 1,
      color)));
  CHECK(system.active_count() == 0);
  CHECK(system.spawn(particle(0, 0, 0, 0, 100, 1, color)));
  CHECK(!system.update(1001));
  CHECK(system.active_count() == 1);
}

void test_camera_clipping_clear_and_alpha() {
  pc2ps3::ParticleSystem system(2);
  const pc2ps3::Color red = {255, 0, 0, 255};
  const pc2ps3::Color half_green = {0, 255, 0, 128};
  CHECK(system.spawn(particle(0, 0, 0, 0, 100, 2, red)));
  CHECK(system.spawn(particle(1, 1, 0, 0, 100, 1, half_green)));
  pc2ps3::Framebuffer framebuffer(2, 2);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer, 1, 1);
  CHECK(pixel_is(framebuffer, 0, 0, 127, 128, 0));
  CHECK(pixel_is(framebuffer, 1, 0, 0, 0, 0));

  system.clear();
  CHECK(system.active_count() == 0);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  system.draw(framebuffer);
  CHECK(pixel_is(framebuffer, 0, 0, 0, 0, 0));
}

}  // namespace

int main() {
  test_capacity_spawn_and_reuse();
  test_rejects_invalid_spawn_and_capacity();
  test_camera_clipping_clear_and_alpha();
  if (failures != 0) {
    std::cerr << failures << " particle assertion(s) failed\n";
    return 1;
  }
  std::cout << "particle tests passed\n";
  return 0;
}
