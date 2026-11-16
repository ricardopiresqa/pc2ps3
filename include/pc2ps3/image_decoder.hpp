#pragma once

#include "pc2ps3/render.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pc2ps3 {

struct ImageDecodeResult {
  ImageDecodeResult() : ok(false) {}

  Texture texture;
  std::string error;
  bool ok;
};

// Decodes bounded, non-interlaced, 8-bit PNG data into an RGBA Texture.
ImageDecodeResult decode_png(const std::vector<std::uint8_t>& bytes);

}  // namespace pc2ps3
