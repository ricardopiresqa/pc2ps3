#include "pc2ps3/render.hpp"

#include <cassert>

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

void expect_same_framebuffer(const pc2ps3::Framebuffer& left,
                             const pc2ps3::Framebuffer& right) {
  assert(left.width() == right.width());
  assert(left.height() == right.height());
  for (int y = 0; y < left.height(); ++y) {
    for (int x = 0; x < left.width(); ++x) {
      pc2ps3::Color left_pixel = {0, 0, 0, 0};
      pc2ps3::Color right_pixel = {0, 0, 0, 0};
      assert(left.read_pixel(x, y, left_pixel));
      assert(right.read_pixel(x, y, right_pixel));
      assert(same_color(left_pixel, right_pixel));
    }
  }
}

}  // namespace

int main() {
  const pc2ps3::Color background = {20, 40, 60, 255};
  const pc2ps3::Color transparent = {220, 140, 100, 0};
  const pc2ps3::Color half_alpha = {220, 140, 100, 128};
  const pc2ps3::Color opaque = {180, 90, 30, 255};

  pc2ps3::Framebuffer unchanged(2, 2);
  unchanged.clear(background);
  unchanged.blend_overlay(transparent);
  for (int y = 0; y < unchanged.height(); ++y) {
    for (int x = 0; x < unchanged.width(); ++x) {
      expect_pixel(unchanged, x, y, background);
    }
  }

  pc2ps3::Framebuffer overlay(2, 2);
  pc2ps3::Framebuffer reference(2, 2);
  overlay.clear(background);
  reference.clear(background);
  overlay.blend_overlay(half_alpha);
  reference.blend_rect(0, 0, reference.width(), reference.height(), half_alpha);
  expect_same_framebuffer(overlay, reference);
  expect_pixel(overlay, 0, 0, pc2ps3::Color{120, 90, 80, 255});
  expect_pixel(overlay, 1, 1, pc2ps3::Color{120, 90, 80, 255});

  overlay.blend_overlay(opaque);
  for (int y = 0; y < overlay.height(); ++y) {
    for (int x = 0; x < overlay.width(); ++x) {
      expect_pixel(overlay, x, y, opaque);
    }
  }

  pc2ps3::Framebuffer empty_width(0, 4);
  pc2ps3::Framebuffer empty_height(4, 0);
  empty_width.blend_overlay(half_alpha);
  empty_height.blend_overlay(half_alpha);
  assert(empty_width.pixel_storage_bytes() == 0);
  assert(empty_height.pixel_storage_bytes() == 0);
}
