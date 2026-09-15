#include "render/frame_calculations.hpp"
#include "scene/system.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace space;
using namespace space::render;

int main() {
    CameraView camera;
    CameraHistory previous{.position = camera.position,
                           .forward = camera.forward,
                           .vertical_fov = camera.vertical_fov,
                           .valid = true,
                           .temporal_aa = true};
    const auto prepare = [&](unsigned frame, bool taa = true) {
        return prepare_camera(camera, previous, {1600, 900}, frame, taa, .02f, 2000.f, 5);
    };
    const auto first = prepare(0);
    assert(first.history_valid && !first.moving);
    const auto cycle = prepare(8);
    for (unsigned i = 0; i < 16; ++i)
        assert(first.projection.m[i] == cycle.projection.m[i]);
    assert(prepare(1).jitter_x != first.jitter_x);
    assert(!prepare(0, false).history_valid && prepare(0, false).jitter_y == 0);
    // Jitter changes projected position by exactly the specified number of pixels.
    const auto plain = prepare(0, false);
    const auto clip = [](const Mat4& m, unsigned row) { return m.at(row, 2) * -10 + m.at(row, 3); };
    const float shift = (clip(first.projection, 1) / clip(first.projection, 3) -
                         clip(plain.projection, 1) / clip(plain.projection, 3)) *
                        450;
    assert(std::abs(shift - first.jitter_y) < 1e-6f);
    camera.cut_serial++;
    assert(!prepare(0).history_valid);
    camera.cut_serial--;
    camera.vertical_fov += .1;
    assert(!prepare(0).history_valid);
    camera.vertical_fov = previous.vertical_fov;
    camera.position = {11, 0, 0};
    assert(!prepare(0).history_valid);
    camera.position = {-1, 2, -6};
    const auto moving = prepare(0);
    assert(moving.history_valid && moving.moving);
    assert((moving.lattice_cell == Vec3f{-1, 0, -2}));
    assert((moving.lattice_offset == Vec3f{4, 2, 4}));
    camera.position = {};
    camera.forward = {1, 0, 0};
    assert(!prepare(0).history_valid);
    camera = {};
    assert(project_sun(camera, {0, 0, -100}, {}, first.tan_half_fov, first.aspect).visible);
    assert(!project_sun(camera, {0, 0, 100}, {}, first.tan_half_fov, first.aspect).visible);
    BodyState blocker{.id = 1, .position = {0, 0, -10}, .radius = 1};
    assert(!project_sun(camera, {0, 0, -100}, {&blocker, 1}, first.tan_half_fov, first.aspect).visible);
    blocker.position.x = 5;
    assert(project_sun(camera, {0, 0, -100}, {&blocker, 1}, first.tan_half_fov, first.aspect).visible);

    const auto system = generate_system(showcase_seed);
    const auto& ring = system.belts.front();
    const Vec3d sun{100, 200, -300};
    const auto near = prepare_belt(ring, {}, sun, true, 1);
    const auto far = prepare_belt(ring, {0, 10000, 0}, sun, true, 1);
    assert(near.disc_weight == 0 && far.disc_weight == 1);
    assert(prepare_belt(ring, {0, 10000, 0}, sun, false, 1).disc_weight == 0);
    assert(near.light_slice_depth > 0 && near.disc.half_x == near.disc.half_y);
    const auto matrix = light_projection(near.light);
    for (float value : matrix.m)
        assert(std::isfinite(value));
    // Orthographic box centre maps to the centre of the map and half-depth.
    const Vec3f centre = near.light.centre;
    const auto projected = [&](unsigned row) {
        return matrix.at(row, 0) * centre.x + matrix.at(row, 1) * centre.y + matrix.at(row, 2) * centre.z +
               matrix.at(row, 3);
    };
    assert(std::abs(projected(0)) < 1e-5 && std::abs(projected(1)) < 1e-5);
    assert(std::abs(projected(2) - .5f) < 1e-5);
    assert(prepare_body_light({1, 0, 0}, {0, 0, 50}, {20, 0, 0}, sun).half_x == 70);
    assert(prepare_body_light({1, 0, 0}, {0, 0, 200}, {20, 0, 0}, sun).half_x == 12);
    assert(prepare_body_light({20, 0, 0}, {0, 0, 200}, {1, 0, 0}, sun).half_x == 10);

    // The exposure meter: a histogram of fixed-point weight over log2 luminance bins.
    ToneSettings tone; // highlight bias .1, strength 1, range .25 to 8
    std::vector<std::uint32_t> bins(ORBITAL_METER_BINS, 0);
    const auto bin_of = [](float luminance) {
        const float position = (std::log2(luminance) - float(ORBITAL_METER_LOG_MIN)) / float(ORBITAL_METER_LOG_RANGE);
        return std::size_t(std::clamp(position * ORBITAL_METER_BINS, 0.f, float(ORBITAL_METER_BINS - 1)));
    };
    const auto centre_of = [](std::size_t bin) {
        return std::exp2(float(ORBITAL_METER_LOG_MIN) +
                         (float(bin) + .5f) * float(ORBITAL_METER_LOG_RANGE) / ORBITAL_METER_BINS);
    };
    const auto bits_of = [](float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    };
    bins[bin_of(.09f)] = 1000; // one level: the metered luminance is that bin's centre, highlight included
    auto reading = meter_exposure(bins, bits_of(.2f), tone);
    assert(reading.has_samples &&
           std::abs(reading.luminance - centre_of(bin_of(.09f)) * (1.f + tone.highlight_bias)) < 1e-6f);
    assert(std::abs(reading.requested - tone.meter_key / reading.luminance) < 1e-5f && reading.peak == .2f);
    std::fill(bins.begin(), bins.end(), 0); // sky: far above the ceiling, clamped and flagged
    bins[bin_of(.003f)] = 1000;
    reading = meter_exposure(bins, bits_of(.01f), tone);
    assert(reading.requested > 8.f && reading.target == 8.f && reading.limited);
    std::fill(bins.begin(), bins.end(),
              0); // a sun in a thousandth of the taps: the band ignores it, the highlight sees it
    bins[bin_of(.01f)] = 9990;
    bins[bin_of(20.f)] = 10;
    reading = meter_exposure(bins, bits_of(20.f), tone);
    assert(std::abs(reading.luminance - (centre_of(bin_of(.01f)) + tone.highlight_bias * centre_of(bin_of(20.f)))) <
           1e-3f);
    tone.highlight_bias = 0.f; // without the highlight the sun's thousandth is outside the band
    assert(std::abs(meter_exposure(bins, bits_of(20.f), tone).luminance - centre_of(bin_of(.01f))) < 1e-6f);
    tone.highlight_bias = .1f;
    tone.adapt_strength = .5f; // half strength halves the excursion in stops
    const auto damped = meter_exposure(bins, bits_of(20.f), tone);
    assert(std::abs(damped.requested - std::sqrt(reading.requested)) < 1e-5f);
    tone.adapt_strength = 0.f;
    assert(meter_exposure(bins, bits_of(20.f), tone).requested == 1.f);
    std::fill(bins.begin(), bins.end(), 0); // no weight at all: no samples, 1x
    reading = meter_exposure(bins, 0, tone);
    assert(!reading.has_samples && reading.requested == 1.f);

    // The dead zone: a request within it leaves the target alone, one beyond it is taken whole.
    assert(settle_target(1.f, 1.2f, .33f) == 1.f && settle_target(1.f, .85f, .33f) == 1.f);
    assert(settle_target(1.f, 1.3f, .33f) == 1.3f && settle_target(1.f, .7f, .33f) == .7f);
    assert(settle_target(2.f, 2.1f, 0.f) == 2.1f); // no dead zone follows every request
    assert(settle_target(0.f, 3.f, .33f) == 3.f);  // an unset target takes the first request
}
