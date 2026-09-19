#include "scene/terrain.hpp"
#include "core/noise.hpp"
#include "scene/terrain_patch.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string_view>

using namespace space;

// The noise is bounded, zero-mean over many samples, and the same for the same seed.
void test_noise() {
    double sum = 0, peak = 0;
    for (int i = 0; i < 20000; i++) {
        const Vec3d p{i * .137, i * .071 + 3, i * .0193 - 7};
        const float n = gradient_noise(p, 42);
        assert(n == gradient_noise(p, 42) && std::abs(n) <= 1);
        sum += n;
        peak = std::max(peak, double(std::abs(n)));
    }
    assert(std::abs(sum / 20000) < .02 && peak > .5);
    assert(gradient_noise({.5, .5, .5}, 1) != gradient_noise({.5, .5, .5}, 2));
    assert(std::abs(fbm({1.3, 2.2, .7}, 4, 9)) <= 1);
    const float r = ridged_fbm({1.3, 2.2, .7}, 4, 9);
    assert(r >= 0 && r <= 1);
    std::uint64_t state = 7;
    const double u = uniform(state);
    assert(u >= 0 && u < 1 && uniform(state) != u && mix64(1) != mix64(2));
}

// The terrain is deterministic, within its stated range, and continuous: neighbouring
// directions differ by little.
void test_terrain() {
    const MinorPlanetTerrain a(1007), b(1007), c(1008);
    assert(a.craters().size() == b.craters().size() && !a.craters().empty());
    unsigned large = 0, faculae = 0;
    for (const auto& crater : a.craters()) {
        assert(crater.radius >= .018f && crater.radius <= .17f && crater.depth > 0);
        large += crater.radius > .08f;
        faculae += crater.facula > 0;
    }
    assert(large > 5 && large < 60 && faculae > 0); // many small, a few large, a few bright
    float low = 1, high = -1, largest_step = 0;
    unsigned differs = 0;
    for (int i = 0; i < 4000; i++) {
        const double z = 2 * (i + .5) / 4000 - 1, phi = 2.399963 * i, s = std::sqrt(1 - z * z);
        const Vec3d d{s * std::cos(phi), z, s * std::sin(phi)};
        const float h = a.height(d);
        assert(h == b.height(d) && h >= MinorPlanetTerrain::height_min && h <= MinorPlanetTerrain::height_max);
        if (h != c.height(d))
            differs++;
        low = std::min(low, h);
        high = std::max(high, h);
        const Vec3d near = normalized(d + Vec3d{1e-4, 0, 0});
        largest_step = std::max(largest_step, std::abs(a.height(near) - h));
        const Vec3f albedo = a.albedo(d, h, .3f);
        assert(albedo.x > 0 && albedo.x <= 1 && albedo.y > 0 && albedo.z > 0);
    }
    std::printf("terrain: heights %g to %g radii, largest step over 1e-4 rad %g, %u of 4000 differ by seed\n",
                double(low), double(high), double(largest_step), differs);
    assert(low < -.005f && high > .01f); // terrain on both sides of the sphere
    assert(largest_step < 2e-3f);        // continuous at the sampling scale of the bake
    assert(differs > 3900);              // a different seed is a different body
}

// A small bake carries the height in the normal map's alpha and an upward normal.
void test_bake() {
    const MinorPlanetTerrain terrain(1007);
    const TerrainMaps maps = bake_terrain_maps(terrain, 64, 32);
    assert(maps.width == 64 && maps.height == 32 && maps.albedo.size() == 64 * 32 * 4 &&
           maps.normal.size() == maps.albedo.size());
    const float range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
    for (unsigned y = 0; y < 32; y++)
        for (unsigned x = 0; x < 64; x++) {
            const std::size_t p = (y * 64 + x) * 4;
            const double angle = ((x + .5) / 64 - .5) * 2 * pi<double>, latitude = (.5 - (y + .5) / 32) * pi<double>;
            const Vec3d d{std::cos(latitude) * std::cos(angle), std::sin(latitude),
                          std::cos(latitude) * std::sin(angle)};
            const float expected = (terrain.height(d) - MinorPlanetTerrain::height_min) / range * 255;
            assert(std::abs(maps.normal[p + 3] - expected) <= 1);
            assert(maps.normal[p + 2] > 128 && maps.albedo[p + 3] == 255 && maps.albedo[p] > 0);
        }
}

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

