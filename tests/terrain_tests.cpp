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

int main() {
    test_noise();
    test_terrain();
    test_bake();
    test_patches();
    return 0;
}
