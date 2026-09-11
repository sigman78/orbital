#pragma once

#include <cstdint>
#include <vector>

namespace space::geometry {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    constexpr Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr bool operator==(const Vec3&) const = default;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    float u = 0.0f, v = 0.0f;
    constexpr bool operator==(const Vertex&) const = default;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct AsteroidInstance {
    Vec3 position;
    Vec3 scale;
    Vec3 rotation; // Euler angles in radians (x, y, z).
    std::uint32_t variant = 0;
};
using BeltInstance = AsteroidInstance;
using Instance = AsteroidInstance;

struct Plane {
    Vec3 normal;
    float distance = 0.0f; // Plane equation: dot(normal, p) + distance >= 0.
};

struct Frustum {
    Plane planes[6];
};

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude);
Mesh generate_rock(std::uint32_t seed, std::uint32_t detail);
std::vector<AsteroidInstance> generate_belt(std::uint64_t seed, std::uint32_t count, float inner, float outer,
                                            float thickness);

// LOD 0 is the cheapest representation, LOD 3 the most detailed. The previous
// level is used to apply a 15% hysteresis band around each transition.
std::uint32_t select_lod(float projected_radius_pixels, std::uint32_t previous);

bool sphere_in_frustum(const Frustum& frustum, Vec3 center, float radius);
bool sphere_in_frustum(const Plane* planes, std::uint32_t plane_count, Vec3 center, float radius);

} // namespace space::geometry
