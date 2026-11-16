#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pc2ps3 {

enum class FileSystemStatus {
  kOk,
  kNotFound,
  kTooLarge,
  kError
};

// Paths are logical to the platform adapter. read_all must enforce max_bytes
// before retaining an oversized file in memory and leave output unchanged on
// failure. write_atomic must preserve the previous file when it fails.
class FileSystem {
 public:
  virtual ~FileSystem() {}

  virtual FileSystemStatus read_all(const std::string& path,
                                    std::size_t max_bytes,
                                    std::vector<std::uint8_t>& output) = 0;
  virtual FileSystemStatus write_atomic(const std::string& path,
                                        const std::uint8_t* data,
                                        std::size_t size) = 0;
  virtual FileSystemStatus remove(const std::string& path) = 0;
};

enum class SaveStatus {
  kOk,
  kNotFound,
  kIoError,
  kTooLarge,
  kInvalidArgument,
  kCorruptData,
  kUnsupportedVersion
};

class SaveManager {
 public:
  SaveManager(FileSystem& file_system, const std::string& trusted_root,
              std::uint32_t slot_count, std::size_t max_payload_bytes);

  bool valid() const;
  SaveStatus save(std::uint32_t slot,
                  const std::vector<std::uint8_t>& payload);
  SaveStatus load(std::uint32_t slot, std::vector<std::uint8_t>& payload);
  SaveStatus erase(std::uint32_t slot);

  std::size_t max_payload_bytes() const;

 private:
  std::string slot_path(std::uint32_t slot) const;
  SaveStatus map_io_status(FileSystemStatus status) const;

  FileSystem& file_system_;
  std::string trusted_root_;
  std::uint32_t slot_count_;
  std::size_t max_payload_bytes_;
  bool valid_;
};

}  // namespace pc2ps3
