#include "runtime_demo_options.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {

void expect_fps(const char* value, std::uint32_t expected) {
  std::uint32_t parsed = 999;
  assert(pc2ps3::parse_runtime_demo_fps(value, parsed));
  assert(parsed == expected);
}

void expect_invalid_fps(const char* value) {
  std::uint32_t parsed = 999;
  assert(!pc2ps3::parse_runtime_demo_fps(value, parsed));
  assert(parsed == 999);
}

void test_frame_rate_values() {
  expect_fps("0", 0);
  expect_fps("1", 1);
  expect_fps("60", 60);
  expect_fps("240", 240);
  expect_fps("00060", 60);
  expect_invalid_fps(0);
  expect_invalid_fps("");
  expect_invalid_fps("-1");
  expect_invalid_fps("+1");
  expect_invalid_fps("241");
  expect_invalid_fps("999999999999999999999999");
  expect_invalid_fps("60fps");
  expect_invalid_fps(" 60");
}

void expect_sprite_index(const char* value, std::size_t expected) {
  std::size_t parsed = 9999;
  assert(pc2ps3::parse_runtime_demo_sprite_index(value, parsed));
  assert(parsed == expected);
}

void expect_invalid_sprite_index(const char* value) {
  std::size_t parsed = 9999;
  assert(!pc2ps3::parse_runtime_demo_sprite_index(value, parsed));
  assert(parsed == 9999);
}

void test_sprite_index_values() {
  expect_sprite_index("0", 0);
  expect_sprite_index("1", 1);
  expect_sprite_index("1023", 1023);
  expect_sprite_index("0002", 2);
  expect_invalid_sprite_index(0);
  expect_invalid_sprite_index("");
  expect_invalid_sprite_index("-1");
  expect_invalid_sprite_index("+1");
  expect_invalid_sprite_index("1024");
  expect_invalid_sprite_index("999999999999999999999999");
  expect_invalid_sprite_index("1x");
}

void expect_invalid_options(int argc, const char* const argv[]) {
  pc2ps3::RuntimeDemoOptions options;
  options.max_fps = 17;
  options.asset_root = "preserved-root";
  options.atlas_index = "preserved-index";
  options.sprite_index = 9;
  assert(!pc2ps3::parse_runtime_demo_options(argc, argv, options));
  assert(options.max_fps == 17);
  assert(options.asset_root == "preserved-root");
  assert(options.atlas_index == "preserved-index");
  assert(options.sprite_index == 9);
}

void test_default_and_valid_options() {
  const char* const default_args[] = {"pc2ps3_pc_demo"};
  pc2ps3::RuntimeDemoOptions options;
  options.max_fps = 17;
  options.asset_root = "old-root";
  assert(pc2ps3::parse_runtime_demo_options(1, default_args, options));
  assert(options.max_fps == 60);
  assert(options.asset_root.empty());
  assert(options.atlas_index.empty());
  assert(options.sprite_index == 0);

  const char* const valid_args[] = {
      "pc2ps3_pc_demo", "--asset-root", "C:\\assets", "--max-fps", "0",
      "--sprite-index", "2", "--atlas-index", "sprites/main.p2sa"};
  assert(pc2ps3::parse_runtime_demo_options(9, valid_args, options));
  assert(options.max_fps == 0);
  assert(options.asset_root == "C:\\assets");
  assert(options.atlas_index == "sprites/main.p2sa");
  assert(options.sprite_index == 2);

  const char* const default_sprite_args[] = {
      "pc2ps3_pc_demo", "--atlas-index", "sprites/main.p2sa",
      "--asset-root", "assets"};
  assert(pc2ps3::parse_runtime_demo_options(5, default_sprite_args, options));
  assert(options.sprite_index == 0);
}

void test_invalid_options_are_atomic() {
  const char* const missing_max[] = {"pc2ps3_pc_demo", "--max-fps"};
  const char* const duplicate_max[] = {"pc2ps3_pc_demo", "--max-fps", "30",
                                       "--max-fps", "60"};
  const char* const unknown[] = {"pc2ps3_pc_demo", "--unknown"};
  const char* const invalid_max[] = {"pc2ps3_pc_demo", "--max-fps", "241"};
  const char* const root_only[] = {"pc2ps3_pc_demo", "--asset-root", "assets"};
  const char* const index_only[] = {"pc2ps3_pc_demo", "--atlas-index",
                                    "sprites/main.p2sa"};
  const char* const sprite_only[] = {"pc2ps3_pc_demo", "--sprite-index", "1"};
  const char* const missing_root[] = {"pc2ps3_pc_demo", "--asset-root"};
  const char* const missing_index[] = {"pc2ps3_pc_demo", "--atlas-index"};
  const char* const root_is_option[] = {"pc2ps3_pc_demo", "--asset-root",
                                       "--atlas-index", "sprites/main.p2sa"};
  const char* const empty_index[] = {"pc2ps3_pc_demo", "--asset-root", "assets",
                                     "--atlas-index", ""};
  const char* const duplicate_root[] = {"pc2ps3_pc_demo", "--asset-root", "a",
                                        "--asset-root", "b", "--atlas-index",
                                        "sprites/main.p2sa"};
  const char* const duplicate_index[] = {"pc2ps3_pc_demo", "--asset-root", "a",
                                         "--atlas-index", "one.p2sa",
                                         "--atlas-index", "two.p2sa"};
  const char* const duplicate_sprite[] = {
      "pc2ps3_pc_demo", "--asset-root", "a", "--atlas-index", "one.p2sa",
      "--sprite-index", "0", "--sprite-index", "1"};
  const char* const invalid_sprite[] = {
      "pc2ps3_pc_demo", "--asset-root", "a", "--atlas-index", "one.p2sa",
      "--sprite-index", "1024"};
  const char* const null_argument[] = {"pc2ps3_pc_demo", 0};

  expect_invalid_options(2, missing_max);
  expect_invalid_options(5, duplicate_max);
  expect_invalid_options(2, unknown);
  expect_invalid_options(3, invalid_max);
  expect_invalid_options(3, root_only);
  expect_invalid_options(3, index_only);
  expect_invalid_options(3, sprite_only);
  expect_invalid_options(2, missing_root);
  expect_invalid_options(2, missing_index);
  expect_invalid_options(4, root_is_option);
  expect_invalid_options(5, empty_index);
  expect_invalid_options(7, duplicate_root);
  expect_invalid_options(7, duplicate_index);
  expect_invalid_options(9, duplicate_sprite);
  expect_invalid_options(7, invalid_sprite);
  expect_invalid_options(2, null_argument);
  expect_invalid_options(0, unknown);
  expect_invalid_options(1, 0);
}

} // namespace

int main() {
  test_frame_rate_values();
  test_sprite_index_values();
  test_default_and_valid_options();
  test_invalid_options_are_atomic();
  return 0;
}
