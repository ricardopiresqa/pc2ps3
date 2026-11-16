#include "pc2ps3/atlas_runtime_index.hpp"
#include "pc2ps3/texture_manager.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <new>
#include <string>
#include <type_traits>
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

Bytes make_atlas_index(const std::string& image_filename,
                       std::uint32_t width = 1,
                       std::uint32_t height = 1) {
  Bytes bytes = {'P', 'C', '2', 'S', 'A', 'T', 'L', 0};
  append_u16(bytes, 1);
  append_u32(bytes, width);
  append_u32(bytes, height);
  append_u32(bytes, 1);
  append_u16(bytes, static_cast<std::uint16_t>(image_filename.size()));
  bytes.insert(bytes.end(), image_filename.begin(), image_filename.end());
  const std::string id = "button";
  append_u16(bytes, static_cast<std::uint16_t>(id.size()));
  append_u32(bytes, 0);
  append_u32(bytes, 0);
  append_u32(bytes, 1);
  append_u32(bytes, 1);
  bytes.insert(bytes.end(), id.begin(), id.end());
  return bytes;
}

class MemoryFileSystem : public pc2ps3::FileSystem {
public:
MemoryFileSystem() : fail_reads(false), read_count(0), last_read_limit(0) {}

  pc2ps3::FileSystemStatus read_all(
      const std::string& path, std::size_t max_bytes,
      std::vector<std::uint8_t>& output) override {
    ++read_count;
    last_path = path;
    last_read_limit = max_bytes;
    read_paths.push_back(path);
    read_limits.push_back(max_bytes);
    if (fail_reads) return pc2ps3::FileSystemStatus::kError;
    const std::map<std::string, Bytes>::const_iterator it = files.find(path);
    if (it == files.end()) return pc2ps3::FileSystemStatus::kNotFound;
    if (it->second.size() > max_bytes) {
      return pc2ps3::FileSystemStatus::kTooLarge;
    }
    output = it->second;
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

  bool fail_reads;
  std::size_t read_count;
  std::size_t last_read_limit;
  std::string last_path;
  std::vector<std::string> read_paths;
  std::vector<std::size_t> read_limits;
  std::map<std::string, Bytes> files;
};

void expect_invalid_path(pc2ps3::TextureManager& manager,
                         MemoryFileSystem& file_system,
                         const std::string& path) {
  const std::size_t reads_before = file_system.read_count;
  pc2ps3::TextureHandle output;
  assert(manager.load(path, output) == pc2ps3::TextureStatus::kInvalidArgument);
  assert(!output.valid());
  assert(manager.loaded_count() == 1);
  assert(file_system.read_count == reads_before);
}

void test_atlas_loading() {
  MemoryFileSystem file_system;
  file_system.files["ui/main.p2sa"] = make_atlas_index("main.png");
  file_system.files["ui/main.png"] = kOnePixelPng;

  const std::size_t encoded_limit = 1024u * 1024u;
  pc2ps3::TextureManager manager(file_system, 2, encoded_limit, 64);
  pc2ps3::TextureHandle first;
  pc2ps3::AtlasRuntimeIndex index;
  assert(manager.load_atlas("ui/main.p2sa", first, index) ==
         pc2ps3::TextureStatus::kOk);
  assert(first.valid());
  assert(index.width == 1 && index.height == 1);
  assert(index.image_filename == "main.png");
  assert(index.sprites.size() == 1 && index.sprites[0].id == "button");
  assert(file_system.read_paths.size() == 2);
  assert(file_system.read_paths[0] == "ui/main.p2sa");
  assert(file_system.read_limits[0] == pc2ps3::kMaxAtlasRuntimeIndexBytes);
  assert(file_system.read_paths[1] == "ui/main.png");
  assert(file_system.read_limits[1] == encoded_limit);
  const pc2ps3::Texture* texture = manager.get(first);
  assert(texture != 0 && texture->width() == 1 && texture->height() == 1);
  const std::size_t reads_after_success = file_system.read_paths.size();
  assert(manager.load_atlas("ui/main.p2sa", first, index) ==
         pc2ps3::TextureStatus::kInvalidArgument);
  assert(first.valid() && file_system.read_paths.size() == reads_after_success);

  pc2ps3::TextureHandle cached;
  pc2ps3::AtlasRuntimeIndex cached_index;
  assert(manager.load_atlas("ui/main.p2sa", cached, cached_index) ==
         pc2ps3::TextureStatus::kOk);
  assert(cached.valid() && manager.loaded_count() == 1);
  assert(file_system.read_paths.size() == 3);
  assert(file_system.read_paths[2] == "ui/main.p2sa");
  assert(manager.unload(first) == pc2ps3::TextureStatus::kOk);
  assert(manager.unload(cached) == pc2ps3::TextureStatus::kOk);
  assert(manager.loaded_count() == 0);

  pc2ps3::TextureHandle reloaded;
  pc2ps3::AtlasRuntimeIndex reloaded_index;
  assert(manager.load_atlas("ui/main.p2sa", reloaded, reloaded_index) ==
         pc2ps3::TextureStatus::kOk);
  assert(file_system.read_paths.size() == 5);
  assert(file_system.read_paths[3] == "ui/main.p2sa");
  assert(file_system.read_paths[4] == "ui/main.png");
  assert(manager.unload(reloaded) == pc2ps3::TextureStatus::kOk);

  const std::size_t invalid_path_reads = file_system.read_paths.size();
  pc2ps3::TextureHandle invalid_path_handle;
  assert(manager.load_atlas("ui/../main.p2sa", invalid_path_handle, index) ==
         pc2ps3::TextureStatus::kInvalidArgument);
  assert(!invalid_path_handle.valid());
  assert(file_system.read_paths.size() == invalid_path_reads);

  MemoryFileSystem malformed_file_system;
  malformed_file_system.files["ui/bad.p2sa"] =
      make_atlas_index("../outside.png");
  pc2ps3::TextureManager malformed_manager(malformed_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle malformed_handle;
  pc2ps3::AtlasRuntimeIndex unchanged;
  unchanged.width = 77;
  unchanged.image_filename = "preserve.png";
  assert(malformed_manager.load_atlas("ui/bad.p2sa", malformed_handle,
                                      unchanged) ==
         pc2ps3::TextureStatus::kDecodeError);
  assert(!malformed_handle.valid() && unchanged.width == 77);
  assert(unchanged.image_filename == "preserve.png");
  assert(malformed_file_system.read_paths.size() == 1);

  MemoryFileSystem missing_index_file_system;
  pc2ps3::TextureManager missing_index_manager(missing_index_file_system, 1,
                                              1024, 64);
  pc2ps3::TextureHandle missing_index_handle;
  assert(missing_index_manager.load_atlas("ui/absent.p2sa",
                                          missing_index_handle, unchanged) ==
         pc2ps3::TextureStatus::kNotFound);
  assert(!missing_index_handle.valid() && unchanged.width == 77);
  assert(missing_index_file_system.read_paths.size() == 1);

  MemoryFileSystem missing_file_system;
  missing_file_system.files["ui/missing.p2sa"] =
      make_atlas_index("missing.png");
  pc2ps3::TextureManager missing_manager(missing_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle missing_handle;
  assert(missing_manager.load_atlas("ui/missing.p2sa", missing_handle,
                                    unchanged) ==
         pc2ps3::TextureStatus::kNotFound);
  assert(!missing_handle.valid() && unchanged.width == 77);
  assert(missing_file_system.read_paths.size() == 2);
  assert(missing_file_system.read_paths[1] == "ui/missing.png");

  MemoryFileSystem mismatch_file_system;
  mismatch_file_system.files["ui/wide.p2sa"] =
      make_atlas_index("wide.png", 2, 1);
  mismatch_file_system.files["ui/wide.png"] = kOnePixelPng;
  pc2ps3::TextureManager mismatch_manager(mismatch_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle mismatch_handle;
  assert(mismatch_manager.load_atlas("ui/wide.p2sa", mismatch_handle,
                                    unchanged) ==
         pc2ps3::TextureStatus::kDecodeError);
  assert(!mismatch_handle.valid() && unchanged.width == 77);
  assert(mismatch_manager.loaded_count() == 0);

  MemoryFileSystem bounded_file_system;
  bounded_file_system.files["ui/large.p2sa"] = make_atlas_index("large.png");
  pc2ps3::TextureManager bounded_manager(bounded_file_system, 1, 16, 64);
  pc2ps3::TextureHandle bounded_handle;
  assert(bounded_manager.load_atlas("ui/large.p2sa", bounded_handle,
                                   unchanged) ==
         pc2ps3::TextureStatus::kTooLarge);
  assert(!bounded_handle.valid() && unchanged.width == 77);
  assert(bounded_file_system.read_paths.size() == 1);
  assert(bounded_file_system.read_limits[0] == 16);
}

} // namespace

int main() {
  test_atlas_loading();

  MemoryFileSystem file_system;
  file_system.files["textures/red.png"] = kOnePixelPng;
  file_system.files["textures/blue.png"] = kOnePixelPng;
  file_system.files["textures/bad.png"] = kOnePixelPng;
  file_system.files["textures/bad.png"][32] ^= 1u;

  pc2ps3::TextureManager manager(file_system, 2, 1024, 64);
  assert(manager.valid());
  assert(manager.loaded_count() == 0);
  assert(manager.pixel_buffer_capacity_bytes() == 0);

  pc2ps3::TextureHandle first;
  assert(manager.load("textures/red.png", first) == pc2ps3::TextureStatus::kOk);
  assert(first.valid());
  assert(file_system.read_count == 1);
  assert(file_system.last_read_limit == 1024);
  assert(file_system.last_path == "textures/red.png");
  const pc2ps3::Texture* texture = manager.get(first);
  assert(texture != 0 && texture->valid());
  assert(texture->width() == 1 && texture->height() == 1);
  const pc2ps3::Color* pixel = texture->pixel_at(0, 0);
  assert(pixel != 0 && pixel->r == 255 && pixel->g == 0 &&
         pixel->b == 0 && pixel->a == 255);

  const std::size_t first_buffer_capacity =
      manager.pixel_buffer_capacity_bytes();
  assert(first_buffer_capacity >= sizeof(pc2ps3::Color));

  pc2ps3::TextureHandle second;
  assert(manager.load("textures/red.png", second) == pc2ps3::TextureStatus::kOk);
  assert(file_system.read_count == 1);
  assert(manager.loaded_count() == 1);
  assert(manager.get(second) == manager.get(first));

  const std::size_t reads_before_valid_output = file_system.read_count;
  assert(manager.load("textures/blue.png", first) ==
         pc2ps3::TextureStatus::kInvalidArgument);
  assert(manager.get(first) == texture);
  assert(file_system.read_count == reads_before_valid_output);

  pc2ps3::Framebuffer framebuffer(1, 1);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  assert(framebuffer.draw_texture_region(*texture, 0, 0, 1, 1, 0, 0));
  pc2ps3::Color drawn = {0, 0, 0, 0};
  assert(framebuffer.read_pixel(0, 0, drawn));
  assert(drawn.r == 255 && drawn.g == 0 && drawn.b == 0 && drawn.a == 255);

  expect_invalid_path(manager, file_system, "");
  expect_invalid_path(manager, file_system, "/textures/red.png");
  expect_invalid_path(manager, file_system, "../red.png");
  expect_invalid_path(manager, file_system, "textures/../red.png");
  expect_invalid_path(manager, file_system, "textures\\red.png");
  expect_invalid_path(manager, file_system, "textures//red.png");
  expect_invalid_path(manager, file_system, "textures/./red.png");
  expect_invalid_path(manager, file_system, "C:/textures/red.png");
  expect_invalid_path(manager, file_system, std::string(513, 'a'));

  pc2ps3::TextureHandle unchanged;
  assert(manager.load("textures/missing.png", unchanged) ==
         pc2ps3::TextureStatus::kNotFound);
  assert(!unchanged.valid() && manager.get(first) != 0);
  assert(manager.load("textures/bad.png", unchanged) ==
         pc2ps3::TextureStatus::kDecodeError);
  assert(!unchanged.valid() && manager.get(first) != 0);

  pc2ps3::TextureHandle stale = first;
  assert(manager.unload(first) == pc2ps3::TextureStatus::kOk);
  assert(!first.valid());
  assert(manager.get(stale) == 0);
  assert(manager.unload(stale) == pc2ps3::TextureStatus::kInvalidHandle);
  assert(manager.get(second) != 0);
  assert(manager.unload(second) == pc2ps3::TextureStatus::kOk);
  assert(!second.valid());
  assert(manager.loaded_count() == 0);
  assert(manager.get(first) == 0);
  assert(manager.pixel_buffer_capacity_bytes() == first_buffer_capacity);

  pc2ps3::TextureHandle reloaded;
  assert(manager.load("textures/blue.png", reloaded) == pc2ps3::TextureStatus::kOk);
  assert(file_system.read_count == 4);
  assert(manager.get(stale) == 0);
  assert(manager.get(reloaded) != 0);
  assert(manager.pixel_buffer_capacity_bytes() == first_buffer_capacity);
  assert(manager.unload(reloaded) == pc2ps3::TextureStatus::kOk);
  const std::size_t released = manager.trim_unused_buffers();
  assert(released >= first_buffer_capacity);
  assert(manager.pixel_buffer_capacity_bytes() == 0);
  assert(manager.encoded_buffer_capacity_bytes() == 0);

  MemoryFileSystem failing_file_system;
  failing_file_system.files["textures/red.png"] = kOnePixelPng;
  failing_file_system.fail_reads = true;
  pc2ps3::TextureManager failing_manager(failing_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle failed_output;
  assert(failing_manager.load("textures/red.png", failed_output) ==
         pc2ps3::TextureStatus::kIoError);
  assert(!failed_output.valid() && failing_manager.loaded_count() == 0);

  MemoryFileSystem bounded_file_system;
  bounded_file_system.files["textures/red.png"] = kOnePixelPng;
  pc2ps3::TextureManager encoded_limited(bounded_file_system, 1, 32, 64);
  pc2ps3::TextureHandle bounded_output;
  assert(encoded_limited.load("textures/red.png", bounded_output) ==
         pc2ps3::TextureStatus::kTooLarge);
  assert(!bounded_output.valid() && encoded_limited.loaded_count() == 0);

  pc2ps3::TextureManager memory_limited(bounded_file_system, 1, 1024, 3);
  assert(memory_limited.valid());
  pc2ps3::TextureHandle memory_output;
  assert(memory_limited.load("textures/red.png", memory_output) ==
         pc2ps3::TextureStatus::kMemoryLimit);
  assert(!memory_output.valid() && memory_limited.loaded_count() == 0);

  pc2ps3::TextureManager no_slots(bounded_file_system, 0, 1024, 64);
  assert(!no_slots.valid());
  assert(no_slots.load("textures/red.png", memory_output) ==
         pc2ps3::TextureStatus::kInvalidArgument);

  pc2ps3::TextureManager full(bounded_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle active;
  assert(full.load("textures/red.png", active) == pc2ps3::TextureStatus::kOk);
  const std::size_t reads_before_full = bounded_file_system.read_count;
  pc2ps3::TextureHandle no_handle;
  assert(full.load("textures/blue.png", no_handle) ==
         pc2ps3::TextureStatus::kCacheFull);
  assert(!no_handle.valid());
  assert(bounded_file_system.read_count == reads_before_full);
  assert(full.unload(active) == pc2ps3::TextureStatus::kOk);

  typedef std::aligned_storage<sizeof(pc2ps3::TextureManager),
                               alignof(pc2ps3::TextureManager)>::type
      ManagerStorage;
  bounded_file_system.files["textures/blue.png"] = kOnePixelPng;
  ManagerStorage manager_storage;
  pc2ps3::TextureManager* previous_manager =
      new (&manager_storage) pc2ps3::TextureManager(
          bounded_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle previous_handle;
  assert(previous_manager->load("textures/red.png", previous_handle) ==
         pc2ps3::TextureStatus::kOk);
  previous_manager->~TextureManager();

  pc2ps3::TextureManager* replacement_manager =
      new (&manager_storage) pc2ps3::TextureManager(
          bounded_file_system, 1, 1024, 64);
  pc2ps3::TextureHandle replacement_handle;
  assert(replacement_manager->load("textures/blue.png", replacement_handle) ==
         pc2ps3::TextureStatus::kOk);
  assert(replacement_manager->get(previous_handle) == 0);
  assert(replacement_manager->get(replacement_handle) != 0);
  assert(replacement_manager->unload(replacement_handle) ==
         pc2ps3::TextureStatus::kOk);
  replacement_manager->~TextureManager();

  std::cout << "texture manager tests passed\n";
  return 0;
}
