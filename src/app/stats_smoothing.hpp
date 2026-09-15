#pragma once
#include "render/renderer.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace space::app {

// Presentation-only smoothing of the panel's readings: an exponential moving
// average over every timing and count, so the numbers hold still enough to
// read while the graph keeps showing raw frames. The renderer's stats and the
// benchmark samples stay raw. The average is timed in seconds, not frames, so
// it settles at the same pace at any frame rate; the first sample primes it
// and a camera cut restarts it, since the old view's numbers say nothing about
// the new one.
class StatsSmoother {
public:
    static constexpr double time_constant_seconds = .5;

    void add(const render::Stats& sample, double dt_seconds, std::size_t cut_serial) {
        if (sample.frame_ms <= 0)
            return;
        if (cut_serial != cut_serial_) {
            cut_serial_ = cut_serial;
            primed_ = false;
        }
        const Values values = collect(sample);
        if (!primed_) {
            values_ = values;
            primed_ = true;
            return;
        }
        const double alpha = 1 - std::exp(-std::max(dt_seconds, 0.0) / time_constant_seconds);
        for (std::size_t i = 0; i < values.size(); i++)
            values_[i] += alpha * (values[i] - values_[i]);
    }
    render::Stats apply(render::Stats latest) const {
        if (!primed_)
            return latest;
        std::size_t i = 0;
        visit(latest, [&](auto& field) {
            using Field = std::remove_reference_t<decltype(field)>;
            if constexpr (std::is_integral_v<Field>)
                field = Field(std::llround(values_[i++]));
            else
                field = Field(values_[i++]);
        });
        return latest;
    }
    void reset() { primed_ = false; }

private:
    // Every reading the panel shows as a number: the CPU times, each GPU pass
    // and the counts from culling and drawing. Anything not listed passes
    // through apply() untouched.
    template <class Stats, class Visitor> static void visit(Stats& stats, Visitor&& visitor) {
        visitor(stats.frame_ms);
        visitor(stats.prepare_ms);
        for (auto& ms : stats.gpu.ms)
            visitor(ms);
        visitor(stats.visible_asteroids);
        visitor(stats.triangles);
        visitor(stats.rock_triangles);
        visitor(stats.draw_calls);
        visitor(stats.rock_groups_drawn);
    }
    static constexpr std::size_t value_count = 2 + render::gpu_pass_count + 5;
    using Values = std::array<double, value_count>;
    static Values collect(const render::Stats& stats) {
        Values values{};
        std::size_t i = 0;
        visit(stats, [&](const auto& field) { values[i++] = double(field); });
        return values;
    }
    Values values_{};
    std::size_t cut_serial_ = 0;
    bool primed_ = false;
};

} // namespace space::app
