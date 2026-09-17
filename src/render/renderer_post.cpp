#include "render/frame_calculations.hpp"
#include "render/renderer_impl.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace space::render {

namespace {
namespace exposure_meter {
inline constexpr unsigned interval = ORBITAL_METER_PHASES; // frames per histogram cycle, one slice of the taps each
inline constexpr float max_step_seconds = 1.f;             // a stalled frame does not snap the adaptation
} // namespace exposure_meter
} // namespace

void Renderer::Impl::apply_metering(const ToneSettings& tone) {
    if (meter_pending) {
        // The histogram of the last cycle, read by meter_exposure (frame_calculations).
        const auto* histogram = reinterpret_cast<const MeterHistogram*>(buffers.meter_readback.range().cpu);
        const auto reading = meter_exposure(histogram->bins, histogram->peak, tone);
        stats.exposure.ready = true;
        stats.exposure.has_samples = reading.has_samples;
        stats.exposure.luminance = reading.luminance;
        stats.exposure.peak_luminance = reading.peak;
        exposure_target = settle_target(exposure_target, reading.target, tone.adapt_deadzone);
        stats.exposure.target = reading.target;
        stats.exposure.limited = reading.limited;
        static_assert(ExposureStats::histogram_bins == ORBITAL_METER_BINS);
        stats.exposure.stops_min = float(ORBITAL_METER_LOG_MIN);
        stats.exposure.stops_range = float(ORBITAL_METER_LOG_RANGE);
        float total = 0;
        for (const auto weight : histogram->bins)
            total += float(weight);
        for (unsigned bin = 0; bin < ORBITAL_METER_BINS; bin++)
            stats.exposure.histogram[bin] = total > 0 ? float(histogram->bins[bin]) / total : 0.f;
        meter_pending = false;
    }
    // The meter sets the target every few frames; the filter runs toward it every
    // frame, so the exposure glides between readbacks instead of stepping at their
    // cadence, which at the darkening time constant was a third of the gap per step.
    const auto now = std::chrono::steady_clock::now();
    const float dt = meter_time == std::chrono::steady_clock::time_point{}
                         ? exposure_meter::max_step_seconds
                         : std::min(std::chrono::duration<float>(now - meter_time).count(),
                                    exposure_meter::max_step_seconds);
    meter_time = now;
    const float tau = std::max(exposure_target < adapted_exposure ? tone.darken_seconds : tone.brighten_seconds, .05f);
    adapted_exposure += (exposure_target - adapted_exposure) * (1 - std::exp(-dt / tau));
    stats.exposure.adapted = adapted_exposure;
}

void Renderer::Impl::fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                                     bool preserve) {
    gpu::ColorAttachment attachment{.render_view = target.view(),
                                    .load = preserve ? gpu::LoadOp::load : gpu::LoadOp::clear};
    {
        RenderPassScope pass(cmd, {.colors = {&attachment, 1}});
        gpu::bind_pso(cmd, pipeline);
        gpu::draw(cmd, root, 3);
        stats.frame.draw_calls++;
    }
    synchronize(cmd, access::color_write, access::fragment_sample);
}

