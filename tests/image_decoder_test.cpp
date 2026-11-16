#include "pc2ps3/image_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

typedef std::vector<std::uint8_t> Bytes;
int failures = 0;

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "             \
                << #expression << '\n';                                       \
      ++failures;                                                              \
    }                                                                          \
  } while (false)

void append_be32(Bytes& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value >> 24));
  bytes.push_back(static_cast<std::uint8_t>(value >> 16));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  bytes.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t read_be32(const Bytes& bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

void write_be32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

std::uint32_t crc32(const std::uint8_t* type, const Bytes& data) {
  std::uint32_t crc = 0xffffffffu;
  for (int i = 0; i < 4; ++i) {
    crc ^= type[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
  }
  for (std::size_t i = 0; i < data.size(); ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
  }
  return crc ^ 0xffffffffu;
}

std::uint32_t adler32(const Bytes& bytes) {
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    a = (a + bytes[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

class BitWriter {
 public:
  BitWriter() : bit_position_(0) {}

  void write_bit(unsigned int bit) {
    if (bit_position_ == 0) bytes_.push_back(0);
    if ((bit & 1u) != 0u) bytes_.back() |= static_cast<std::uint8_t>(1u << bit_position_);
    bit_position_ = (bit_position_ + 1u) & 7u;
  }

  void write_bits_lsb(std::uint32_t value, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) write_bit((value >> i) & 1u);
  }

  void write_huffman(std::uint32_t code, unsigned int length) {
    while (length != 0) {
      --length;
      write_bit((code >> length) & 1u);
    }
  }

  const Bytes& bytes() const { return bytes_; }

 private:
  Bytes bytes_;
  unsigned int bit_position_;
};

Bytes make_zlib_fixed(const Bytes& input) {
  Bytes result = {0x78, 0x01};
  BitWriter writer;
  writer.write_bits_lsb(1, 1);  // final block
  writer.write_bits_lsb(1, 2);  // fixed Huffman block
  for (std::size_t i = 0; i < input.size(); ++i) {
    const unsigned int symbol = input[i];
    if (symbol <= 143) {
      writer.write_huffman(0x30u + symbol, 8);
    } else {
      writer.write_huffman(0x190u + symbol - 144u, 9);
    }
  }
  writer.write_huffman(0, 7);  // end-of-block symbol 256
  result.insert(result.end(), writer.bytes().begin(), writer.bytes().end());
  append_be32(result, adler32(input));
  return result;
}

void append_chunk(Bytes& png, const char* type, const Bytes& data) {
  append_be32(png, static_cast<std::uint32_t>(data.size()));
  const std::size_t type_offset = png.size();
  for (int i = 0; i < 4; ++i) png.push_back(static_cast<std::uint8_t>(type[i]));
  png.insert(png.end(), data.begin(), data.end());
  append_be32(png, crc32(&png[type_offset], data));
}

Bytes make_zlib_stored(const Bytes& input) {
  Bytes result;
  result.push_back(0x78);
  result.push_back(0x01);
  std::size_t position = 0;
  do {
    const std::size_t count = std::min<std::size_t>(65535, input.size() - position);
    const bool final_block = position + count == input.size();
    result.push_back(final_block ? 1 : 0);
    const std::uint16_t length = static_cast<std::uint16_t>(count);
    const std::uint16_t complement = static_cast<std::uint16_t>(~length);
    result.push_back(static_cast<std::uint8_t>(length));
    result.push_back(static_cast<std::uint8_t>(length >> 8));
    result.push_back(static_cast<std::uint8_t>(complement));
    result.push_back(static_cast<std::uint8_t>(complement >> 8));
    result.insert(result.end(), input.begin() + position,
                  input.begin() + position + count);
    position += count;
  } while (position < input.size());
  append_be32(result, adler32(input));
  return result;
}

std::uint8_t paeth(std::uint8_t left, std::uint8_t above,
                   std::uint8_t upper_left) {
  const int a = left;
  const int b = above;
  const int c = upper_left;
  const int p = a + b - c;
  const int pa = p >= a ? p - a : a - p;
  const int pb = p >= b ? p - b : b - p;
  const int pc = p >= c ? p - c : c - p;
  if (pa <= pb && pa <= pc) return left;
  if (pb <= pc) return above;
  return upper_left;
}

Bytes encode_rows(const std::vector<Bytes>& rows, unsigned int bytes_per_pixel,
                  bool use_filters) {
  Bytes result;
  Bytes previous(rows.empty() ? 0 : rows[0].size(), 0);
  for (std::size_t y = 0; y < rows.size(); ++y) {
    const std::uint8_t filter =
        static_cast<std::uint8_t>(use_filters ? y % 5 : 0);
    result.push_back(filter);
    for (std::size_t x = 0; x < rows[y].size(); ++x) {
      const std::uint8_t left = x < bytes_per_pixel ? 0 : rows[y][x - bytes_per_pixel];
      const std::uint8_t above = y == 0 ? 0 : previous[x];
      const std::uint8_t upper_left =
          (y == 0 || x < bytes_per_pixel) ? 0 : previous[x - bytes_per_pixel];
      std::uint8_t predictor = 0;
      if (filter == 1) predictor = left;
      if (filter == 2) predictor = above;
      if (filter == 3) {
        predictor = static_cast<std::uint8_t>(
            (static_cast<unsigned int>(left) + above) / 2u);
      }
      if (filter == 4) predictor = paeth(left, above, upper_left);
      result.push_back(static_cast<std::uint8_t>(rows[y][x] - predictor));
    }
    previous = rows[y];
  }
  return result;
}

Bytes make_png(std::uint32_t width, std::uint32_t height,
               std::uint8_t color_type, const Bytes& filtered_rows,
               const Bytes& palette = Bytes(),
               const Bytes& transparency = Bytes(),
               std::uint8_t bit_depth = 8, std::uint8_t interlace = 0,
               bool split_idat = false, bool unknown_critical = false,
               bool fixed_deflate = false) {
  Bytes png = {137, 80, 78, 71, 13, 10, 26, 10};
  Bytes ihdr;
  append_be32(ihdr, width);
  append_be32(ihdr, height);
  ihdr.push_back(bit_depth);
  ihdr.push_back(color_type);
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(interlace);
  append_chunk(png, "IHDR", ihdr);
  if (unknown_critical) append_chunk(png, "ABCD", Bytes());
  if (!palette.empty()) append_chunk(png, "PLTE", palette);
  if (!transparency.empty()) append_chunk(png, "tRNS", transparency);
  const Bytes compressed = fixed_deflate ? make_zlib_fixed(filtered_rows)
                                          : make_zlib_stored(filtered_rows);
  if (split_idat && compressed.size() > 1) {
    const std::size_t split = compressed.size() / 2;
    append_chunk(png, "IDAT", Bytes(compressed.begin(), compressed.begin() + split));
    append_chunk(png, "IDAT", Bytes(compressed.begin() + split, compressed.end()));
  } else {
    append_chunk(png, "IDAT", compressed);
  }
  append_chunk(png, "IEND", Bytes());
  return png;
}

std::size_t find_chunk(const Bytes& png, const char* type) {
  std::size_t position = 8;
  while (position + 12 <= png.size()) {
    const std::uint32_t length = read_be32(png, position);
    if (position + 12u + length > png.size()) return png.size();
    if (png[position + 4] == static_cast<std::uint8_t>(type[0]) &&
        png[position + 5] == static_cast<std::uint8_t>(type[1]) &&
        png[position + 6] == static_cast<std::uint8_t>(type[2]) &&
        png[position + 7] == static_cast<std::uint8_t>(type[3])) {
      return position;
    }
    position += 12u + length;
  }
  return png.size();
}

void refresh_chunk_crc(Bytes& png, std::size_t chunk_offset) {
  const std::uint32_t length = read_be32(png, chunk_offset);
  const std::size_t data_offset = chunk_offset + 8;
  const Bytes data(png.begin() + data_offset, png.begin() + data_offset + length);
  const std::uint32_t crc = crc32(&png[chunk_offset + 4], data);
  write_be32(png, data_offset + length, crc);
}

Bytes read_file(const std::string& path) {
  std::ifstream file(path.c_str(), std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(file),
               std::istreambuf_iterator<char>());
}

void expect_pixel(const pc2ps3::Texture& texture, int x, int y,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::uint8_t a) {
  const pc2ps3::Color* pixel = texture.pixel_at(x, y);
  CHECK(pixel != 0);
  if (!pixel) return;
  CHECK(pixel->r == r);
  CHECK(pixel->g == g);
  CHECK(pixel->b == b);
  CHECK(pixel->a == a);
}

void expect_rejected(const Bytes& png) {
  const pc2ps3::ImageDecodeResult result = pc2ps3::decode_png(png);
  CHECK(!result.ok);
  CHECK(!result.texture.valid());
  CHECK(!result.error.empty());
}

void test_rgba_filters_and_split_idat() {
  std::vector<Bytes> rows(5, Bytes(8));
  for (std::size_t y = 0; y < rows.size(); ++y) {
    for (std::size_t x = 0; x < rows[y].size(); ++x) {
      rows[y][x] = static_cast<std::uint8_t>(17 + y * 23 + x * 7);
    }
    rows[y][3] = static_cast<std::uint8_t>(80 + y * 20);
    rows[y][7] = static_cast<std::uint8_t>(160 + y * 15);
  }
  const Bytes filtered = encode_rows(rows, 4, true);
  const pc2ps3::ImageDecodeResult result =
      pc2ps3::decode_png(make_png(2, 5, 6, filtered, Bytes(), Bytes(), 8, 0, true));
  CHECK(result.ok);
  CHECK(result.texture.valid());
  CHECK(result.texture.width() == 2);
  CHECK(result.texture.height() == 5);
  for (int y = 0; y < 5; ++y) {
    expect_pixel(result.texture, 0, y, rows[y][0], rows[y][1], rows[y][2], rows[y][3]);
    expect_pixel(result.texture, 1, y, rows[y][4], rows[y][5], rows[y][6], rows[y][7]);
  }
}

void test_grayscale_and_rgb_transparency() {
  const pc2ps3::ImageDecodeResult gray = pc2ps3::decode_png(
      make_png(2, 1, 0, Bytes{0, 7, 200}, Bytes(), Bytes{0, 7}));
  CHECK(gray.ok);
  expect_pixel(gray.texture, 0, 0, 7, 7, 7, 0);
  expect_pixel(gray.texture, 1, 0, 200, 200, 200, 255);

  const pc2ps3::ImageDecodeResult rgb = pc2ps3::decode_png(
      make_png(2, 1, 2, Bytes{0, 1, 2, 3, 4, 5, 6}, Bytes(),
               Bytes{0, 1, 0, 2, 0, 3}));
  CHECK(rgb.ok);
  expect_pixel(rgb.texture, 0, 0, 1, 2, 3, 0);
  expect_pixel(rgb.texture, 1, 0, 4, 5, 6, 255);

  const pc2ps3::ImageDecodeResult fixed = pc2ps3::decode_png(
      make_png(1, 1, 2, Bytes{0, 200, 210, 220}, Bytes(), Bytes(), 8, 0, false,
               false, true));
  CHECK(fixed.ok);
  expect_pixel(fixed.texture, 0, 0, 200, 210, 220, 255);
}

void test_indexed_and_gray_alpha() {
  const pc2ps3::ImageDecodeResult indexed = pc2ps3::decode_png(
      make_png(2, 1, 3, Bytes{0, 1, 0}, Bytes{10, 20, 30, 40, 50, 60},
               Bytes{0, 128}));
  CHECK(indexed.ok);
  expect_pixel(indexed.texture, 0, 0, 40, 50, 60, 128);
  expect_pixel(indexed.texture, 1, 0, 10, 20, 30, 0);

  const pc2ps3::ImageDecodeResult gray_alpha =
      pc2ps3::decode_png(make_png(1, 1, 4, Bytes{0, 77, 39}));
  CHECK(gray_alpha.ok);
  expect_pixel(gray_alpha.texture, 0, 0, 77, 77, 77, 39);
}

void test_project_png_assets() {
  const Bytes icon = read_file("assets/phase0/ICON0.PNG");
  const Bytes pic = read_file("assets/phase0/PIC1.PNG");
  CHECK(!icon.empty());
  CHECK(!pic.empty());
  const pc2ps3::ImageDecodeResult icon_result = pc2ps3::decode_png(icon);
  const pc2ps3::ImageDecodeResult pic_result = pc2ps3::decode_png(pic);
  if (!icon_result.ok) std::cerr << "ICON0.PNG: " << icon_result.error << '\n';
  if (!pic_result.ok) std::cerr << "PIC1.PNG: " << pic_result.error << '\n';
  CHECK(icon_result.ok);
  CHECK(pic_result.ok);
  CHECK(icon_result.texture.width() == 320);
  CHECK(icon_result.texture.height() == 176);
  CHECK(pic_result.texture.width() == 1920);
  CHECK(pic_result.texture.height() == 1080);
}

void test_rejections() {
  const Bytes valid = make_png(1, 1, 2, Bytes{0, 3, 4, 5});
  Bytes truncated = valid;
  truncated.pop_back();
  expect_rejected(truncated);

  Bytes bad_crc = valid;
  const std::size_t idat = find_chunk(bad_crc, "IDAT");
  CHECK(idat < bad_crc.size());
  bad_crc[idat + 8] ^= 1;
  expect_rejected(bad_crc);

  Bytes bad_adler = valid;
  const std::size_t adler_idat = find_chunk(bad_adler, "IDAT");
  const std::uint32_t adler_length = read_be32(bad_adler, adler_idat);
  bad_adler[adler_idat + 8 + adler_length - 1] ^= 1;
  refresh_chunk_crc(bad_adler, adler_idat);
  expect_rejected(bad_adler);

  Bytes bad_deflate = valid;
  const std::size_t deflate_idat = find_chunk(bad_deflate, "IDAT");
  bad_deflate[deflate_idat + 8 + 2] = 0x07;
  refresh_chunk_crc(bad_deflate, deflate_idat);
  expect_rejected(bad_deflate);

  expect_rejected(make_png(1, 1, 2, Bytes{0, 1, 2, 3}, Bytes(), Bytes(), 16));
  expect_rejected(make_png(1, 1, 2, Bytes{0, 1, 2, 3}, Bytes(), Bytes(), 8, 1));
  expect_rejected(make_png(1, 1, 2, Bytes{5, 1, 2, 3}));
  expect_rejected(make_png(1, 1, 2, Bytes{0, 1, 2, 3}, Bytes(), Bytes(),
                            8, 0, false, true));
  expect_rejected(make_png(1, 1, 3, Bytes{0, 1}, Bytes{1, 2, 3}));
  expect_rejected(make_png(4097, 1, 2, Bytes()));
  expect_rejected(make_png(4096, 1025, 2, Bytes()));

  Bytes oversized(32u * 1024u * 1024u + 1u, 0);
  expect_rejected(oversized);
}

}  // namespace

int main() {
  test_rgba_filters_and_split_idat();
  test_grayscale_and_rgb_transparency();
  test_indexed_and_gray_alpha();
  test_project_png_assets();
  test_rejections();
  if (failures != 0) {
    std::cerr << failures << " image decoder assertion(s) failed\n";
    return 1;
  }
  std::cout << "image decoder tests passed\n";
  return 0;
}
