#include "pc2ps3/texture_manager.hpp"

#include "pc2ps3/atlas_runtime_index.hpp"
#include "pc2ps3/image_decoder.hpp"

#include <algorithm>
#include <atomic>
#include <new>
#include <utility>

namespace pc2ps3 {
namespace {

const std::size_t kMaxSlots = 256u;
const std::size_t kMaxEncodedBytes = 32u * 1024u * 1024u;
const std::size_t kMaxPixelBufferBytes = 64u * 1024u * 1024u;
const std::size_t kMaxPathBytes = 512u;
const std::size_t kMaxReferencesPerTexture = 1024u;

std::uint64_t next_manager_instance_id() {
  static std::atomic<std::uint64_t> next_id(1);
  std::uint64_t id = next_id.fetch_add(1, std::memory_order_relaxed);
  while (id == 0) {
    id = next_id.fetch_add(1, std::memory_order_relaxed);
  }
  return id;
}

std::size_t color_buffer_bytes(const Texture& texture) {
  return texture.pixel_storage_bytes();
}

} // namespace

TextureManager::TextureManager(FileSystem& file_system, std::size_t max_slots,
                               std::size_t max_encoded_bytes,
                               std::size_t max_pixel_buffer_bytes)
    : instance_id_(next_manager_instance_id()), file_system_(file_system),
      max_encoded_bytes_(max_encoded_bytes),
      max_pixel_buffer_bytes_(max_pixel_buffer_bytes), next_lease_id_(1),
      valid_(false) {
  if (max_slots == 0 || max_slots > kMaxSlots || max_encoded_bytes == 0 ||
      max_encoded_bytes > kMaxEncodedBytes || max_pixel_buffer_bytes == 0 ||
      max_pixel_buffer_bytes > kMaxPixelBufferBytes) {
    return;
  }
  try {
    entries_.resize(max_slots);
  } catch (const std::bad_alloc&) {
    entries_.clear();
    return;
  }
  valid_ = true;
}

bool TextureManager::valid() const { return valid_; }

TextureStatus TextureManager::load(const std::string& logical_path,
                                   TextureHandle& handle) {
  if (!valid_ || handle.valid() || !valid_logical_path(logical_path)) {
    return TextureStatus::kInvalidArgument;
  }

  const std::size_t cached = find_active_path(logical_path);
  if (cached != entries_.size()) {
    return create_handle(cached, handle);
  }

  if (find_free_slot() == entries_.size()) return TextureStatus::kCacheFull;

  encoded_buffer_.clear();
  FileSystemStatus io_status = FileSystemStatus::kError;
  try {
    io_status = file_system_.read_all(logical_path, max_encoded_bytes_,
                                      encoded_buffer_);
  } catch (const std::bad_alloc&) {
    encoded_buffer_.clear();
    return TextureStatus::kOutOfMemory;
  }
  if (io_status != FileSystemStatus::kOk) {
    encoded_buffer_.clear();
    return map_io_status(io_status);
  }
  if (encoded_buffer_.size() > max_encoded_bytes_) {
    encoded_buffer_.clear();
    std::vector<std::uint8_t>().swap(encoded_buffer_);
    return TextureStatus::kTooLarge;
  }

  ImageDecodeResult decoded;
  try {
    decoded = decode_png(encoded_buffer_);
  } catch (const std::bad_alloc&) {
    encoded_buffer_.clear();
    return TextureStatus::kOutOfMemory;
  }
  encoded_buffer_.clear();
  if (!decoded.ok || !decoded.texture.valid()) return TextureStatus::kDecodeError;

  const std::size_t pixel_count = decoded.texture.pixels_.size();
  const std::size_t decoded_capacity =
      color_buffer_bytes(decoded.texture);
  const std::size_t slot = find_reusable_slot(pixel_count, decoded_capacity);
  if (slot == entries_.size()) return TextureStatus::kMemoryLimit;

  Entry& entry = entries_[slot];
  try {
    entry.path = logical_path;
    if (entry.texture.pixels_.capacity() >= pixel_count) {
      // A released slot keeps its allocation so same-or-smaller images reuse it.
      entry.texture.pixels_.assign(decoded.texture.pixels_.begin(),
                                   decoded.texture.pixels_.end());
    } else {
      entry.texture.pixels_.swap(decoded.texture.pixels_);
    }
  } catch (const std::bad_alloc&) {
    clear_entry(entry);
    return TextureStatus::kOutOfMemory;
  }

  entry.texture.width_ = decoded.texture.width_;
  entry.texture.height_ = decoded.texture.height_;
  if (pixel_buffer_capacity_bytes() > max_pixel_buffer_bytes_) {
    clear_entry(entry);
    std::vector<Color>().swap(entry.texture.pixels_);
    return TextureStatus::kMemoryLimit;
  }

  if (++entry.generation == 0) ++entry.generation;
  entry.active = true;
  const TextureStatus handle_status = create_handle(slot, handle);
  if (handle_status != TextureStatus::kOk) clear_entry(entry);
  return handle_status;
}

TextureStatus TextureManager::load_atlas(const std::string& index_path,
                                          TextureHandle& handle,
                                          AtlasRuntimeIndex& index) {
    if (!valid_ || handle.valid() || !valid_logical_path(index_path)) {
        return TextureStatus::kInvalidArgument;
    }

    const std::size_t index_read_limit =
        std::min(max_encoded_bytes_, kMaxAtlasRuntimeIndexBytes);
    std::vector<std::uint8_t> index_bytes;
    FileSystemStatus io_status = FileSystemStatus::kError;
    try {
        io_status = file_system_.read_all(index_path, index_read_limit,
                                          index_bytes);
    } catch (const std::bad_alloc&) {
        return TextureStatus::kOutOfMemory;
    }
    if (io_status != FileSystemStatus::kOk) return map_io_status(io_status);
    if (index_bytes.size() > index_read_limit) return TextureStatus::kTooLarge;

    AtlasRuntimeIndex parsed;
    try {
        if (!parse_atlas_runtime_index(index_bytes, &parsed)) {
            return TextureStatus::kDecodeError;
        }
    } catch (const std::bad_alloc&) {
        return TextureStatus::kOutOfMemory;
    }

    const std::size_t slash = index_path.find_last_of('/');
    std::string image_path;
    try {
        if (slash != std::string::npos) {
            image_path.assign(index_path, 0, slash + 1);
        }
        image_path.append(parsed.image_filename);
    } catch (const std::bad_alloc&) {
        return TextureStatus::kOutOfMemory;
    }
    if (!valid_logical_path(image_path)) return TextureStatus::kDecodeError;

    TextureHandle loaded_handle;
    const TextureStatus load_status = load(image_path, loaded_handle);
    if (load_status != TextureStatus::kOk) return load_status;

    const Texture* texture = get(loaded_handle);
    if (texture == NULL || !texture->valid() ||
        static_cast<std::uint32_t>(texture->width()) != parsed.width ||
        static_cast<std::uint32_t>(texture->height()) != parsed.height) {
        unload(loaded_handle);
        return TextureStatus::kDecodeError;
    }

    using std::swap;
    swap(index, parsed);
    handle = loaded_handle;
    return TextureStatus::kOk;
}

const Texture* TextureManager::get(const TextureHandle& handle) const {
  if (handle.slot_ >= entries_.size()) return 0;
  const Entry& entry = entries_[handle.slot_];
  return matches(entry, handle) ? &entry.texture : 0;
}

TextureStatus TextureManager::unload(TextureHandle& handle) {
  if (handle.slot_ >= entries_.size()) return TextureStatus::kInvalidHandle;
  Entry& entry = entries_[handle.slot_];
  if (!matches(entry, handle)) return TextureStatus::kInvalidHandle;

  const std::vector<std::uint64_t>::iterator lease =
      std::find(entry.leases.begin(), entry.leases.end(), handle.lease_id_);
  entry.leases.erase(lease);
  handle.invalidate();
  if (entry.leases.empty()) clear_entry(entry);
  return TextureStatus::kOk;
}

std::size_t TextureManager::trim_unused_buffers() {
  std::size_t released = encoded_buffer_.capacity();
  std::vector<std::uint8_t>().swap(encoded_buffer_);
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    Entry& entry = entries_[i];
    if (entry.active) continue;
    released += color_buffer_bytes(entry.texture);
    std::vector<Color>().swap(entry.texture.pixels_);
    entry.texture.width_ = 0;
    entry.texture.height_ = 0;
  }
  return released;
}

