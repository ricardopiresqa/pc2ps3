#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "pc2ps3/host_file_system.hpp"

#ifdef _WIN32

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace pc2ps3 {
namespace {

const std::size_t kMaxLogicalPathBytes = 512;
const DWORD kIoChunkBytes = 1024u * 1024u;
volatile LONG g_temp_sequence = 0;

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE)
        : handle_(handle) {}
    ~ScopedHandle() { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    bool valid() const { return handle_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return handle_; }
    void reset(HANDLE handle) {
        reset();
        handle_ = handle;
    }
    void reset() {
        if (valid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

class TempFileCleanup {
public:
    TempFileCleanup() : path_(NULL), active_(false) {}
    ~TempFileCleanup() {
        if (active_ && path_) {
            DeleteFileW(path_->c_str());
        }
    }

    void activate(const std::wstring& path) {
        path_ = &path;
        active_ = true;
    }
    void release() { active_ = false; }

private:
    const std::wstring* path_;
    bool active_;
};

bool to_wide(const std::string& input, std::wstring* output) {
    if (!output || input.empty() ||
        input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const int input_size = static_cast<int>(input.size());
    const int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), input_size, NULL, 0);
    if (needed <= 0) {
        return false;
    }
    std::wstring replacement(static_cast<std::size_t>(needed), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                            input_size, &replacement[0], needed) != needed) {
        return false;
    }
    output->swap(replacement);
    return true;
}

bool final_path(HANDLE handle, std::wstring* output) {
    if (!output) {
        return false;
    }
    const DWORD needed = GetFinalPathNameByHandleW(
        handle, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (needed == 0) {
        return false;
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1u, L'\0');
    const DWORD written = GetFinalPathNameByHandleW(
        handle, &buffer[0], static_cast<DWORD>(buffer.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (written == 0 || written >= buffer.size()) {
        return false;
    }
    output->assign(&buffer[0], written);
    return true;
}

bool is_not_found_error(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

bool has_prefix_case_insensitive(const std::wstring& value,
                                 const std::wstring& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    return CompareStringOrdinal(value.data(), static_cast<int>(prefix.size()),
                                prefix.data(), static_cast<int>(prefix.size()),
                                TRUE) == CSTR_EQUAL;
}

bool is_within_root(const std::wstring& path, const std::wstring& root) {
    if (CompareStringOrdinal(path.data(), static_cast<int>(path.size()),
                             root.data(), static_cast<int>(root.size()), TRUE) ==
        CSTR_EQUAL) {
        return true;
    }
    std::wstring prefix = root;
    if (prefix.empty() || prefix[prefix.size() - 1] != L'\\') {
        prefix.push_back(L'\\');
    }
    return path.size() > prefix.size() &&
           has_prefix_case_insensitive(path, prefix);
}

bool parse_logical_path(const std::string& path,
                        std::vector<std::string>* components) {
    if (!components || path.empty() || path.size() > kMaxLogicalPathBytes ||
        path[0] == '/' || path[0] == '\\' ||
        path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos ||
        path.find('\0') != std::string::npos) {
        return false;
    }

    std::vector<std::string> replacement;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end =
            slash == std::string::npos ? path.size() : slash;
        const std::string part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
        replacement.push_back(part);
        if (slash == std::string::npos) {
            start = path.size();
        } else {
            start = slash + 1;
            if (start == path.size()) {
                return false;
            }
        }
    }
    if (replacement.empty()) {
        return false;
    }
    components->swap(replacement);
    return true;
}

bool get_attributes(HANDLE handle, BY_HANDLE_FILE_INFORMATION* info) {
    return info && GetFileInformationByHandle(handle, info) != FALSE;
}

}  // namespace

HostFileSystem::HostFileSystem(const std::string& root_path)
    : root_path_(), valid_(false) {
    std::wstring requested_root;
    if (!to_wide(root_path, &requested_root)) {
        return;
    }

    const DWORD capacity = GetFullPathNameW(requested_root.c_str(), 0, NULL, NULL);
    if (capacity == 0) {
        return;
    }
    std::vector<wchar_t> full_path(static_cast<std::size_t>(capacity) + 1u, L'\0');
    const DWORD written =
        GetFullPathNameW(requested_root.c_str(), static_cast<DWORD>(full_path.size()),
                         &full_path[0], NULL);
    if (written == 0 || written >= full_path.size()) {
        return;
    }

    ScopedHandle root(CreateFileW(
        &full_path[0], FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL));
    if (!root.valid()) {
        return;
    }

    BY_HANDLE_FILE_INFORMATION info;
    std::wstring canonical_root;
    const bool safe_root =
        get_attributes(root.get(), &info) &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
        final_path(root.get(), &canonical_root);
    if (!safe_root) {
        return;
    }

    while (canonical_root.size() > 7u &&
           canonical_root[canonical_root.size() - 1] == L'\\') {
        canonical_root.erase(canonical_root.size() - 1);
    }
    root_path_.swap(canonical_root);
    valid_ = true;
}

HostFileSystem::~HostFileSystem() {}

bool HostFileSystem::valid() const {
    return valid_;
}

FileSystemStatus HostFileSystem::read_all(
    const std::string& path, std::size_t max_bytes,
    std::vector<std::uint8_t>& output) try {
    if (!valid_) {
        return FileSystemStatus::kError;
    }
    std::vector<std::string> components;
    if (!parse_logical_path(path, &components)) {
        return FileSystemStatus::kError;
    }

    std::wstring current = root_path_;
    for (std::size_t i = 0; i < components.size(); ++i) {
        std::wstring part;
        if (!to_wide(components[i], &part)) {
            return FileSystemStatus::kError;
        }
        current.push_back(L'\\');
        current.append(part);

        const bool is_leaf = i + 1 == components.size();
        ScopedHandle component(CreateFileW(
            current.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT |
                (is_leaf ? FILE_ATTRIBUTE_NORMAL : FILE_FLAG_BACKUP_SEMANTICS),
            NULL));
        if (!component.valid()) {
            return is_not_found_error(GetLastError()) ? FileSystemStatus::kNotFound
                                                      : FileSystemStatus::kError;
        }
        BY_HANDLE_FILE_INFORMATION info;
        std::wstring resolved;
        const bool safe =
            get_attributes(component.get(), &info) &&
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
            (is_leaf
                 ? (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                 : (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) &&
            final_path(component.get(), &resolved) &&
            is_within_root(resolved, root_path_);
        if (!safe) {
            return FileSystemStatus::kError;
        }
    }

    ScopedHandle file(CreateFileW(
        current.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL));
    if (!file.valid()) {
        return is_not_found_error(GetLastError()) ? FileSystemStatus::kNotFound
                                                  : FileSystemStatus::kError;
    }

    BY_HANDLE_FILE_INFORMATION info;
    std::wstring resolved;
    LARGE_INTEGER file_size = {};
    const bool safe_file =
        get_attributes(file.get(), &info) &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
        info.nNumberOfLinks == 1 &&
        final_path(file.get(), &resolved) &&
        is_within_root(resolved, root_path_) &&
        GetFileSizeEx(file.get(), &file_size) != FALSE &&
        file_size.QuadPart >= 0;
    if (!safe_file) {
        return FileSystemStatus::kError;
    }
    const unsigned long long size =
        static_cast<unsigned long long>(file_size.QuadPart);
    if (size > static_cast<unsigned long long>(max_bytes) ||
        size > static_cast<unsigned long long>(output.max_size())) {
        return FileSystemStatus::kTooLarge;
    }

    try {
        std::vector<std::uint8_t> replacement(static_cast<std::size_t>(size));
        std::size_t offset = 0;
        while (offset < replacement.size()) {
            const DWORD request = static_cast<DWORD>(
                (std::min)(replacement.size() - offset,
                           static_cast<std::size_t>(kIoChunkBytes)));
            DWORD read_bytes = 0;
            if (!ReadFile(file.get(), &replacement[offset], request, &read_bytes,
                          NULL) ||
                read_bytes == 0) {
                return FileSystemStatus::kError;
            }
            offset += read_bytes;
        }

        LARGE_INTEGER final_size;
        const bool unchanged =
            GetFileSizeEx(file.get(), &final_size) != FALSE &&
            final_size.QuadPart == file_size.QuadPart;
        if (!unchanged) {
            return FileSystemStatus::kError;
        }
        output.swap(replacement);
        return FileSystemStatus::kOk;
    } catch (const std::bad_alloc&) {
        return FileSystemStatus::kError;
    } catch (const std::length_error&) {
        return FileSystemStatus::kTooLarge;
    }
} catch (...) {
    return FileSystemStatus::kError;
}

FileSystemStatus HostFileSystem::write_atomic(
    const std::string& path, const std::uint8_t* data,
    std::size_t size) try {
    if (!valid_ || (size != 0 && data == NULL)) {
        return FileSystemStatus::kError;
    }
    std::vector<std::string> components;
    if (!parse_logical_path(path, &components)) {
        return FileSystemStatus::kError;
    }

    std::wstring target = root_path_;
    std::vector<std::wstring> wide_components;
    try {
        wide_components.reserve(components.size());
        for (std::size_t i = 0; i < components.size(); ++i) {
            std::wstring part;
            if (!to_wide(components[i], &part)) {
                return FileSystemStatus::kError;
            }
            wide_components.push_back(part);
            target.push_back(L'\\');
            target.append(part);
        }
    } catch (const std::bad_alloc&) {
        return FileSystemStatus::kError;
    }

    std::wstring parent = root_path_;
    for (std::size_t i = 0; i + 1 < wide_components.size(); ++i) {
        parent.push_back(L'\\');
        parent.append(wide_components[i]);
        ScopedHandle directory(CreateFileW(
            parent.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
        if (!directory.valid()) {
            return FileSystemStatus::kError;
        }
        BY_HANDLE_FILE_INFORMATION info;
        std::wstring resolved;
        const bool safe =
            get_attributes(directory.get(), &info) &&
            (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
            final_path(directory.get(), &resolved) &&
            is_within_root(resolved, root_path_);
        if (!safe) {
            return FileSystemStatus::kError;
        }
    }

    ScopedHandle parent_handle(CreateFileW(
        parent.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (!parent_handle.valid()) {
        return FileSystemStatus::kError;
    }
    BY_HANDLE_FILE_INFORMATION parent_info;
    std::wstring resolved_parent;
    const bool safe_parent =
        get_attributes(parent_handle.get(), &parent_info) &&
        (parent_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (parent_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
        final_path(parent_handle.get(), &resolved_parent) &&
        is_within_root(resolved_parent, root_path_);
    if (!safe_parent) {
        return FileSystemStatus::kError;
    }

    ScopedHandle existing_target(CreateFileW(
        target.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (existing_target.valid()) {
        BY_HANDLE_FILE_INFORMATION target_info;
        std::wstring resolved_target;
        const bool safe_target =
            get_attributes(existing_target.get(), &target_info) &&
            (target_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            (target_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
            target_info.nNumberOfLinks == 1 &&
            final_path(existing_target.get(), &resolved_target) &&
            is_within_root(resolved_target, root_path_);
        if (!safe_target) {
            return FileSystemStatus::kError;
        }
    } else if (!is_not_found_error(GetLastError())) {
        return FileSystemStatus::kError;
    }
    existing_target.reset();

    std::wstring temporary;
    TempFileCleanup cleanup;
    ScopedHandle file;
    for (int attempt = 0; attempt < 32; ++attempt) {
        const LONG sequence = InterlockedIncrement(&g_temp_sequence);
        temporary = target + L".pc2ps3-" +
                    std::to_wstring(GetCurrentProcessId()) + L"-" +
                    std::to_wstring(sequence) + L".tmp";
        file.reset(CreateFileW(
            temporary.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, NULL,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL));
        if (file.valid()) {
            cleanup.activate(temporary);
            break;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            return FileSystemStatus::kError;
        }
    }
    if (!file.valid()) {
        return FileSystemStatus::kError;
    }

    BY_HANDLE_FILE_INFORMATION temp_info;
    std::wstring resolved_temp;
    const bool safe_temp =
        get_attributes(file.get(), &temp_info) &&
        (temp_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
        final_path(file.get(), &resolved_temp) &&
        is_within_root(resolved_temp, root_path_);
    if (!safe_temp) {
        file.reset();
        return FileSystemStatus::kError;
    }

    std::size_t offset = 0;
    while (offset < size) {
        const DWORD request = static_cast<DWORD>(
            (std::min)(size - offset, static_cast<std::size_t>(kIoChunkBytes)));
        DWORD written = 0;
        if (!WriteFile(file.get(), data + offset, request, &written, NULL) ||
            written == 0) {
            file.reset();
            return FileSystemStatus::kError;
        }
        offset += written;
    }
    if (!FlushFileBuffers(file.get())) {
        file.reset();
        return FileSystemStatus::kError;
    }
    file.reset();

    const DWORD destination_attributes = GetFileAttributesW(target.c_str());
    const bool destination_exists =
        destination_attributes != INVALID_FILE_ATTRIBUTES;
    if (!destination_exists && !is_not_found_error(GetLastError())) {
        return FileSystemStatus::kError;
    }
    const DWORD move_flags =
        destination_exists ? MOVEFILE_REPLACE_EXISTING : 0;
    if (!MoveFileExW(temporary.c_str(), target.c_str(), move_flags)) {
        return FileSystemStatus::kError;
    }
    cleanup.release();
    return FileSystemStatus::kOk;
} catch (...) {
    return FileSystemStatus::kError;
}

FileSystemStatus HostFileSystem::remove(const std::string& path) try {
    if (!valid_) {
        return FileSystemStatus::kError;
    }
    std::vector<std::string> components;
    if (!parse_logical_path(path, &components)) {
        return FileSystemStatus::kError;
    }

    std::wstring current = root_path_;
    for (std::size_t i = 0; i < components.size(); ++i) {
        std::wstring part;
        if (!to_wide(components[i], &part)) {
            return FileSystemStatus::kError;
        }
        current.push_back(L'\\');
        current.append(part);
        const bool is_leaf = i + 1 == components.size();
        ScopedHandle component(CreateFileW(
            current.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT |
                (is_leaf ? FILE_ATTRIBUTE_NORMAL : FILE_FLAG_BACKUP_SEMANTICS),
            NULL));
        if (!component.valid()) {
            return is_not_found_error(GetLastError()) ? FileSystemStatus::kNotFound
                                                      : FileSystemStatus::kError;
        }
        BY_HANDLE_FILE_INFORMATION info;
        std::wstring resolved;
        const bool safe =
            get_attributes(component.get(), &info) &&
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
            (is_leaf
                 ? (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                 : (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) &&
            final_path(component.get(), &resolved) &&
            is_within_root(resolved, root_path_);
        if (!safe) {
            return FileSystemStatus::kError;
        }
    }

    ScopedHandle file(CreateFileW(
        current.c_str(), DELETE | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (!file.valid()) {
        return is_not_found_error(GetLastError()) ? FileSystemStatus::kNotFound
                                                  : FileSystemStatus::kError;
    }
    BY_HANDLE_FILE_INFORMATION info;
    std::wstring resolved;
    const bool safe =
        get_attributes(file.get(), &info) &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
        info.nNumberOfLinks == 1 &&
        final_path(file.get(), &resolved) &&
        is_within_root(resolved, root_path_);
    if (!safe) {
        return FileSystemStatus::kError;
    }
    FILE_DISPOSITION_INFO disposition;
    disposition.DeleteFile = TRUE;
    const bool deleted = SetFileInformationByHandle(
        file.get(), FileDispositionInfo, &disposition, sizeof(disposition)) !=
                   FALSE;
    return deleted ? FileSystemStatus::kOk : FileSystemStatus::kError;
} catch (...) {
    return FileSystemStatus::kError;
}

}  // namespace pc2ps3

#endif  // _WIN32
