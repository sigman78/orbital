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

    float frame_ms = 0;       // the loop's frame delta, what the frame rate is made of
    render::FrameStats frame; // the latest frame stats with the jumping readings eased
    std::size_t cut_serial = 0;
    bool primed = false;

    void add(const render::FrameStats& sample, float sample_frame_ms, double dt_seconds, std::size_t cut) {
        if (cut != cut_serial) {
            cut_serial = cut;
            primed = false;
        }
        const float a = primed ? std::clamp(float(dt_seconds) / time_constant_seconds, 0.f, 1.f) : 1.f;
        const auto ease = [a](float& value, float latest) { value += a * (latest - value); };
        ease(frame_ms, sample_frame_ms);
        ease(frame.draw_ms, sample.draw_ms);
        ease(frame.prepare_ms, sample.prepare_ms);
        ease(frame.belt_ms, sample.belt_ms);
        for (std::size_t i = 0; i < render::gpu_pass_count; i++)
            ease(frame.gpu.ms[i], sample.gpu.ms[i]);
        ease(rocks_, float(sample.visible_asteroids));
        ease(triangles_, float(sample.triangles));
        // The rest changes rarely or is a small count: shown as is.
        frame.rock_triangles = sample.rock_triangles;
        frame.draw_calls = sample.draw_calls;
        frame.rock_groups_drawn = sample.rock_groups_drawn;
        frame.belt_lod = sample.belt_lod;
        frame.rock_candidates = sample.rock_candidates;
        frame.visible_asteroids = unsigned(rocks_ + .5f);
        frame.triangles = unsigned(triangles_ + .5f);
        primed = true;
    }

private:
    float rocks_ = 0, triangles_ = 0; // the counts as they ease, before rounding
};

} // namespace space::app
