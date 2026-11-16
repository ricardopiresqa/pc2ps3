#include "pc2ps3/atlas_runtime_index.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
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

void append_u16_le(Bytes& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32_le(Bytes& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  bytes.push_back(static_cast<std::uint8_t>(value >> 16));
  bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void set_u16_le(Bytes& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void set_u32_le(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
  bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void append_text(Bytes& bytes, const std::string& text) {
  bytes.insert(bytes.end(), text.begin(), text.end());
}

void append_sprite(Bytes& bytes, const std::string& id, std::uint32_t x,
                   std::uint32_t y, std::uint32_t width,
                   std::uint32_t height) {
  append_u16_le(bytes, static_cast<std::uint16_t>(id.size()));
  append_u32_le(bytes, x);
  append_u32_le(bytes, y);
  append_u32_le(bytes, width);
  append_u32_le(bytes, height);
  append_text(bytes, id);
}

Bytes make_index(const std::string& filename = "sprites.png",
                 const std::string& first_id = "hero",
                 const std::string& second_id = "fx") {
  Bytes bytes;
  const std::uint8_t magic[] = {'P', 'C', '2', 'S', 'A', 'T', 'L', 0};
  bytes.insert(bytes.end(), magic, magic + sizeof(magic));
  append_u16_le(bytes, 1);
  append_u32_le(bytes, 16);
  append_u32_le(bytes, 8);
  append_u32_le(bytes, 2);
  append_u16_le(bytes, static_cast<std::uint16_t>(filename.size()));
  append_text(bytes, filename);
  append_sprite(bytes, first_id, 0, 0, 4, 4);
  append_sprite(bytes, second_id, 4, 0, 2, 2);
  return bytes;
}

std::size_t first_record(const std::string& filename = "sprites.png") {
  return 24 + filename.size();
}

void expect_failure_preserves_result(const Bytes& bytes) {
  pc2ps3::AtlasRuntimeIndex result;
  result.width = 77;
  result.height = 55;
  result.image_filename = "keep.png";
  pc2ps3::AtlasSpriteRegion region = {"keep", 1, 2, 3, 4};
  result.sprites.push_back(region);

  CHECK(!pc2ps3::parse_atlas_runtime_index(bytes, &result));
  CHECK(result.width == 77);
  CHECK(result.height == 55);
  CHECK(result.image_filename == "keep.png");
  CHECK(result.sprites.size() == 1);
  CHECK(result.sprites[0].id == "keep");
}

void test_valid_index() {
  const Bytes bytes = make_index();
  pc2ps3::AtlasRuntimeIndex result;
  CHECK(pc2ps3::parse_atlas_runtime_index(bytes, &result));
  CHECK(result.width == 16);
  CHECK(result.height == 8);
  CHECK(result.image_filename == "sprites.png");
  CHECK(result.sprites.size() == 2);
  if (result.sprites.size() == 2) {
    CHECK(result.sprites[0].id == "hero");
    CHECK(result.sprites[0].x == 0 && result.sprites[0].y == 0);
    CHECK(result.sprites[0].width == 4 && result.sprites[0].height == 4);
    CHECK(result.sprites[1].id == "fx");
    CHECK(result.sprites[1].x == 4 && result.sprites[1].width == 2);
  }
}

void test_utf8_ids() {
  const std::string valid_id("\xc3\xa9", 2);
  pc2ps3::AtlasRuntimeIndex result;
  CHECK(pc2ps3::parse_atlas_runtime_index(
      make_index("sprites.png", valid_id), &result));
  CHECK(result.sprites.size() == 2);
  if (result.sprites.size() == 2) CHECK(result.sprites[0].id == valid_id);

  expect_failure_preserves_result(
      make_index("sprites.png", std::string("\xc0\xaf", 2)));
  expect_failure_preserves_result(
      make_index("sprites.png", std::string("\xed\xa0\x80", 3)));
}

void test_invalid_headers_and_lengths() {
  Bytes bytes = make_index();
  bytes[0] = 'X';
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u16_le(bytes, 8, 2);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  bytes[8] = 0;
  bytes[9] = 1;
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  bytes.pop_back();
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  bytes.push_back(0);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u32_le(bytes, 18, 1025);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u16_le(bytes, 22, 69);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  bytes.resize(pc2ps3::kMaxAtlasRuntimeIndexBytes + 1, 0);
  expect_failure_preserves_result(bytes);
}

void test_invalid_names_ids_and_regions() {
  expect_failure_preserves_result(make_index("../evil.png"));
  expect_failure_preserves_result(make_index("sprites.PNG"));
  expect_failure_preserves_result(make_index("sprites/evil.png"));
  expect_failure_preserves_result(make_index("sprites.png", "same", "same"));
  expect_failure_preserves_result(make_index("sprites.png", std::string()));

  Bytes bytes = make_index();
  const std::size_t record = first_record();
  bytes[record + 2 + 16] = 0xff;
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u16_le(bytes, record, 129);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u32_le(bytes, record + 2, 0xffffffffu);
  expect_failure_preserves_result(bytes);

  bytes = make_index();
  set_u32_le(bytes, record + 2 + 8, 0);
  expect_failure_preserves_result(bytes);
}

} // namespace

int main() {
  test_valid_index();
  test_utf8_ids();
  test_invalid_headers_and_lengths();
  test_invalid_names_ids_and_regions();
  if (failures != 0) {
    std::cerr << failures << " atlas runtime index checks failed\n";
    return 1;
  }
  std::cout << "atlas runtime index checks passed\n";
  return 0;
}
