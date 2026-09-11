#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace space {

struct Vec3d {
    double x = 0.0, y = 0.0, z = 0.0;
};

Vec3d operator+(Vec3d a, Vec3d b);
Vec3d operator-(Vec3d a, Vec3d b);
Vec3d operator*(Vec3d a, double s);
double dot(Vec3d a, Vec3d b);
double length(Vec3d a);
Vec3d normalized(Vec3d a);

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
std::vector<std::string> validate_system(const SystemDescription& system);
std::vector<BodyState> evaluate_system(const SystemDescription& system, double seconds);

} // namespace space
