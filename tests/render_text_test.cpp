#include "pc2ps3/render.hpp"

#include <cassert>
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
  const pc2ps3::Color background = {10, 20, 30, 255};
  const pc2ps3::Color ink = {240, 230, 220, 255};

  pc2ps3::Framebuffer uppercase(16, 12);
  pc2ps3::Framebuffer lowercase(16, 12);
  uppercase.clear(background);
  lowercase.clear(background);
  assert(uppercase.draw_text("A0%", 3, 1, 1, ink));
  assert(lowercase.draw_text("a0%", 3, 1, 1, ink));
  expect_same_framebuffer(uppercase, lowercase);
  expect_pixel(uppercase, 2, 1, ink);
  expect_pixel(uppercase, 1, 1, background);

  pc2ps3::Framebuffer fallback(5, 5);
  pc2ps3::Framebuffer question_mark(5, 5);
  fallback.clear(background);
  question_mark.clear(background);
  const char unsupported[] = {static_cast<char>(0xff)};
  assert(fallback.draw_text(unsupported, sizeof(unsupported), 0, 0, ink));
  assert(question_mark.draw_text("?", 1, 0, 0, ink));
  expect_same_framebuffer(fallback, question_mark);

  pc2ps3::Framebuffer scaled(8, 10);
  scaled.clear(background);
  assert(scaled.draw_text("1", 1, 0, 0, ink, 2));
  expect_pixel(scaled, 2, 0, ink);
  expect_pixel(scaled, 3, 1, ink);
  expect_pixel(scaled, 0, 0, background);
  expect_pixel(scaled, 5, 8, ink);

  pc2ps3::Framebuffer multiline(8, 12);
  multiline.clear(background);
  assert(multiline.draw_text("A\n1", 3, 0, 0, ink));
  expect_pixel(multiline, 1, 0, ink);
  expect_pixel(multiline, 1, 6, ink);
  expect_pixel(multiline, 0, 6, background);

  pc2ps3::Framebuffer clipped(4, 4);
  clipped.clear(background);
  assert(clipped.draw_text("A", 1, -1, -1, ink));
  expect_pixel(clipped, 1, 0, ink);
  expect_pixel(clipped, 0, 1, ink);

  pc2ps3::Framebuffer unchanged(8, 8);
  unchanged.clear(background);
  assert(!unchanged.draw_text(0, 1, 0, 0, ink));
  assert(!unchanged.draw_text("x", pc2ps3::Framebuffer::kMaxTextBytes + 1,
                              0, 0, ink));
  assert(!unchanged.draw_text("x", 1, 0, 0, ink, 0));
  assert(!unchanged.draw_text("x", 1, 0, 0, ink, 9));
  assert(unchanged.draw_text(0, 0, 0, 0, ink));
  std::vector<char> maximum_length(pc2ps3::Framebuffer::kMaxTextBytes, '\n');
  assert(unchanged.draw_text(&maximum_length[0], maximum_length.size(), 0, 0,
                             ink));
  for (int y = 0; y < unchanged.height(); ++y) {
    for (int x = 0; x < unchanged.width(); ++x) {
      expect_pixel(unchanged, x, y, background);
    }
  }
}
