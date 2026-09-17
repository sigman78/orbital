#include "scene/belt_motion.hpp"

#include <algorithm>
#include <cmath>

namespace space {

namespace {
constexpr double two_pi = 6.283185307179586;

struct BandAngles {
    float cos[BeltMotion::bands], sin[BeltMotion::bands];
};

BandAngles band_angles(const float* rates, double seconds) {
    BandAngles angles;
    for (unsigned band = 0; band < BeltMotion::bands; band++) {
        const double angle = std::fmod(double(rates[band]) * seconds, two_pi);
        angles.cos[band] = float(std::cos(angle));
        angles.sin[band] = float(std::sin(angle));
    }
    return angles;
}
} // namespace

BeltMotion::BeltMotion(std::vector<RockSeed> rocks, std::span<const float, bands> rates)
    : rocks_(std::move(rocks)), states_(rocks_.size()) {
    std::copy(rates.begin(), rates.end(), rates_);
    radii_.reserve(rocks_.size());
    for (const RockSeed& rock : rocks_)
        radii_.push_back(rock.radius);
}

// The exact state at the time: the seed's centre turned by the band's angle, the
// phase the rate times the time.
void BeltMotion::seed(std::size_t begin, std::size_t end, double time, RockState* out) {
    const BandAngles angles = band_angles(rates_, time);
    for (std::size_t i = begin; i < end; i++) {
        const RockSeed& rock = rocks_[i];
        const float c = angles.cos[rock.band], s = angles.sin[rock.band];
        states_[i] = {.x = rock.centre.x * c - rock.centre.z * s,
                      .y = rock.centre.y,
                      .z = rock.centre.x * s + rock.centre.z * c,
                      .phase = float(double(rock.spin_rate) * time)};
        out[i] = states_[i];
    }
}

// One step of the given length from the current state: the position turned on by
// the band's angle for it, the phase advanced by the rock's rate.
void BeltMotion::step(std::size_t begin, std::size_t end, double seconds, RockState* out) {
    const BandAngles angles = band_angles(rates_, seconds);
    const float dt = float(seconds);
    for (std::size_t i = begin; i < end; i++) {
        const RockSeed& rock = rocks_[i];
        RockState& state = states_[i];
        const float c = angles.cos[rock.band], s = angles.sin[rock.band];
        const float x = state.x, z = state.z;
        state.x = x * c - z * s;
        state.z = x * s + z * c;
        state.phase += rock.spin_rate * dt;
        out[i] = state;
    }
}

void BeltMotion::write(unsigned count, RockState* out) const {
    count = std::min(count, this->count());
    std::copy_n(states_.data(), count, out);
}

// Every test is evaluated and the id appended under a predicate: the branches
// mispredict on belt data and cost more than the arithmetic they skip.
unsigned BeltMotion::cull(unsigned count, const BeltCullView& v, std::uint32_t* out) const {
    count = std::min(count, this->count());
    constexpr float radius_margin = 1.02f, radius_slack = 1e-3f; // covers the two sides' float paths
    unsigned kept = 0;
    for (unsigned i = 0; i < count; i++) {
        const RockState& s = states_[i];
        const float r = radii_[i] * radius_margin + radius_slack;
        const float px = v.origin.x + s.x;
        const float py = v.origin.y + s.y * v.tilt_y_scale - s.z * v.tilt_y_from_z;
        const float pz = v.origin.z + s.z * v.tilt_z_scale;
        const float z = v.forward.x * px + v.forward.y * py + v.forward.z * pz;
        const float zc = std::max(z, 0.f);
        const float pixels = r * v.pixels_per_unit / std::max(z, .1f);
        const float padding = r + zc * v.pad_per_depth;
        const float x = std::fabs(v.right.x * px + v.right.y * py + v.right.z * pz);
        const float y = std::fabs(v.up.x * px + v.up.y * py + v.up.z * pz);
        const bool in = (z >= -r) & (pixels >= v.min_pixels) & (x <= zc * v.tan_x + padding * v.scale_x) &
                        (y <= zc * v.tan_y + padding * v.scale_y);
        out[kept] = i;
        kept += in;
    }
    return kept;
}

bool BeltMotion::advance(double time, unsigned count, RockState* out) {
    count = std::min(count, this->count());
    const double gap = time - stepped_time_;
    if (!seeded_ || gap < 0 || gap >= (max_steps + 1) * step_seconds) {
        seed(0, count, time, out);
        stepped_time_ = time;
        step_index_ = 0;
        seeded_count_ = count;
        seeded_ = true;
        ++version_;
        return true;
    }
    const auto steps = unsigned(gap / step_seconds);
    if (steps == 0) {
        if (count <= seeded_count_)
            return false;
        seed(seeded_count_, count, stepped_time_, out); // the tier grew: the new rocks exactly, the rest stand
        seeded_count_ = count;
        ++version_;
        return true;
    }
    if (count > seeded_count_)
        seed(seeded_count_, count, stepped_time_, out);
    const double seconds = steps * step_seconds;
    step(0, count, seconds, out);
    stepped_time_ += seconds;
    step_index_ += steps;
    seeded_count_ = count;
    ++version_;
    // The slice whose turn it is goes back to the exact state.
    const std::size_t slice = (count + reseed_period - 1) / reseed_period;
    const std::size_t begin = std::min<std::size_t>((step_index_ % reseed_period) * slice, count);
    seed(begin, std::min<std::size_t>(begin + slice, count), stepped_time_, out);
    return true;
}

} // namespace space
