#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pc2ps3 {

static const std::size_t kMaxAtlasRuntimeIndexBytes =
    24u + 68u + 1024u * (18u + 128u);

struct AtlasSpriteRegion {
  std::string id;
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t width;
  std::uint32_t height;
};

struct AtlasRuntimeIndex {
  AtlasRuntimeIndex() : width(0), height(0) {}

  std::uint32_t width;
  std::uint32_t height;
  std::string image_filename;
  std::vector<AtlasSpriteRegion> sprites;
};

// Parses the bounded PC2SATL v1 payload. On failure, result is unchanged.
bool parse_atlas_runtime_index(const std::vector<std::uint8_t>& bytes,
                               AtlasRuntimeIndex* result);

} // namespace pc2ps3
