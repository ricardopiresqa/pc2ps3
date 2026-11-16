#include "pc2ps3/host_file_system.hpp"

#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::cerr << "Check failed: " #condition << " at line "           \
                      << __LINE__ << "\n";                                    \
            return 1;                                                         \
        }                                                                     \
    } while (false)

namespace {

bool write_file(const std::wstring& path, const std::string& value);

class TempDirectory {
public:
    TempDirectory() : valid_(false) {
        wchar_t temp_path[MAX_PATH] = {};
        const DWORD length = GetCurrentDirectoryW(MAX_PATH, temp_path);
        if (length == 0 || length >= MAX_PATH) {
            return;
        }
        wchar_t unique_path[MAX_PATH] = {};
        if (GetTempFileNameW(temp_path, L"p2p", 0, unique_path) == 0) {
            return;
        }
        if (!DeleteFileW(unique_path) || !CreateDirectoryW(unique_path, NULL)) {
            DeleteFileW(unique_path);
            return;
        }
        path_ = unique_path;
        outside_file_ = path_ + L"_outside.txt";
        outside_directory_ = path_ + L"_outside_dir";
        alias_path_ = path_ + L"_alias";
        if (CreateDirectoryW(outside_directory_.c_str(), NULL) &&
            write_file(outside_directory_ + L"\\secret.txt", "outside")) {
            valid_ = true;
        } else {
            DeleteFileW((outside_directory_ + L"\\secret.txt").c_str());
            RemoveDirectoryW(outside_directory_.c_str());
            RemoveDirectoryW(path_.c_str());
        }
    }

    ~TempDirectory() {
        if (!valid_) {
            return;
        }
        DeleteFileW((path_ + L"\\asset.bin").c_str());
        DeleteFileW((path_ + L"\\saves\\slot.pc2save").c_str());
        DeleteFileW((path_ + L"\\linked.txt").c_str());
        DeleteFileW((path_ + L"\\hardlinked.txt").c_str());
        RemoveDirectoryW((path_ + L"\\linked_dir").c_str());
        DeleteFileW(outside_file_.c_str());
        DeleteFileW((outside_directory_ + L"\\secret.txt").c_str());
        RemoveDirectoryW(outside_directory_.c_str());
        RemoveDirectoryW((path_ + L"\\saves").c_str());
        RemoveDirectoryW(alias_path_.c_str());
        RemoveDirectoryW(path_.c_str());
    }

    bool valid() const { return valid_; }
    const std::wstring& path() const { return path_; }
    const std::wstring& outside_file() const { return outside_file_; }
    const std::wstring& outside_directory() const { return outside_directory_; }
    const std::wstring& alias_path() const { return alias_path_; }

private:
    bool valid_;
    std::wstring path_;
    std::wstring outside_file_;
    std::wstring outside_directory_;
    std::wstring alias_path_;
};

bool to_utf8(const std::wstring& input, std::string* output) {
    if (!output || input.empty()) {
        return false;
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0,
                                           NULL, NULL);
    if (needed <= 1) {
        return false;
    }
    std::string replacement(static_cast<std::size_t>(needed), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, &replacement[0],
                            needed, NULL, NULL) != needed) {
        return false;
    }
    replacement.resize(static_cast<std::size_t>(needed - 1));
    output->swap(replacement);
    return true;
}

bool write_file(const std::wstring& path, const std::string& value) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok =
        value.empty() ||
        (WriteFile(file, value.data(), static_cast<DWORD>(value.size()), &written,
                   NULL) &&
         written == static_cast<DWORD>(value.size()));
    CloseHandle(file);
    return ok;
}

bool read_native_file(const std::wstring& path, std::string* output) {
    if (!output) {
        return false;
    }
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    char buffer[64] = {};
    DWORD read_bytes = 0;
    const bool ok = ReadFile(file, buffer, sizeof(buffer), &read_bytes, NULL) !=
                    FALSE;
    CloseHandle(file);
    if (!ok) {
        return false;
    }
    output->assign(buffer, read_bytes);
    return true;
}

bool read_file(pc2ps3::HostFileSystem& file_system, const std::string& path,
               const std::string& expected) {
    std::vector<std::uint8_t> bytes;
    if (file_system.read_all(path, 1024, bytes) !=
        pc2ps3::FileSystemStatus::kOk) {
        return false;
    }
    return std::string(bytes.begin(), bytes.end()) == expected;
}

bool create_file_symlink(const std::wstring& link_path,
                         const std::wstring& target_path) {
    DWORD flags = 0;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
    if (CreateSymbolicLinkW(link_path.c_str(), target_path.c_str(), flags)) {
        return true;
    }
    return CreateSymbolicLinkW(link_path.c_str(), target_path.c_str(), 0) != FALSE;
}

