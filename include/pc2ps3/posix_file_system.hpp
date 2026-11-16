#pragma once

#include "pc2ps3/storage.hpp"

#include <string>

namespace pc2ps3 {

// Portable stdio adapter (PS3 newlib/PSL1GHT, Linux). Logical paths are
// relative, use '/' and may not contain empty, "." or ".." segments, so every
// access stays under root. Symlinks are not resolved (PS3 has none).
class PosixFileSystem : public FileSystem {
public:
    explicit PosixFileSystem(const std::string& root_path);

    bool valid() const { return valid_; }
    static bool valid_logical_path(const std::string& path);

    FileSystemStatus read_all(const std::string& path, std::size_t max_bytes,
                              std::vector<std::uint8_t>& output) override;
    FileSystemStatus write_atomic(const std::string& path,
                                  const std::uint8_t* data,
                                  std::size_t size) override;
    FileSystemStatus remove(const std::string& path) override;

private:
    std::string resolve(const std::string& path) const;

    std::string root_;
    bool valid_;
};

} // namespace pc2ps3
