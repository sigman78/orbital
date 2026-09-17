#include "scene/terrain.hpp"
#include "core/noise.hpp"
#include "scene/terrain_patch.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>

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

// A patch's grid lies on the terrain exactly (its crater region is complete),
// neighbours share their edge, the skirt hangs below, and the indices fit.
void test_patches() {
    const MinorPlanetTerrain terrain(1007);
    for (unsigned face = 0; face < 6; face++)
        for (double s = -1; s <= 1; s += .5)
            assert(std::abs(length(cube_direction(face, s, .25)) - 1) < 1e-12);
    std::vector<geometry::Vertex> a(patch_vertex_count), b(patch_vertex_count);
    const PatchKey left{2, 3, 4, 5}, right{2, 3, 5, 5};
    const float error = generate_patch(terrain, left, a);
    generate_patch(terrain, right, b);
    assert(error > 0 && error < .01f); // a level-3 cell: its curvature and terrain, well under the height range
    for (unsigned y = 0; y < patch_side; y++)
        assert(a[y * patch_side + patch_quads] == b[y * patch_side]);
    const PatchBounds bounds = patch_bounds(left);
    for (unsigned i = 0; i < patch_side * patch_side; i++) {
        const Vec3d d = to_double(a[i].normal);
        assert(std::abs(length(to_double(a[i].position)) - (1 + terrain.height(d))) < 1e-6);
        assert(std::acos(std::min(dot(d, bounds.centre), 1.0)) <= bounds.angular_radius);
    }
    for (unsigned i = patch_side * patch_side; i < patch_vertex_count; i++)
        assert(length(a[i].position) < 1 + MinorPlanetTerrain::height_max);
    const auto indices = patch_indices();
    assert(indices.size() == patch_index_count);
    for (const auto index : indices)
        assert(index < patch_vertex_count);
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 8; i++)
        generate_patch(terrain, PatchKey{std::uint8_t(i % 6), 5, std::uint16_t(i), std::uint16_t(3 * i)}, b);
    std::printf("terrain: eight patches of %u vertices in %.2f ms on one thread\n", patch_vertex_count,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    std::printf("terrain: patch error by level, radii:");
    for (unsigned level = 0; level <= 8; level++)
        std::printf(" %.2g", double(generate_patch(terrain,
                                                   PatchKey{1, std::uint8_t(level), std::uint16_t((1u << level) / 2),
                                                            std::uint16_t((1u << level) / 3)},
                                                   b)));
    std::printf("\n");
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
    // A direction on the face boundary goes to the right face.
    const CubeCoord c = cube_coordinates(normalized(Vec3d{1, .5, .3}));
    assert(c.face == 0); // +x dominant
}

void test_tiles() {
    const MinorPlanetTerrain terrain(1007);
    // Height tile determinism.
    std::vector<float> a(tile_side * tile_side), b(tile_side * tile_side);
    const PatchKey key{2, 3, 4, 5};
    generate_height_tile(terrain, key, a);
    generate_height_tile(terrain, key, b);
    for (unsigned i = 0; i < a.size(); i++)
        assert(a[i] == b[i]);
    // Heights within range.
    for (float h : a)
        assert(h >= MinorPlanetTerrain::height_min && h <= MinorPlanetTerrain::height_max);
    // Parent-child even-texel agreement: child's even texels sample the same
    // directions as the parent's texels in the overlapping region.
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
    // Neighbours share their edge exactly.
    const PatchKey left_key{2, 3, 4, 5}, right_key{2, 3, 5, 5};
    std::vector<float> lh(tile_side * tile_side), rh(tile_side * tile_side);
    generate_height_tile(terrain, left_key, lh);
    generate_height_tile(terrain, right_key, rh);
    for (unsigned y = 0; y < tile_side; y++)
        assert(lh[y * tile_side + (tile_side - 1)] == rh[y * tile_side]);
    // Colour tiles match expected layout.
    std::vector<std::uint8_t> albedo(tile_side * tile_side * 4), norm(tile_side * tile_side * 4);
    generate_colour_tiles(terrain, key, a, albedo, norm);
    for (unsigned i = 0; i < tile_side * tile_side; i++) {
        assert(albedo[i * 4 + 3] == 255);
        assert(norm[i * 4 + 2] > 0); // z component of normal is positive (outward)
    }
    // Patch error positive and reasonable.
    const float error = patch_error(terrain, key);
    assert(error > 0 && error < .01f);
    std::printf("tiles: patch error at level 3: %g radii\n", double(error));
}

void calibrate_level_errors() {
    const MinorPlanetTerrain terrain(1007);
    std::uint64_t rng = 42;
    std::printf("calibration: max patch_error per level, 64 random patches, seed 1007, Everitt warp:\n");
    for (unsigned level = 0; level <= 8; level++) {
        const unsigned cells = 1u << level;
        float worst = 0;
        for (unsigned trial = 0; trial < 64; trial++) {
            const unsigned face = unsigned(uniform(rng) * 6) % 6;
            const unsigned x = unsigned(uniform(rng) * cells) % cells;
            const unsigned y = unsigned(uniform(rng) * cells) % cells;
            const float e = patch_error(
                terrain, PatchKey{std::uint8_t(face), std::uint8_t(level), std::uint16_t(x), std::uint16_t(y)});
            if (e > worst)
                worst = e;
        }
        std::printf("  level %u: %.6g\n", level, double(worst));
    }
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

int main() {
    test_noise();
    test_terrain();
    test_bake();
    test_patches();
    test_cube_coordinates();
    test_tiles();
    test_grid_mesh();
    calibrate_level_errors();
    return 0;
}
