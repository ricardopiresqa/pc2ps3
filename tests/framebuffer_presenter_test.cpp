#include "pc2ps3/framebuffer_presenter.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

pc2ps3::Color color(std::uint8_t r, std::uint8_t g,
                    std::uint8_t b, std::uint8_t a) {
  pc2ps3::Color value;
  value.r = r;
  value.g = g;
  value.b = b;
  value.a = a;
  return value;
}

}  // namespace

int main() {
  pc2ps3::Framebuffer framebuffer(2, 2);
  framebuffer.fill_rect(0, 0, 1, 1, color(1, 2, 3, 4));
  framebuffer.fill_rect(1, 0, 1, 1, color(5, 6, 7, 8));
  framebuffer.fill_rect(0, 1, 1, 1, color(9, 10, 11, 12));
  framebuffer.fill_rect(1, 1, 1, 1, color(13, 14, 15, 16));

  std::vector<std::uint8_t> rgba(20, 0xCC);
  assert(pc2ps3::present_framebuffer(framebuffer, &rgba[0], rgba.size(), 12,
                                    pc2ps3::SurfacePixelOrder::kRGBA8888));
  assert(rgba[0] == 1 && rgba[1] == 2 && rgba[2] == 3 && rgba[3] == 4);
  assert(rgba[4] == 5 && rgba[5] == 6 && rgba[6] == 7 && rgba[7] == 8);
  assert(rgba[8] == 0xCC && rgba[11] == 0xCC);
  assert(rgba[12] == 9 && rgba[13] == 10 && rgba[14] == 11 && rgba[15] == 12);
  assert(rgba[16] == 13 && rgba[17] == 14 && rgba[18] == 15 && rgba[19] == 16);

  std::vector<std::uint8_t> bgra(16, 0);
  assert(pc2ps3::present_framebuffer(framebuffer, &bgra[0], bgra.size(), 8,
                                    pc2ps3::SurfacePixelOrder::kBGRA8888));
  assert(bgra[0] == 3 && bgra[1] == 2 && bgra[2] == 1 && bgra[3] == 4);

  std::vector<std::uint8_t> unchanged(20, 0xA5);
  assert(!pc2ps3::present_framebuffer(framebuffer, &unchanged[0],
                                      unchanged.size(), 7,
                                      pc2ps3::SurfacePixelOrder::kRGBA8888));
  assert(!pc2ps3::present_framebuffer(framebuffer, &unchanged[0], 19, 12,
                                      pc2ps3::SurfacePixelOrder::kRGBA8888));
  assert(!pc2ps3::present_framebuffer(
      framebuffer, &unchanged[0], unchanged.size(),
      std::numeric_limits<std::size_t>::max(),
      pc2ps3::SurfacePixelOrder::kRGBA8888));
  for (std::size_t i = 0; i < unchanged.size(); ++i) assert(unchanged[i] == 0xA5);
  assert(!pc2ps3::present_framebuffer(framebuffer, 0, 0, 8,
                                      pc2ps3::SurfacePixelOrder::kRGBA8888));

  pc2ps3::Framebuffer invalid(0, 2);
  assert(!pc2ps3::present_framebuffer(invalid, &unchanged[0],
                                      unchanged.size(), 8,
                                      pc2ps3::SurfacePixelOrder::kRGBA8888));
  return 0;
}
