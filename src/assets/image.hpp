#pragma once
#include "core/math.hpp"
#include "core/types.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace space::assets {

// Tightly packed RGBA8 image; the one owning pixel container in the code base.
struct Image {
    Extent2D extent{};
    Bytes pixels;

    std::size_t pixel_count() const { return static_cast<std::size_t>(extent.width) * extent.height; }
    ByteView bytes() const { return pixels; }
};

// Base level first, each level half the previous, down to 1x1.
using MipChain = std::vector<Image>;

// Decodes a PNG file into RGBA8. Grayscale and RGB sources are expanded and a
// missing alpha channel becomes 255. try_load_png reports failures by logging
// and returning nullopt; load_png panics, for assets the demo cannot run without.
std::optional<Image> try_load_png(const std::filesystem::path& path);
Image load_png(const std::filesystem::path& path);

// Encodes tightly packed 8-bit pixels with 1 to 4 channels as PNG, creating
// parent directories as needed. pixels must hold exactly
// extent.width * extent.height * channels bytes. Returns false (after logging)
// if the file could not be written.
bool save_png(const std::filesystem::path& path, Extent2D extent, unsigned channels, ByteView pixels);

} // namespace space::assets
