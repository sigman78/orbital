#include "render/frame_calculations.hpp"
#include "planet/clod.hpp"
#include "planet/patch.hpp"
#include "planet/tile_sampling.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace space;
using namespace planet;

constexpr unsigned screen_height = 900;
constexpr double body_radius = 1.0;    // distances below are expressed in body radii
constexpr double camera_radius = 1.08; // matches the app's collision floor
constexpr double sample_stride = 4;    // lattice cells; measurements are unweighted
constexpr double direction_s = .6, direction_t = .3;

struct Projection {
    double x = 0, y = 0, depth = 0;
};

struct CameraSetup {
    const char* name;
    Vec3d target;
    Vec3d camera_local;
    render::CameraView camera;
    TierView tier;
    double tan_y = 0;
};

CameraSetup camera_setup(const char* name, double azimuth_radians) {
    const Vec3d target = normalized(Vec3d{direction_s, direction_t, 1});
    // Rotate the stand point to the side of the target. Eighteen degrees is inside
    // the 21.8 degree geometric horizon at radius 1.08 and gives a near-limb view.
    const Vec3d axis = normalized(cross(target, Vec3d{0, 1, 0}));
    const Vec3d stand = rotate_about(target, axis, azimuth_radians);
    const Vec3d camera_local = stand * camera_radius;
    const Vec3d forward = normalized(target - camera_local);
    const Vec3d up_reference = std::abs(forward.y) < .95 ? Vec3d{0, 1, 0} : Vec3d{1, 0, 0};
    const Vec3d right = normalized(cross(forward, up_reference));
    const Vec3d up = normalized(cross(right, forward));
    render::CameraView camera{.position = camera_local, .forward = forward, .right = right, .up = up};
    const double tan_y = std::tan(camera.vertical_fov * .5);
    constexpr double aspect = 16.0 / 9.0;
    TierView view{.body_centre = camera_local * -body_radius,
                  .radius = body_radius,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = camera_local,
                  .height_pixels = float(screen_height),
                  .tan_y = float(tan_y)};
    const geometry::Frustum frustum = render::view_frustum(camera, float(tan_y * aspect), float(tan_y));
    for (unsigned i = 0; i < 5; i++)
        view.planes[i] = {.normal = frustum.planes[i].normal, .distance = frustum.planes[i].distance};
    return {name, target, camera_local, camera, view, tan_y};
}

std::uint64_t draw_signature(const TerrainTier& tier) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const TerrainTier::Draw& draw : tier.draws()) {
        const std::uint32_t words[] = {draw.key.packed(), draw.quadrants};
        for (std::uint32_t word : words)
            for (unsigned byte = 0; byte < 4; ++byte)
                hash = (hash ^ std::uint8_t(word >> (byte * 8))) * 1099511628211ull;
    }
    return hash;
}

bool converge_tier(TerrainTier& tier, const TierView& view, const MinorPlanetTerrain& terrain) {
    unsigned unchanged = 0;
    std::uint64_t previous = 0;
    for (unsigned frame = 1; frame <= 20000; ++frame) {
        tier.update(view, frame);
        for (const TerrainTier::Generation& generation : tier.generate()) {
            std::vector<float> heights(tile_side * tile_side);
            generate_height_tile(terrain, generation.key, heights);
            if (!tier.mark_resident(generation.slot, generation.key, generation.stamp, height_tile_range(heights)))
                return false;
        }
        const std::uint64_t current = draw_signature(tier);
        if (tier.active() && tier.generate().empty() && current == previous)
            ++unchanged;
        else
            unchanged = 0;
        previous = current;
        if (unchanged >= 8)
            return true;
    }
    return false;
}

bool project(const CameraSetup& setup, Vec3d point, Projection& out) {
    const Vec3d relative = point - setup.camera_local;
    const double z = dot(relative, setup.camera.forward);
    if (z <= 1e-9)
        return false;
    const double tan_x = setup.tan_y * 16.0 / 9.0;
    const double nx = dot(relative, setup.camera.right) / (z * tan_x);
    const double ny = dot(relative, setup.camera.up) / (z * setup.tan_y);
    if (std::abs(nx) > 1 || std::abs(ny) > 1)
        return false;
    out = {(nx + 1) * (screen_height * (16.0 / 9.0)) * .5, (1 - ny) * screen_height * .5, z};
    return true;
}