bool create_directory_symlink(const std::wstring& link_path,
                              const std::wstring& target_path) {
    DWORD flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
    if (CreateSymbolicLinkW(link_path.c_str(), target_path.c_str(), flags)) {
        return true;
    }
    return CreateSymbolicLinkW(link_path.c_str(), target_path.c_str(),
                               SYMBOLIC_LINK_FLAG_DIRECTORY) != FALSE;
}

bool create_directory_junction(const std::wstring& link_path,
                               const std::wstring& target_path) {
    if (!CreateDirectoryW(link_path.c_str(), NULL)) {
        return false;
    }
    HANDLE directory = CreateFileW(
        link_path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (directory == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(link_path.c_str());
        return false;
    }

    struct MountPointBuffer {
        ULONG tag;
        USHORT data_length;
        USHORT reserved;
        USHORT substitute_offset;
        USHORT substitute_length;
        USHORT print_offset;
        USHORT print_length;
        WCHAR path_buffer[1024];
    } buffer = {};

    const std::wstring substitute = L"\\??\\" + target_path;
    const std::size_t substitute_bytes = substitute.size() * sizeof(WCHAR);
    const std::size_t print_bytes = target_path.size() * sizeof(WCHAR);
    const std::size_t path_bytes = substitute_bytes + sizeof(WCHAR) + print_bytes;
    const std::size_t header_bytes = offsetof(MountPointBuffer, path_buffer);
    if (path_bytes > sizeof(buffer.path_buffer) ||
        substitute_bytes > (std::numeric_limits<USHORT>::max)() ||
        print_bytes > (std::numeric_limits<USHORT>::max)()) {
        CloseHandle(directory);
        RemoveDirectoryW(link_path.c_str());
        return false;
    }

    buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
    buffer.data_length = static_cast<USHORT>(8u + path_bytes);
    buffer.substitute_offset = 0;
    buffer.substitute_length = static_cast<USHORT>(substitute_bytes);
    buffer.print_offset = static_cast<USHORT>(substitute_bytes + sizeof(WCHAR));
    buffer.print_length = static_cast<USHORT>(print_bytes);
    std::copy(substitute.begin(), substitute.end(), buffer.path_buffer);
    buffer.path_buffer[substitute.size()] = L'\0';
    std::copy(target_path.begin(), target_path.end(),
              buffer.path_buffer + substitute.size() + 1u);

    DWORD returned = 0;
    const bool created =
        DeviceIoControl(directory, FSCTL_SET_REPARSE_POINT, &buffer,
                        static_cast<DWORD>(header_bytes + path_bytes), NULL, 0,
                        &returned, NULL) != FALSE;
    CloseHandle(directory);
    if (!created) {
        RemoveDirectoryW(link_path.c_str());
    }
    return created;
}

}  // namespace