void test_tiles() {
    const MinorPlanetTerrain terrain(1007);
    std::vector<float> a(tile_side * tile_side), b(tile_side * tile_side);
    const PatchKey key{2, 3, 4, 5};
    generate_height_tile(terrain, key, a);
    generate_height_tile(terrain, key, b);
    for (unsigned i = 0; i < a.size(); i++)
        assert(a[i] == b[i]);
    for (float h : a)
        assert(h >= MinorPlanetTerrain::height_min && h <= MinorPlanetTerrain::height_max);
    // Even child texels coincide with parent texels.
    std::vector<float> parent(tile_side * tile_side), child(tile_side * tile_side);
    const PatchKey parent_key{0, 2, 1, 1};
    generate_height_tile(terrain, parent_key, parent);
    generate_height_tile(terrain, parent_key.child(0), child);
    unsigned mismatches = 0;
    for (unsigned y = 0; y < tile_side; y += 2)
        for (unsigned x = 0; x < tile_side; x += 2) {
            const float ph = parent[y / 2 * tile_side + x / 2];
            const float ch = child[y * tile_side + x];
            if (std::abs(ph - ch) > 1.0f / 65535)
                mismatches++;
        }
    std::printf("tiles: parent-child even-texel mismatches %u of %u\n", mismatches,
                (tile_side / 2 + 1) * (tile_side / 2 + 1));
    assert(mismatches == 0);
    const PatchKey left_key{2, 3, 4, 5}, right_key{2, 3, 5, 5};
    std::vector<float> lh(tile_side * tile_side), rh(tile_side * tile_side);
    generate_height_tile(terrain, left_key, lh);
    generate_height_tile(terrain, right_key, rh);
    for (unsigned y = 0; y < tile_side; y++)
        assert(lh[y * tile_side + (tile_side - 1)] == rh[y * tile_side]);
    // Adjacent tiles must share identical edge texels.
    const auto tile_normal = [](const std::uint16_t* slope, unsigned face, Vec3d d) {
        const TangentFrame frame = patch_tangent_frame(face, d);
        const auto s = [&](unsigned c) { return (slope[c] / 65535.0 * 2 - 1) * tile_slope_scale; };
        return normalized(d - frame.tangent * s(0) - frame.bitangent * s(1));
    };
    constexpr unsigned colour_quads = tile_colour_side - 1;
    std::vector<std::uint8_t> albedo(tile_colour_side * tile_colour_side * 4);
    std::vector<std::uint16_t> norm(tile_colour_side * tile_colour_side * 2);
    generate_colour_tiles(terrain, key, albedo, norm);
    for (unsigned i = 0; i < tile_colour_side * tile_colour_side; i++) {
        assert(albedo[i * 4 + 3] == 255);
        const double size = 2.0 / double(1u << key.level);
        const Vec3d d = cube_direction(key.face, -1 + key.x * size + (i % tile_colour_side) * size / colour_quads,
                                       -1 + key.y * size + (i / tile_colour_side) * size / colour_quads);
        assert(dot(tile_normal(&norm[i * 2], key.face, d), d) > .7);
    }
    std::vector<std::uint8_t> ra(albedo.size());
    std::vector<std::uint16_t> rn(norm.size());
    generate_colour_tiles(terrain, right_key, ra, rn);
    generate_colour_tiles(terrain, left_key, albedo, norm);
    for (unsigned y = 0; y < tile_colour_side; y++) {
        for (unsigned c = 0; c < 4; c++)
            assert(albedo[(y * tile_colour_side + tile_colour_side - 1) * 4 + c] == ra[(y * tile_colour_side) * 4 + c]);
        for (unsigned c = 0; c < 2; c++)
            assert(norm[(y * tile_colour_side + tile_colour_side - 1) * 2 + c] == rn[(y * tile_colour_side) * 2 + c]);
    }
    // Cube-edge slopes differ by face frame; compare decoded normals, excluding corners.
    {
        const auto world_normal = [&](unsigned face, unsigned x, unsigned y, const std::uint16_t* slope, PatchKey key) {
            const double size = 2.0 / double(1u << key.level);
            const Vec3d d = cube_direction(face, -1 + key.x * size + x * size / (tile_colour_side - 1),
                                           -1 + key.y * size + y * size / (tile_colour_side - 1));
            return tile_normal(slope, face, d);
        };
        // Match shared texels by direction across faces.
        const auto colour_direction = [](PatchKey k, unsigned x, unsigned y) {
            constexpr unsigned q = tile_colour_side - 1;
            const double size = 2.0 / double(1u << k.level);
            return cube_direction(k.face, -1 + k.x * size + x * size / q, -1 + k.y * size + y * size / q);
        };
        constexpr std::size_t colour = tile_colour_side * tile_colour_side;
        std::vector<std::uint8_t> a1(colour * 4), a2(colour * 4);
        std::vector<std::uint16_t> n1(colour * 2), n2(colour * 2);
        unsigned compared = 0;
        double worst = 0;
        for (unsigned f1 = 0; f1 < 6; f1++)
            for (unsigned f2 = f1 + 1; f2 < 6; f2++) {
                if (f2 == (f1 ^ 1)) // opposite faces share no edge
                    continue;
                for (unsigned i1 = 0; i1 < 4; i1++)
                    for (unsigned i2 = 0; i2 < 4; i2++) {
                        const PatchKey p1{std::uint8_t(f1), 1, std::uint16_t(i1 & 1), std::uint16_t(i1 >> 1)};
                        const PatchKey p2{std::uint8_t(f2), 1, std::uint16_t(i2 & 1), std::uint16_t(i2 >> 1)};
                        generate_colour_tiles(terrain, p1, a1, n1);
                        generate_colour_tiles(terrain, p2, a2, n2);
                        for (unsigned y1 = 0; y1 < tile_colour_side; y1 += 8)
                            for (unsigned x1 = 0; x1 < tile_colour_side; x1 += 8)
                                for (unsigned y2 = 0; y2 < tile_colour_side; y2 += 8)
                                    for (unsigned x2 = 0; x2 < tile_colour_side; x2 += 8) {
                                        const unsigned j1 = y1 * tile_colour_side + x1, j2 = y2 * tile_colour_side + x2;
                                        const bool edge_x = x1 == 0 || x1 == tile_colour_side - 1,
                                                   edge_y = y1 == 0 || y1 == tile_colour_side - 1;
                                        if (length(colour_direction(p1, x1, y1) - colour_direction(p2, x2, y2)) >
                                                1e-12 ||
                                            !(edge_x ^ edge_y))
                                            continue;
                                        for (unsigned c = 0; c < 4; c++)
                                            assert(a1[j1 * 4 + c] == a2[j2 * 4 + c]);
                                        const Vec3d w1 = world_normal(f1, x1, y1, &n1[j1 * 2], p1);
                                        const Vec3d w2 = world_normal(f2, x2, y2, &n2[j2 * 2], p2);
                                        worst = std::max(worst, length(w1 - w2));
                                        compared++;
                                    }
                    }
            }
        std::fprintf(stderr, "tiles: %u cube-edge texel pairs compared, world normals within %.4f\n", compared, worst);
        assert(compared > 100 && worst < 5e-4); // measured 1e-4, about two 16-bit slope codes on each side
    }
    // Regression: coordinate-magnitude ties at corners selected inconsistent derivative stencils.
    {
        struct Corner {
            Vec3d direction;
            unsigned face, x, y;
        };
        constexpr unsigned level = 2, cells = 1u << level, last = tile_colour_side - 1;
        std::vector<Corner> corners;
        for (unsigned face = 0; face < 6; face++)
            for (unsigned i = 0; i < 4; i++) {
                const double s = i & 1 ? 1 : -1, t = i >> 1 ? 1 : -1;
                corners.push_back({cube_direction(face, s, t), face, s > 0 ? cells - 1 : 0, t > 0 ? cells - 1 : 0});
            }
        std::vector<std::uint8_t> ca(tile_colour_side * tile_colour_side * 4);
        std::vector<std::uint16_t> cs(tile_colour_side * tile_colour_side * 2);
        const auto corner_normal = [&](const Corner& c) {
            const PatchKey ck{std::uint8_t(c.face), level, std::uint16_t(c.x), std::uint16_t(c.y)};
            generate_colour_tiles(terrain, ck, ca, cs);
            const unsigned tx = c.x ? last : 0, ty = c.y ? last : 0;
            return tile_normal(&cs[(ty * tile_colour_side + tx) * 2], c.face, c.direction);
        };
        unsigned compared = 0;
        double worst = 0;
        for (std::size_t i = 0; i < corners.size(); i++)
            for (std::size_t j = i + 1; j < corners.size(); j++) {
                if (length(corners[i].direction - corners[j].direction) > 1e-9)
                    continue;
                worst = std::max(worst, length(corner_normal(corners[i]) - corner_normal(corners[j])));
                compared++;
            }
        std::fprintf(stderr, "tiles: %u cube-corner face pairs compared, world normals within %.4f\n", compared, worst);
        assert(compared == 24 && worst < 5e-4); // three faces at each of the eight corners
    }
    // The support must hold every point of the shell, from every direction, without slack the
    // cell does not require. A sphere is the comparison: how much of it the cell test removes.
    {
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
                             {Range<float>{MinorPlanetTerrain::height_min, MinorPlanetTerrain::height_max},
                              Range<float>{-.031f, -.029f}}) {
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
    const float error = patch_error(terrain, key);
    assert(error > 0 && error < .01f);
    std::printf("tiles: patch error at level 3: %g radii\n", double(error));
}

// Minutes of work, and only needed when level_error is re-derived: --calibrate runs it.
void calibrate_level_errors() {
    const MinorPlanetTerrain terrain(1007);
    std::uint64_t rng = 42;
    std::printf("calibration: max patch_error per level, seed 1007, Everitt warp:\n");
    for (unsigned level = 0; level <= 12; level++) {
        const unsigned cells = 1u << level;
        // A deep cell covers so little of the body that a uniform sample of 64 misses the
        // relief entirely; take more, then climb to the local worst, which relief clusters near.
        const unsigned trials = level <= 8 ? 64 : 1024;
        float worst = 0;
        PatchKey peak{};
        for (unsigned trial = 0; trial < trials; trial++) {
            const unsigned face = unsigned(uniform(rng) * 6) % 6;
            const unsigned x = unsigned(uniform(rng) * cells) % cells;
            const unsigned y = unsigned(uniform(rng) * cells) % cells;
            const PatchKey key{std::uint8_t(face), std::uint8_t(level), std::uint16_t(x), std::uint16_t(y)};
            const float e = patch_error(terrain, key);
            if (e > worst) {
                worst = e;
                peak = key;
            }
        }
        const float sampled = worst;
        for (unsigned round = 0; round < 6; round++) {
            PatchKey best = peak;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    const int nx = int(peak.x) + dx, ny = int(peak.y) + dy;
                    if (nx < 0 || ny < 0 || nx >= int(cells) || ny >= int(cells))
                        continue;
                    const PatchKey key{peak.face, peak.level, std::uint16_t(nx), std::uint16_t(ny)};
                    if (const float e = patch_error(terrain, key); e > worst) {
                        worst = e;
                        best = key;
                    }
                }
            if (best == peak)
                break;
            peak = best;
        }
        std::printf("  level %2u: %.6g (%u sampled: %.6g)\n", level, double(worst), trials, double(sampled));
    }
    // How far a child's heights reach outside its parent's, which is what the cull bound must
    // be padded by before a patch may be culled against its own tile.
    std::printf("descendant relief: worst child excess over the parent tile's range:\n");
    for (unsigned level = 1; level < patch_level_max; level++) {
        const unsigned cells = 1u << level, trials = level <= 6 ? 120 : 400;
        const auto range_of = [&](PatchKey key) {
            std::vector<float> h(tile_side * tile_side);
            generate_height_tile(terrain, key, h);
            return height_tile_range(h);
        };
        double worst = 0;
        for (unsigned t = 0; t < trials; t++) {
            const unsigned face = unsigned(uniform(rng) * 6) % 6;
            const PatchKey parent{std::uint8_t(face), std::uint8_t(level),
                                  std::uint16_t(unsigned(uniform(rng) * cells) % cells),
                                  std::uint16_t(unsigned(uniform(rng) * cells) % cells)};
            const Range<float> p = range_of(parent);
            for (unsigned i = 0; i < 4; i++) {
                const Range<float> c = range_of(parent.child(i));
                worst = std::max({worst, double(c.max - p.max), double(p.min - c.min)});
            }
        }
        std::printf("  level %2u: %.6g\n", level, worst);
    }
}

