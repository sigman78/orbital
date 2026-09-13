#pragma once
#include "assets/materials.hpp"
#include <array>
#include <string_view>
namespace space::assets {
struct MaterialSource {
    std::string_view file;
    MaterialDesc desc;
};
inline constexpr std::array material_catalog = {
    MaterialSource{"earth_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"gas_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"earth_clouds.png", {.luminance_to_alpha = true}},
    MaterialSource{"earth_normal.png", {.normal_map = true}},
    MaterialSource{"earth_specular.png", {}},
    MaterialSource{"earth_night.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"moon_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"rock_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"rock_normal.png", {.normal_map = true}},
    MaterialSource{"rock_roughness.png", {}},
    MaterialSource{"mars_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"mars_normal.png", {.normal_map = true}},
    MaterialSource{"moon_normal.png", {.normal_map = true}},
    MaterialSource{"rock_face_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"rock_face_normal.png", {.normal_map = true}},
    MaterialSource{"rock_face_roughness.png", {}},
    MaterialSource{"rock_boulder_albedo.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"rock_boulder_normal.png", {.normal_map = true}},
    MaterialSource{"rock_boulder_roughness.png", {}},
    MaterialSource{"gas_flow.png", {}},
    MaterialSource{"gas_detail.png", {}},
    MaterialSource{"gas_relief.png", {}},
    MaterialSource{"gas_polar.png", {.encoding = MaterialEncoding::SRGB}},
    MaterialSource{"gas_polar_flow.png", {}},
    MaterialSource{"galaxy_low.png", {}},
    MaterialSource{"galaxy_clouds.png", {}},
    MaterialSource{"galaxy_filaments.png", {}}};
inline MaterialDesc material_description(std::string_view file) {
    for (const auto& source : material_catalog)
        if (source.file == file)
            return source.desc;
    return {}; // Unlisted data maps are linear; batch compression uses only the catalog.
}
} // namespace space::assets
