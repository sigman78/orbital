#pragma once
#include "render/renderer.hpp"
#include <algorithm>
#include <cstddef>

namespace space::app {

// Presentation-only smoothing of the panel's numbers: each reading eases toward
// the latest sample by the share of the time constant that has passed, so the
// numbers settle in about half a second at any frame rate. Not an exact average
// and not meant to be; the graph shows the raw frames, and the renderer's stats
// and the benchmark stay raw. The first sample after a camera cut is taken as
// is, since the old view's numbers say nothing about the new one.
struct SmoothedStats {
    static constexpr float time_constant_seconds = .5f;

    render::PassTimings gpu;
    float frame_ms = 0, prepare_ms = 0, rocks = 0, triangles = 0;
    std::size_t cut_serial = 0;
    bool primed = false;

    void add(const render::Stats& sample, double dt_seconds, std::size_t cut) {
        if (sample.frame_ms <= 0)
            return;
        if (cut != cut_serial) {
            cut_serial = cut;
            primed = false;
        }
        const float a = primed ? std::clamp(float(dt_seconds) / time_constant_seconds, 0.f, 1.f) : 1.f;
        primed = true;
        const auto ease = [a](float& value, float latest) { value += a * (latest - value); };
        for (std::size_t i = 0; i < render::gpu_pass_count; i++)
            ease(gpu.ms[i], sample.gpu.ms[i]);
        ease(frame_ms, sample.frame_ms);
        ease(prepare_ms, sample.prepare_ms);
        ease(rocks, float(sample.visible_asteroids));
        ease(triangles, float(sample.triangles));
    }
    // The latest stats with the smoothed readings in place of the raw ones.
    render::Stats apply(render::Stats latest) const {
        if (!primed)
            return latest;
        latest.gpu = gpu;
        latest.frame_ms = frame_ms;
        latest.prepare_ms = prepare_ms;
        latest.visible_asteroids = unsigned(rocks + .5f);
        latest.triangles = unsigned(triangles + .5f);
        return latest;
    }
};

} // namespace space::app
