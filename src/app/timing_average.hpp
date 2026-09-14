#pragma once
#include "render/renderer.hpp"
#include <array>
#include <deque>

namespace space::app {

// Presentation-only smoothing; renderer stats and benchmark samples remain raw.
class TimingAverage {
public:
    void add(const render::Stats& sample) {
        if (sample.frame_ms <= 0)
            return;
        samples_.push_back(sample);
        accumulate(sample, 1);
        while (samples_.size() > 1 && sums_[0] - samples_.front().frame_ms >= window_ms) {
            accumulate(samples_.front(), -1);
            samples_.pop_front();
        }
    }
    render::Stats apply(render::Stats latest) const {
        if (!samples_.empty())
            for (std::size_t i = 0; i < fields.size(); ++i)
                latest.*fields[i] = float(sums_[i] / double(samples_.size()));
        return latest;
    }

private:
    static constexpr double window_ms = 500;
    static constexpr std::array fields{
        &render::Stats::frame_ms,     &render::Stats::gpu_ms,         &render::Stats::shadow_ms,
        &render::Stats::surface_ms,   &render::Stats::atmosphere_ms,  &render::Stats::post_ms,
        &render::Stats::cull_ms,      &render::Stats::body_shadow_ms, &render::Stats::belt_light_ms,
        &render::Stats::belt_disc_ms, &render::Stats::prepare_ms};
    void accumulate(const render::Stats& sample, double sign) {
        for (std::size_t i = 0; i < fields.size(); ++i)
            sums_[i] += sign * (sample.*fields[i]);
    }
    std::deque<render::Stats> samples_;
    std::array<double, fields.size()> sums_{};
};

} // namespace space::app
