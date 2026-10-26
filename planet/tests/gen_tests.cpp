#include "core/noise.hpp"
#include "planet/crater_terrain.hpp"
#include "planet/patch.hpp"
#include "planet/tile_sampling.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

using namespace space;
using namespace planet;

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

// A continuous bump field must not jump when its lattice search window moves.
void test_detail_continuity() {
    double worst = 0;
    for (unsigned seed : {1007u, 1008u, 2024u}) {
        const MinorPlanetTerrain terrain(seed);
        for (unsigned axis = 0; axis < 3; ++axis)
            for (int plane = -100; plane <= 100; plane += 8)
                for (unsigned k = 0; k < 32; ++k) {
                    const double phi = (k + .31) * 2 * pi<double> / 32;
                    const auto direction = [&](double epsilon) {
                        const double x = (plane + epsilon) / MinorPlanetTerrain::detail_frequency;
                        const double r = std::sqrt(1 - x * x);
                        const Vec3d d{x, r * std::cos(phi), r * std::sin(phi)};
                        return axis == 0 ? d : axis == 1 ? Vec3d{d.z, d.x, d.y} : Vec3d{d.y, d.z, d.x};
                    };
                    const double jump = std::abs(double(terrain.detail(direction(1e-7), 1e6, 1)) -
                                                 terrain.detail(direction(-1e-7), 1e6, 1));
                    worst = std::max(worst, jump);
                }
    }
    std::printf("detail: maximum lattice-boundary step %.12g radii\n", worst);
    assert(worst < 1e-8);

    // Isolate one crater's outer support: both its height and derivative taper out.
    MinorPlanetTerrain terrain(1007);
    MinorPlanetTerrain::DetailRegion region;
    region.weights[0] = 1;
    region.craters[0].push_back({{0, 0, 1}, .4, .1});
    const auto at = [&](double x) {
        const double chord = x * .4 / MinorPlanetTerrain::detail_frequency;
        const double angle = 2 * std::asin(chord / 2);
        return terrain.detail({std::sin(angle), 0, std::cos(angle)}, region, 1);
    };
    assert(at(MinorPlanetTerrain::detail_reach + 1e-4) == 0);
    assert(std::abs(at(MinorPlanetTerrain::detail_reach - 1e-3)) < 1e-10);
    assert(terrain.detail({0, 0, 1}, 1e6, 0) == 0);
}

