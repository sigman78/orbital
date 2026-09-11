#include "../src/app/camera.hpp"

#include <cassert>
#include <cmath>

using namespace space;
static bool near(double a, double b) {
    return std::abs(a - b) < 1e-6;
}
int main() {
    Camera camera;
    assert(near(length(camera.forward()), 1.0));
    const auto initial = camera.position;
    std::vector<BodyState> bodies{{1, {0, 0, 0}, 0, 2}, {2, {12, 3, -18}, 0, 6}};
    camera.step(1.0, 0.0, Input{1, 0, 0, 0, 0, 1}, bodies);
    assert(length(camera.position - initial) > 0.1);
    camera.position = {0, 0, 0};
    camera.step(0.01, 1.0, Input{}, bodies);
    assert(length(camera.position) >= 2.0 * 1.08 - 1e-6);
    camera.set_orbit_target(0);
    assert(camera.mode() == CameraMode::Orbit);
    const auto before = camera.position;
    camera.step(0.1, 2.0, Input{0, 0, 0, 5, 0, 1}, bodies);
    assert(length(camera.position - before) > 1e-8);
    camera.toggle_tour();
    assert(camera.mode() == CameraMode::Tour);
    camera.step(0.1, 10.0, Input{}, bodies);
    const auto toured = camera.position;
    camera.step(0.1, 10.1, Input{}, bodies);
    assert(length(camera.position - toured) > 1e-8);
    camera.toggle_tour();
    assert(camera.mode() == CameraMode::Free);
    Camera fixed_a, fixed_b;
    fixed_a.set_tour_time(29.999);
    fixed_b.set_tour_time(29.999);
    fixed_a.step(0.016, 0, Input{}, bodies);
    fixed_b.step(0.1, 99, Input{}, bodies);
    assert(length(fixed_a.position - fixed_b.position) < 1e-9);
    fixed_a.set_tour_time(30.001);
    fixed_b.set_tour_time(30.001);
    fixed_a.step(0.016, 0, Input{}, bodies);
    fixed_b.step(0.1, 99, Input{}, bodies);
    assert(length(fixed_a.position - fixed_b.position) < 1e-9);
    Camera moving;
    moving.set_bookmark(0, bodies);
    const auto relative = moving.position - bodies[0].position;
    bodies[0].position = {10, 2, -3};
    moving.set_bookmark(0, bodies);
    assert(length((moving.position - bodies[0].position) - relative) < 1e-9);
    camera.set_bookmark(0, bodies);
    const auto bookmark = camera.position;
    camera.position = {20, 20, 20};
    camera.set_bookmark(0, bodies);
    assert(length(camera.position - bookmark) < 1e-9);
    return 0;
}
