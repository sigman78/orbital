#pragma once

#include "assets/image.hpp"
#include <filesystem>
#include <vector>

namespace space::assets {

enum class MaterialEncoding { SRGB, Linear };

// How a source map is turned into upload data. SRGB inputs (normally
// albedo/color maps) are converted to linear-light bytes before filtering;
// normal, specular, roughness, height, and mask maps use Linear. With
// luminance_to_alpha, RGB is set to white and source luminance becomes alpha
// (the expected cloud-mask layout). normal_map renormalizes RGB vectors at
// every mip level. Roughness convention: 0 is smooth and 255 is rough.
struct MaterialDesc {
    MaterialEncoding encoding = MaterialEncoding::Linear;
    bool luminance_to_alpha = false;
    bool normal_map = false;
};

// Prepares an already decoded image with the same conversion and mip rules.
MipChain prepare_material(Image base, const MaterialDesc& desc);

// Loads a PNG material and returns its complete RGBA8_UNORM mip chain down to
// 1x1. Panics if the file is missing or unreadable.
MipChain load_material(const std::filesystem::path& path, const MaterialDesc& desc);

} // namespace space::assets
