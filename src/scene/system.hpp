#pragma once

#include "core/math.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Deterministic description and evaluation of the planetary system. Distances
// are artistic scene units, not physical ones; everything here is double
// because the simulation runs for minutes and the tests pin positions to 1e-12.
namespace space {

// The seed the demo ships with; other seeds apply small size variations.
constexpr std::uint64_t showcase_seed = 20260911;

// Upper bound validate_system enforces, so per-frame evaluation can use fixed storage.
constexpr std::size_t max_body_count = 8;

// Terrestrial: Earth-like with clouds and a thick atmosphere. Desert: Mars-like
// with a thin dusty one. RockyMoon: a large airless sphere. Moonlet: a small
// irregular captured body rendered as a rock.
enum class BodyClass { Terrestrial, GasGiant, RockyMoon, Desert, Moonlet };

struct BodyDescription {
    std::uint64_t id = 0;
    std::uint64_t parent_id = 0; // zero means the system origin
    BodyClass body_class = BodyClass::Terrestrial;
    double radius = 1.0;
    double axial_tilt = 0.0;
    double rotation_period = 1.0; // finite, nonnegative; zero means no spin
    double rotation_phase = 0.0;
    Vec3d orbit_offset{}; // local position at t=0, in the right-handed Y-up frame
    Vec3d orbit_axis{0.0, 1.0, 0.0};
    double orbit_angular_rate = 0.0; // radians per second; intentionally artistic scale
    std::uint64_t material_seed = 0;
    double atmosphere_scale = 0.0; // descriptive metadata; showcase atmosphere comes from render settings
};

struct StarDescription {
    // The showcase consumes position only; radius/temperature/intensity are
    // descriptive metadata, not controls for its artistic sun and lighting.
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
    double scale_policy = 1.0; // metadata; evaluation/rendering use scene units without applying this multiplier
};

struct BodyState {
    std::uint64_t id = 0;
    Vec3d position{};
    double rotation_angle = 0.0;
    double radius = 0.0;
};

using BodyStates = std::vector<BodyState>;         // one entry per body, in description order
using ValidationErrors = std::vector<std::string>; // empty means valid

SystemDescription generate_system(std::uint64_t seed);

// Empty when the description is consistent; otherwise one message per problem.
ValidationErrors validate_system(const SystemDescription& system);

// Body positions and rotations at the given simulation time. Empty for an
// invalid description or non-finite time.
BodyStates evaluate_system(const SystemDescription& system, double seconds);

} // namespace space
