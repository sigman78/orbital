#include "planet/procedural_provider.hpp"

#include "core/panic.hpp"
#include "planet/tile_sampling.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace planet {

bool ProceduralProvider::produce(const TileRequest& request, TileOutput& out) {
    for (unsigned i = 0; i < tile_layout.plane_count; i++) {
        ORBITAL_ASSERT(out.planes[i].size() == tile_layout.planes[i].bytes());
        ORBITAL_ASSERT(reinterpret_cast<std::uintptr_t>(out.planes[i].data()) % 4 == 0);
    }
    float heights[tile_side * tile_side];
    generate_height_tile(*terrain_, request.key, heights);
    // The albedo and slope planes start four-byte aligned, so the sampling writes them in place.
    generate_colour_tiles(
        *terrain_, request.key, {reinterpret_cast<std::uint8_t*>(out.planes[1].data()), out.planes[1].size()},
        {reinterpret_cast<std::uint16_t*>(out.planes[2].data()), out.planes[2].size() / 2}, request.detail);
    const Range<float> shell = MinorPlanetTerrain::shape.heights;
    std::uint16_t quantized[tile_side * tile_side];
    for (unsigned i = 0; i < tile_side * tile_side; i++)
        quantized[i] = std::uint16_t(std::clamp((heights[i] - shell.min) / shell.span(), 0.0f, 1.0f) * 65535.0f + .5f);
    std::memcpy(out.planes[0].data(), quantized, sizeof quantized);
    out.heights = height_tile_range(heights);
    return true;
}

} // namespace planet
