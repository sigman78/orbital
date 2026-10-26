#include "planet/tile_sampling.hpp"

#include "core/panic.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace planet {

using namespace space; // the host's math and noise vocabulary

float patch_error(const MinorPlanetTerrain& terrain, PatchKey key) {
    const PatchCell cell = patch_cell(key);
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre, bounds.angular_radius);
    constexpr unsigned quads = tile_side - 1;
    const auto point = [&](double x, double y) {
        const Vec3d d = cube_direction(key.face, cell.s0 + x * cell.size / quads, cell.t0 + y * cell.size / quads);
        return d * (1 + double(terrain.height(d, region)));
    };
    Vec3d vertices[tile_side * tile_side];
    for (unsigned y = 0; y < tile_side; y++)
        for (unsigned x = 0; x < tile_side; x++)
            vertices[y * tile_side + x] = point(x, y);
    double error = 0;
    for (unsigned y = 0; y < quads; y++)
        for (unsigned x = 0; x < quads; x++) {
            const Vec3d a = vertices[y * tile_side + x], b = vertices[y * tile_side + x + 1],
                        c = vertices[(y + 1) * tile_side + x + 1], d = vertices[(y + 1) * tile_side + x];
            // The mesh diagonal is a--c, not the four-corner bilinear interpolant. Sample
            // that diagonal and both triangle interiors in 3D, including sphere curvature.
            error = std::max({error, length(point(x + .5, y + .5) - (a + c) * .5),
                              length(point(x + 2. / 3, y + 1. / 3) - (a + b + c) * (1. / 3)),
                              length(point(x + 1. / 3, y + 2. / 3) - (a + c + d) * (1. / 3))});
        }
    return float(error);
}

void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out) {
    ORBITAL_ASSERT(out.size() == tile_side * tile_side);
    const PatchCell cell = patch_cell(key);
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre, bounds.angular_radius);
    constexpr unsigned quads = tile_side - 1;
    for (unsigned y = 0; y < tile_side; y++)
        for (unsigned x = 0; x < tile_side; x++) {
            const Vec3d d = cube_direction(key.face, cell.s0 + double(x) * cell.size / quads,
                                           cell.t0 + double(y) * cell.size / quads);
            out[y * tile_side + x] = terrain.height(d, region);
        }
}

void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail) {
    ORBITAL_ASSERT(albedo.size() == tile_colour_side * tile_colour_side * 4);
    ORBITAL_ASSERT(slope.size() == tile_colour_side * tile_colour_side * 2);
    const PatchCell cell = patch_cell(key);
    constexpr unsigned quads = tile_colour_side - 1, bordered = tile_colour_side + 2;
    const auto direction = [&](int x, int y) {
        return cube_ring_direction(key.face, cell.s0 + x * cell.size / quads, cell.t0 + y * cell.size / quads);
    };
    // A one-texel border gives adjacent tiles matching central differences.
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre,
                                                             bounds.angular_radius + 2 * bounds.angular_size / quads);
    const double nyquist = .5 * quads / bounds.angular_size;
    const MinorPlanetTerrain::DetailRegion detail_region =
        detail > 0
            ? terrain.detail_region(bounds.centre, bounds.angular_radius + 2 * bounds.angular_size / quads, nyquist)
            : MinorPlanetTerrain::DetailRegion{};
    std::vector<float> h(bordered * bordered);
    const auto at = [&](int x, int y) -> float& { return h[(y + 1) * bordered + (x + 1)]; };
    for (int y = -1; y <= int(tile_colour_side); y++)
        for (int x = -1; x <= int(tile_colour_side); x++) {
            const Vec3d d = direction(x, y);
            at(x, y) = terrain.height(d, region) + terrain.detail(d, detail_region, detail);
        }
    for (int y = 0; y < int(tile_colour_side); y++)
        for (int x = 0; x < int(tile_colour_side); x++) {
            const unsigned i = y * tile_colour_side + x;
            const Vec3d d = direction(x, y);
            const float hc = at(x, y);
            // Solve the gradient in the skewed grid basis.
            const Vec3d us = direction(x + 1, y) - direction(x - 1, y), vt = direction(x, y + 1) - direction(x, y - 1);
            const Vec3d u = normalized(us), v = normalized(vt);
            const double ds = (at(x + 1, y) - at(x - 1, y)) / length(us),
                         dt = (at(x, y + 1) - at(x, y - 1)) / length(vt);
            const double c = dot(u, v), det = std::max(1 - c * c, 1e-6);
            const Vec3d g = u * ((ds - c * dt) / det) + v * ((dt - c * ds) / det);
            const Vec3f a = terrain.albedo(d, hc, float(1 - dot(normalized(d - g), d)),
                                           region); // the slope term as the bake passes it
            const auto u8 = [](float v) { return std::uint8_t(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f)); };
            const std::size_t p = i * 4;
            albedo[p] = u8(a.x);
            albedo[p + 1] = u8(a.y);
            albedo[p + 2] = u8(a.z);
            albedo[p + 3] = 255;
            const TangentFrame frame = patch_tangent_frame(key.face, d);
            const auto u16 = [](double s) {
                return std::uint16_t(std::clamp(s / tile_slope_scale * .5 + .5, 0.0, 1.0) * 65535.0 + .5);
            };
            slope[i * 2] = u16(dot(g, frame.tangent));
            slope[i * 2 + 1] = u16(dot(g, frame.bitangent));
        }
}

} // namespace planet