std::size_t TextureManager::loaded_count() const {
  std::size_t count = 0;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].active) ++count;
  }
  return count;
}

std::size_t TextureManager::pixel_buffer_capacity_bytes() const {
  std::size_t total = 0;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    total += color_buffer_bytes(entries_[i].texture);
  }
  return total;
}

std::size_t TextureManager::encoded_buffer_capacity_bytes() const {
  return encoded_buffer_.capacity();
}

bool TextureManager::valid_logical_path(const std::string& path) {
  if (path.empty() || path.size() > kMaxPathBytes || path[0] == '/' ||
      path[path.size() - 1] == '/') {
    return false;
  }

  std::size_t segment_start = 0;
  for (std::size_t i = 0; i <= path.size(); ++i) {
    if (i == path.size() || path[i] == '/') {
      const std::size_t segment_size = i - segment_start;
      if (segment_size == 0 ||
          (segment_size == 1 && path[segment_start] == '.') ||
          (segment_size == 2 && path[segment_start] == '.' &&
           path[segment_start + 1] == '.')) {
        return false;
      }
      segment_start = i + 1;
      continue;
    }
    const unsigned char value = static_cast<unsigned char>(path[i]);
    if (path[i] == '\\' || path[i] == ':' || value < 0x20u ||
        value == 0x7fu) {
      return false;
    }
  }
  return true;
}

