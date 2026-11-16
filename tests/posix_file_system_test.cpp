#include "pc2ps3/posix_file_system.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

std::vector<std::uint8_t> bytes(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

} // namespace

int main() {
    using pc2ps3::FileSystemStatus;
    using pc2ps3::PosixFileSystem;

    check(PosixFileSystem::valid_logical_path("save.bin"), "plain name");
    check(PosixFileSystem::valid_logical_path("player/atlas.p2sa"), "nested name");
    const char* rejected[] = {"", "/etc/passwd", "../x", "a/../b", "a//b", "a/",
                              "./a", "a\\b", "c:x", "a/."};
    for (std::size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        check(!PosixFileSystem::valid_logical_path(rejected[i]), rejected[i]);
    }
    check(!PosixFileSystem("relative/root").valid(), "relative root rejected");

    char root_template[] = "/tmp/pc2ps3-posixfs-XXXXXX";
    const char* root = mkdtemp(root_template);
    check(root != 0, "mkdtemp");
    if (root == 0) return 1;
    PosixFileSystem fs(std::string(root) + "/");
    check(fs.valid(), "valid root");

    std::vector<std::uint8_t> out = bytes("keep");
    check(fs.read_all("missing.bin", 64, out) == FileSystemStatus::kNotFound, "missing -> kNotFound");
    check(out == bytes("keep"), "output untouched on failure");

    const std::vector<std::uint8_t> first = bytes("first-version");
    check(fs.write_atomic("save.bin", &first[0], first.size()) == FileSystemStatus::kOk, "write new");
    check(fs.read_all("save.bin", 64, out) == FileSystemStatus::kOk && out == first, "read back");

    const std::vector<std::uint8_t> second = bytes("second");
    check(fs.write_atomic("save.bin", &second[0], second.size()) == FileSystemStatus::kOk, "overwrite");
    check(fs.read_all("save.bin", 64, out) == FileSystemStatus::kOk && out == second, "read overwrite");
    check(access((std::string(root) + "/save.bin.tmp").c_str(), F_OK) != 0, "no temp left behind");

    out = bytes("keep");
    check(fs.read_all("save.bin", 3, out) == FileSystemStatus::kTooLarge, "max_bytes enforced");
    check(out == bytes("keep"), "output untouched when too large");

    // Failed write (missing parent dir) must not touch existing data.
    check(fs.write_atomic("nodir/save.bin", &first[0], first.size()) == FileSystemStatus::kError, "write into missing dir fails");
    check(fs.read_all("save.bin", 64, out) == FileSystemStatus::kOk && out == second, "previous file preserved");

    check(fs.write_atomic("empty.bin", 0, 0) == FileSystemStatus::kOk, "empty write");
    check(fs.read_all("empty.bin", 64, out) == FileSystemStatus::kOk && out.empty(), "empty read");
    check(fs.read_all("../escape", 64, out) == FileSystemStatus::kError, "escape refused");

    check(fs.remove("save.bin") == FileSystemStatus::kOk, "remove");
    check(fs.remove("save.bin") == FileSystemStatus::kNotFound, "remove missing");
    fs.remove("empty.bin");
    rmdir(root);

    if (failures == 0) std::printf("posix_file_system: ok\n");
    return failures == 0 ? 0 : 1;
}