Vec3d surface_point(const MinorPlanetTerrain& terrain, unsigned face, double s, double t) {
    const Vec3d d = cube_direction(face, s, t);
    return d * (1 + double(terrain.height(d)));
}

struct Samples {
    std::vector<double> mesh_edge, triangle_area, triangle_longest;
    std::vector<double> material_major, material_minor, anisotropy;
    std::size_t considered = 0, rejected = 0;
    std::size_t patch_draws = 0, drawn_quadrants = 0, triangles = 0;
    unsigned deepest = 0;
};

void sample_draws(const CameraSetup& camera, const TerrainTier& tier, const MinorPlanetTerrain& terrain,
                  Samples& samples) {
    constexpr unsigned mesh_quads = tile_side - 1;
    constexpr unsigned quadrant_quads = mesh_quads / 2;
    constexpr double mesh_step = 1.0 / mesh_quads;
    constexpr double material_step = 1.0 / (tile_colour_side - 1);
    constexpr unsigned triangles_per_quadrant = (quadrant_quads * quadrant_quads + 2 * quadrant_quads) * 2;

    for (const TerrainTier::Draw& draw : tier.draws()) {
        samples.patch_draws++;
        samples.deepest = std::max(samples.deepest, unsigned(draw.key.level));
        const double cell_size = 2.0 / double(1u << draw.key.level);
        for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
            if ((draw.quadrants & (1u << quadrant)) == 0)
                continue;
            samples.drawn_quadrants++;
            samples.triangles += triangles_per_quadrant;
            const unsigned x0 = (quadrant & 1) * quadrant_quads;
            const unsigned y0 = (quadrant >> 1) * quadrant_quads;
            for (unsigned y = y0; y < y0 + quadrant_quads; y += unsigned(sample_stride))
                for (unsigned x = x0; x < x0 + quadrant_quads; x += unsigned(sample_stride)) {
                    samples.considered++;
                    const double s = -1 + (double(draw.key.x) + double(x) * mesh_step) * cell_size;
                    const double t = -1 + (double(draw.key.y) + double(y) * mesh_step) * cell_size;
                    Projection centre, mx, my, mxy, cu, cv;
                    const Vec3d p = surface_point(terrain, draw.key.face, s, t);
                    const Vec3d px = surface_point(terrain, draw.key.face, s + cell_size * mesh_step, t);
                    const Vec3d py = surface_point(terrain, draw.key.face, s, t + cell_size * mesh_step);
                    const Vec3d pxy = surface_point(terrain, draw.key.face, s + cell_size * mesh_step,
                                                    t + cell_size * mesh_step);
                    const Vec3d pu = surface_point(terrain, draw.key.face, s + cell_size * material_step, t);
                    const Vec3d pv = surface_point(terrain, draw.key.face, s, t + cell_size * material_step);
                    if (!project(camera, p, centre) || !project(camera, px, mx) || !project(camera, py, my) ||
                        !project(camera, pxy, mxy) || !project(camera, pu, cu) || !project(camera, pv, cv)) {
                        samples.rejected++;
                        continue;
                    }
                    samples.mesh_edge.push_back(std::hypot(mx.x - centre.x, mx.y - centre.y));
                    samples.mesh_edge.push_back(std::hypot(my.x - centre.x, my.y - centre.y));
                    const double ex = mx.x - centre.x, ey = mx.y - centre.y;
                    const double fx = my.x - centre.x, fy = my.y - centre.y;
                    const double dx = mxy.x - centre.x, dy = mxy.y - centre.y;
                    samples.triangle_area.push_back(.5 * std::abs(ex * dy - ey * dx));
                    samples.triangle_area.push_back(.5 * std::abs(dx * fy - dy * fx));
                    samples.triangle_longest.push_back(
                        std::max({std::hypot(ex, ey), std::hypot(dx, dy), std::hypot(dx - ex, dy - ey)}));
                    samples.triangle_longest.push_back(
                        std::max({std::hypot(fx, fy), std::hypot(dx, dy), std::hypot(dx - fx, dy - fy)}));

                    // Singular values of the screen-pixel Jacobian per colour/slope texel.
                    const double ux = cu.x - centre.x, uy = cu.y - centre.y;
                    const double vx = cv.x - centre.x, vy = cv.y - centre.y;
                    const double a = ux * ux + uy * uy, b = ux * vx + uy * vy, c = vx * vx + vy * vy;
                    const double root = std::sqrt(std::max(0.0, (a - c) * (a - c) + 4 * b * b));
                    const double major = std::sqrt(std::max(0.0, (a + c + root) * .5));
                    const double minor = std::sqrt(std::max(0.0, (a + c - root) * .5));
                    samples.material_major.push_back(major);
                    samples.material_minor.push_back(minor);
                    samples.anisotropy.push_back(major / std::max(minor, 1e-12));
                }
        }
    }
}

