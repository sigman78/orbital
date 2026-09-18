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
    // Colour tiles match expected layout, and neighbours agree on their shared
    // edge's texels byte for byte (the seam test).
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
        // The normal the slope reads back points outward, within 45 degrees of the sphere's.
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
    // Across every cube edge the two faces agree on the albedo and on the normal at
    // every shared edge texel, corners excepted: three grids meet there and no one
    // difference pair serves all three. The two faces hold the gradient in their own
    // frames, so the bytes differ there and the normal read back does not.
    {
        const auto world_normal = [&](unsigned face, unsigned x, unsigned y, const std::uint16_t* slope, PatchKey key) {
            const double size = 2.0 / double(1u << key.level);
            const Vec3d d = cube_direction(face, -1 + key.x * size + x * size / (tile_colour_side - 1),
                                           -1 + key.y * size + y * size / (tile_colour_side - 1));
            return tile_normal(slope, face, d);
        };
        // The colour grid is finer than the height grid, so a shared texel is found by
        // its direction rather than by its height.
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
    // The eight cube corners, where three faces meet and each writes a texel. The
    // ring texel past an edge is the neighbour's texel one step in along the edge
    // that was crossed; at a corner both of the neighbour's coordinates read 1, so
    // naming it by the larger of them picked arbitrarily and the three faces
    // differentiated over three stencils, up to 26 degrees apart at any level.
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
    // The bounding sphere the cull tests the frustum against holds every point the
    // patch can draw: its grid over the height shell and its skirt below that. A
    // sphere short of it culls a patch that shows, and an invisible child is not
    // drawn by its parent either, so the miss is a hole. It must not be far over
    // it either: the cull is only as tight as this.
    {
        double worst_fill = 0, tightest = 1e9;
        unsigned sampled = 0;
        for (unsigned level = 0; level <= 6; level++) {
            const unsigned cells = 1u << level, step = std::max(1u, cells / 4);
            for (unsigned face = 0; face < 6; face++)
                for (unsigned cx = 0; cx < cells; cx += step)
                    for (unsigned cy = 0; cy < cells; cy += step) {
                        const PatchKey k{std::uint8_t(face), std::uint8_t(level), std::uint16_t(cx), std::uint16_t(cy)};
                        const PatchBounds bounds = patch_bounds(k);
                        const double size = 2.0 / cells;
                        double reach = 0;
                        for (unsigned i = 0; i <= 16; i++)
                            for (unsigned j = 0; j <= 16; j++) {
                                const Vec3d d = cube_direction(face, -1 + cx * size + i * size / 16,
                                                               -1 + cy * size + j * size / 16);
                                for (double h : {double(MinorPlanetTerrain::height_max),
                                                 double(MinorPlanetTerrain::height_min) - patch_skirt_drop})
                                    reach = std::max(reach, length(d * (1 + h) - bounds.centre));
                            }
                        assert(reach <= bounds.bound_radius); // conservative, always
                        worst_fill = std::max(worst_fill, bounds.bound_radius / reach);
                        tightest = std::min(tightest, bounds.bound_radius / reach);
                        sampled++;
                    }
        }
        std::fprintf(stderr, "tiles: %u patch bounds, the sphere is %.3f to %.3f times the reach it must hold\n",
                     sampled, tightest, worst_fill);
        assert(sampled > 100 && worst_fill < 1.01); // the cap's corner is a sampled point, so it is nearly exact
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
