#include "pc2ps3/bundle_file_system.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

namespace pc2ps3 {
namespace {

const std::uint64_t kHeaderBytes = 24u;
const std::uint64_t kEntryBytes = 24u;
const std::uint64_t kMaxEntries = 100000u;
const std::uint64_t kMaxPathBytes = 512u;
const std::uint64_t kMaxIndexBytes = 64u * 1024u * 1024u;
const char kMagic[8] = {'P', 'C', '2', 'P', 'A', 'K', '1', '\0'};

std::uint16_t read_u16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(bytes[1] << 8);
}

std::uint32_t read_u32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t read_u64(const std::uint8_t* bytes) {
  return static_cast<std::uint64_t>(read_u32(bytes)) |
         (static_cast<std::uint64_t>(read_u32(bytes + 4)) << 32);
}

void write_u64(std::uint8_t* bytes, std::uint64_t value) {
  for (unsigned int i = 0; i < 8; ++i) {
    bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}

bool read_exact(std::ifstream& stream, std::uint8_t* output,
                std::size_t size) {
  if (size == 0) return true;
  if (size > static_cast<std::size_t>(
                 (std::numeric_limits<std::streamsize>::max)())) {
    return false;
  }
  stream.read(reinterpret_cast<char*>(output),
              static_cast<std::streamsize>(size));
  return stream.good() &&
         stream.gcount() == static_cast<std::streamsize>(size);
}

bool bytewise_less(const std::string& left, const std::string& right) {
  const std::size_t common = (std::min)(left.size(), right.size());
  for (std::size_t i = 0; i < common; ++i) {
    const unsigned char a = static_cast<unsigned char>(left[i]);
    const unsigned char b = static_cast<unsigned char>(right[i]);
    if (a != b) return a < b;
  }
  return left.size() < right.size();
}

std::uint32_t calculate_crc32(const std::vector<std::uint8_t>& bytes) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    crc ^= bytes[i];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

bool tell_file_size(std::ifstream& stream, std::uint64_t& size) {
  stream.seekg(0, std::ios::end);
  const std::streamoff end = stream.tellg();
  if (!stream || end < 0) return false;
  size = static_cast<std::uint64_t>(end);
  stream.seekg(0, std::ios::beg);
  return stream.good();
}

}  // namespace

BundleFileSystem::BundleFileSystem(const std::string& package_path)
    : package_path_(package_path),
      package_size_(0),
      status_(FileSystemStatus::kError) {
  if (package_path_.empty() || package_path_.find('\0') != std::string::npos) {
    return;
  }
  if (load_index()) status_ = FileSystemStatus::kOk;
}

bool BundleFileSystem::valid() const {
  return status_ == FileSystemStatus::kOk;
}

bool BundleFileSystem::load_index() {
  try {
    std::ifstream stream(package_path_.c_str(), std::ios::binary);
    if (!stream || !tell_file_size(stream, package_size_) ||
        package_size_ < kHeaderBytes) {
      return false;
    }

    std::uint8_t header[kHeaderBytes];
    if (!read_exact(stream, header, sizeof(header))) return false;
    if (!std::equal(kMagic, kMagic + sizeof(kMagic),
                    reinterpret_cast<const char*>(header))) {
      return false;
    }

    const std::uint16_t version = read_u16(header + 8);
    const std::uint16_t flags = read_u16(header + 10);
    const std::uint32_t count = read_u32(header + 12);
    const std::uint64_t index_size = read_u64(header + 16);
    if (version != 1 || flags != 0 || count > kMaxEntries ||
        index_size > kMaxIndexBytes ||
        index_size > package_size_ - kHeaderBytes ||
        static_cast<std::uint64_t>(count) > index_size / kEntryBytes) {
      return false;
    }

    std::vector<std::uint8_t> index(static_cast<std::size_t>(index_size));
    if (!read_exact(stream, index.empty() ? NULL : &index[0], index.size())) {
      return false;
    }

    std::vector<Entry> parsed;
    parsed.reserve(count);
    std::size_t cursor = 0;
    std::uint64_t expected_offset = kHeaderBytes + index_size;
    for (std::uint32_t i = 0; i < count; ++i) {
      if (cursor > index.size() || index.size() - cursor < kEntryBytes) {
        return false;
      }
      const std::uint8_t* record = &index[cursor];
      const std::uint16_t path_size = read_u16(record);
      const std::uint16_t reserved = read_u16(record + 2);
      const std::uint64_t offset = read_u64(record + 4);
      const std::uint64_t size = read_u64(record + 12);
      const std::uint32_t crc = read_u32(record + 20);
      cursor += static_cast<std::size_t>(kEntryBytes);
      if (reserved != 0 || path_size == 0 || path_size > kMaxPathBytes ||
          cursor > index.size() || index.size() - cursor < path_size) {
        return false;
      }

      Entry entry;
      entry.path.assign(reinterpret_cast<const char*>(&index[cursor]),
                        path_size);
      entry.offset = offset;
      entry.size = size;
      entry.crc32 = crc;
      cursor += path_size;

      if (!valid_logical_path(entry.path) || offset != expected_offset ||
          offset > package_size_ || size > package_size_ - offset) {
        return false;
      }
      if (!parsed.empty() &&
          !bytewise_less(parsed.back().path, entry.path)) {
        return false;
      }
      expected_offset += size;
      parsed.push_back(entry);
    }

    if (cursor != index.size() || expected_offset != package_size_) {
      return false;
    }
    entries_.swap(parsed);
    return true;
  } catch (const std::bad_alloc&) {
    return false;
  } catch (const std::length_error&) {
    return false;
  } catch (const std::ios_base::failure&) {
    return false;
  }
}

bool BundleFileSystem::valid_logical_path(const std::string& path) {
  if (path.empty() || path.size() > kMaxPathBytes || path[0] == '/' ||
      path[path.size() - 1] == '/') {
    return false;
  }

  std::size_t segment_start = 0;
  for (std::size_t i = 0; i < path.size();) {
    const unsigned char first = static_cast<unsigned char>(path[i]);
    if (path[i] == '\\' || path[i] == ':' || first < 0x20u ||
        first == 0x7Fu) {
      return false;
    }
    if (path[i] == '/') {
      const std::size_t segment_size = i - segment_start;
      if (segment_size == 0 ||
          (segment_size == 1 && path[segment_start] == '.') ||
          (segment_size == 2 && path[segment_start] == '.' &&
           path[segment_start + 1] == '.')) {
        return false;
      }
      segment_start = ++i;
      continue;
    }

    std::size_t continuation = 0;
    if (first <= 0x7Fu) {
      ++i;
      continue;
    } else if (first >= 0xC2u && first <= 0xDFu) {
      continuation = 1;
    } else if (first >= 0xE0u && first <= 0xEFu) {
      continuation = 2;
    } else if (first >= 0xF0u && first <= 0xF4u) {
      continuation = 3;
    } else {
      return false;
    }
    if (path.size() - i <= continuation) return false;
    const unsigned char second = static_cast<unsigned char>(path[i + 1]);
    if ((first == 0xE0u && second < 0xA0u) ||
        (first == 0xEDu && second >= 0xA0u) ||
        (first == 0xF0u && second < 0x90u) ||
        (first == 0xF4u && second >= 0x90u)) {
      return false;
    }
    for (std::size_t j = 1; j <= continuation; ++j) {
      const unsigned char next = static_cast<unsigned char>(path[i + j]);
      if (next < 0x80u || next > 0xBFu) return false;
    }
    i += continuation + 1;
  }

  const std::size_t final_size = path.size() - segment_start;
  return final_size != 0 && !(final_size == 1 && path[segment_start] == '.') &&
         !(final_size == 2 && path[segment_start] == '.' &&
           path[segment_start + 1] == '.');
}

FileSystemStatus BundleFileSystem::read_all(
    const std::string& path, std::size_t max_bytes,
    std::vector<std::uint8_t>& output) {
  if (!valid()) return status_;
  if (!valid_logical_path(path)) return FileSystemStatus::kError;

  const std::vector<Entry>::const_iterator entry = std::lower_bound(
      entries_.begin(), entries_.end(), path,
      [](const Entry& candidate, const std::string& value) {
        return bytewise_less(candidate.path, value);
      });
  if (entry == entries_.end() || entry->path != path) {
    return FileSystemStatus::kNotFound;
  }
  if (entry->size > max_bytes ||
      entry->size > (std::numeric_limits<std::size_t>::max)()) {
    return FileSystemStatus::kTooLarge;
  }
  if (entry->offset > static_cast<std::uint64_t>(
                          (std::numeric_limits<std::streamoff>::max)()) ||
      entry->size > static_cast<std::uint64_t>(
                        (std::numeric_limits<std::streamsize>::max)())) {
    return FileSystemStatus::kTooLarge;
  }

  try {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry->size));
    std::ifstream stream(package_path_.c_str(), std::ios::binary);
    std::uint64_t current_size = 0;
    if (!stream || !tell_file_size(stream, current_size) ||
        current_size != package_size_) {
      return FileSystemStatus::kError;
    }
    stream.seekg(static_cast<std::streamoff>(entry->offset), std::ios::beg);
    if (!stream || !read_exact(stream, bytes.empty() ? NULL : &bytes[0],
                               bytes.size()) ||
        calculate_crc32(bytes) != entry->crc32) {
      return FileSystemStatus::kError;
    }
    output.swap(bytes);
    return FileSystemStatus::kOk;
  } catch (const std::bad_alloc&) {
    return FileSystemStatus::kError;
  } catch (const std::length_error&) {
    return FileSystemStatus::kError;
  } catch (const std::ios_base::failure&) {
    return FileSystemStatus::kError;
  }
}

FileSystemStatus BundleFileSystem::write_atomic(
    const std::string&, const std::uint8_t*, std::size_t) {
  return FileSystemStatus::kError;
}

FileSystemStatus BundleFileSystem::remove(const std::string&) {
  return FileSystemStatus::kError;
}

}  // namespace pc2ps3
