#include "planet/patch.hpp"
#include "core/noise.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace space;
using namespace planet;

// The warp lands on the unit sphere, and a key names the child it should.
void test_patches() {
    for (unsigned face = 0; face < 6; face++)
        for (double s = -1; s <= 1; s += .5)
            assert(std::abs(length(cube_direction(face, s, .25)) - 1) < 1e-12);
    const PatchKey left{2, 3, 4, 5}, right{2, 3, 5, 5};
    const PatchKey child{2, 4, 9, 11};
    assert(left.child(3) == child && left.packed() != right.packed());
}

void test_cube_coordinates() {
    for (unsigned face = 0; face < 6; face++)
        for (double s = -.9; s <= .9; s += .3)
            for (double t = -.9; t <= .9; t += .3) {
                const Vec3d d = cube_direction(face, s, t);
                const CubeCoord c = cube_coordinates(d);
                assert(c.face == face);
                assert(std::abs(c.s - s) < 1e-9 && std::abs(c.t - t) < 1e-9);
            }
    const CubeCoord c = cube_coordinates(normalized(Vec3d{1, .5, .3}));
    assert(c.face == 0); // +x dominant
}

// The support must hold every point of the shell, from every direction, without slack the
// cell does not require. A sphere is the comparison: how much of it the cell test removes.
void test_cell_support() {
    double worst_fill = 0, tightest = 1e9, best_gain = 0, mean_gain = 0, worst_deep = 0;
    unsigned sampled = 0;
    std::uint64_t rng = 3;
    for (unsigned level = 0; level <= 6; level++) {
        const unsigned cells = 1u << level, step = std::max(1u, cells / 4);
        for (unsigned face = 0; face < 6; face++)
            for (unsigned cx = 0; cx < cells; cx += step)
                for (unsigned cy = 0; cy < cells; cy += step) {
                    const PatchKey k{std::uint8_t(face), std::uint8_t(level), std::uint16_t(cx), std::uint16_t(cy)};
                    const PatchBounds bounds = patch_bounds(k);
                    const double size = 2.0 / cells;
                    // Both the global shell and a tight interval well off the reference
                    // surface, which is where centring the sphere on it used to cost.
                    for (Range<float> heights :
                         {Range<float>{shell_height_min, shell_height_max}, Range<float>{-.031f, -.029f}}) {
                        // A sphere over the same shell, to measure what the cell removes.
                        const double top = 1 + double(heights.max);
                        const double bottom = 1 + double(heights.min) - patch_skirt_drop;
                        const double mid = (top + bottom) * .5;
                        const auto chord = [&](double r) {
                            const double cos_radius = std::cos(bounds.angular_radius);
                            return std::sqrt(std::max(0.0, r * r + mid * mid - 2 * r * mid * cos_radius));
                        };
                        const double ball = std::max(chord(top), chord(bottom));
                        for (unsigned trial = 0; trial < 8; trial++) {
                            const double z = uniform(rng) * 2 - 1, phi = uniform(rng) * 2 * pi<double>;
                            const double s = std::sqrt(std::max(0.0, 1 - z * z));
                            const Vec3d n{s * std::cos(phi), z, s * std::sin(phi)};
                            const double support = patch_cell_support(bounds, n, heights);
                            double reach = -1e9;
                            for (unsigned i = 0; i <= 16; i++)
                                for (unsigned j = 0; j <= 16; j++) {
                                    const Vec3d d = cube_direction(face, -1 + cx * size + i * size / 16,
                                                                   -1 + cy * size + j * size / 16);
                                    for (double h : {top, bottom})
                                        reach = std::max(reach, dot(n, d * h));
                                }
                            assert(reach <= support + 1e-12); // conservative, from every side
                            worst_fill = std::max(worst_fill, support - reach);
                            tightest = std::min(tightest, support - reach);
                            if (level >= 3)
                                worst_deep = std::max(worst_deep, support - reach);
                            // The sphere's support from the same side, always the looser.
                            const double ball_support = dot(n, bounds.centre * mid) + ball;
                            assert(ball_support >= support - 1e-12);
                            best_gain = std::max(best_gain, ball_support - support);
                            mean_gain += ball_support - support;
                        }
                    }
                    sampled++;
                }
    }
    std::fprintf(stderr,
                 "tiles: %u patch bounds, support over the shell by %.2e to %.2e radii (%.2e from level 3 "
                 "down); a sphere would reach %.4f further, %.4f on average\n",
                 sampled, tightest, worst_fill, worst_deep, best_gain, mean_gain / std::max(sampled * 16u, 1u));
    // The support is exact over the cell, so the slack left is the 17 by 17 lattice below
    // missing the true maximum between its points: 5.6 degrees apart on a whole face, and
    // nothing from level 3 down. It touches zero from above, hence the epsilon. Ceilings,
    // not targets: the cull the bound buys is measured by test_cull_waste rather than here.
    assert(sampled > 100 && tightest >= -1e-9 && worst_fill < .01 && worst_deep < 1e-3);
    assert(best_gain > .1); // a sphere gives away this much reach at the coarse levels
}