void Renderer::Impl::record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view,
                                        bool temporal_aa, SpatialAA spatial_aa, bool bloom, bool flare,
                                        bool motion_streaks, const ImDrawData* ui, std::uint8_t* ui_cpu,
                                        std::uint64_t ui_gpu) {
    const unsigned history_write = frame_index % 2;
    root.mode = 0;
    // Off, the pass would only copy the scene; the frame bound the history slot to the scene
    // target instead, and the composite and the meter read that.
    if (temporal_aa) {
        GpuTimingScope timing(timings, GpuPass::Temporal);
        fullscreen_pass(cmd, frame_targets.history[history_write], pso.post.temporal, root);
    }
    if (motion_streaks) {
        GpuTimingScope timing(timings, GpuPass::MotionStreaks);
        record_motion_streaks(cmd, root, temporal_aa);
    }
    if (bloom) { // off, the composite does not read the halo, so its images may hold stale content
        GpuTimingScope timing(timings, GpuPass::Bloom);
        root.mode = std::uint32_t(BloomMode::prefilter);
        fullscreen_pass(cmd, frame_targets.bloom_a, pso.post.bloom, root);
        root.mode = std::uint32_t(BloomMode::horizontal);
        fullscreen_pass(cmd, frame_targets.bloom_b, pso.post.bloom, root);
        // The horizontal pass sampled A; finish that read before reusing A as a target.
        synchronize(cmd, access::fragment_sample, access::color_write);
        root.mode = std::uint32_t(BloomMode::vertical);
        fullscreen_pass(cmd, frame_targets.bloom_a, pso.post.bloom, root);
    }
    root.mode = 0;
    {
        GpuTimingScope timing(timings, GpuPass::SunVisibility);
        fullscreen_pass(cmd, frame_targets.sun_visibility, pso.post.sun_visibility, root);
    }
    // The soft flare stack at a fraction of the frame; off, the composite does not read it.
    if (flare) {
        GpuTimingScope timing(timings, GpuPass::Flare);
        fullscreen_pass(cmd, frame_targets.flare, pso.post.flare, root);
    }
    {
        // Tone map into the final image, or through an intermediate when a spatial pass follows.
        // The HDR variants of these pipelines target the 16-bit float intermediates.
        GpuTimingScope timing(timings, GpuPass::Composite);
        fullscreen_pass(cmd, spatial_aa != SpatialAA::Off ? frame_targets.ldr : frame_targets.final_image,
                        hdr() ? pso.post.composite_hdr : pso.post.composite, root);
    }
    if (spatial_aa != SpatialAA::Off) {
        GpuTimingScope timing(timings, GpuPass::SpatialAA);
        if (spatial_aa == SpatialAA::FXAA) {
            fullscreen_pass(cmd, frame_targets.final_image, hdr() ? pso.post.fxaa_hdr : pso.post.fxaa, root);
        } else {
            // SMAA: edges, blending weights, neighbourhood blend (modes 0, 1, 2 of smaa.slang).
            root.mode = 0;
            fullscreen_pass(cmd, frame_targets.smaa_edges, pso.post.smaa_edges, root);
            root.mode = 1;
            fullscreen_pass(cmd, frame_targets.smaa_weights, pso.post.smaa_weights, root);
            root.mode = 2;
            fullscreen_pass(cmd, frame_targets.final_image, hdr() ? pso.post.smaa_blend_hdr : pso.post.smaa_blend,
                            root);
        }
    }
    {
        // The exposure histogram: one slice of the tap grid a frame, zeroed at the
        // start of a cycle and read back at its end, so no frame carries the whole
        // meter. Its timing scope brackets the slice, and the copies on the cycle's ends.
        GpuTimingScope timing(timings, GpuPass::Meter);
        const unsigned phase = frame_index % exposure_meter::interval;
        const gpu::GpuRange histogram = gpu::gpu_range(buffers.meter_device.get());
        if (phase == 0) {
            gpu::copy_memory(cmd, gpu::gpu_range(buffers.meter_zero.get()), histogram);
            synchronize(cmd, access::transfer_write, access::compute_read_write);
        }
        const MeterRoot meter_root{.frame = root.frame,
                                   .histogram = reinterpret_cast<std::uint64_t>(histogram.gpu),
                                   .phase = phase,
                                   .unused = 0};
        const unsigned taps_x = (extent.width + 3) / 4;
        const unsigned rows = ((extent.height + 3) / 4 + exposure_meter::interval - 1) / exposure_meter::interval;
        gpu::bind_pso(cmd, pso.post.meter);
        gpu::dispatch(cmd, meter_root, {(taps_x + 7) / 8, (rows + 7) / 8, 1});
        if (phase + 1 == exposure_meter::interval) {
            synchronize(cmd, access::compute_write, access::transfer_read);
            gpu::copy_memory(cmd, histogram, gpu::gpu_range(buffers.meter_readback.get()));
            synchronize(cmd, access::transfer_write, access::host_read);
            meter_pending = true;
        } else {
            synchronize(cmd, access::compute_write, access::compute_read_write);
        }
    }
    gpu::ColorAttachment color{.render_view = swapchain_view, .load = gpu::LoadOp::clear};
    {
        GpuTimingScope timing(timings, GpuPass::Present);
        RenderPassScope pass(cmd, {.colors = {&color, 1}});
        gpu::bind_pso(cmd, present_pso());
        gpu::draw(cmd, root, 3);
        record_ui(cmd, ui, ui_cpu, ui_gpu);
    }
}

} // namespace space::render
