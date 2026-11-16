#include "pc2ps3/render.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
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
  if (!same_color(actual, expected)) {
    std::cerr << "pixel (" << x << "," << y << ") expected "
              << static_cast<int>(expected.r) << ","
              << static_cast<int>(expected.g) << ","
              << static_cast<int>(expected.b) << ","
              << static_cast<int>(expected.a) << " got "
              << static_cast<int>(actual.r) << ","
              << static_cast<int>(actual.g) << ","
              << static_cast<int>(actual.b) << ","
              << static_cast<int>(actual.a) << "\n";
  }
  assert(same_color(actual, expected));
}

}  // namespace

int main() {
  const pc2ps3::Color background = {20, 40, 60, 128};
  const pc2ps3::Color opaque_red = {200, 20, 0, 255};
  const pc2ps3::Color transparent_green = {0, 200, 0, 0};
  const pc2ps3::Color half_blue = {0, 0, 200, 128};
  const std::vector<pc2ps3::Color> pixels = {
      opaque_red, transparent_green, half_blue};
  const pc2ps3::Texture texture(3, 1, pixels);
  assert(texture.valid() && texture.width() == 3 && texture.height() == 1);
  assert(texture.pixel_storage_bytes() >=
         pixels.size() * sizeof(pc2ps3::Color));

  const pc2ps3::Texture empty_texture;
  const pc2ps3::Texture bad_dimensions(2, 2, pixels);
  assert(!empty_texture.valid());
  assert(empty_texture.pixel_storage_bytes() == 0);
  assert(!bad_dimensions.valid());

  pc2ps3::Framebuffer framebuffer(5, 3);
  framebuffer.clear(background);
  assert(framebuffer.draw_texture_region(texture, 0, 0, 3, 1, 1, 1));
  expect_pixel(framebuffer, 1, 1, opaque_red);
  expect_pixel(framebuffer, 2, 1, background);
  expect_pixel(framebuffer, 3, 1, pc2ps3::Color{7, 13, 153, 192});

  pc2ps3::Framebuffer clipped(2, 1);
  clipped.clear(background);
  assert(clipped.draw_texture_region(texture, 0, 0, 3, 1, -1, 0));
  expect_pixel(clipped, 0, 0, background);
  expect_pixel(clipped, 1, 0, pc2ps3::Color{7, 13, 153, 192});

  const pc2ps3::Texture opaque_grid(
      3, 3, std::vector<pc2ps3::Color>(9, opaque_red));
  pc2ps3::Framebuffer clipped_all_sides(2, 2);
  clipped_all_sides.clear(background);
  assert(clipped_all_sides.draw_texture_region(
      opaque_grid, 0, 0, 3, 3, -1, -1));
  expect_pixel(clipped_all_sides, 0, 0, opaque_red);
  expect_pixel(clipped_all_sides, 1, 1, opaque_red);
  clipped_all_sides.clear(background);
  assert(clipped_all_sides.draw_texture_region(
      opaque_grid, 0, 0, 3, 3, 1, 1));
  expect_pixel(clipped_all_sides, 1, 1, opaque_red);

  const pc2ps3::Color before = {9, 8, 7, 6};
  pc2ps3::Framebuffer unchanged(2, 1);
  unchanged.clear(before);
  assert(!unchanged.draw_texture_region(texture, 2, 0, 2, 1, 0, 0));
  expect_pixel(unchanged, 0, 0, before);
  assert(!unchanged.draw_texture_region(empty_texture, 0, 0, 1, 1, 0, 0));
  expect_pixel(unchanged, 0, 0, before);

  assert(framebuffer.draw_texture_region(
      texture, 0, 0, 3, 1, std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max()));
  expect_pixel(framebuffer, 0, 0, background);

  const pc2ps3::Color scale_green = {0, 220, 0, 255};
  const pc2ps3::Color scale_blue = {0, 0, 220, 255};
  const pc2ps3::Color scale_white = {220, 220, 220, 255};
  const pc2ps3::Texture scale_texture(
      2, 2, std::vector<pc2ps3::Color>{opaque_red, scale_green, scale_blue,
                                        scale_white});
  pc2ps3::Framebuffer scaled(4, 4);
  scaled.clear(background);
  assert(scaled.draw_texture_region_scaled(scale_texture, 0, 0, 2, 2, 0, 0,
                                           4, 4));
  expect_pixel(scaled, 0, 0, opaque_red);
  expect_pixel(scaled, 1, 1, opaque_red);
  expect_pixel(scaled, 2, 0, scale_green);
  expect_pixel(scaled, 0, 2, scale_blue);
  expect_pixel(scaled, 3, 3, scale_white);
  scaled.clear(background);
  assert(scaled.draw_texture_region_scaled(scale_texture, 0, 0, 2, 2, 0, 0,
                                           4, 4, true));
  expect_pixel(scaled, 3, 0, opaque_red);
  expect_pixel(scaled, 0, 0, scale_green);
  expect_pixel(scaled, 0, 3, scale_white);

  pc2ps3::Framebuffer downscaled(1, 1);
  downscaled.clear(background);
  assert(downscaled.draw_texture_region_scaled(scale_texture, 0, 0, 2, 2,
                                               0, 0, 1, 1));
  expect_pixel(downscaled, 0, 0, opaque_red);

  pc2ps3::Framebuffer clipped_scaled(4, 4);
  clipped_scaled.clear(background);
  assert(clipped_scaled.draw_texture_region_scaled(
      scale_texture, 0, 0, 2, 2, -2, -2, 4, 4));
  expect_pixel(clipped_scaled, 0, 0, scale_white);
  clipped_scaled.clear(background);
  assert(clipped_scaled.draw_texture_region_scaled(
      scale_texture, 0, 0, 2, 2, 2, 2, 4, 4));
  expect_pixel(clipped_scaled, 3, 3, opaque_red);
  assert(clipped_scaled.draw_texture_region_scaled(
      scale_texture, 0, 0, 2, 2, std::numeric_limits<int>::max(), 0, 4, 4));
  assert(!clipped_scaled.draw_texture_region_scaled(
      scale_texture, 0, 0, 2, 2, 0, 0, 0, 4));
  assert(!clipped_scaled.draw_texture_region_scaled(
      scale_texture, 1, 0, 2, 1, 0, 0, 2, 2));
  expect_pixel(clipped_scaled, 3, 3, opaque_red);

  pc2ps3::Framebuffer rejected_scaled(4, 4);
  rejected_scaled.clear(background);
  assert(!rejected_scaled.draw_texture_region_scaled(
      scale_texture, 1, 0, 2, 1, 0, 0, 2, 2));
  expect_pixel(rejected_scaled, 0, 0, background);
  expect_pixel(rejected_scaled, 3, 3, background);
  assert(!rejected_scaled.draw_texture_region_scaled(
      scale_texture, 0, 0, 2, 2, 0, 0, -1, 2));
  expect_pixel(rejected_scaled, 0, 0, background);
  expect_pixel(rejected_scaled, 3, 3, background);

  pc2ps3::Framebuffer scaled_alpha(6, 2);
  scaled_alpha.clear(background);
  assert(scaled_alpha.draw_texture_region_scaled(texture, 0, 0, 3, 1, 0, 0,
                                                 6, 2));
  expect_pixel(scaled_alpha, 0, 0, opaque_red);
  expect_pixel(scaled_alpha, 2, 0, background);
  expect_pixel(scaled_alpha, 4, 0, pc2ps3::Color{7, 13, 153, 192});

  return 0;
}
