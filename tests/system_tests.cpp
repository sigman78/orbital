#include "../src/scene/system.hpp"
#include <cassert>
#include <cmath>

int main() {
    using namespace space;
    const auto a = generate_system(42), b = generate_system(42), c = generate_system(43);
    assert(validate_system(a).empty());
    assert(a.bodies.size() >= 3 && a.belts.size() >= 1);
    assert(a.bodies[0].id == b.bodies[0].id && a.bodies[0].material_seed == b.bodies[0].material_seed);
    assert(a.bodies[0].material_seed != c.bodies[0].material_seed);
    const auto t0 = evaluate_system(a, 0), t1 = evaluate_system(a, 1000), t0again = evaluate_system(a, 0);
    assert(t0.size() == 3 && t0again.size() == t0.size());
    assert(std::abs(t0[0].position.x) < 1e-12 && std::abs(t0[1].position.x - 75) < 1e-12 &&
           std::abs(t0[2].position.x + 65) < 1e-12);
    assert(std::abs(t0[0].position.y) < 1e-12 && std::abs(t0[1].position.y - 20) < 1e-12 &&
           std::abs(t0[2].position.y - 8) < 1e-12);
    assert(std::abs(t0[2].position.z + 45) < 1e-12 && std::abs(t1[2].position.x - t0[2].position.x) > 1e-6);
    assert(a.bodies[0].radius > 2.4 && a.bodies[1].radius > 27.0 && a.bodies[1].radius / a.bodies[0].radius > 10.0);
    assert(a.belts[0].inner_radius == 42.0 && a.belts[0].outer_radius == 62.0);
    assert(t0[0].position.x == t0again[0].position.x);
    auto invalid = a;
    invalid.bodies[0].radius = -1;
    assert(!validate_system(invalid).empty());
    return 0;
}