int main() {
    TempDirectory temporary;
    CHECK(temporary.valid());
    CHECK(write_file(temporary.path() + L"\\asset.bin", "abc"));
    CHECK(write_file(temporary.outside_file(), "outside"));
    CHECK(CreateDirectoryW((temporary.path() + L"\\saves").c_str(), NULL));

    std::string root;
    CHECK(to_utf8(temporary.path(), &root));
    pc2ps3::HostFileSystem file_system(root);
    CHECK(file_system.valid());

    std::vector<std::uint8_t> bytes(1, 0xeeu);
    CHECK(file_system.read_all("asset.bin", 3, bytes) ==
           pc2ps3::FileSystemStatus::kOk);
    CHECK(bytes.size() == 3 && bytes[0] == 'a' && bytes[2] == 'c');

    bytes.assign(1, 0xeeu);
    CHECK(file_system.read_all("asset.bin", 2, bytes) ==
           pc2ps3::FileSystemStatus::kTooLarge);
    CHECK(bytes.size() == 1 && bytes[0] == 0xeeu);
    CHECK(file_system.read_all("missing.bin", 10, bytes) ==
           pc2ps3::FileSystemStatus::kNotFound);
    CHECK(bytes.size() == 1 && bytes[0] == 0xeeu);

    const char* invalid_paths[] = {
        "../asset.bin", "a/../asset.bin", "/asset.bin", "\\asset.bin",
        "C:/asset.bin", "saves\\slot.pc2save", "saves//slot.pc2save",
        "saves/./slot.pc2save", "saves/"
    };
    for (std::size_t i = 0;
         i < sizeof(invalid_paths) / sizeof(invalid_paths[0]); ++i) {
        CHECK(file_system.read_all(invalid_paths[i], 1024, bytes) ==
               pc2ps3::FileSystemStatus::kError);
    }

    const std::uint8_t old_save[] = {'o', 'l', 'd'};
    const std::uint8_t new_save[] = {'n', 'e', 'w', '!'};
    CHECK(file_system.write_atomic("saves/slot.pc2save", old_save,
                                    sizeof(old_save)) ==
           pc2ps3::FileSystemStatus::kOk);
    CHECK(read_file(file_system, "saves/slot.pc2save", "old"));
    CHECK(file_system.write_atomic("../slot.pc2save", new_save,
                                    sizeof(new_save)) ==
           pc2ps3::FileSystemStatus::kError);
    CHECK(read_file(file_system, "saves/slot.pc2save", "old"));
    CHECK(file_system.write_atomic("saves/slot.pc2save", NULL, 1) ==
           pc2ps3::FileSystemStatus::kError);
    CHECK(read_file(file_system, "saves/slot.pc2save", "old"));
    CHECK(file_system.write_atomic("saves/slot.pc2save", new_save,
                                    sizeof(new_save)) ==
           pc2ps3::FileSystemStatus::kOk);
    CHECK(read_file(file_system, "saves/slot.pc2save", "new!"));

    CHECK(file_system.remove("saves/slot.pc2save") ==
           pc2ps3::FileSystemStatus::kOk);
    CHECK(file_system.remove("saves/slot.pc2save") ==
           pc2ps3::FileSystemStatus::kNotFound);
    CHECK(file_system.remove("../asset.bin") ==
           pc2ps3::FileSystemStatus::kError);

    const std::wstring linked_path = temporary.path() + L"\\linked.txt";
    const bool file_symlinks_available =
        create_file_symlink(linked_path, temporary.outside_file());
    if (file_symlinks_available) {
        CHECK(file_system.read_all("linked.txt", 1024, bytes) ==
               pc2ps3::FileSystemStatus::kError);
        CHECK(file_system.remove("linked.txt") ==
               pc2ps3::FileSystemStatus::kError);
        CHECK(DeleteFileW(linked_path.c_str()));
    } else {
        std::cout << "File symlink scenario skipped: host denied creation.\n";
    }

    const std::wstring hardlink_path =
        temporary.path() + L"\\hardlinked.txt";
    if (CreateHardLinkW(hardlink_path.c_str(),
                        temporary.outside_file().c_str(), NULL)) {
        CHECK(file_system.read_all("hardlinked.txt", 1024, bytes) ==
              pc2ps3::FileSystemStatus::kError);
        const std::uint8_t attempted[] = {'x'};
        CHECK(file_system.write_atomic("hardlinked.txt", attempted,
                                       sizeof(attempted)) ==
              pc2ps3::FileSystemStatus::kError);
        CHECK(file_system.remove("hardlinked.txt") ==
              pc2ps3::FileSystemStatus::kError);
        std::string external_contents;
        CHECK(read_native_file(temporary.outside_file(), &external_contents));
        CHECK(external_contents == "outside");
    } else {
        std::cout << "Hard-link scenario skipped: host denied creation.\n";
    }

    const std::wstring linked_directory = temporary.path() + L"\\linked_dir";
    const bool junction_created =
        create_directory_junction(linked_directory,
                                  temporary.outside_directory());
    if (junction_created) {
        CHECK(file_system.read_all("linked_dir/secret.txt", 1024, bytes) ==
              pc2ps3::FileSystemStatus::kError);
        CHECK(RemoveDirectoryW(linked_directory.c_str()));
    } else {
        const bool directory_symlink_created =
            create_directory_symlink(linked_directory,
                                     temporary.outside_directory());
        if (directory_symlink_created) {
            CHECK(file_system.read_all("linked_dir/secret.txt", 1024, bytes) ==
                  pc2ps3::FileSystemStatus::kError);
            CHECK(RemoveDirectoryW(linked_directory.c_str()));
        } else {
            std::cout << "Directory reparse scenario skipped: host denied "
                         "creation.\n";
        }
    }

    std::string alias;
    CHECK(to_utf8(temporary.alias_path(), &alias));
    DWORD alias_flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    alias_flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
    const bool alias_created = CreateSymbolicLinkW(
        temporary.alias_path().c_str(), temporary.path().c_str(), alias_flags) !=
                               FALSE;
    if (alias_created) {
        pc2ps3::HostFileSystem alias_file_system(alias);
        CHECK(!alias_file_system.valid());
        CHECK(RemoveDirectoryW(temporary.alias_path().c_str()));
    }

    std::cout << "HostFileSystem bounds, root confinement, atomic writes, and "
                 "removal passed.\n";
    return 0;
}
