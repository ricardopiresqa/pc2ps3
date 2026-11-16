#include "pc2ps3/image_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace pc2ps3 {
namespace {

const std::size_t kMaxInputBytes = 32u * 1024u * 1024u;
const std::uint32_t kMaxDimension = 4096u;
const std::uint64_t kMaxPixels = 4194304u;

bool set_error(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}

std::uint32_t read_be32(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) |
         static_cast<std::uint32_t>(bytes[3]);
}

std::uint16_t read_be16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(
      (static_cast<unsigned int>(bytes[0]) << 8) |
      static_cast<unsigned int>(bytes[1]));
}

const std::array<std::uint32_t, 256>& crc_table() {
  static const std::array<std::uint32_t, 256> table = []() {
    std::array<std::uint32_t, 256> values = {};
    for (std::uint32_t i = 0; i < values.size(); ++i) {
      std::uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1u) ? (value >> 1) ^ 0xedb88320u : value >> 1;
      }
      values[i] = value;
    }
    return values;
  }();
  return table;
}

std::uint32_t chunk_crc(const std::uint8_t* type, const std::uint8_t* data,
                        std::size_t length) {
  const std::array<std::uint32_t, 256>& table = crc_table();
  std::uint32_t crc = 0xffffffffu;
  for (int i = 0; i < 4; ++i) {
    crc = table[(crc ^ type[i]) & 0xffu] ^ (crc >> 8);
  }
  for (std::size_t i = 0; i < length; ++i) {
    crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
  }
  return crc ^ 0xffffffffu;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& bytes) {
  const std::uint32_t modulus = 65521u;
  std::uint32_t a = 1u;
  std::uint32_t b = 0u;
  std::size_t block_count = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    a += bytes[i];
    b += a;
    if (++block_count == 5552u) {
      a %= modulus;
      b %= modulus;
      block_count = 0;
    }
  }
  a %= modulus;
  b %= modulus;
  return (b << 16) | a;
}

class BitReader {
 public:
  BitReader(const std::uint8_t* bytes, std::size_t size)
      : bytes_(bytes), size_(size), position_(0), bit_position_(0) {}

  bool read_bit(std::uint32_t& value) {
    if (position_ >= size_) return false;
    value = (bytes_[position_] >> bit_position_) & 1u;
    ++bit_position_;
    if (bit_position_ == 8) {
      bit_position_ = 0;
      ++position_;
    }
    return true;
  }

  bool read_bits(int count, std::uint32_t& value) {
    value = 0;
    for (int i = 0; i < count; ++i) {
      std::uint32_t bit = 0;
      if (!read_bit(bit)) return false;
      value |= bit << i;
    }
    return true;
  }

  void align_to_byte() {
    if (bit_position_ != 0) {
      bit_position_ = 0;
      ++position_;
    }
  }

  bool at_end_after_padding() {
    align_to_byte();
    return position_ == size_;
  }

 private:
  const std::uint8_t* bytes_;
  std::size_t size_;
  std::size_t position_;
  unsigned int bit_position_;
};

class HuffmanTable {
 public:
  HuffmanTable() : max_length_(0) {}

