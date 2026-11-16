#pragma once

#include "pc2ps3/storage.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pc2ps3 {

// Read-only FileSystem view over an uncompressed PC2PAK v1 package.
class BundleFileSystem : public FileSystem {
 public:
  explicit BundleFileSystem(const std::string& package_path);

  BundleFileSystem(const BundleFileSystem&) = delete;
  BundleFileSystem& operator=(const BundleFileSystem&) = delete;

  bool valid() const;

  FileSystemStatus read_all(const std::string& path, std::size_t max_bytes,
                            std::vector<std::uint8_t>& output) override;
  FileSystemStatus write_atomic(const std::string& path,
                                const std::uint8_t* data,
                                std::size_t size) override;
  FileSystemStatus remove(const std::string& path) override;

 private:
  struct Entry {
    std::string path;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t crc32;
  };

  bool load_index();
  static bool valid_logical_path(const std::string& path);

  std::string package_path_;
  std::vector<Entry> entries_;
  std::uint64_t package_size_;
  FileSystemStatus status_;
};

}  // namespace pc2ps3
