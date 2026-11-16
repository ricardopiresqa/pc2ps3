#include "pc2ps3/host_file_system.hpp"
#include "pc2ps3/texture_manager.hpp"

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
        if (GetTempFileNameW(current, L"p2t", 0, unique) == 0) {
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
        DeleteFileW((path_ + L"\\assets\\red.png").c_str());
        RemoveDirectoryW((path_ + L"\\assets").c_str());
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

const std::uint8_t kRedOnePixelPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
    0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99,
    0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

}  // namespace

int main() {
    TempDirectory temporary;
    CHECK(temporary.valid());
    CHECK(CreateDirectoryW((temporary.path() + L"\\assets").c_str(), NULL));

    std::string root;
    CHECK(to_utf8(temporary.path(), &root));
    pc2ps3::HostFileSystem file_system(root);
    CHECK(file_system.valid());
    CHECK(file_system.write_atomic("assets/red.png", kRedOnePixelPng,
                                   sizeof(kRedOnePixelPng)) ==
          pc2ps3::FileSystemStatus::kOk);

    pc2ps3::TextureManager textures(file_system, 4, 1024, 16);
    CHECK(textures.valid());
    pc2ps3::TextureHandle first;
    pc2ps3::TextureHandle second;
    CHECK(textures.load("assets/red.png", first) == pc2ps3::TextureStatus::kOk);
    const pc2ps3::Texture* cached = textures.get(first);
    CHECK(cached != NULL && cached->width() == 1 && cached->height() == 1);
    const pc2ps3::Color* pixel = cached->pixel_at(0, 0);
    CHECK(pixel != NULL && pixel->r == 255 && pixel->g == 0 &&
          pixel->b == 0 && pixel->a == 255);

    CHECK(textures.load("assets/red.png", second) == pc2ps3::TextureStatus::kOk);
    CHECK(textures.get(second) == cached);
    CHECK(textures.unload(first) == pc2ps3::TextureStatus::kOk);
    CHECK(textures.get(second) == cached);
    CHECK(textures.unload(second) == pc2ps3::TextureStatus::kOk);

    pc2ps3::TextureHandle reloaded;
    CHECK(textures.load("assets/red.png", reloaded) ==
          pc2ps3::TextureStatus::kOk);
    CHECK(textures.get(reloaded) != NULL);
    CHECK(textures.unload(reloaded) == pc2ps3::TextureStatus::kOk);

    pc2ps3::TextureHandle invalid_path;
    CHECK(textures.load("../red.png", invalid_path) ==
          pc2ps3::TextureStatus::kInvalidArgument);
    CHECK(!invalid_path.valid());

    std::cout << "TextureManager disk PNG load, cache, and reload passed.\n";
    return 0;
}