  bool build(const std::vector<std::uint8_t>& lengths, bool allow_empty,
             bool allow_single_incomplete = false) {
    std::array<unsigned int, 16> counts = {};
    max_length_ = 0;
    for (std::size_t i = 0; i < lengths.size(); ++i) {
      if (lengths[i] > 15) return false;
      if (lengths[i] != 0) {
        ++counts[lengths[i]];
        max_length_ = std::max(max_length_,
                               static_cast<unsigned int>(lengths[i]));
      }
    }
    if (max_length_ == 0) {
      for (std::size_t i = 0; i < symbols_.size(); ++i) symbols_[i].clear();
      return allow_empty;
    }

    int remaining = 1;
    for (unsigned int length = 1; length <= 15; ++length) {
      remaining = (remaining << 1) - static_cast<int>(counts[length]);
      if (remaining < 0) return false;
    }
    if (remaining > 0 &&
        !(allow_single_incomplete && max_length_ == 1 && counts[1] == 1)) {
      return false;
    }

    std::array<unsigned int, 16> next_code = {};
    unsigned int code = 0;
    for (unsigned int length = 1; length <= 15; ++length) {
      code = (code + counts[length - 1]) << 1;
      next_code[length] = code;
      if (counts[length] == 0) {
        symbols_[length].clear();
      } else {
        symbols_[length].assign(static_cast<std::size_t>(1) << length, -1);
      }
    }

    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
      const unsigned int length = lengths[symbol];
      if (length == 0) continue;
      const unsigned int assigned = next_code[length]++;
      if (assigned >= symbols_[length].size()) return false;
      if (symbols_[length][assigned] != -1) return false;
      symbols_[length][assigned] = static_cast<std::int16_t>(symbol);
    }
    return true;
  }

  bool decode(BitReader& reader, unsigned int& symbol) const {
    unsigned int code = 0;
    for (unsigned int length = 1; length <= max_length_; ++length) {
      std::uint32_t bit = 0;
      if (!reader.read_bit(bit)) return false;
      code = (code << 1) | static_cast<unsigned int>(bit);
      if (symbols_[length].empty() || code >= symbols_[length].size()) continue;
      const std::int16_t value = symbols_[length][code];
      if (value >= 0) {
        symbol = static_cast<unsigned int>(value);
        return true;
      }
    }
    return false;
  }

 private:
  unsigned int max_length_;
  std::array<std::vector<std::int16_t>, 16> symbols_;
};

const HuffmanTable& fixed_literal_table() {
  static const HuffmanTable table = []() {
    std::vector<std::uint8_t> lengths(288, 0);
    for (std::size_t i = 0; i <= 143; ++i) lengths[i] = 8;
    for (std::size_t i = 144; i <= 255; ++i) lengths[i] = 9;
    for (std::size_t i = 256; i <= 279; ++i) lengths[i] = 7;
    for (std::size_t i = 280; i <= 287; ++i) lengths[i] = 8;
    HuffmanTable result;
    result.build(lengths, false);
    return result;
  }();
  return table;
}

const HuffmanTable& fixed_distance_table() {
  static const HuffmanTable table = []() {
    HuffmanTable result;
    result.build(std::vector<std::uint8_t>(32, 5), false);
    return result;
  }();
  return table;
}

bool read_dynamic_tables(BitReader& reader, HuffmanTable& literal_table,
                         HuffmanTable& distance_table) {
  std::uint32_t value = 0;
  if (!reader.read_bits(5, value)) return false;
  const unsigned int literal_count = value + 257u;
  if (literal_count > 286u) return false;
  if (!reader.read_bits(5, value)) return false;
  const unsigned int distance_count = value + 1u;
  if (!reader.read_bits(4, value)) return false;
  const unsigned int code_length_count = value + 4u;

  const unsigned int order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                  11, 4, 12, 3, 13, 2, 14, 1, 15};
  std::vector<std::uint8_t> code_lengths(19, 0);
  for (unsigned int i = 0; i < code_length_count; ++i) {
    if (!reader.read_bits(3, value)) return false;
    code_lengths[order[i]] = static_cast<std::uint8_t>(value);
  }

  HuffmanTable code_length_table;
  if (!code_length_table.build(code_lengths, false)) return false;
  const std::size_t total = literal_count + distance_count;
  std::vector<std::uint8_t> lengths;
  lengths.reserve(total);
  while (lengths.size() < total) {
    unsigned int symbol = 0;
    if (!code_length_table.decode(reader, symbol)) return false;
    if (symbol <= 15u) {
      lengths.push_back(static_cast<std::uint8_t>(symbol));
      continue;
    }

    unsigned int repeat_count = 0;
    std::uint8_t repeated_value = 0;
    if (symbol == 16u) {
      if (lengths.empty() || !reader.read_bits(2, value)) return false;
      repeat_count = value + 3u;
      repeated_value = lengths.back();
    } else if (symbol == 17u) {
      if (!reader.read_bits(3, value)) return false;
      repeat_count = value + 3u;
    } else if (symbol == 18u) {
      if (!reader.read_bits(7, value)) return false;
      repeat_count = value + 11u;
    } else {
      return false;
    }
    if (repeat_count > total - lengths.size()) return false;
    lengths.insert(lengths.end(), repeat_count, repeated_value);
  }

  std::vector<std::uint8_t> literal_lengths(
      lengths.begin(), lengths.begin() + literal_count);
  std::vector<std::uint8_t> distance_lengths(
      lengths.begin() + literal_count, lengths.end());
  if (literal_lengths.size() <= 256 || literal_lengths[256] == 0) return false;
  return literal_table.build(literal_lengths, false, true) &&
         distance_table.build(distance_lengths, true, true);
}