// The accelerated tile path must retain all contributions, including derivative borders.
void test_detail_regions() {
    for (unsigned seed : {1007u, 1008u, 2024u}) {
        const MinorPlanetTerrain terrain(seed);
        for (unsigned face = 0; face < 6; ++face)
            for (unsigned level : {3u, 5u, 7u, 9u, 11u}) {
                const unsigned cells = 1u << level;
                const PatchKey key{std::uint8_t(face), std::uint8_t(level), std::uint16_t(face & 1 ? 0 : cells / 2),
                                   std::uint16_t(cells / 2)};
                const auto bounds = patch_bounds(key);
                const double nyquist = .5 * (tile_colour_side - 1) / bounds.angular_size;
                const auto region = terrain.detail_region(
                    bounds.centre, bounds.angular_radius + 2 * bounds.angular_size / (tile_colour_side - 1), nyquist);
                const double step = 2.0 / cells;
                for (int y : {-1, 0, 16, 32, 64, 65})
                    for (int x : {-1, 0, 16, 32, 64, 65}) {
                        const auto d = cube_direction(face, -1 + (key.x + double(x) / 64) * step,
                                                      -1 + (key.y + double(y) / 64) * step);
                        assert(terrain.detail(d, region, 1) == terrain.detail(d, nyquist, 1));
                    }
            }
    }
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

// The regional material path must be identical to the full crater list at tile
// boundaries as well as in tile interiors. Regions preserve source order, so an
// exact component comparison catches both missing craters and changed arithmetic.
void test_regional_albedo() {
    for (const std::uint64_t seed : {1007ull, 1008ull, 4242ull}) {
        const MinorPlanetTerrain terrain(seed);
        for (const unsigned level : {0u, 2u, 5u, 8u}) {
            const unsigned cells = 1u << level;
            for (unsigned face = 0; face < 6; ++face)
                for (const unsigned x : {0u, cells - 1})
                    for (const unsigned y : {0u, cells - 1}) {
                        const PatchKey key{std::uint8_t(face), std::uint8_t(level), std::uint16_t(x), std::uint16_t(y)};
                        const PatchBounds bounds = patch_bounds(key);
                        const auto region = terrain.region(bounds.centre, bounds.angular_radius + 1e-9);
                        const double size = 2.0 / cells;
                        for (unsigned iy = 0; iy < 3; ++iy)
                            for (unsigned ix = 0; ix < 3; ++ix) {
                                const double s = -1 + (x + ix * .5) * size;
                                const double t = -1 + (y + iy * .5) * size;
                                const Vec3d d = cube_direction(face, s, t);
                                const float h = terrain.height(d);
                                assert(terrain.height(d, region) == h);
                                for (const float slope : {0.f, .3f, 1.f}) {
                                    const Vec3f full = terrain.albedo(d, h, slope);
                                    const Vec3f local = terrain.albedo(d, h, slope, region);
                                    assert(full.x == local.x && full.y == local.y && full.z == local.z);
                                }
                            }
                    }
        }
    }
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

static Vec3d tile_normal(const std::uint16_t* slope, unsigned face, Vec3d d) {
    const TangentFrame frame = patch_tangent_frame(face, d);
    const auto s = [&](unsigned c) { return (slope[c] / 65535.0 * 2 - 1) * tile_slope_scale; };
    return normalized(d - frame.tangent * s(0) - frame.bitangent * s(1));
}

void test_tile_determinism() {
    const MinorPlanetTerrain terrain(1007);
    std::vector<float> a(tile_side * tile_side), b(tile_side * tile_side);
    const PatchKey key{2, 3, 4, 5};
    generate_height_tile(terrain, key, a);
    generate_height_tile(terrain, key, b);
    for (unsigned i = 0; i < a.size(); i++)
        assert(a[i] == b[i]);
    for (float h : a)
        assert(h >= MinorPlanetTerrain::height_min && h <= MinorPlanetTerrain::height_max);
}

// Even child texels coincide with parent texels.
void test_tile_parent_child() {
    const MinorPlanetTerrain terrain(1007);
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
}

void test_tile_edge_heights() {
    const MinorPlanetTerrain terrain(1007);
    const PatchKey left_key{2, 3, 4, 5}, right_key{2, 3, 5, 5};
    std::vector<float> lh(tile_side * tile_side), rh(tile_side * tile_side);
    generate_height_tile(terrain, left_key, lh);
    generate_height_tile(terrain, right_key, rh);
    for (unsigned y = 0; y < tile_side; y++)
        assert(lh[y * tile_side + (tile_side - 1)] == rh[y * tile_side]);
}

// Adjacent tiles must share identical edge texels.
void test_colour_tile_edges() {
    const MinorPlanetTerrain terrain(1007);
    const PatchKey key{2, 3, 4, 5};
    const PatchKey left_key{2, 3, 4, 5}, right_key{2, 3, 5, 5};
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
}

// Cube-edge slopes differ by face frame; compare decoded normals, excluding corners.
void test_cube_edge_normals() {
    const MinorPlanetTerrain terrain(1007);
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
                                    if (length(colour_direction(p1, x1, y1) - colour_direction(p2, x2, y2)) > 1e-12 ||
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
void test_cube_corner_normals() {
    const MinorPlanetTerrain terrain(1007);
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

void test_patch_error() {
    const MinorPlanetTerrain terrain(1007);
    const PatchKey key{2, 3, 4, 5};
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

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++)
        if (std::string_view(argv[i]) == "--calibrate") {
            calibrate_level_errors();
            return 0;
        }
    test_noise();
    test_detail_continuity();
    test_detail_regions();
    test_terrain();
    test_regional_albedo();
    test_bake();
    test_tile_determinism();
    test_tile_parent_child();
    test_tile_edge_heights();
    test_colour_tile_edges();
    test_cube_edge_normals();
    test_cube_corner_normals();
    test_patch_error();
    test_height_tile_range();

    return 0;
}
