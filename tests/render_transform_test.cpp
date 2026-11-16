#include "pc2ps3/render.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

namespace {

bool same_color(const pc2ps3::Color& left, const pc2ps3::Color& right) {
  return left.r == right.r && left.g == right.g && left.b == right.b &&
         left.a == right.a;
}

void expect_pixel(const pc2ps3::Framebuffer& framebuffer, int x, int y,
                  const pc2ps3::Color& expected) {
  pc2ps3::Color actual = {0, 0, 0, 0};
  assert(framebuffer.read_pixel(x, y, actual));
  assert(same_color(actual, expected));
}

std::size_t count_color(const pc2ps3::Framebuffer& framebuffer,
                        const pc2ps3::Color& color) {
  std::size_t count = 0;
  for (int y = 0; y < framebuffer.height(); ++y) {
    for (int x = 0; x < framebuffer.width(); ++x) {
      pc2ps3::Color actual = {0, 0, 0, 0};
      assert(framebuffer.read_pixel(x, y, actual));
      if (same_color(actual, color)) ++count;
    }
  }
  return count;
}

}  // namespace

int main() {
  const pc2ps3::Color background = {10, 20, 30, 255};
  const pc2ps3::Color red = {220, 0, 0, 255};
  const pc2ps3::Color green = {0, 220, 0, 255};
  const pc2ps3::Color blue = {0, 0, 220, 255};
  const pc2ps3::Color white = {220, 220, 220, 255};
  const pc2ps3::Texture square(
      2, 2, std::vector<pc2ps3::Color>{red, green, blue, white});

  pc2ps3::Framebuffer rotated(4, 4);
  rotated.clear(background);
  assert(rotated.draw_texture_region_rotated(square, 0, 0, 2, 2, 1, 1, 90));
  expect_pixel(rotated, 1, 1, blue);
  expect_pixel(rotated, 2, 1, red);
  expect_pixel(rotated, 1, 2, white);
  expect_pixel(rotated, 2, 2, green);

  rotated.clear(background);
  assert(rotated.draw_texture_region_rotated(square, 0, 0, 2, 2, 0, 0, 180));
  expect_pixel(rotated, 0, 0, white);
  expect_pixel(rotated, 1, 0, blue);
  expect_pixel(rotated, 0, 1, green);
  expect_pixel(rotated, 1, 1, red);

  rotated.clear(background);
  assert(rotated.draw_texture_region_rotated(square, 0, 0, 2, 2, 0, 0, 270));
  expect_pixel(rotated, 0, 0, green);
  expect_pixel(rotated, 1, 0, white);
  expect_pixel(rotated, 0, 1, red);
  expect_pixel(rotated, 1, 1, blue);

  const pc2ps3::Texture line(2, 1, std::vector<pc2ps3::Color>{red, green});
  rotated.clear(background);
  assert(rotated.draw_texture_region_rotated(line, 0, 0, 2, 1, 1, 1, 90));
  expect_pixel(rotated, 1, 0, red);
  expect_pixel(rotated, 1, 1, green);

  pc2ps3::Framebuffer clipped(2, 2);
  clipped.clear(background);
  assert(clipped.draw_texture_region_rotated(square, 0, 0, 2, 2, -1, -1, 90));
  expect_pixel(clipped, 0, 0, green);
  clipped.clear(background);
  assert(clipped.draw_texture_region_rotated(square, 0, 0, 2, 2, 1, 1, 90));
  expect_pixel(clipped, 1, 1, blue);

  const pc2ps3::Color transparent = {200, 20, 40, 0};
  const pc2ps3::Color half_blue = {0, 0, 200, 128};
  const pc2ps3::Texture alpha_line(
      2, 1, std::vector<pc2ps3::Color>{transparent, half_blue});
  pc2ps3::Framebuffer alpha(3, 3);
  alpha.clear(background);
  assert(alpha.draw_texture_region_rotated(alpha_line, 0, 0, 2, 1, 1, 1, 90));
  expect_pixel(alpha, 1, 0, background);
  expect_pixel(alpha, 1, 1, pc2ps3::Color{5, 10, 115, 255});

  const pc2ps3::Color center = {250, 180, 10, 255};
  const pc2ps3::Texture grid(
      3, 3, std::vector<pc2ps3::Color>{red, green, blue, white, center,
                                        red, blue, white, green});
  rotated.clear(background);
  assert(rotated.draw_texture_region_rotated(grid, 0, 0, 3, 3, 0, 0, 45));
  expect_pixel(rotated, 1, 1, center);
  assert(count_color(rotated, background) < 16);

  rotated.clear(background);
  assert(!rotated.draw_texture_region_rotated(square, 0, 0, 2, 2, 0, 0, 360));
  assert(!rotated.draw_texture_region_rotated(square, 1, 0, 2, 1, 0, 0, 90));
  expect_pixel(rotated, 0, 0, background);
  return 0;
}