double percentile(std::vector<double> values, double q) {
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    const std::size_t i = std::min(values.size() - 1, std::size_t(std::ceil(q * values.size()) - 1));
    return values[i];
}

bool emit_row(const CameraSetup& base, float bias, float mesh_pixels, const MinorPlanetTerrain& terrain) {
    CameraSetup camera = base;
    camera.tier.lod_bias = bias;
    camera.tier.mesh_pixels = mesh_pixels;
    TerrainTier tier(2048);
    if (!converge_tier(tier, camera.tier, terrain)) {
        std::fprintf(stderr, "terrain_footprint: tier did not converge for %s bias %.0f mesh %.0f\n", camera.name, bias,
                     mesh_pixels);
        return false;
    }
    Samples samples;
    sample_draws(camera, tier, terrain, samples);
    std::printf("%s,%.0f,%.0f,%zu,%zu,%zu,%u,%zu,%zu,%zu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,",
                camera.name, double(bias), double(mesh_pixels), samples.considered, samples.rejected,
                samples.patch_draws, samples.deepest, samples.drawn_quadrants, samples.triangles,
                samples.mesh_edge.size(), percentile(samples.mesh_edge, .50), percentile(samples.mesh_edge, .90),
                percentile(samples.mesh_edge, .95), percentile(samples.material_major, .50),
                percentile(samples.material_major, .90), percentile(samples.material_major, .95),
                percentile(samples.material_minor, .50), percentile(samples.material_minor, .90),
                percentile(samples.material_minor, .95), percentile(samples.anisotropy, .50),
                percentile(samples.anisotropy, .90), percentile(samples.anisotropy, .95));
    std::printf("%.4f,%.4f,%.4f,%u\n", percentile(samples.triangle_longest, .50),
                percentile(samples.triangle_longest, .95), percentile(samples.triangle_area, .50),
                tier.cache_stats(20000).resident);
    return true;
}

} // namespace

int main() {
    constexpr std::uint64_t seed = 1007;
    const MinorPlanetTerrain terrain(seed);
    std::puts("# Approximation: surface positions use direct terrain.height(), not the exact generated/morphed mesh.");
    std::puts("# Draw list already applies frustum and horizon culling; samples are unweighted and not ray-occlusion "
              "tested.");
    std::puts("# 60-degree vertical FOV, 900px high, 16:9, radial camera 1.08, target direction normalize(.6,.3,1), "
              "seed 1007.");
    std::puts("# Mesh: 33x33 vertices, 32x32 quads, 2304 triangles including skirts per full patch.");
    std::puts("# Material values are projected pixels per albedo/slope texel (shared 65x65 layout at ratio 2).");
    std::puts("camera,lod_bias,mesh_target_px,sample_sites,rejected_sites,draw_patches,deepest_level,drawn_quadrants,"
              "submitted_"
              "triangles,mesh_edge_samples,mesh_edge_px_p50,mesh_edge_px_p90,mesh_edge_px_p95,material_major_px_p50,"
              "material_major_px_p90,material_major_px_p95,material_minor_px_p50,material_minor_px_p90,material_minor_"
              "px_p95,anisotropy_p50,anisotropy_p90,anisotropy_p95,triangle_longest_px_p50,triangle_longest_px_p95,"
              "triangle_area_px2_p50,resident_tiles");
    const CameraSetup nadir = camera_setup("nadir", 0);
    const CameraSetup glancing = camera_setup("glancing18deg", 18 * pi<double> / 180);
    for (const CameraSetup& camera : {nadir, glancing})
        for (const float bias : {0.f, 1.f, 2.f})
            if (!emit_row(camera, bias, 0, terrain))
                return 1;
    for (const CameraSetup& camera : {nadir, glancing})
        for (const float mesh_pixels : {16.f, 8.f, 4.f})
            if (!emit_row(camera, 0, mesh_pixels, terrain))
                return 1;
}
