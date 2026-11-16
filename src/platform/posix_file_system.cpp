#include "pc2ps3/posix_file_system.hpp"

#include <cerrno>
#include <cstdio>

namespace pc2ps3 {

PosixFileSystem::PosixFileSystem(const std::string& root_path)
    : root_(root_path), valid_(false) {
    while (root_.size() > 1 && root_[root_.size() - 1] == '/') {
        root_.erase(root_.size() - 1);
    }
    valid_ = !root_.empty() && root_[0] == '/';
}

bool PosixFileSystem::valid_logical_path(const std::string& path) {
    if (path.empty() || path[0] == '/' || path.size() > 1024) return false;
    std::size_t start = 0;
    for (;;) {
        const std::size_t slash = path.find('/', start);
        const std::string segment = path.substr(
            start, slash == std::string::npos ? std::string::npos : slash - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        for (std::size_t i = 0; i < segment.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(segment[i]);
            if (c < 0x20 || c == '\\' || c == ':') return false;
        }
        if (slash == std::string::npos) return true;
        start = slash + 1;
    }
}

std::string PosixFileSystem::resolve(const std::string& path) const {
    return root_ == "/" ? root_ + path : root_ + "/" + path;
}

FileSystemStatus PosixFileSystem::read_all(const std::string& path,
                                           std::size_t max_bytes,
                                           std::vector<std::uint8_t>& output) {
    if (!valid_ || !valid_logical_path(path)) return FileSystemStatus::kError;
    std::FILE* file = std::fopen(resolve(path).c_str(), "rb");
    if (file == 0) {
        return errno == ENOENT ? FileSystemStatus::kNotFound
                               : FileSystemStatus::kError;
    }
    FileSystemStatus status = FileSystemStatus::kError;
    long size = -1;
    if (std::fseek(file, 0, SEEK_END) == 0) size = std::ftell(file);
    if (size >= 0 && std::fseek(file, 0, SEEK_SET) == 0) {
        if (static_cast<unsigned long>(size) > max_bytes) {
            status = FileSystemStatus::kTooLarge;
        } else {
            std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
            if (data.empty() ||
                std::fread(&data[0], 1, data.size(), file) == data.size()) {
                output.swap(data);
                status = FileSystemStatus::kOk;
            }
        }
    }
    std::fclose(file);
    return status;
}

FileSystemStatus PosixFileSystem::write_atomic(const std::string& path,
                                               const std::uint8_t* data,
                                               std::size_t size) {
    if (!valid_ || !valid_logical_path(path) || (data == 0 && size != 0)) {
        return FileSystemStatus::kError;
    }
    const std::string target = resolve(path);
    const std::string temp = target + ".tmp";
    std::FILE* file = std::fopen(temp.c_str(), "wb");
    if (file == 0) return FileSystemStatus::kError;
    const bool written = size == 0 || std::fwrite(data, 1, size, file) == size;
    const bool flushed = std::fflush(file) == 0;
    if (std::fclose(file) != 0 || !written || !flushed) {
        std::remove(temp.c_str());
        return FileSystemStatus::kError;
    }
    if (std::rename(temp.c_str(), target.c_str()) == 0) {
        return FileSystemStatus::kOk;
    }
    // shortcut: cellFs rename refuses an existing destination, so replace via
    // remove+rename; a crash between them leaves only <path>.tmp. Upgrade to a
    // two-slot save scheme if saves become critical on console.
    std::FILE* existing = std::fopen(target.c_str(), "rb");
    if (existing == 0) {
        std::remove(temp.c_str());
        return FileSystemStatus::kError;
    }
    std::fclose(existing);
    if (std::remove(target.c_str()) != 0) {
        std::remove(temp.c_str());  // previous file is still intact
        return FileSystemStatus::kError;
    }
    // Old file is gone now: never delete the temp copy, it is the only data left.
    return std::rename(temp.c_str(), target.c_str()) == 0
               ? FileSystemStatus::kOk
               : FileSystemStatus::kError;
}

FileSystemStatus PosixFileSystem::remove(const std::string& path) {
    if (!valid_ || !valid_logical_path(path)) return FileSystemStatus::kError;
    if (std::remove(resolve(path).c_str()) == 0) return FileSystemStatus::kOk;
    return errno == ENOENT ? FileSystemStatus::kNotFound : FileSystemStatus::kError;
}

} // namespace pc2ps3
