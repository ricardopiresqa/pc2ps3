#include "runtime_demo_atlas.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

typedef std::vector<std::uint8_t> Bytes;

const Bytes kOnePixelPng = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
    0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99,
    0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

void append_u16(Bytes& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_u32(Bytes& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

void append_text(Bytes& bytes, const std::string& value) {
  bytes.insert(bytes.end(), value.begin(), value.end());
}

Bytes make_index() {
  const std::string filename = "atlas.png";
  const std::string sprite_id = "hero";
  Bytes bytes = {'P', 'C', '2', 'S', 'A', 'T', 'L', 0};
  append_u16(bytes, 1);
  append_u32(bytes, 1);
  append_u32(bytes, 1);
  append_u32(bytes, 1);
  append_u16(bytes, static_cast<std::uint16_t>(filename.size()));
  append_text(bytes, filename);
  append_u16(bytes, static_cast<std::uint16_t>(sprite_id.size()));
  append_u32(bytes, 0);
  append_u32(bytes, 0);
  append_u32(bytes, 1);
  append_u32(bytes, 1);
  append_text(bytes, sprite_id);
  return bytes;
}

class MemoryFileSystem : public pc2ps3::FileSystem {
 public:
  pc2ps3::FileSystemStatus read_all(const std::string& path,
                                    std::size_t max_bytes,
                                    Bytes& output) override {
    ++read_count;
    const std::map<std::string, Bytes>::const_iterator found = files.find(path);
    if (found == files.end()) return pc2ps3::FileSystemStatus::kNotFound;
    if (found->second.size() > max_bytes) {
      return pc2ps3::FileSystemStatus::kTooLarge;
    }
    output = found->second;
    return pc2ps3::FileSystemStatus::kOk;
  }

  pc2ps3::FileSystemStatus write_atomic(const std::string&,
                                        const std::uint8_t*,
                                        std::size_t) override {
    return pc2ps3::FileSystemStatus::kError;
  }

  pc2ps3::FileSystemStatus remove(const std::string&) override {
    return pc2ps3::FileSystemStatus::kError;
  }

  std::map<std::string, Bytes> files;
  std::size_t read_count = 0;
};

void populate_valid_files(MemoryFileSystem& file_system) {
  file_system.files["ui/sprites.p2sa"] = make_index();
  file_system.files["ui/atlas.png"] = kOnePixelPng;
}

void test_success_and_explicit_unload() {
  MemoryFileSystem file_system;
  populate_valid_files(file_system);
  pc2ps3::RuntimeDemoAtlas atlas(file_system);

  assert(atlas.load("ui/sprites.p2sa", 0));
  assert(atlas.valid());
  assert(file_system.read_count == 2);
  assert(atlas.texture() != 0 && atlas.texture()->valid());
  assert(atlas.texture()->width() == 1 && atlas.texture()->height() == 1);
  assert(atlas.sprite_region() != 0);
  assert(atlas.sprite_region()->id == "hero");
  assert(atlas.sprite_region()->x == 0 && atlas.sprite_region()->y == 0);
  assert(atlas.sprite_region()->width == 1 && atlas.sprite_region()->height == 1);

  assert(atlas.unload());
  assert(!atlas.valid());
  assert(atlas.texture() == 0);
  assert(atlas.sprite_region() == 0);
  assert(atlas.load("ui/sprites.p2sa", 0));
}

void test_missing_index_is_inert() {
  MemoryFileSystem file_system;
  pc2ps3::RuntimeDemoAtlas atlas(file_system);

  assert(!atlas.load("ui/missing.p2sa", 0));
  assert(!atlas.valid());
  assert(atlas.texture() == 0);
  assert(atlas.sprite_region() == 0);
  assert(file_system.read_count == 1);
}

void test_corrupt_index_is_inert() {
  MemoryFileSystem file_system;
  file_system.files["ui/sprites.p2sa"] = Bytes{0, 1, 2, 3};
  pc2ps3::RuntimeDemoAtlas atlas(file_system);

  assert(!atlas.load("ui/sprites.p2sa", 0));
  assert(!atlas.valid());
  assert(atlas.texture() == 0);
  assert(atlas.sprite_region() == 0);
  assert(file_system.read_count == 1);
}

void test_missing_image_is_inert() {
  MemoryFileSystem file_system;
  file_system.files["ui/sprites.p2sa"] = make_index();
  pc2ps3::RuntimeDemoAtlas atlas(file_system);

  assert(!atlas.load("ui/sprites.p2sa", 0));
  assert(!atlas.valid());
  assert(atlas.texture() == 0);
  assert(atlas.sprite_region() == 0);
  assert(file_system.read_count == 2);
}

void test_invalid_selection_releases_temporary_handle() {
  MemoryFileSystem file_system;
  populate_valid_files(file_system);
  pc2ps3::RuntimeDemoAtlas atlas(file_system);

  assert(!atlas.load("ui/sprites.p2sa", 1));
  assert(!atlas.valid());
  assert(atlas.texture() == 0);
  assert(atlas.sprite_region() == 0);
  assert(atlas.load("ui/sprites.p2sa", 0));
}

bool same_color(const pc2ps3::Color& left, const pc2ps3::Color& right) {
  return left.r == right.r && left.g == right.g && left.b == right.b &&
         left.a == right.a;
}

void test_sprite_draws_selected_region_and_does_not_read_again() {
  MemoryFileSystem file_system;
  populate_valid_files(file_system);
  pc2ps3::RuntimeDemoAtlas atlas(file_system);
  assert(atlas.load("ui/sprites.p2sa", 0));
  const pc2ps3::Color* source = atlas.texture()->pixel_at(0, 0);
  assert(source != 0);

  pc2ps3::Framebuffer framebuffer(64, 64);
  const pc2ps3::RuntimeDemoPlayerRenderResult result =
      pc2ps3::render_runtime_demo_player(
          framebuffer, &atlas, 32, 48, pc2ps3::Color{222, 82, 74, 255},
          pc2ps3::Color{246, 214, 92, 255});
  pc2ps3::Color rendered = {};
  assert(result == pc2ps3::RuntimeDemoPlayerRenderResult::kAtlasSprite);
  assert(framebuffer.read_pixel(20, 10, rendered));
  assert(same_color(rendered, *source));

  pc2ps3::Framebuffer clipped(16, 16);
  assert(pc2ps3::render_runtime_demo_player(
             clipped, &atlas, 4, 38, pc2ps3::Color{222, 82, 74, 255},
             pc2ps3::Color{246, 214, 92, 255}) ==
         pc2ps3::RuntimeDemoPlayerRenderResult::kAtlasSprite);
  assert(clipped.read_pixel(0, 0, rendered));
  assert(same_color(rendered, *source));
  assert(file_system.read_count == 2);
}

void test_invalid_selection_uses_vector_fallback() {
  MemoryFileSystem file_system;
  populate_valid_files(file_system);
  pc2ps3::RuntimeDemoAtlas atlas(file_system);
  assert(!atlas.load("ui/sprites.p2sa", 1));

  pc2ps3::Framebuffer framebuffer(64, 64);
  const pc2ps3::Color body = {222, 82, 74, 255};
  const pc2ps3::Color accent = {246, 214, 92, 255};
  assert(pc2ps3::render_runtime_demo_player(
             framebuffer, &atlas, 32, 48, body, accent) ==
         pc2ps3::RuntimeDemoPlayerRenderResult::kVectorFallback);
  pc2ps3::Color rendered = {};
  assert(framebuffer.read_pixel(20, 16, rendered));
  assert(same_color(rendered, body));
  assert(framebuffer.read_pixel(24, 10, rendered));
  assert(same_color(rendered, accent));
}

void test_missing_atlas_uses_vector_fallback() {
  MemoryFileSystem file_system;
  pc2ps3::RuntimeDemoAtlas atlas(file_system);
  pc2ps3::Framebuffer framebuffer(64, 64);
  const pc2ps3::Color body = {222, 82, 74, 255};
  const pc2ps3::Color accent = {246, 214, 92, 255};

  assert(pc2ps3::render_runtime_demo_player(
             framebuffer, &atlas, 32, 48, body, accent) ==
         pc2ps3::RuntimeDemoPlayerRenderResult::kVectorFallback);
  pc2ps3::Color rendered = {};
  assert(framebuffer.read_pixel(20, 16, rendered));
  assert(same_color(rendered, body));
  assert(file_system.read_count == 0);
}

} // namespace

int main() {
  test_success_and_explicit_unload();
  test_missing_index_is_inert();
  test_corrupt_index_is_inert();
  test_missing_image_is_inert();
  test_invalid_selection_releases_temporary_handle();
  test_sprite_draws_selected_region_and_does_not_read_again();
  test_invalid_selection_uses_vector_fallback();
  test_missing_atlas_uses_vector_fallback();
  return 0;
}
