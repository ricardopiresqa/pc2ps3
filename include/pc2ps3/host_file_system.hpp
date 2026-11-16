#pragma once

#include "pc2ps3/storage.hpp"

#include <string>

namespace pc2ps3 {

// Windows host adapter. Logical paths use forward slashes and stay under root.
class HostFileSystem : public FileSystem {
public:
    explicit HostFileSystem(const std::string& root_path);
    ~HostFileSystem() override;

    HostFileSystem(const HostFileSystem&) = delete;
    HostFileSystem& operator=(const HostFileSystem&) = delete;

    bool valid() const;

    FileSystemStatus read_all(const std::string& path,
                              std::size_t max_bytes,
                              std::vector<std::uint8_t>& output) override;
    FileSystemStatus write_atomic(const std::string& path,
                                  const std::uint8_t* data,
                                  std::size_t size) override;
    FileSystemStatus remove(const std::string& path) override;

private:
    std::wstring root_path_;
    bool valid_;
};

}  // namespace pc2ps3
