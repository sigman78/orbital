#include "scene/system.hpp"

#include <cmath>
#include <format>
#include <unordered_map>
#include <unordered_set>

namespace space {
namespace {

constexpr double epsilon = 1e-9;

// Hash tags that keep the derived seeds independent of each other, and the
// stable identifiers of the showcase system's objects.
struct SeedTags {
    std::uint64_t variation = 0x4f52424954414cull;  // "ORBITAL"
    std::uint64_t material = 0x535552464143454full; // "SURFACEO"
    std::uint64_t belt_id = 2001;
    std::uint64_t body_ids[3] = {1001, 1002, 1003};
};
constexpr SeedTags tags{};

std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

using BodyIndex = std::unordered_map<std::uint64_t, const BodyDescription*>;

// Local orbit position composed with every ancestor, memoized per body.
Vec3d resolve_position(std::uint64_t id, const BodyIndex& by_id, std::unordered_map<std::uint64_t, Vec3d>& positions,
                       double seconds) {
    if (const auto cached = positions.find(id); cached != positions.end())
        return cached->second;
    const auto& body = *by_id.at(id);
    Vec3d position = rotate_about(body.orbit_offset, body.orbit_axis, body.orbit_angular_rate * seconds);
    if (body.parent_id)
        position += resolve_position(body.parent_id, by_id, positions, seconds);
    return positions[id] = position;
}

bool has_parent_cycle(const BodyDescription& body, const BodyIndex& by_id) {
    std::unordered_set<std::uint64_t> seen;
    for (auto parent = body.parent_id; parent;) {
        if (!seen.insert(parent).second)
            return true;
        const auto it = by_id.find(parent);
        if (it == by_id.end())
            return false;
        parent = it->second->parent_id;
    }
    return false;
}

} // namespace

SystemDescription generate_system(std::uint64_t seed) {
    // Small size variation for non-showcase seeds, in [-1%, +1%].
    const double variation = seed == showcase_seed ? 0.0
                                                   : (double(mix(seed ^ tags.variation) % 2001) - 1000.0) / 100000.0;
    const auto material = [seed](std::uint64_t id) { return mix(seed ^ id ^ tags.material); };
    SystemDescription s;
    s.master_seed = seed;
    s.star = {.id = 1,
              .radius = 35.0,
              .temperature = 5600.0 + double(mix(seed) % 1800),
              .intensity = 4.0,
              .position = {-3500.0, 1400.0, 1200.0}};
    s.bodies = {
        BodyDescription{.id = tags.body_ids[0],
                        .body_class = BodyClass::Terrestrial,
                        .radius = 2.5 * (1.0 + variation),
                        .axial_tilt = 0.41,
                        .rotation_period = 1600.0,
                        .orbit_offset = {0, 0, 0},
                        .orbit_axis = {0, 1, 0},
                        .orbit_angular_rate = 0.000004,
                        .material_seed = material(tags.body_ids[0]),
                        .atmosphere_scale = 0.32},
        BodyDescription{.id = tags.body_ids[1],
                        .body_class = BodyClass::GasGiant,
                        .radius = 28.0 * (1.0 + variation * 0.6),
                        .axial_tilt = 0.12,
                        .rotation_period = 1100.0,
                        .orbit_offset = {75, 20, -175},
                        .orbit_axis = {0.2, 1, 0.1},
                        .orbit_angular_rate = 0.000009,
                        .material_seed = material(tags.body_ids[1])},
        BodyDescription{.id = tags.body_ids[2],
                        .body_class = BodyClass::RockyMoon,
                        .radius = 0.68,
                        .axial_tilt = 0.08,
                        .rotation_period = 22000.0,
                        .orbit_offset = {-65, 8, -45},
                        .orbit_axis = {0, 1, 0},
                        .orbit_angular_rate = 0.000025,
                        .material_seed = material(tags.body_ids[2])},
    };
    s.belts = {BeltDescription{.id = tags.belt_id,
                               .parent_id = tags.body_ids[1],
                               .inner_radius = 42.0,
                               .outer_radius = 62.0,
                               .thickness = 1.4,
                               .density = 0.65,
                               .seed = mix(seed ^ tags.belt_id)}};
    return s;
}

std::vector<std::string> validate_system(const SystemDescription& s) {
    std::vector<std::string> errors;
    if (s.schema_version == 0)
        errors.emplace_back("schema_version must be non-zero");
    if (!std::isfinite(s.scale_policy) || s.scale_policy <= 0)
        errors.emplace_back("scale_policy must be finite and positive");
    if (!is_finite(s.star.position) || !std::isfinite(s.star.radius) || s.star.radius <= 0)
        errors.emplace_back("star has invalid position or radius");
    BodyIndex by_id;
    for (const auto& b : s.bodies) {
        if (b.id == 0 || !by_id.emplace(b.id, &b).second)
            errors.emplace_back("body IDs must be unique and non-zero");
        if (!std::isfinite(b.radius) || b.radius <= 0 || !is_finite(b.orbit_offset) || !is_finite(b.orbit_axis) ||
            !std::isfinite(b.orbit_angular_rate))
            errors.push_back(std::format("body {} has invalid finite/radius parameters", b.id));
        if (b.parent_id == b.id)
            errors.push_back(std::format("body {} is its own parent", b.id));
    }
    for (const auto& b : s.bodies) {
        if (b.parent_id && !by_id.count(b.parent_id))
            errors.push_back(std::format("body {} references missing parent", b.id));
        if (has_parent_cycle(b, by_id))
            errors.emplace_back("parent graph contains a cycle");
    }
    for (const auto& belt : s.belts) {
        if (belt.id == 0 || !std::isfinite(belt.inner_radius) || !std::isfinite(belt.outer_radius) ||
            belt.inner_radius <= 0 || belt.outer_radius <= belt.inner_radius || belt.thickness < 0 ||
            !std::isfinite(belt.density) || belt.density < 0)
            errors.emplace_back("belt has invalid bounds or density");
        if (belt.parent_id && !by_id.count(belt.parent_id))
            errors.emplace_back("belt references missing parent");
    }
    return errors;
}

std::vector<BodyState> evaluate_system(const SystemDescription& s, double seconds) {
    std::vector<BodyState> result;
    if (!std::isfinite(seconds) || !validate_system(s).empty())
        return result;
    BodyIndex by_id;
    for (const auto& b : s.bodies)
        by_id[b.id] = &b;
    std::unordered_map<std::uint64_t, Vec3d> positions;
    result.reserve(s.bodies.size());
    for (const auto& b : s.bodies) {
        const double spin = b.rotation_period > epsilon ? 2 * pi<double> * seconds / b.rotation_period : 0.0;
        result.push_back({.id = b.id,
                          .position = resolve_position(b.id, by_id, positions, seconds),
                          .rotation_angle = b.rotation_phase + spin,
                          .radius = b.radius});
    }
    return result;
}

} // namespace space
