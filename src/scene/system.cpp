#include "scene/system.hpp"

#include "core/small_vec.hpp"

#include <cmath>
#include <format>

namespace space {
namespace {

constexpr double epsilon = 1e-9;

// Hash tags that keep the derived seeds independent of each other, and the
// stable identifiers of the showcase system's objects.
namespace tags {
inline constexpr std::uint64_t variation = 0x4f52424954414cull;  // "ORBITAL"
inline constexpr std::uint64_t material = 0x535552464143454full; // "SURFACEO"
inline constexpr std::uint64_t belt_id = 2001;
inline constexpr std::uint64_t body_ids[3] = {1001, 1002, 1003};
} // namespace tags

// Bodies per system are few (three today), so identifier lookups are linear
// and per-frame evaluation never touches the heap.

std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

// Index of the body with the given id, or the body count when absent.
std::size_t find_body(std::span<const BodyDescription> bodies, std::uint64_t id) {
    for (std::size_t i = 0; i < bodies.size(); ++i)
        if (bodies[i].id == id)
            return i;
    return bodies.size();
}

// A parent chain longer than the body count must contain a cycle.
bool has_parent_cycle(std::span<const BodyDescription> bodies, const BodyDescription& body) {
    std::uint64_t parent = body.parent_id;
    for (std::size_t steps = 0; parent && steps <= bodies.size(); ++steps) {
        const std::size_t index = find_body(bodies, parent);
        if (index == bodies.size())
            return false;
        parent = bodies[index].parent_id;
    }
    return parent != 0;
}

struct ResolvedPosition {
    bool done = false;
    Vec3d position{};
};

// Local orbit position composed with every ancestor, memoized per body index.
Vec3d resolve_position(std::span<const BodyDescription> bodies, std::size_t index, std::span<ResolvedPosition> resolved,
                       double seconds) {
    if (resolved[index].done)
        return resolved[index].position;
    const auto& body = bodies[index];
    Vec3d position = rotate_about(body.orbit_offset, body.orbit_axis, body.orbit_angular_rate * seconds);
    if (body.parent_id)
        position += resolve_position(bodies, find_body(bodies, body.parent_id), resolved, seconds);
    resolved[index] = {.done = true, .position = position};
    return position;
}

} // namespace

SystemDescription generate_system(std::uint64_t seed) {
    // Small size variation for non-showcase seeds, in [-1%, +1%].
    const double variation = seed == showcase_seed ? 0.0
                                                   : (double(mix(seed ^ tags::variation) % 2001) - 1000.0) / 100000.0;
    const auto material = [seed](std::uint64_t id) { return mix(seed ^ id ^ tags::material); };
    SystemDescription s;
    s.master_seed = seed;
    s.star = {.id = 1,
              .radius = 35.0,
              .temperature = 5600.0 + double(mix(seed) % 1800),
              .intensity = 4.0,
              .position = {-3500.0, 1400.0, 1200.0}};
    s.bodies = {
        BodyDescription{.id = tags::body_ids[0],
                        .body_class = BodyClass::Terrestrial,
                        .radius = 2.5 * (1.0 + variation),
                        .axial_tilt = 0.41,
                        .rotation_period = 1600.0,
                        .rotation_phase = -0.95, // aligns the day map with the lighting
                        .orbit_offset = {0, 0, 0},
                        .orbit_axis = {0, 1, 0},
                        .orbit_angular_rate = 0.000004,
                        .material_seed = material(tags::body_ids[0]),
                        .atmosphere_scale = 0.32},
        BodyDescription{.id = tags::body_ids[1],
                        .body_class = BodyClass::GasGiant,
                        .radius = 28.0 * (1.0 + variation * 0.6),
                        .axial_tilt = 0.12,
                        .rotation_period = 1100.0,
                        .orbit_offset = {75, 20, -175},
                        .orbit_axis = {0.2, 1, 0.1},
                        .orbit_angular_rate = 0.000009,
                        .material_seed = material(tags::body_ids[1])},
        BodyDescription{.id = tags::body_ids[2],
                        .body_class = BodyClass::RockyMoon,
                        .radius = 0.68,
                        .axial_tilt = 0.08,
                        .rotation_period = 22000.0,
                        .orbit_offset = {-65, 8, -45},
                        .orbit_axis = {0, 1, 0},
                        .orbit_angular_rate = 0.000025,
                        .material_seed = material(tags::body_ids[2])},
    };
    s.belts = {BeltDescription{.id = tags::belt_id,
                               .parent_id = tags::body_ids[1],
                               .inner_radius = 42.0,
                               .outer_radius = 62.0,
                               .thickness = 1.4,
                               .density = 0.65,
                               .seed = mix(seed ^ tags::belt_id)}};
    return s;
}

ValidationErrors validate_system(const SystemDescription& s) {
    ValidationErrors errors;
    const std::span<const BodyDescription> bodies = s.bodies;
    if (s.schema_version == 0)
        errors.emplace_back("schema_version must be non-zero");
    if (bodies.size() > max_body_count)
        errors.push_back(std::format("at most {} bodies are supported", max_body_count));
    if (!std::isfinite(s.scale_policy) || s.scale_policy <= 0)
        errors.emplace_back("scale_policy must be finite and positive");
    if (!is_finite(s.star.position) || !std::isfinite(s.star.radius) || s.star.radius <= 0)
        errors.emplace_back("star has invalid position or radius");
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& b = bodies[i];
        if (b.id == 0 || find_body(bodies, b.id) != i)
            errors.emplace_back("body IDs must be unique and non-zero");
        if (!std::isfinite(b.radius) || b.radius <= 0 || !is_finite(b.orbit_offset) || !is_finite(b.orbit_axis) ||
            !std::isfinite(b.orbit_angular_rate))
            errors.push_back(std::format("body {} has invalid finite/radius parameters", b.id));
        if (b.parent_id == b.id)
            errors.push_back(std::format("body {} is its own parent", b.id));
        if (b.parent_id && find_body(bodies, b.parent_id) == bodies.size())
            errors.push_back(std::format("body {} references missing parent", b.id));
        if (has_parent_cycle(bodies, b))
            errors.emplace_back("parent graph contains a cycle");
    }
    for (const auto& belt : s.belts) {
        if (belt.id == 0 || !std::isfinite(belt.inner_radius) || !std::isfinite(belt.outer_radius) ||
            belt.inner_radius <= 0 || belt.outer_radius <= belt.inner_radius || belt.thickness < 0 ||
            !std::isfinite(belt.density) || belt.density < 0)
            errors.emplace_back("belt has invalid bounds or density");
        if (belt.parent_id && find_body(bodies, belt.parent_id) == bodies.size())
            errors.emplace_back("belt references missing parent");
    }
    return errors;
}

BodyStates evaluate_system(const SystemDescription& s, double seconds) {
    BodyStates result;
    if (!std::isfinite(seconds) || !validate_system(s).empty())
        return result;
    const std::span<const BodyDescription> bodies = s.bodies;
    SmallVec<ResolvedPosition, max_body_count> resolved;
    for (std::size_t i = 0; i < bodies.size(); ++i)
        resolved.emplace_back();
    result.reserve(bodies.size());
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const auto& b = bodies[i];
        const double spin = b.rotation_period > epsilon ? 2 * pi<double> * seconds / b.rotation_period : 0.0;
        result.push_back({.id = b.id,
                          .position = resolve_position(bodies, i, resolved, seconds),
                          .rotation_angle = b.rotation_phase + spin,
                          .radius = b.radius});
    }
    return result;
}

} // namespace space
