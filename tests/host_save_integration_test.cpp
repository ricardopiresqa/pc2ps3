#include "pc2ps3/host_file_system.hpp"
#include "pc2ps3/storage.hpp"

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <iostream>
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

class TempDirectory {
public:
    TempDirectory() : valid_(false) {
        wchar_t current[MAX_PATH] = {};
        const DWORD length = GetCurrentDirectoryW(MAX_PATH, current);
        if (length == 0 || length >= MAX_PATH) {
            return;
        }
        wchar_t unique[MAX_PATH] = {};
        if (GetTempFileNameW(current, L"p2s", 0, unique) == 0) {
            return;
        }
        if (!DeleteFileW(unique) || !CreateDirectoryW(unique, NULL)) {
            DeleteFileW(unique);
            return;
        }
        path_ = unique;
        valid_ = true;
    }

    ~TempDirectory() {
        if (!valid_) {
            return;
        }
        DeleteFileW((path_ + L"\\saves\\slot_1.pc2save").c_str());
        RemoveDirectoryW((path_ + L"\\saves").c_str());
        RemoveDirectoryW(path_.c_str());
    }

    bool valid() const { return valid_; }
    const std::wstring& path() const { return path_; }

private:
    bool valid_;
    std::wstring path_;
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

}  // namespace

int main() {
    TempDirectory temporary;
    CHECK(temporary.valid());
    CHECK(CreateDirectoryW((temporary.path() + L"\\saves").c_str(), NULL));

    std::string root;
    CHECK(to_utf8(temporary.path(), &root));
    pc2ps3::HostFileSystem file_system(root);
    CHECK(file_system.valid());

    const std::vector<std::uint8_t> expected = {1, 4, 9, 16, 25, 36};
    {
        pc2ps3::SaveManager writer(file_system, "saves", 3, 64);
        CHECK(writer.valid());
        CHECK(writer.save(1, expected) == pc2ps3::SaveStatus::kOk);
    }

    {
        pc2ps3::SaveManager reader(file_system, "saves", 3, 64);
        CHECK(reader.valid());
        std::vector<std::uint8_t> loaded(1, 0xeeu);
        CHECK(reader.load(1, loaded) == pc2ps3::SaveStatus::kOk);
        CHECK(loaded == expected);
    }

    const std::uint8_t corrupt_record[] = {'n', 'o', 't', 'a', 's', 'a', 'v', 'e'};
    CHECK(file_system.write_atomic("saves/slot_1.pc2save", corrupt_record,
                                   sizeof(corrupt_record)) ==
          pc2ps3::FileSystemStatus::kOk);
    {
        pc2ps3::SaveManager reader(file_system, "saves", 3, 64);
        std::vector<std::uint8_t> unchanged(1, 0xa5u);
        CHECK(reader.load(1, unchanged) == pc2ps3::SaveStatus::kCorruptData);
        CHECK(unchanged.size() == 1 && unchanged[0] == 0xa5u);
    }

    std::cout << "SaveManager disk round-trip and corruption handling passed.\n";
    return 0;
}
