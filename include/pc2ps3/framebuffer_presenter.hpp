#pragma once

#include "pc2ps3/render.hpp"
#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

enum class SurfacePixelOrder {
  kRGBA8888,
  kBGRA8888
};

// Copy to caller-owned 32-bit surface memory. row_stride_bytes may include
// padding; no device API or surface ownership is involved.
bool present_framebuffer(const Framebuffer& framebuffer,
                         std::uint8_t* destination,
                         std::size_t destination_bytes,
                         std::size_t row_stride_bytes,
                         SurfacePixelOrder pixel_order);

}  // namespace pc2ps3
