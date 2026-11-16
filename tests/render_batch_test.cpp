#include "pc2ps3/render_batch.hpp"

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

}  // namespace

int main() {
  const pc2ps3::Color background = {10, 20, 30, 255};
  const pc2ps3::Color red = {220, 0, 0, 255};
  const pc2ps3::Color green = {0, 220, 0, 255};
  const pc2ps3::Color blue = {0, 0, 220, 255};
  const pc2ps3::Color half_blue = {0, 0, 200, 128};
  const pc2ps3::Texture red_texture(1, 1, std::vector<pc2ps3::Color>{red});
  const pc2ps3::Texture green_texture(1, 1,
                                      std::vector<pc2ps3::Color>{green});
  const pc2ps3::Texture blue_texture(1, 1,
                                     std::vector<pc2ps3::Color>{blue});
  const pc2ps3::Texture half_blue_texture(
      1, 1, std::vector<pc2ps3::Color>{half_blue});
  const pc2ps3::Texture line(2, 1,
                             std::vector<pc2ps3::Color>{red, green});

  pc2ps3::RenderBatch batch(5);
  assert(batch.valid() && batch.capacity() == 5 && batch.empty());
  pc2ps3::Framebuffer framebuffer(4, 3);
  framebuffer.clear(background);
  assert(batch.flush(framebuffer));
  assert(batch.empty());

  assert(batch.submit_texture_region(red_texture, 0, 0, 1, 1, 0, 0));
  assert(batch.submit_texture_region(green_texture, 0, 0, 1, 1, 0, 0));
  assert(batch.submit_texture_region(half_blue_texture, 0, 0, 1, 1, 0, 0));
  assert(batch.submit_texture_region_scaled(blue_texture, 0, 0, 1, 1, 1, 0,
                                            2, 1));
  assert(batch.submit_texture_region_rotated(line, 0, 0, 2, 1, 3, 1, 90));
  assert(batch.size() == 5 && !batch.empty());
  assert(!batch.submit_texture_region(red_texture, 0, 0, 1, 1, 0, 0));
  assert(batch.size() == 5);
  assert(batch.flush(framebuffer));
  assert(batch.empty());
  expect_pixel(framebuffer, 0, 0, pc2ps3::Color{0, 110, 100, 255});
  expect_pixel(framebuffer, 1, 0, blue);
  expect_pixel(framebuffer, 2, 0, blue);
  expect_pixel(framebuffer, 3, 0, red);
  expect_pixel(framebuffer, 3, 1, green);

  pc2ps3::RenderBatch retry(1);
  assert(retry.valid());
  assert(retry.submit_texture_region(red_texture, 0, 0, 1, 1, 0, 0));
  pc2ps3::Framebuffer unavailable(0, 0);
  assert(!retry.flush(unavailable));
  assert(retry.size() == 1);
  pc2ps3::Framebuffer retry_target(1, 1);
  retry_target.clear(background);
  assert(retry.flush(retry_target));
  assert(retry.empty());
  expect_pixel(retry_target, 0, 0, red);

  pc2ps3::RenderBatch invalid_submission(2);
  assert(invalid_submission.submit_texture_region(red_texture, 0, 0, 1, 1,
                                                  0, 0));
  assert(!invalid_submission.submit_texture_region(red_texture, 0, 0, 2, 1,
                                                   0, 0));
  assert(invalid_submission.size() == 1);

  pc2ps3::RenderBatch zero_capacity(0);
  pc2ps3::RenderBatch excessive_capacity(
      static_cast<std::size_t>(pc2ps3::RenderBatch::kMaxCommands) + 1u);
  assert(!zero_capacity.valid() && !excessive_capacity.valid());
  pc2ps3::RenderBatch cleared(2);
  assert(cleared.submit_texture_region(red_texture, 0, 0, 1, 1, 0, 0));
  cleared.clear();
  assert(cleared.empty());
  return 0;
}
