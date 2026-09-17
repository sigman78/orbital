#pragma once
#include "core/math.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace space {

// What the CPU knows about a rock and never changes on its own: the seed of its
// motion. The GPU's static record is written from this once.
struct RockSeed {
    Vec3f centre;             // belt frame, before the band spin
    Vec3f spin_axis;          // unit tumble axis in the rock's Euler frame
    float spin_rate = 0;      // radians per simulation second about it
    float radius = 0;         // world units
    std::uint8_t band = 0;    // radial band, which sets the orbital rate
    std::uint8_t variant = 0; // shape
};

// A rock's moving part, the record the GPU passes read: its position in the
// belt frame after the band spin (y stands for now) and its tumble phase.
struct RockState {
    float x, y, z, phase;
};

// The cull camera as the frustum pass sees the rocks: a rock's state tilted into the
// belt parent's frame and offset by origin is camera relative. Tangents and pixel
// scales mirror the GPU cull's parameters (belt/cull.slang), the caller widens them.
struct BeltCullView {
    Vec3f origin;                   // the belt parent relative to the cull camera
    Vec3f right, up, forward;       // camera basis
    float tan_x = 1, tan_y = 1;     // frustum half tangents
    float scale_x = 1, scale_y = 1; // sqrt(1 + tan^2): a radius's reach across a side plane
    float pixels_per_unit = 1;      // pixels a unit spans at unit depth
    float min_pixels = 0;           // rocks projecting smaller are dropped
    float pad_per_depth = 0;        // pixel padding per unit depth (jitter, expanded splats)
    float tilt_y_scale = 1, tilt_y_from_z = 0, tilt_z_scale = 1; // the belt plane to the parent's frame
};

// The belt's motion on the CPU: two arrays, the seeds that never change and the
// states the sweep advances in fixed steps and hands to the GPU every frame.
// The motion is the banded spin: every rock turns about the belt's axis at its
// band's rate and tumbles about its own axis at its own, so a step is one
// rotation per band and one phase increment per rock, applied to every rock in
// one pass. The seeds serve the exact re-seeds: on a time jump, when the active
// count grows, and for a slice of the population every step, since repeated
// float rotations drift in radius.
class BeltMotion {
public:
    static constexpr unsigned bands = 8;
    static constexpr double step_seconds = 1.0 / 120;
    static constexpr unsigned max_steps = 16;      // a longer gap re-seeds instead of stepping through it
    static constexpr unsigned reseed_period = 256; // steps between exact re-seeds of any one rock

    BeltMotion() = default;
    // rates: the orbital rate of each band, radians per simulation second.
    BeltMotion(std::vector<RockSeed> rocks, std::span<const float, bands> rates);

    unsigned count() const { return unsigned(rocks_.size()); }
    const std::vector<RockSeed>& rocks() const { return rocks_; }
    const std::vector<RockState>& states() const { return states_; }
    double stepped_time() const { return stepped_time_; }
    // Changes whenever a state changes; a caller keeping several output buffers
    // compares it to know whether one still holds the current states.
    std::uint64_t version() const { return version_; }
    // Brings the first count rocks to the simulation time in whole steps and
    // writes their states to out. Returns false when nothing moved and out was
    // not touched (a fixed time), so the previous write still stands.
    bool advance(double time, unsigned count, RockState* out);
    // Writes the first count rocks' current states to out without stepping.
    void write(unsigned count, RockState* out) const;
    // The frustum pass: the ids of the first count rocks whose sphere can be in the
    // view, in id order, into out (room for count). The test is the GPU cull's with
    // a margin, so the GPU decides the same set from the candidates as from the
    // population; it is one branchless sweep over the states.
    unsigned cull(unsigned count, const BeltCullView& view, std::uint32_t* out) const;

private:
    void seed(std::size_t begin, std::size_t end, double time, RockState* out);
    void step(std::size_t begin, std::size_t end, double seconds, RockState* out);
    std::vector<RockSeed> rocks_;
    std::vector<RockState> states_;
    std::vector<float> radii_; // the seeds' radii again, compact for the frustum pass
    float rates_[bands] = {};
    double stepped_time_ = 0;
    std::uint64_t step_index_ = 0;
    std::uint64_t version_ = 0;
    unsigned seeded_count_ = 0; // rocks holding a valid state; the rest are stale
    bool seeded_ = false;
};

} // namespace space
