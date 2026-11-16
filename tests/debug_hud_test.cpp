#include "pc2ps3/render.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

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

}  // namespace

int main() {
  const pc2ps3::Color background = {30, 40, 50, 255};
  const pc2ps3::Color panel = {12, 16, 24, 255};
  const pc2ps3::Color text = {236, 240, 248, 255};

  pc2ps3::Framebuffer framebuffer(320, 40);
  assert(framebuffer.pixel_storage_bytes() >=
         static_cast<std::size_t>(320 * 40 * sizeof(pc2ps3::Color)));
  framebuffer.clear(background);
  framebuffer.draw_debug_hud(2, 2, 59.5, UINT64_C(12345678901234567890));

  expect_pixel(framebuffer, 2, 2, panel);
  expect_pixel(framebuffer, 6, 6, text);   // F in FPS:60
  expect_pixel(framebuffer, 38, 6, text);  // 6 in FPS:60
  expect_pixel(framebuffer, 38, 12, text); // distinguishes 6 from 9
  expect_pixel(framebuffer, 6, 18, text);  // M in MEM:...
  expect_pixel(framebuffer, 8, 18, panel); // center gap of M
  expect_pixel(framebuffer, 10, 18, text);
  expect_pixel(framebuffer, 198, 18, text); // final B of the full uint64 value

  pc2ps3::Framebuffer zero_fps(80, 32);
  zero_fps.clear(background);
  zero_fps.draw_debug_hud(0, 0, -1.0, 0);
  expect_pixel(zero_fps, 38, 4, text);  // zero glyph in FPS:0

  pc2ps3::Framebuffer invalid_fps(80, 32);
  invalid_fps.clear(background);
  invalid_fps.draw_debug_hud(0, 0,
                             std::numeric_limits<double>::infinity(), 0);
  expect_pixel(invalid_fps, 38, 4, text);

  pc2ps3::Framebuffer nan_fps(80, 32);
  nan_fps.clear(background);
  nan_fps.draw_debug_hud(0, 0,
                         std::numeric_limits<double>::quiet_NaN(), 0);
  expect_pixel(nan_fps, 38, 4, text);

  pc2ps3::Framebuffer high_fps(120, 32);
  high_fps.clear(background);
  high_fps.draw_debug_hud(0, 0, 1.0e300, 0);
  expect_pixel(high_fps, 38, 4, text);  // clamped FPS starts with 9
  expect_pixel(high_fps, 62, 4, text);  // fourth digit of 9999

  pc2ps3::Framebuffer clipped(16, 12);
  clipped.clear(background);
  clipped.draw_debug_hud(-6, -3, 60.0, UINT64_C(18446744073709551615));
  expect_pixel(clipped, 0, 0, panel);
  pc2ps3::Color ignored = {0, 0, 0, 0};
  assert(!clipped.read_pixel(16, 0, ignored));

  pc2ps3::Framebuffer empty(0, 10);
  assert(empty.width() == 0 && empty.height() == 0);
  assert(empty.pixel_storage_bytes() == 0);
  assert(!empty.read_pixel(0, 0, ignored));
  empty.draw_debug_hud(0, 0, 60.0, 1024);

  pc2ps3::Framebuffer scaled(160, 60);
  scaled.clear(background);
  scaled.draw_debug_hud(2, 2, 60.0, 0, 4);
  expect_pixel(scaled, 2, 2, panel);
  expect_pixel(scaled, 10, 10, text);
  expect_pixel(scaled, 21, 10, text);

  pc2ps3::Framebuffer extreme(8, 8);
  extreme.draw_debug_hud(std::numeric_limits<int>::max(),
                         std::numeric_limits<int>::max(), 60.0, 0);
  expect_pixel(extreme, 0, 0, pc2ps3::Color{0, 0, 0, 0});
  return 0;
}