// The instanced surface and skirt meshes must reproduce each quadrant of the old shared grid,
// including its diagonal and skirt winding. Surface indices stay in the original x/y orientation;
// only skirt ribbons rotate, since their topology has no surface diagonal to change.
void test_quadrant_mask() {
    const PatchGrid full = patch_grid();
    const PatchGrid surface = patch_quadrant_surface_grid();
    const PatchGrid skirts = patch_quadrant_skirt_grid();
    using Point = std::array<unsigned, 3>; // x, y, skirt flag
    using Triangle = std::array<Point, 3>;
    const auto point = [](const PatchVertex& v, unsigned x, unsigned y) -> Point { return {x, y, unsigned(v.skirt)}; };
    for (unsigned quadrant = 0; quadrant < 4; quadrant++) {
        const unsigned origin_x = quadrant & 1 ? 16 : 0;
        const unsigned origin_y = quadrant & 2 ? 16 : 0;
        std::vector<Triangle> expected_surface, actual_surface, expected_skirts, actual_skirts;
        for (std::size_t i = 0; i < full.indices.size(); i += 3) {
            const std::uint32_t a = full.indices[i], b = full.indices[i + 1], c = full.indices[i + 2];
            if (full.vertices[a].quadrant != quadrant || full.vertices[b].quadrant != quadrant ||
                full.vertices[c].quadrant != quadrant)
                continue;
            const bool skirt_triangle = full.vertices[a].skirt || full.vertices[b].skirt || full.vertices[c].skirt;
            auto& expected = skirt_triangle ? expected_skirts : expected_surface;
            expected.push_back({point(full.vertices[a], full.vertices[a].x, full.vertices[a].y),
                                point(full.vertices[b], full.vertices[b].x, full.vertices[b].y),
                                point(full.vertices[c], full.vertices[c].x, full.vertices[c].y)});
        }
        for (std::size_t i = 0; i < surface.indices.size(); i += 3) {
            Triangle triangle{};
            for (unsigned corner = 0; corner < 3; corner++) {
                const PatchVertex& v = surface.vertices[surface.indices[i + corner]];
                triangle[corner] = point(v, v.x + origin_x, v.y + origin_y);
            }
            actual_surface.push_back(triangle);
        }
        for (std::size_t i = 0; i < skirts.indices.size(); i += 3) {
            Triangle triangle{};
            for (unsigned corner = 0; corner < 3; corner++) {
                const PatchVertex& v = skirts.vertices[skirts.indices[i + corner]];
                unsigned x = v.x, y = v.y;
                if (quadrant == 1) {
                    x = 32 - v.y;
                    y = v.x;
                } else if (quadrant == 2) {
                    x = v.y;
                    y = 32 - v.x;
                } else if (quadrant == 3) {
                    x = 32 - v.x;
                    y = 32 - v.y;
                }
                triangle[corner] = point(v, x, y);
            }
            actual_skirts.push_back(triangle);
        }
        std::sort(expected_surface.begin(), expected_surface.end());
        std::sort(actual_surface.begin(), actual_surface.end());
        std::sort(expected_skirts.begin(), expected_skirts.end());
        std::sort(actual_skirts.begin(), actual_skirts.end());
        assert(actual_surface == expected_surface);
        assert(actual_skirts == expected_skirts);
        assert(expected_surface.size() == 512);
        assert(expected_skirts.size() == 64);
    }
    assert(surface.vertices.size() == 289 && surface.indices.size() == 1536);
    assert(skirts.vertices.size() == 68 && skirts.indices.size() == 192);
    std::printf("patch quadrant meshes: all four surfaces and skirts preserve original triangle topology\n");
}

void test_grid_mesh() {
    const PatchGrid grid = patch_grid();
    // Four quadrants of their own, each carrying the row and column it shares, and the skirt
    // ring split the same way: see patch_grid.
    constexpr unsigned span = (tile_side - 1) / 2 + 1;
    constexpr unsigned expected_vertices = 4 * span * span + 8 * span;
    constexpr unsigned expected_indices = ((tile_side - 1) * (tile_side - 1) + 4 * (tile_side - 1)) * 6;
    assert(grid.vertices.size() == expected_vertices);
    assert(grid.indices.size() == expected_indices);
    constexpr unsigned half = (tile_side - 1) / 2;
    for (const PatchVertex& v : grid.vertices) {
        assert(v.x <= tile_side - 1 && v.y <= tile_side - 1 && v.quadrant < 4);
        // A quadrant's copy lies in its own half, the row and column it shares included.
        assert(v.quadrant & 1 ? v.x >= half : v.x <= half);
        assert(v.quadrant >> 1 ? v.y >= half : v.y <= half);
    }
    for (const auto index : grid.indices)
        assert(index < expected_vertices);
    std::printf("patch grid: %zu vertices, %zu indices\n", grid.vertices.size(), grid.indices.size());
}

// Cube-neighbour mapping must be reciprocal and share the same physical edge, including corners.
void test_patch_neighbours() {
    for (unsigned level : {0u, 1u, 4u, patch_level_max}) {
        const unsigned cells = 1u << level;
        for (unsigned face = 0; face < 6; face++)
            for (unsigned along : {0u, cells / 2, cells - 1})
                for (unsigned side = 0; side < 4; side++) {
                    const PatchKey key{std::uint8_t(face), std::uint8_t(level),
                                       std::uint16_t(side < 2 ? (side == 0 ? 0 : cells - 1) : along),
                                       std::uint16_t(side >= 2 ? (side == 2 ? 0 : cells - 1) : along)};
                    const auto n = patch_neighbour(key, side);
                    const auto back = patch_neighbour(n.key, n.side);
                    assert(back.key == key && back.side == side);
                    const auto midpoint = [cells](PatchKey k, unsigned edge) {
                        return cube_direction(k.face,
                                              -1 + (k.x + (edge == 0   ? 0
                                                           : edge == 1 ? 1
                                                                       : .5)) *
                                                       2 / cells,
                                              -1 + (k.y + (edge == 2   ? 0
                                                           : edge == 3 ? 1
                                                                       : .5)) *
                                                       2 / cells);
                    };
                    assert(length(midpoint(key, side) - midpoint(n.key, n.side)) < 1e-12);
                }
    }
}

int main() {
    test_patch_neighbours();
    test_patches();
    test_cube_coordinates();
    test_cell_support();
    test_grid_mesh();
    test_quadrant_mask();

    return 0;
}
