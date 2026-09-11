#pragma once

#include "assets.hpp"
#include <filesystem>
#include <vector>

namespace space::assets {

enum class MaterialEncoding { SRGB, Linear };

// Loads PNG/JPEG through the native Windows Imaging Component and returns a
// complete RGBA8_UNORM mip chain. SRGB inputs (normally albedo/color maps) are
// converted to linear-light bytes before storage; normal, specular, roughness,
// height, and mask maps should use Linear. With luminance_to_alpha, RGB is set
// to white and source luminance becomes alpha (the expected cloud-mask layout).
// normal_map renormalizes RGB vectors at every mip level. Roughness convention:
// 0 is smooth and 255 is rough.
std::vector<Image> load_material(const std::filesystem::path& path, MaterialEncoding encoding,
                                 bool luminance_to_alpha = false, bool normal_map = false);

} // namespace space::assets
