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

// Number of LOD levels for spheres and rocks; 0 is the cheapest, 3 the most detailed.
constexpr std::uint32_t lod_count = 4;

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude);
Mesh generate_rock(std::uint32_t seed, std::uint32_t detail);
std::vector<AsteroidInstance> generate_belt(const BeltParams& params);

// Picks a LOD from the projected radius, keeping the previous level inside a
// 15% hysteresis band around each transition.
std::uint32_t select_lod(float projected_radius_pixels, std::uint32_t previous);

bool sphere_in_frustum(std::span<const Plane> planes, Vec3f center, float radius);
inline bool sphere_in_frustum(const Frustum& frustum, Vec3f center, float radius) {
    return sphere_in_frustum(frustum.planes, center, radius);
}

} // namespace space::geometry
