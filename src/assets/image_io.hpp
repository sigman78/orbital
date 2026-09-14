#pragma once
#include "assets/image.hpp"
#include <filesystem>
#include <optional>

namespace space::assets {

// Decodes a PNG file into RGBA8. Grayscale and RGB sources are expanded and a
// missing alpha channel becomes 255. try_load_png reports failures by logging
// and returning nullopt; load_png panics, for assets the demo cannot run without.
std::optional<Rgba8Image> try_load_png(const std::filesystem::path& path);
Rgba8Image load_png(const std::filesystem::path& path);

// Encodes a checked pixel view as PNG, creating parent directories as needed.
// Returns false (after logging) if dimensions exceed encoder limits or writing fails.
bool save_png(const std::filesystem::path& path, ImageView image);

} // namespace space::assets
