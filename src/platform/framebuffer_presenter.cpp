#include "pc2ps3/framebuffer_presenter.hpp"

#include <limits>

namespace pc2ps3 {

bool present_framebuffer(const Framebuffer& framebuffer,
                         std::uint8_t* destination,
                         std::size_t destination_bytes,
                         std::size_t row_stride_bytes,
                         SurfacePixelOrder pixel_order) {
  const int width = framebuffer.width();
  const int height = framebuffer.height();
  if (destination == 0 || width <= 0 || height <= 0 ||
      (pixel_order != SurfacePixelOrder::kRGBA8888 &&
       pixel_order != SurfacePixelOrder::kBGRA8888)) {
    return false;
  }

  const std::size_t width_size = static_cast<std::size_t>(width);
  const std::size_t height_size = static_cast<std::size_t>(height);
  if (width_size > std::numeric_limits<std::size_t>::max() / 4) return false;
  const std::size_t row_bytes = width_size * 4;
  if (row_stride_bytes < row_bytes) return false;
  if (height_size - 1 >
      (std::numeric_limits<std::size_t>::max() - row_bytes) /
          row_stride_bytes) {
    return false;
  }
  const std::size_t required_bytes =
      (height_size - 1) * row_stride_bytes + row_bytes;
  if (destination_bytes < required_bytes) return false;

  const Color* source = framebuffer.pixel_data();
  if (source == 0) return false;

  for (std::size_t y = 0; y < height_size; ++y) {
    std::uint8_t* output_row = destination + y * row_stride_bytes;
    const Color* input_row = source + y * width_size;
    for (std::size_t x = 0; x < width_size; ++x) {
      std::uint8_t* pixel = output_row + x * 4;
      if (pixel_order == SurfacePixelOrder::kRGBA8888) {
        pixel[0] = input_row[x].r;
        pixel[1] = input_row[x].g;
        pixel[2] = input_row[x].b;
      } else {
        pixel[0] = input_row[x].b;
        pixel[1] = input_row[x].g;
        pixel[2] = input_row[x].r;
      }
      pixel[3] = input_row[x].a;
    }
  }
  return true;
}

}  // namespace pc2ps3