TextureStatus TextureManager::map_io_status(FileSystemStatus status) {
  switch (status) {
  case FileSystemStatus::kOk: return TextureStatus::kOk;
  case FileSystemStatus::kNotFound: return TextureStatus::kNotFound;
  case FileSystemStatus::kTooLarge: return TextureStatus::kTooLarge;
  case FileSystemStatus::kError: return TextureStatus::kIoError;
  }
  return TextureStatus::kIoError;
}

bool TextureManager::matches(const Entry& entry,
                             const TextureHandle& handle) const {
  return handle.manager_instance_id_ == instance_id_ &&
         handle.generation_ != 0 && entry.active &&
         entry.generation == handle.generation_ &&
         std::find(entry.leases.begin(), entry.leases.end(),
                   handle.lease_id_) != entry.leases.end();
}

bool TextureManager::lease_in_use(std::uint64_t lease_id) const {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const Entry& entry = entries_[i];
    if (std::find(entry.leases.begin(), entry.leases.end(), lease_id) !=
        entry.leases.end()) {
      return true;
    }
  }
  return false;
}

TextureStatus TextureManager::create_handle(std::size_t slot,
                                            TextureHandle& handle) {
  Entry& entry = entries_[slot];
  if (entry.leases.size() >= kMaxReferencesPerTexture) {
    return TextureStatus::kTooManyReferences;
  }

  std::uint64_t lease_id = 0;
  do {
    lease_id = next_lease_id_++;
    if (next_lease_id_ == 0) next_lease_id_ = 1;
  } while (lease_id == 0 || lease_in_use(lease_id));

  try {
    entry.leases.push_back(lease_id);
  } catch (const std::bad_alloc&) {
    return TextureStatus::kOutOfMemory;
  }
  handle.slot_ = slot;
  handle.generation_ = entry.generation;
  handle.lease_id_ = lease_id;
  handle.manager_instance_id_ = instance_id_;
  return TextureStatus::kOk;
}

std::size_t TextureManager::find_active_path(const std::string& path) const {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].active && entries_[i].path == path) return i;
  }
  return entries_.size();
}

std::size_t TextureManager::find_free_slot() const {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (!entries_[i].active) return i;
  }
  return entries_.size();
}

std::size_t TextureManager::find_reusable_slot(
    std::size_t pixel_count, std::size_t decoded_capacity_bytes) const {
  const std::size_t total = pixel_buffer_capacity_bytes();
  std::size_t best_fit = entries_.size();
  std::size_t best_capacity = 0;

  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const Entry& entry = entries_[i];
    if (entry.active) continue;
    const std::size_t capacity = entry.texture.pixels_.capacity();
    if (capacity >= pixel_count) {
      if (best_fit == entries_.size() || capacity < best_capacity) {
        best_fit = i;
        best_capacity = capacity;
      }
    }
  }
  if (best_fit != entries_.size()) return best_fit;

  std::size_t selected = entries_.size();
  std::size_t selected_capacity = 0;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const Entry& entry = entries_[i];
    if (entry.active) continue;
    const std::size_t old_bytes = color_buffer_bytes(entry.texture);
    if (old_bytes > total) continue;
    const std::size_t other_bytes = total - old_bytes;
    if (other_bytes > max_pixel_buffer_bytes_ ||
        decoded_capacity_bytes > max_pixel_buffer_bytes_ - other_bytes) {
      continue;
    }
    if (selected == entries_.size() || old_bytes > selected_capacity) {
      selected = i;
      selected_capacity = old_bytes;
    }
  }
  return selected;
}

void TextureManager::clear_entry(Entry& entry) {
  entry.path.clear();
  entry.texture.width_ = 0;
  entry.texture.height_ = 0;
  entry.texture.pixels_.clear();
  entry.leases.clear();
  entry.active = false;
}

void TextureHandle::invalidate() {
  slot_ = std::numeric_limits<std::size_t>::max();
  generation_ = 0;
  lease_id_ = 0;
  manager_instance_id_ = 0;
}

} // namespace pc2ps3
