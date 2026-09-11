#pragma once

#include "core/math.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Deterministic description and evaluation of the planetary system. Distances
// are artistic scene units, not physical ones; everything here is double
// because the simulation runs for minutes and the tests pin positions to 1e-12.
namespace space {

// The seed the demo ships with; other seeds apply small size variations.
constexpr std::uint64_t showcase_seed = 20260911;

enum class BodyClass { Terrestrial, GasGiant, RockyMoon };

struct BodyDescription {
    std::uint64_t id = 0;
    std::uint64_t parent_id = 0; // zero means the system origin
    BodyClass body_class = BodyClass::Terrestrial;
    double radius = 1.0;
    double axial_tilt = 0.0;
    double rotation_period = 1.0;
    double rotation_phase = 0.0;
    Vec3d orbit_offset{}; // local position at t=0, in the right-handed Y-up frame
    Vec3d orbit_axis{0.0, 1.0, 0.0};
    double orbit_angular_rate = 0.0; // radians per second; intentionally artistic scale
    std::uint64_t material_seed = 0;
    double atmosphere_scale = 0.0;
};

struct StarDescription {
    std::uint64_t id = 0;
    double radius = 1.0;
    double temperature = 5778.0;
    double intensity = 1.0;
    Vec3d position{};
};

struct BeltDescription {
    std::uint64_t id = 0;
    std::uint64_t parent_id = 0;
    double inner_radius = 1.0;
    double outer_radius = 2.0;
    double thickness = 0.1;
    double density = 1.0;
    std::uint64_t seed = 0;
};

struct SystemDescription {
    std::uint32_t schema_version = 1;
    std::uint64_t master_seed = 0;
    StarDescription star{};
    std::vector<BodyDescription> bodies;
    std::vector<BeltDescription> belts;
    double scale_policy = 1.0; // artistic scene units; not a physical distance scale
};

struct BodyState {
    std::uint64_t id = 0;
    Vec3d position{};
    double rotation_angle = 0.0;
    double radius = 0.0;
};

SystemDescription generate_system(std::uint64_t seed);

// Empty when the description is consistent; otherwise one message per problem.
std::vector<std::string> validate_system(const SystemDescription& system);

// Body positions and rotations at the given simulation time. Empty for an
// invalid description or non-finite time.
std::vector<BodyState> evaluate_system(const SystemDescription& system, double seconds);

} // namespace space
