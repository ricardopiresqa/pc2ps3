#include "pc2ps3/atlas_runtime_index.hpp"

#include <new>
#include <set>
#include <stdexcept>

namespace pc2ps3 {
namespace {

const std::size_t kHeaderBytes = 24;
const std::size_t kMaxFilenameBytes = 68;
const std::size_t kMaxSprites = 1024;
const std::size_t kMaxIdBytes = 128;
const std::uint32_t kMaxDimension = 4096;
const std::uint64_t kMaxPixels = 8u * 1024u * 1024u;
const std::uint8_t kMagic[8] = {'P', 'C', '2', 'S', 'A', 'T', 'L', 0};

bool read_u16_le(const std::vector<std::uint8_t>& bytes, std::size_t* cursor,
                 std::uint16_t* value) {
  if (*cursor > bytes.size() || bytes.size() - *cursor < 2) return false;
  *value = static_cast<std::uint16_t>(bytes[*cursor]) |
           static_cast<std::uint16_t>(
               static_cast<std::uint16_t>(bytes[*cursor + 1]) << 8);
  *cursor += 2;
  return true;
}

bool read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t* cursor,
                 std::uint32_t* value) {
  if (*cursor > bytes.size() || bytes.size() - *cursor < 4) return false;
  *value = static_cast<std::uint32_t>(bytes[*cursor]) |
           (static_cast<std::uint32_t>(bytes[*cursor + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[*cursor + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[*cursor + 3]) << 24);
  *cursor += 4;
  return true;
}

bool safe_image_filename(const std::string& filename) {
  if (filename.size() < 5 || filename.size() > kMaxFilenameBytes ||
      filename.compare(filename.size() - 4, 4, ".png") != 0) {
    return false;
  }
  const std::size_t base_length = filename.size() - 4;
  for (std::size_t i = 0; i < base_length; ++i) {
    const char c = filename[i];
    const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    const bool digit = c >= '0' && c <= '9';
    if (!letter && !digit && c != '_' && c != '-') return false;
  }
  return true;
}

bool continuation(std::uint8_t byte) { return byte >= 0x80 && byte <= 0xbf; }

bool valid_utf8_id(const std::string& id) {
  const std::uint8_t* data =
      reinterpret_cast<const std::uint8_t*>(id.data());
  std::size_t i = 0;
  while (i < id.size()) {
    const std::uint8_t first = data[i];
    if (first <= 0x7f) {
      if (first < 0x20 || first == 0x7f) return false;
      ++i;
      continue;
    }
    if (first >= 0xc2 && first <= 0xdf) {
      if (id.size() - i < 2 || !continuation(data[i + 1]) ||
          (first == 0xc2 && data[i + 1] <= 0x9f)) {
        return false;
      }
      i += 2;
      continue;
    }
    if (first >= 0xe0 && first <= 0xef) {
      if (id.size() - i < 3 || !continuation(data[i + 2])) return false;
      const std::uint8_t second = data[i + 1];
      if ((first == 0xe0 && (second < 0xa0 || second > 0xbf)) ||
          (first == 0xed && (second < 0x80 || second > 0x9f)) ||
          (first != 0xe0 && first != 0xed && !continuation(second))) {
        return false;
      }
      i += 3;
      continue;
    }
    if (first >= 0xf0 && first <= 0xf4) {
      if (id.size() - i < 4 || !continuation(data[i + 2]) ||
          !continuation(data[i + 3])) {
        return false;
      }
      const std::uint8_t second = data[i + 1];
      if ((first == 0xf0 && (second < 0x90 || second > 0xbf)) ||
          (first == 0xf4 && (second < 0x80 || second > 0x8f)) ||
          (first != 0xf0 && first != 0xf4 && !continuation(second))) {
        return false;
      }
      i += 4;
      continue;
    }
    return false;
  }
  return !id.empty();
}

bool valid_dimensions(std::uint32_t width, std::uint32_t height) {
  return width != 0 && height != 0 && width <= kMaxDimension &&
         height <= kMaxDimension &&
         static_cast<std::uint64_t>(width) * height <= kMaxPixels;
}

bool valid_region(std::uint32_t atlas_width, std::uint32_t atlas_height,
                  std::uint32_t x, std::uint32_t y, std::uint32_t width,
                  std::uint32_t height) {
  return width != 0 && height != 0 && x <= atlas_width &&
         y <= atlas_height && width <= atlas_width - x &&
         height <= atlas_height - y;
}

} // namespace

bool parse_atlas_runtime_index(const std::vector<std::uint8_t>& bytes,
                               AtlasRuntimeIndex* result) {
  if (result == 0 || bytes.size() < kHeaderBytes ||
      bytes.size() > kMaxAtlasRuntimeIndexBytes) {
    return false;
  }
  for (std::size_t i = 0; i < sizeof(kMagic); ++i) {
    if (bytes[i] != kMagic[i]) return false;
  }

  try {
    std::size_t cursor = sizeof(kMagic);
    std::uint16_t version = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t sprite_count = 0;
    std::uint16_t filename_length = 0;
    if (!read_u16_le(bytes, &cursor, &version) ||
        !read_u32_le(bytes, &cursor, &width) ||
        !read_u32_le(bytes, &cursor, &height) ||
        !read_u32_le(bytes, &cursor, &sprite_count) ||
        !read_u16_le(bytes, &cursor, &filename_length) || version != 1 ||
        !valid_dimensions(width, height) || sprite_count == 0 ||
        sprite_count > kMaxSprites || filename_length < 5 ||
        filename_length > kMaxFilenameBytes ||
        filename_length > bytes.size() - cursor) {
      return false;
    }

    AtlasRuntimeIndex parsed;
    parsed.width = width;
    parsed.height = height;
    parsed.image_filename.assign(
        reinterpret_cast<const char*>(&bytes[cursor]), filename_length);
    cursor += filename_length;
    if (!safe_image_filename(parsed.image_filename)) return false;

    std::set<std::string> identifiers;
    parsed.sprites.reserve(sprite_count);
    for (std::uint32_t i = 0; i < sprite_count; ++i) {
      std::uint16_t id_length = 0;
      std::uint32_t x = 0;
      std::uint32_t y = 0;
      std::uint32_t sprite_width = 0;
      std::uint32_t sprite_height = 0;
      if (!read_u16_le(bytes, &cursor, &id_length) ||
          !read_u32_le(bytes, &cursor, &x) ||
          !read_u32_le(bytes, &cursor, &y) ||
          !read_u32_le(bytes, &cursor, &sprite_width) ||
          !read_u32_le(bytes, &cursor, &sprite_height) || id_length == 0 ||
          id_length > kMaxIdBytes || id_length > bytes.size() - cursor) {
        return false;
      }
      std::string id(reinterpret_cast<const char*>(&bytes[cursor]), id_length);
      cursor += id_length;
      if (!valid_utf8_id(id) || !valid_region(width, height, x, y,
                                               sprite_width, sprite_height) ||
          !identifiers.insert(id).second) {
        return false;
      }

      AtlasSpriteRegion region = {id, x, y, sprite_width, sprite_height};
      parsed.sprites.push_back(region);
    }
    if (cursor != bytes.size()) return false;

    result->width = parsed.width;
    result->height = parsed.height;
    result->image_filename.swap(parsed.image_filename);
    result->sprites.swap(parsed.sprites);
    return true;
  } catch (const std::bad_alloc&) {
    return false;
  } catch (const std::length_error&) {
    return false;
  }
}

} // namespace pc2ps3
