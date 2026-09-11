#pragma once

#include "core/math.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Procedural meshes and belt layout. Everything is float: meshes live in a
// unit-radius local frame and instances are placed relative to their parent.
namespace space::geometry {

struct Vertex {
    Vec3f position;
    Vec3f normal;
    float u = 0.0f, v = 0.0f;
    constexpr bool operator==(const Vertex&) const = default;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct AsteroidInstance {
    Vec3f position;
    Vec3f scale;
    Vec3f rotation; // Euler angles in radians (x, y, z).
    Vec3f spin;     // tumble rate per axis, relative to the belt's rock spin rate
    std::uint32_t variant = 0;
};

struct Plane {
    Vec3f normal;
    float distance = 0.0f; // Plane equation: dot(normal, p) + distance >= 0.
};

struct Frustum {
    Plane planes[6];
};

struct BeltParams {
    std::uint64_t seed = 0;
    std::uint32_t count = 0;
    float inner_radius = 0.0f;
    float outer_radius = 0.0f;
    float thickness = 0.0f;
};

// Number of LOD levels for spheres; 0 is the cheapest, 3 the most detailed.
constexpr std::uint32_t lod_count = 4;

// Rocks are icospheres displaced by a seeded shape function and come in
// rock_level_count subdivision levels (20 to 20480 triangles). Every level
// samples the same function, so silhouettes and normals agree across levels.
// The belt draws from a library of rock_shape_count seeded shapes.
constexpr std::uint32_t rock_level_count = 6;
constexpr std::uint32_t rock_shape_count = 16;

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude);
// level is the number of icosphere subdivisions, below rock_level_count.
// Vertices are shared and indexed (ready for meshlet building) with normals
// taken from the shape function rather than from the faces.
Mesh generate_rock(std::uint32_t seed, std::uint32_t level);
// Projected radius in pixels above which each level from 1 upward is used.
// The GPU culling pass applies the same thresholds.
constexpr float rock_level_thresholds[rock_level_count - 1] = {3.f, 8.f, 20.f, 50.f, 130.f};
// Rock level for a projected radius in pixels; no hysteresis, the levels
// share one shape so switches are small.
std::uint32_t select_rock_level(float projected_radius_pixels);
std::vector<AsteroidInstance> generate_belt(const BeltParams& params);

// Relative rock density across the belt, t = 0 at the inner edge and 1 at the
// outer: three soft rings and one thin gap, so the belt reads as rings rather
// than a uniform disc. Always within (0, 1]. The gas giant's belt shadow in
// common.slang mirrors this profile.
float belt_ring_density(float t);

// Picks a LOD from the projected radius, keeping the previous level inside a
// 15% hysteresis band around each transition.
std::uint32_t select_lod(float projected_radius_pixels, std::uint32_t previous);

bool sphere_in_frustum(std::span<const Plane> planes, Vec3f center, float radius);
inline bool sphere_in_frustum(const Frustum& frustum, Vec3f center, float radius) {
    return sphere_in_frustum(frustum.planes, center, radius);
}

} // namespace space::geometry
