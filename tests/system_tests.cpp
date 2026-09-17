#include "scene/system.hpp"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace space;
    const auto a = generate_system(42), b = generate_system(42), c = generate_system(43);
    assert(validate_system(a).empty());
    assert(a.bodies.size() >= 3 && a.belts.size() >= 1);
    assert(a.bodies[0].id == b.bodies[0].id && a.bodies[0].material_seed == b.bodies[0].material_seed);
    assert(a.bodies[0].material_seed != c.bodies[0].material_seed);
    const auto t0 = evaluate_system(a, 0), t1 = evaluate_system(a, 1000), t0again = evaluate_system(a, 0);
    assert(t0.size() == 7 && t0again.size() == t0.size());
    assert(std::abs(t0[0].position.x) < 1e-12 && std::abs(t0[1].position.x - 75) < 1e-12 &&
           std::abs(t0[2].position.x + 65) < 1e-12);
    assert(std::abs(t0[0].position.y) < 1e-12 && std::abs(t0[1].position.y - 20) < 1e-12 &&
           std::abs(t0[2].position.y - 8) < 1e-12);
    assert(std::abs(t0[2].position.z + 45) < 1e-12 && std::abs(t1[2].position.x - t0[2].position.x) > 1e-6);
    assert(a.bodies[0].radius > 2.4 && a.bodies[1].radius > 27.0 && a.bodies[1].radius / a.bodies[0].radius > 10.0);
    assert(a.belts[0].inner_radius == 42.0 && a.belts[0].outer_radius == 62.0);
    // Mars carries two moonlets that orbit it: their offset from Mars keeps its
    // length while its direction turns.
    assert(a.bodies[3].body_class == BodyClass::Desert && a.bodies[4].parent_id == a.bodies[3].id &&
           a.bodies[5].parent_id == a.bodies[3].id);
    for (std::size_t moon : {std::size_t(4), std::size_t(5)}) {
        const double r0 = length(t0[moon].position - t0[3].position), r1 = length(t1[moon].position - t1[3].position);
        assert(std::abs(r0 - r1) < 1e-9 && r0 > a.bodies[3].radius * 2);
        assert(length((t1[moon].position - t1[3].position) - (t0[moon].position - t0[3].position)) > 1e-3);
    }
    assert(t0[0].position.x == t0again[0].position.x);
    auto invalid = a;
    invalid.bodies[0].radius = -1;
    assert(!validate_system(invalid).empty());
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        for (auto member :
             {&BodyDescription::rotation_phase, &BodyDescription::rotation_period, &BodyDescription::axial_tilt}) {
            invalid = a;
            invalid.bodies[0].*member = value;
            assert(!validate_system(invalid).empty());
            assert(evaluate_system(invalid, 0).empty());
        }
        invalid = a;
        invalid.belts[0].thickness = value;
        assert(!validate_system(invalid).empty());
    }
    invalid = a;
    invalid.bodies[0].rotation_period = -1;
    assert(!validate_system(invalid).empty());
    auto stationary = a;
    stationary.bodies[0].rotation_period = 0;
    assert(validate_system(stationary).empty());
    assert(evaluate_system(stationary, 1000)[0].rotation_angle == stationary.bodies[0].rotation_phase);
    return 0;
}