void test_height_tile_range() {
    const MinorPlanetTerrain terrain(1007);
    for (PatchKey key : {PatchKey{4, 1, 0, 1}, PatchKey{4, 6, 37, 31}}) {
        std::vector<float> heights(tile_side * tile_side);
        generate_height_tile(terrain, key, heights);
        const Range<float> bounds = height_tile_range(heights);
        for (float h : heights) {
            // Match the renderer's encoding and the shader's float decode.
            const float span = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
            const auto encoded = std::uint16_t(
                std::clamp((h - MinorPlanetTerrain::height_min) / span, 0.f, 1.f) * 65535.f + .5f);
            const float decoded = MinorPlanetTerrain::height_min + (float(encoded) / 65535.f) * span;
            assert(bounds.contains(h) && bounds.contains(decoded));
        }
        // Bilinear morph samples must remain within the height range.
        for (unsigned i = 0; i + 1 < heights.size(); i++)
            assert(bounds.contains(heights[i] * .37f + heights[i + 1] * .63f));
    }
}

// A quadrant a patch does not draw must contribute no triangle at all. The vertex shader can
// only drop vertices, and one on the centre row or column belongs to the quadrants either side
// of it, so the rule has to be "keep it while any quadrant it touches is drawn". Keeping the
// whole centre cross regardless, as it once did, left a flap: the quad diagonally across the
// centre has three corners on the cross, and one of its two triangles outlived the mask. This
// models shaders/surface/patch.slang -- the two have to say the same thing.
void test_quadrant_mask() {
    const auto mesh = patch_grid_mesh();
    constexpr float centre = (tile_side - 1) * .5f;
    for (unsigned mask = 1; mask <= 0xf; mask++) {
        const auto dropped = [&](const Vec3f& p) {
            const bool low_x = p.x<centre, high_x = p.x> centre;
            const bool low_y = p.y<centre, high_y = p.y> centre;
            const unsigned touched = (!high_x && !high_y ? 1u : 0u) | (!low_x && !high_y ? 2u : 0u) |
                                     (!high_x && !low_y ? 4u : 0u) | (!low_x && !low_y ? 8u : 0u);
            return (mask & touched) == 0;
        };
        unsigned alive = 0;
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const Vec3f a = mesh.vertices[mesh.indices[i]].position;
            const Vec3f b = mesh.vertices[mesh.indices[i + 1]].position;
            const Vec3f c = mesh.vertices[mesh.indices[i + 2]].position;
            if (dropped(a) || dropped(b) || dropped(c))
                continue;
            alive++;
            // Where its body lies, not where its corners do: a triangle of the drawn set must
            // sit in a drawn quadrant.
            const float x = (a.x + b.x + c.x) / 3, y = (a.y + b.y + c.y) / 3;
            const unsigned quadrant = (x > centre ? 1u : 0u) | (y > centre ? 2u : 0u);
            assert(mask >> quadrant & 1);
        }
        assert(alive > 0);
        // Whole quadrants, skirt included: a quarter of the grid and a quarter of the ring each.
        const unsigned quarters = unsigned(std::popcount(mask));
        assert(alive == quarters * unsigned(mesh.indices.size() / 3) / 4);
    }
    std::printf("grid mesh: every triangle of all 15 quadrant masks lies in a drawn quadrant\n");
}

void test_grid_mesh() {
    const auto mesh = patch_grid_mesh();
    constexpr unsigned expected_vertices = tile_side * tile_side + 4 * tile_side;
    constexpr unsigned expected_indices = ((tile_side - 1) * (tile_side - 1) + 4 * (tile_side - 1)) * 6;
    assert(mesh.vertices.size() == expected_vertices);
    assert(mesh.indices.size() == expected_indices);
    for (const auto& v : mesh.vertices)
        assert(v.position.x >= 0 && v.position.x <= tile_side - 1 && v.position.y >= 0 &&
               v.position.y <= tile_side - 1 && (v.position.z == 0 || v.position.z == 1));
    for (const auto index : mesh.indices)
        assert(index < expected_vertices);
    std::printf("grid mesh: %zu vertices, %zu indices\n", mesh.vertices.size(), mesh.indices.size());
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++)
        if (std::string_view(argv[i]) == "--calibrate") {
            calibrate_level_errors();
            return 0;
        }
    test_noise();
    test_terrain();
    test_bake();
    test_patches();
    test_cube_coordinates();
    test_tiles();
    test_grid_mesh();
    test_quadrant_mask();
    test_height_tile_range();

    return 0;
}