bool inflate_deflate(BitReader& reader, std::size_t expected_size,
                     std::vector<std::uint8_t>& output) {
  static const unsigned int length_base[29] = {
      3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const unsigned int length_extra[29] = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const unsigned int distance_base[30] = {
      1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
      193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
      6145, 8193, 12289, 16385, 24577};
  static const unsigned int distance_extra[30] = {
      0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

  bool final_block = false;
  while (!final_block) {
    std::uint32_t final_value = 0;
    std::uint32_t block_type = 0;
    if (!reader.read_bits(1, final_value) || !reader.read_bits(2, block_type)) {
      return false;
    }
    final_block = final_value != 0;

    if (block_type == 0) {
      reader.align_to_byte();
      std::uint32_t length = 0;
      std::uint32_t complement = 0;
      if (!reader.read_bits(16, length) || !reader.read_bits(16, complement) ||
          (length ^ 0xffffu) != complement ||
          length > expected_size - output.size()) {
        return false;
      }
      for (std::uint32_t i = 0; i < length; ++i) {
        std::uint32_t byte = 0;
        if (!reader.read_bits(8, byte)) return false;
        output.push_back(static_cast<std::uint8_t>(byte));
      }
      continue;
    }
    if (block_type == 3) return false;

    HuffmanTable dynamic_literal;
    HuffmanTable dynamic_distance;
    const HuffmanTable* literal_table = 0;
    const HuffmanTable* distance_table = 0;
    if (block_type == 1) {
      literal_table = &fixed_literal_table();
      distance_table = &fixed_distance_table();
    } else {
      if (!read_dynamic_tables(reader, dynamic_literal, dynamic_distance)) {
        return false;
      }
      literal_table = &dynamic_literal;
      distance_table = &dynamic_distance;
    }

    for (;;) {
      unsigned int symbol = 0;
      if (!literal_table->decode(reader, symbol)) return false;
      if (symbol < 256u) {
        if (output.size() >= expected_size) return false;
        output.push_back(static_cast<std::uint8_t>(symbol));
        continue;
      }
      if (symbol == 256u) break;
      if (symbol < 257u || symbol > 285u) return false;

      const unsigned int length_index = symbol - 257u;
      unsigned int match_length = length_base[length_index];
      if (length_extra[length_index] != 0) {
        std::uint32_t extra = 0;
        if (!reader.read_bits(static_cast<int>(length_extra[length_index]), extra)) {
          return false;
        }
        match_length += extra;
      }

      unsigned int distance_symbol = 0;
      if (!distance_table->decode(reader, distance_symbol) ||
          distance_symbol > 29u) {
        return false;
      }
      std::uint32_t extra_distance = 0;
      if (distance_extra[distance_symbol] != 0 &&
          !reader.read_bits(static_cast<int>(distance_extra[distance_symbol]),
                            extra_distance)) {
        return false;
      }
      const std::size_t distance =
          static_cast<std::size_t>(distance_base[distance_symbol] + extra_distance);
      if (distance == 0 || distance > output.size() ||
          match_length > expected_size - output.size()) {
        return false;
      }
      for (unsigned int i = 0; i < match_length; ++i) {
        output.push_back(output[output.size() - distance]);
      }
    }
  }
  return reader.at_end_after_padding() && output.size() == expected_size;
}

bool inflate_zlib(const std::vector<std::uint8_t>& input,
                  std::size_t expected_size,
                  std::vector<std::uint8_t>& output) {
  if (input.size() < 6) return false;
  const unsigned int cmf = input[0];
  const unsigned int flags = input[1];
  if ((cmf & 0x0fu) != 8u || (cmf >> 4) > 7u ||
      ((cmf << 8) | flags) % 31u != 0u || (flags & 0x20u) != 0u) {
    return false;
  }

  BitReader reader(&input[2], input.size() - 6);
  output.clear();
  output.reserve(expected_size);
  if (!inflate_deflate(reader, expected_size, output)) return false;
  const std::uint32_t expected_adler = read_be32(&input[input.size() - 4]);
  return adler32(output) == expected_adler;
}

bool is_letter(std::uint8_t value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

unsigned int channels_for_color_type(std::uint8_t color_type) {
  switch (color_type) {
    case 0: return 1;
    case 2: return 3;
    case 3: return 1;
    case 4: return 2;
    case 6: return 4;
    default: return 0;
  }
}

std::uint8_t paeth_predictor(std::uint8_t left, std::uint8_t above,
                            std::uint8_t upper_left) {
  const int a = left;
  const int b = above;
  const int c = upper_left;
  const int p = a + b - c;
  const int pa = std::abs(p - a);
  const int pb = std::abs(p - b);
  const int pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return left;
  if (pb <= pc) return above;
  return upper_left;
}

bool unfilter(std::vector<std::uint8_t>& scanlines, std::size_t row_bytes,
              std::size_t height, std::size_t bytes_per_pixel) {
  const std::size_t stride = row_bytes + 1;
  for (std::size_t y = 0; y < height; ++y) {
    const std::size_t row_offset = y * stride;
    const std::uint8_t filter = scanlines[row_offset];
    if (filter > 4) return false;
    const std::size_t previous_offset = y == 0 ? 0 : row_offset - stride;
    for (std::size_t x = 0; x < row_bytes; ++x) {
      const std::size_t index = row_offset + 1 + x;
      const std::uint8_t left = x < bytes_per_pixel
                                    ? 0
                                    : scanlines[index - bytes_per_pixel];
      const std::uint8_t above = y == 0 ? 0 : scanlines[previous_offset + 1 + x];
      const std::uint8_t upper_left =
          (y == 0 || x < bytes_per_pixel)
              ? 0
              : scanlines[previous_offset + 1 + x - bytes_per_pixel];
      std::uint8_t predictor = 0;
      switch (filter) {
        case 0: break;
        case 1: predictor = left; break;
        case 2: predictor = above; break;
        case 3:
          predictor = static_cast<std::uint8_t>(
              (static_cast<unsigned int>(left) + above) / 2u);
          break;
        case 4: predictor = paeth_predictor(left, above, upper_left); break;
      }
      scanlines[index] = static_cast<std::uint8_t>(scanlines[index] + predictor);
    }
  }
  return true;
}

ImageDecodeResult failure(const char* error) {
  ImageDecodeResult result;
  result.error = error;
  return result;
}

}  // namespace

ImageDecodeResult decode_png(const std::vector<std::uint8_t>& bytes) {
  static const std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (bytes.size() > kMaxInputBytes) return failure("PNG input exceeds 32 MiB");
  if (bytes.size() < sizeof(signature) ||
      !std::equal(signature, signature + sizeof(signature), bytes.begin())) {
    return failure("invalid PNG signature");
  }

  bool saw_ihdr = false;
  bool saw_plte = false;
  bool saw_trns = false;
  bool saw_idat = false;
  bool idat_closed = false;
  bool saw_iend = false;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t color_type = 0;
  std::vector<std::uint8_t> palette;
  std::vector<std::uint8_t> transparency;
  std::vector<std::uint8_t> compressed;
  std::size_t position = sizeof(signature);

  while (position < bytes.size()) {
    if (bytes.size() - position < 12) return failure("truncated PNG chunk");
    const std::uint32_t chunk_length_u32 = read_be32(&bytes[position]);
    const std::size_t chunk_length = static_cast<std::size_t>(chunk_length_u32);
    const std::uint8_t* type = &bytes[position + 4];
    if (chunk_length > bytes.size() - position - 12) {
      return failure("truncated PNG chunk data");
    }
    const std::uint8_t* data = &bytes[position + 8];
    const std::size_t crc_position = position + 8 + chunk_length;
    if (read_be32(&bytes[crc_position]) != chunk_crc(type, data, chunk_length)) {
      return failure("PNG chunk CRC mismatch");
    }
    for (int i = 0; i < 4; ++i) {
      if (!is_letter(type[i])) return failure("invalid PNG chunk type");
    }
    if ((type[2] & 0x20u) != 0u) return failure("invalid PNG reserved chunk bit");

    const std::string chunk_type(reinterpret_cast<const char*>(type), 4);
    if (!saw_ihdr && chunk_type != "IHDR") {
      return failure("IHDR must be the first PNG chunk");
    }
    if (saw_idat && chunk_type != "IDAT") idat_closed = true;
    if (chunk_type == "IDAT" && idat_closed) {
      return failure("PNG IDAT chunks must be consecutive");
    }

    if (chunk_type == "IHDR") {
      if (saw_ihdr || position != sizeof(signature) || chunk_length != 13) {
        return failure("invalid PNG IHDR");
      }
      width = read_be32(data);
      height = read_be32(data + 4);
      const std::uint8_t bit_depth = data[8];
      color_type = data[9];
      if (width == 0 || height == 0 || width > kMaxDimension ||
          height > kMaxDimension ||
          static_cast<std::uint64_t>(width) * height > kMaxPixels) {
        return failure("PNG dimensions exceed supported limits");
      }
      if (bit_depth != 8 || channels_for_color_type(color_type) == 0) {
        return failure("unsupported PNG bit depth or color type");
      }
      if (data[10] != 0 || data[11] != 0 || data[12] != 0) {
        return failure("unsupported PNG compression, filter, or interlace method");
      }
      saw_ihdr = true;
    } else if (chunk_type == "PLTE") {
      if (saw_plte || saw_idat || saw_trns || chunk_length == 0 ||
          chunk_length > 768 || chunk_length % 3 != 0 || color_type == 0 ||
          color_type == 4) {
        return failure("invalid PNG palette");
      }
      if (color_type == 3 && chunk_length / 3 > 256) {
        return failure("PNG palette has too many entries");
      }
      palette.assign(data, data + chunk_length);
      saw_plte = true;
    } else if (chunk_type == "tRNS") {
      if (saw_trns || saw_idat || color_type == 4 || color_type == 6) {
        return failure("invalid PNG transparency chunk");
      }
      if (color_type == 3) {
        if (!saw_plte || chunk_length == 0 || chunk_length > palette.size() / 3) {
          return failure("invalid indexed PNG transparency");
        }
      } else if (color_type == 0) {
        if (chunk_length != 2 || read_be16(data) > 255u) {
          return failure("invalid grayscale PNG transparency");
        }
      } else if (color_type == 2) {
        if (chunk_length != 6 || read_be16(data) > 255u ||
            read_be16(data + 2) > 255u || read_be16(data + 4) > 255u) {
          return failure("invalid RGB PNG transparency");
        }
      }
      transparency.assign(data, data + chunk_length);
      saw_trns = true;
    } else if (chunk_type == "IDAT") {
      if (!saw_ihdr || (color_type == 3 && !saw_plte) ||
          chunk_length > kMaxInputBytes - compressed.size()) {
        return failure("invalid PNG image data");
      }
      saw_idat = true;
      compressed.insert(compressed.end(), data, data + chunk_length);
    } else if (chunk_type == "IEND") {
      if (!saw_idat || chunk_length != 0) return failure("invalid PNG IEND");
      saw_iend = true;
      position = crc_position + 4;
      if (position != bytes.size()) return failure("data follows PNG IEND");
      break;
    } else if ((type[0] & 0x20u) == 0u) {
      return failure("unknown critical PNG chunk");
    }
    position = crc_position + 4;
  }

  if (!saw_ihdr || !saw_idat || !saw_iend ||
      (color_type == 3 && !saw_plte)) {
    return failure("PNG is missing a required chunk");
  }

  const unsigned int channels = channels_for_color_type(color_type);
  const std::size_t row_bytes = static_cast<std::size_t>(width) * channels;
  const std::size_t scanline_size = row_bytes + 1;
  const std::size_t expected_scanline_bytes =
      scanline_size * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> scanlines;
  if (!inflate_zlib(compressed, expected_scanline_bytes, scanlines)) {
    return failure("invalid PNG zlib/DEFLATE stream or Adler-32 checksum");
  }
  if (!unfilter(scanlines, row_bytes, height, channels)) {
    return failure("invalid PNG row filter");
  }

  const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
  std::vector<Color> pixels(pixel_count);
  const std::uint16_t transparent_gray =
      color_type == 0 && saw_trns ? read_be16(&transparency[0]) : 256u;
  const std::uint16_t transparent_red =
      color_type == 2 && saw_trns ? read_be16(&transparency[0]) : 256u;
  const std::uint16_t transparent_green =
      color_type == 2 && saw_trns ? read_be16(&transparency[2]) : 256u;
  const std::uint16_t transparent_blue =
      color_type == 2 && saw_trns ? read_be16(&transparency[4]) : 256u;

  for (std::uint32_t y = 0; y < height; ++y) {
    const std::uint8_t* row =
        &scanlines[static_cast<std::size_t>(y) * scanline_size + 1];
    for (std::uint32_t x = 0; x < width; ++x) {
      Color color = {0, 0, 0, 255};
      const std::size_t pixel_index =
          static_cast<std::size_t>(y) * width + x;
      const std::size_t source_index = static_cast<std::size_t>(x) * channels;
      switch (color_type) {
        case 0: {
          const std::uint8_t gray = row[source_index];
          color.r = gray;
          color.g = gray;
          color.b = gray;
          color.a = gray == transparent_gray ? 0 : 255;
          break;
        }
        case 2:
          color.r = row[source_index];
          color.g = row[source_index + 1];
          color.b = row[source_index + 2];
          color.a = color.r == transparent_red && color.g == transparent_green &&
                            color.b == transparent_blue
                        ? 0
                        : 255;
          break;
        case 3: {
          const std::size_t palette_index = row[source_index];
          if (palette_index >= palette.size() / 3) {
            return failure("PNG palette index is out of range");
          }
          color.r = palette[palette_index * 3];
          color.g = palette[palette_index * 3 + 1];
          color.b = palette[palette_index * 3 + 2];
          color.a = palette_index < transparency.size()
                        ? transparency[palette_index]
                        : 255;
          break;
        }
        case 4:
          color.r = row[source_index];
          color.g = color.r;
          color.b = color.r;
          color.a = row[source_index + 1];
          break;
        case 6:
          color.r = row[source_index];
          color.g = row[source_index + 1];
          color.b = row[source_index + 2];
          color.a = row[source_index + 3];
          break;
      }
      pixels[pixel_index] = color;
    }
  }

  ImageDecodeResult result;
  result.texture = Texture(static_cast<int>(width), static_cast<int>(height),
                           std::move(pixels));
  if (!result.texture.valid()) return failure("decoded PNG texture is invalid");
  result.ok = true;
  return result;
}

}  // namespace pc2ps3
