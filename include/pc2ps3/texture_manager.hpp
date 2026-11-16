#pragma once

#include "pc2ps3/storage.hpp"
#include "pc2ps3/render.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace pc2ps3 {

struct AtlasRuntimeIndex;

enum class TextureStatus {
  kOk,
  kInvalidArgument,
  kInvalidHandle,
  kNotFound,
  kIoError,
  kTooLarge,
  kDecodeError,
  kCacheFull,
  kMemoryLimit,
  kTooManyReferences,
  kOutOfMemory
};

class TextureManager;

class TextureHandle {
public:
  TextureHandle()
      : slot_(std::numeric_limits<std::size_t>::max()), generation_(0),
        lease_id_(0), manager_instance_id_(0) {}

  bool valid() const {
    return manager_instance_id_ != 0 && generation_ != 0 && lease_id_ != 0;
  }

private:
  friend class TextureManager;

  std::size_t slot_;
  std::uint64_t generation_;
  std::uint64_t lease_id_;
  std::uint64_t manager_instance_id_;
  void invalidate();
};

// Single-threaded cache for bounded PNG textures read through logical paths.
class TextureManager {
public:
  TextureManager(FileSystem& file_system, std::size_t max_slots,
                 std::size_t max_encoded_bytes,
                 std::size_t max_pixel_buffer_bytes);

  TextureManager(const TextureManager&) = delete;
  TextureManager& operator=(const TextureManager&) = delete;

  bool valid() const;
  TextureStatus load(const std::string& logical_path, TextureHandle& handle);
    // Loads a bounded atlas index and its sibling PNG. The index output and
    // handle are only published when both the metadata and image agree.
    TextureStatus load_atlas(const std::string& index_path,
                             TextureHandle& handle,
                             AtlasRuntimeIndex& index);
  const Texture* get(const TextureHandle& handle) const;
  TextureStatus unload(TextureHandle& handle);

  // Frees retained buffers from unloaded entries and the reusable PNG input.
  std::size_t trim_unused_buffers();
  std::size_t loaded_count() const;
  std::size_t pixel_buffer_capacity_bytes() const;
  std::size_t encoded_buffer_capacity_bytes() const;

private:
  struct Entry {
    Entry() : generation(0), active(false) {}

    std::string path;
    Texture texture;
    std::uint64_t generation;
    std::vector<std::uint64_t> leases;
    bool active;
  };

  static bool valid_logical_path(const std::string& path);
  static TextureStatus map_io_status(FileSystemStatus status);
  bool matches(const Entry& entry, const TextureHandle& handle) const;
  bool lease_in_use(std::uint64_t lease_id) const;
  TextureStatus create_handle(std::size_t slot, TextureHandle& handle);
  std::size_t find_active_path(const std::string& path) const;
  std::size_t find_free_slot() const;
  std::size_t find_reusable_slot(std::size_t pixel_count,
                                 std::size_t decoded_capacity_bytes) const;
  void clear_entry(Entry& entry);

  const std::uint64_t instance_id_;
  FileSystem& file_system_;
  std::vector<Entry> entries_;
  std::vector<std::uint8_t> encoded_buffer_;
  std::size_t max_encoded_bytes_;
  std::size_t max_pixel_buffer_bytes_;
  std::uint64_t next_lease_id_;
  bool valid_;
};

} // namespace pc2ps3
