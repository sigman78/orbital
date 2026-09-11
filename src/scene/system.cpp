#include "system.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace space {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double eps = 1e-9;

std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

Vec3d cross(Vec3d a, Vec3d b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3d rotate(Vec3d v, Vec3d axis, double radians) {
    axis = normalized(axis);
    const double c = std::cos(radians), s = std::sin(radians);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0 - c));
}
bool finite(Vec3d v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool finite(double x) {
    return std::isfinite(x);
}
} // namespace

Vec3d operator+(Vec3d a, Vec3d b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3d operator-(Vec3d a, Vec3d b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3d operator*(Vec3d a, double s) {
    return {a.x * s, a.y * s, a.z * s};
}
double dot(Vec3d a, Vec3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
double length(Vec3d a) {
    return std::sqrt(dot(a, a));
}
Vec3d normalized(Vec3d a) {
    const double n = length(a);
    return n > eps ? a * (1.0 / n) : Vec3d{0, 1, 0};
}

SystemDescription generate_system(std::uint64_t seed) {
    SystemDescription s;
    s.master_seed = seed;
    const bool showcase = seed == 20260911ull;
    const double variation = showcase ? 0.0 : (double(mix(seed ^ 0x4f52424954414cull) % 2001) - 1000.0) / 100000.0;
    s.star = {1, 35.0, 5600.0 + double(mix(seed) % 1800), 4.0, {-3500.0, 1400.0, 1200.0}};
    const auto material = [seed](std::uint64_t id) { return mix(seed ^ id ^ 0x535552464143454full); };
    const double earth_r = 2.5 * (1.0 + variation), giant_r = 28.0 * (1.0 + variation * 0.6);
    s.bodies = {{1001,
                 0,
                 BodyClass::Terrestrial,
                 earth_r,
                 0.41,
                 1600.0,
                 0.0,
                 {0, 0, 0},
                 {0, 1, 0},
                 0.000004,
                 material(1001),
                 0.32},
                {1002,
                 0,
                 BodyClass::GasGiant,
                 giant_r,
                 0.12,
                 1100.0,
                 0.0,
                 {75, 20, -175},
                 {0.2, 1, 0.1},
                 0.000009,
                 material(1002),
                 0.0},
                {1003,
                 0,
                 BodyClass::RockyMoon,
                 0.68,
                 0.08,
                 22000.0,
                 0.0,
                 {-65, 8, -45},
                 {0, 1, 0},
                 0.000025,
                 material(1003),
                 0.0}};
    s.belts = {{2001, 1002, 42.0, 62.0, 1.4, 0.65, mix(seed ^ 2001)}};
    return s;
}

std::vector<std::string> validate_system(const SystemDescription& s) {
    std::vector<std::string> errors;
    if (s.schema_version == 0)
        errors.emplace_back("schema_version must be non-zero");
    if (!finite(s.scale_policy) || s.scale_policy <= 0)
        errors.emplace_back("scale_policy must be finite and positive");
    if (!finite(s.star.position) || !finite(s.star.radius) || s.star.radius <= 0)
        errors.emplace_back("star has invalid position or radius");
    std::unordered_map<std::uint64_t, const BodyDescription*> by_id;
    for (const auto& b : s.bodies) {
        if (b.id == 0 || !by_id.emplace(b.id, &b).second)
            errors.emplace_back("body IDs must be unique and non-zero");
        if (!finite(b.radius) || b.radius <= 0 || !finite(b.orbit_offset) || !finite(b.orbit_axis) ||
            !finite(b.orbit_angular_rate))
            errors.emplace_back("body " + std::to_string(b.id) + " has invalid finite/radius parameters");
        if (b.parent_id == b.id)
            errors.emplace_back("body " + std::to_string(b.id) + " is its own parent");
    }
    for (const auto& b : s.bodies)
        if (b.parent_id && !by_id.count(b.parent_id))
            errors.emplace_back("body " + std::to_string(b.id) + " references missing parent");
    for (const auto& b : s.bodies) {
        std::unordered_set<std::uint64_t> seen;
        for (auto p = b.parent_id; p;) {
            if (!seen.insert(p).second) {
                errors.emplace_back("parent graph contains a cycle");
                break;
            }
            auto it = by_id.find(p);
            if (it == by_id.end())
                break;
            p = it->second->parent_id;
        }
    }
    for (const auto& belt : s.belts) {
        if (belt.id == 0 || !finite(belt.inner_radius) || !finite(belt.outer_radius) || belt.inner_radius <= 0 ||
            belt.outer_radius <= belt.inner_radius || belt.thickness < 0 || !finite(belt.density) || belt.density < 0)
            errors.emplace_back("belt has invalid bounds or density");
        if (belt.parent_id && !by_id.count(belt.parent_id))
            errors.emplace_back("belt references missing parent");
    }
    return errors;
}

std::vector<BodyState> evaluate_system(const SystemDescription& s, double seconds) {
    std::vector<BodyState> result;
    if (!finite(seconds) || !validate_system(s).empty())
        return result;
    std::unordered_map<std::uint64_t, const BodyDescription*> by_id;
    for (const auto& b : s.bodies)
        by_id[b.id] = &b;
    std::unordered_map<std::uint64_t, Vec3d> positions;
    std::function<Vec3d(std::uint64_t)> position = [&](std::uint64_t id) {
        auto cached = positions.find(id);
        if (cached != positions.end())
            return cached->second;
        const auto& b = *by_id.at(id);
        Vec3d p = rotate(b.orbit_offset, b.orbit_axis, b.orbit_angular_rate * seconds);
        if (b.parent_id)
            p = position(b.parent_id) + p;
        return positions[id] = p;
    };
    for (const auto& b : s.bodies)
        result.push_back({b.id, position(b.id),
                          b.rotation_phase + (b.rotation_period > eps ? 2 * pi * seconds / b.rotation_period : 0.0),
                          b.radius});
    return result;
}
} // namespace space
