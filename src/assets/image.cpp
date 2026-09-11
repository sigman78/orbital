#include "image.hpp"

#include <climits>
#include <fstream>
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_image.h>
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace space::assets {
namespace {

std::vector<unsigned char> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open image file: " + path.string());
    const auto size = file.tellg();
    if (size <= 0 || size > INT_MAX)
        throw std::runtime_error("Image file is empty or too large: " + path.string());
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file)
        throw std::runtime_error("Cannot read image file: " + path.string());
    return bytes;
}

} // namespace

Image load_png(const std::filesystem::path& path) {
    const auto bytes = read_file(path);
    int width = 0, height = 0, channels = 0;
    unsigned char* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height,
                                                   &channels, 4);
    if (!decoded)
        throw std::runtime_error("PNG decode failed for '" + path.string() + "': " + stbi_failure_reason());
    Image image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
    image.pixels.assign(decoded, decoded + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(decoded);
    return image;
}

void save_png(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height, unsigned channels,
              const std::uint8_t* pixels) {
    if (!width || !height || channels < 1 || channels > 4 || !pixels)
        throw std::invalid_argument("save_png: invalid image description");
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot create image file: " + path.string());
    const auto write = [](void* context, void* data, int size) {
        static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), size);
    };
    const int stride = static_cast<int>(width * channels);
    if (!stbi_write_png_to_func(write, &file, static_cast<int>(width), static_cast<int>(height),
                                static_cast<int>(channels), pixels, stride) ||
        !file)
        throw std::runtime_error("PNG encode failed: " + path.string());
}

} // namespace space::assets
