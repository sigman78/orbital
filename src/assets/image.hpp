#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace space::assets {

struct Image {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> pixels; // Tightly packed RGBA8.
};

// Decodes a PNG file into RGBA8. Grayscale and RGB sources are expanded and a
// missing alpha channel becomes 255. Throws std::runtime_error on failure.
Image load_png(const std::filesystem::path& path);

// Encodes tightly packed 8-bit pixels with 1 to 4 channels as PNG, creating
// parent directories as needed. Throws std::runtime_error on failure.
void save_png(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height, unsigned channels,
              const std::uint8_t* pixels);

} // namespace space::assets
