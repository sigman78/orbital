#include "core/file.hpp"

#include <cstdio>
#include <string>

namespace space::file {
namespace {

struct File {
    std::FILE* handle = nullptr;
    File(const std::filesystem::path& path, const char* mode) {
#if defined(_WIN32)
        const std::wstring wide_mode(mode, mode + std::char_traits<char>::length(mode));
        if (_wfopen_s(&handle, path.c_str(), wide_mode.c_str()) != 0)
            handle = nullptr;
#else
        handle = std::fopen(path.c_str(), mode);
#endif
    }
    ~File() {
        if (handle)
            std::fclose(handle);
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    explicit operator bool() const { return handle != nullptr; }
    bool close() {
        auto* closing = handle;
        handle = nullptr;
        return !closing || std::fclose(closing) == 0;
    }
};

} // namespace

std::optional<Bytes> read(const std::filesystem::path& path) {
    File file(path, "rb");
    if (!file)
        return std::nullopt;
    if (std::fseek(file.handle, 0, SEEK_END) != 0)
        return std::nullopt;
    const long size = std::ftell(file.handle);
    if (size < 0 || std::fseek(file.handle, 0, SEEK_SET) != 0)
        return std::nullopt;
    Bytes bytes(static_cast<std::size_t>(size));
    if (!bytes.empty() && std::fread(bytes.data(), 1, bytes.size(), file.handle) != bytes.size())
        return std::nullopt;
    return bytes;
}

bool write(const std::filesystem::path& path, ByteView bytes) {
    std::error_code error;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;
    File file(path, "wb");
    if (!file)
        return false;
    const bool written = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), file.handle) == bytes.size();
    const bool closed = file.close();
    return written && closed;
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
    return write(path, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

} // namespace space::file
