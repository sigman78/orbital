#include "render/renderer_impl.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace space::render {

namespace {
namespace exposure_meter {
inline constexpr unsigned interval = 16;                               // frames between readbacks
inline constexpr float min_luminance = 0.004f;                         // darker texels (space) do not vote
inline constexpr float key = 0.18f;                                    // middle gray
inline constexpr float brighten_seconds = 2.5f, darken_seconds = 0.6f; // time constants of the exposure change
inline constexpr float max_step_seconds = 1.f;                         // a stalled frame does not snap the adaptation
inline constexpr Range<float> adapted{0.6f, 1.8f};
} // namespace exposure_meter
} // namespace

void Renderer::Impl::apply_metering() {
    if (meter_pending) {
        // Each meter texel holds log luminance, luminance and its centre weight.
        const auto* values = reinterpret_cast<const float*>(luminance_readback.range.cpu);
        float log_sum = 0, weight_sum = 0;
        for (unsigned i = 0; i < targets::meter_size * targets::meter_size; i++)
            if (values[i * 4 + 1] > exposure_meter::min_luminance) {
                log_sum += values[i * 4] * values[i * 4 + 2];
                weight_sum += values[i * 4 + 2];
            }
        exposure_target = weight_sum > 0
                              ? exposure_meter::adapted.clamp(exposure_meter::key / std::exp(log_sum / weight_sum))
                              : 1.f;
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
    const float tau = exposure_target < adapted_exposure ? exposure_meter::darken_seconds
                                                         : exposure_meter::brighten_seconds;
    adapted_exposure += (exposure_target - adapted_exposure) * (1 - std::exp(-dt / tau));
}

void Renderer::Impl::fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                                     bool preserve) {
    gpu::ColorAttachment attachment{.render_view = target.view,
                                    .load = preserve ? gpu::LoadOp::load : gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
    gpu::bind_pso(cmd, pipeline);
    gpu::draw(cmd, root, 3);
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
}

void Renderer::Impl::record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view,
                                        SpatialAA spatial_aa, bool bloom, bool motion_streaks, const ImDrawData* ui,
                                        std::uint8_t* ui_cpu, std::uint64_t ui_gpu) {
    const unsigned history_write = frame_index % 2;
    root.mode = 0;
    fullscreen_pass(cmd, history[history_write], pso.post.temporal, root);
    if (motion_streaks)
        record_motion_streaks(cmd, root);
    if (bloom) { // off, the composite does not read the halo, so its images may hold stale content
        root.mode = std::uint32_t(BloomMode::prefilter);
        fullscreen_pass(cmd, bloom_a, pso.post.bloom, root);
        root.mode = std::uint32_t(BloomMode::horizontal);
        fullscreen_pass(cmd, bloom_b, pso.post.bloom, root);
        // The horizontal pass sampled A; finish that read before reusing A as a target.
        gpu::barrier(cmd, gpu::Stage::fragment, gpu::Access::shader_read, gpu::Stage::color_output,
                     gpu::Access::color_write);
        root.mode = std::uint32_t(BloomMode::vertical);
        fullscreen_pass(cmd, bloom_a, pso.post.bloom, root);
    }
    root.mode = 0;
    fullscreen_pass(cmd, sun_visibility, pso.post.sun_visibility, root);
    // Tone map into the final image, or through an intermediate when a spatial pass follows.
    fullscreen_pass(cmd, spatial_aa != SpatialAA::Off ? ldr : final_image, pso.post.composite, root);
    if (spatial_aa == SpatialAA::FXAA) {
        fullscreen_pass(cmd, final_image, pso.post.fxaa, root);
    } else if (spatial_aa == SpatialAA::SMAA) {
        // SMAA: edges, blending weights, neighbourhood blend (modes 0, 1, 2 of smaa.slang).
        root.mode = 0;
        fullscreen_pass(cmd, smaa_edges, pso.post.smaa_edges, root);
        root.mode = 1;
        fullscreen_pass(cmd, smaa_weights, pso.post.smaa_weights, root);
        root.mode = 2;
        fullscreen_pass(cmd, final_image, pso.post.smaa_blend, root);
    }
    if (frame_index % exposure_meter::interval == 0) {
        fullscreen_pass(cmd, luminance, pso.post.meter, root);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                     gpu::Access::transfer_read);
        gpu::copy_texture_to_memory(cmd, luminance.texture, gpu::gpu_range(luminance_readback));
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
        meter_pending = true;
    }
    gpu::ColorAttachment color{.render_view = swapchain_view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd, {.colors = {&color, 1}});
    gpu::bind_pso(cmd, pso.post.present);
    gpu::draw(cmd, root, 3);
    record_ui(cmd, ui, ui_cpu, ui_gpu);
    gpu::end_render_pass(cmd);
}

} // namespace space::render
