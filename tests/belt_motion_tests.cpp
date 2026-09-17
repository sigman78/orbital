#include "scene/belt_motion.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace space;

namespace {

struct Population {
    std::vector<RockSeed> rocks;
    float rates[BeltMotion::bands];
};

Population make_population(unsigned count) {
    Population p;
    std::uint32_t state = 12345;
    const auto next = [&] {
        state = state * 1664525u + 1013904223u;
        return float(state >> 8) / float(1u << 24);
    };
    for (unsigned i = 0; i < count; i++) {
        const float radius = 900.f + 300.f * next(), angle = 6.2831853f * next();
        p.rocks.push_back({.centre = {radius * std::cos(angle), 10.f * (next() - .5f), radius * std::sin(angle)},
                           .spin_axis = {0, 1, 0},
                           .spin_rate = .01f + .05f * next(),
                           .radius = 1 + next(),
                           .band = std::uint8_t(i % BeltMotion::bands),
                           .variant = std::uint8_t(i % 16)});
    }
    for (unsigned band = 0; band < BeltMotion::bands; band++)
        p.rates[band] = .0012f * (1.f + .1f * float(band));
    return p;
}

// The closed form the sweep must reproduce: the centre turned by the band's angle
// at the time, y standing, the phase the rock's rate times the time.
RockState expected(const Population& p, unsigned i, double time) {
    const RockSeed& rock = p.rocks[i];
    const double angle = std::fmod(double(p.rates[rock.band]) * time, 6.283185307179586);
    const float c = float(std::cos(angle)), s = float(std::sin(angle));
    return {.x = rock.centre.x * c - rock.centre.z * s,
            .y = rock.centre.y,
            .z = rock.centre.x * s + rock.centre.z * c,
            .phase = float(double(rock.spin_rate) * time)};
}

// Positions are about 1000 units, so a float rotation moves one by up to 1e-4; the
// stepped checks allow the drift of the steps since a rock's last exact re-seed.
void check_against(const char* label, const Population& p, const std::vector<RockState>& out, unsigned begin,
                   unsigned end, double time, float tolerance) {
    for (unsigned i = begin; i < end; i++) {
        const RockState e = expected(p, i, time);
        const float error = std::max(std::max(std::abs(out[i].x - e.x), std::abs(out[i].y - e.y)),
                                     std::max(std::abs(out[i].z - e.z), std::abs(out[i].phase - e.phase)));
        if (error > tolerance)
            std::printf("%s: rock %u off by %g at t=%g (tolerance %g)\n", label, i, double(error), time,
                        double(tolerance));
        assert(error <= tolerance);
    }
}

void test_seed_and_steps() {
    const auto p = make_population(40000);
    BeltMotion motion(p.rocks, p.rates);
    assert(motion.rocks().size() == 40000 && motion.rocks()[7].variant == 7);
    std::vector<RockState> out(p.rocks.size());
    // The first call seeds exactly at the time given, whatever it is.
    assert(motion.advance(3.25, 40000, out.data()));
    assert(motion.stepped_time() == 3.25);
    check_against("seed", p, out, 0, 40000, 3.25, 1e-3f);
    // A fixed time stands still and does not touch the output.
    out[0].x = 12345.f;
    assert(!motion.advance(3.25, 40000, out.data()));
    assert(out[0].x == 12345.f);
    // Frames advance in whole steps; the remainder waits for the next frame.
    double time = 3.25;
    for (unsigned frame = 0; frame < 300; frame++) {
        time += 1.0 / 60 + 1e-4 * (frame % 3); // an uneven frame time
        assert(motion.advance(time, 40000, out.data()));
        const double lag = time - motion.stepped_time();
        assert(lag >= 0 && lag < BeltMotion::step_seconds);
        check_against("steps", p, out, 0, 40000, motion.stepped_time(), 5e-2f);
    }
    // A jump back re-seeds exactly.
    assert(motion.advance(1.0, 40000, out.data()));
    assert(motion.stepped_time() == 1.0);
    check_against("jump back", p, out, 0, 40000, 1.0, 1e-3f);
    // A jump forward past the step budget re-seeds too.
    assert(motion.advance(100.0, 40000, out.data()));
    assert(motion.stepped_time() == 100.0);
    check_against("jump forward", p, out, 0, 40000, 100.0, 1e-3f);
}

void test_tier_growth() {
    const auto p = make_population(20000);
    BeltMotion motion(p.rocks, p.rates);
    std::vector<RockState> out(p.rocks.size());
    assert(motion.advance(0.0, 5000, out.data()));
    assert(motion.advance(0.0 + 1.0 / 60, 5000, out.data()));
    // More rocks at a standing time: only the new ones are written.
    out[0].x = -1.f;
    assert(motion.advance(motion.stepped_time(), 12000, out.data()));
    assert(out[0].x == -1.f);
    check_against("tier growth", p, out, 5000, 12000, motion.stepped_time(), 1e-3f);
    // More rocks while stepping: the new ones join at the same time as the rest.
    assert(motion.advance(motion.stepped_time() + 1.0 / 60, 20000, out.data()));
    check_against("tier growth, stepping", p, out, 0, 20000, motion.stepped_time(), 5e-2f);
}

void test_drift_bounded() {
    const auto p = make_population(2048);
    BeltMotion motion(p.rocks, p.rates);
    std::vector<RockState> out(p.rocks.size());
    double time = 0;
    motion.advance(time, 2048, out.data());
    for (unsigned frame = 0; frame < 20000; frame++) {
        time += BeltMotion::step_seconds;
        motion.advance(time, 2048, out.data());
    }
    // 20000 steps: the periodic re-seed holds the radius and the phase.
    check_against("drift", p, out, 0, 2048, motion.stepped_time(), 1e-1f);
}

} // namespace

int main() {
    test_seed_and_steps();
    test_tier_growth();
    test_drift_bounded();
    return 0;
}
