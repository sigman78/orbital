#pragma once
#include "planet/crater_terrain.hpp"
#include "planet/patch.hpp"

#include <cstdint>
#include <span>

namespace planet {

// Sampling the procedural terrain into a patch's tiles.

// Sampled 3D error against the grid's two triangles per quad, in radii (not a proven bound).
float patch_error(const MinorPlanetTerrain& terrain, PatchKey key);

// Outputs: tile_side squared heights; colour-side RGBA8 albedo and RG16_UNORM tangent slopes.
void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out);
void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail = 1);

} // namespace planet
