#include "assets/image_io.hpp"
#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace space::render {

namespace {
constexpr AccessScope frame_render_access{
    gpu::Stage::all_commands, gpu::Access::shader_read | gpu::Access::color_write | gpu::Access::depth_stencil_write};
constexpr AccessScope cull_input_access{gpu::Stage::compute | gpu::Stage::vertex,
                                        gpu::Access::shader_read | gpu::Access::shader_write};
} // namespace

void Renderer::Impl::read_gpu_timings() {
    if (!frame_index)
        return;
    for (std::size_t pass = 0; pass < gpu_pass_count; pass++)
        stats.frame.gpu.ms[pass] = timings.milliseconds(GpuPass(pass)).value_or(0.f);
}

bool Renderer::draw(const FrameInput& supplied) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = *impl_;
    std::array<BodyState, max_body_count> ordered;
    const std::span states{ordered.data(), s.body_count};
    ORBITAL_ASSERT(s.showcase.order_states(supplied.bodies, states));
    FrameInput input = supplied;
    input.bodies = states;
    s.submissions.wait_last();
    s.read_gpu_timings();
    s.apply_metering(input.tone);
    const auto drawable = gpu::get_drawable_extent(s.device);
    if (!drawable.x || !drawable.y)
        return false;
    s.set_hdr_output(input.tone.hdr_output);
    s.update_hdr_metadata(input.tone, input.display);
    s.resize({drawable.x, drawable.y}, unsigned(input.sky.galaxy_resolution), unsigned(input.sun.flare_resolution));
    const auto swap = gpu::acquire(s.device);
    if (!swap.render_view)
        return false;
    // The presentation mode the driver granted, once per change: what a frame-rate cap is made of.
    if (const auto info = gpu::get_swapchain_info(s.device);
        info.present_mode != s.logged_swapchain.present_mode || info.image_count != s.logged_swapchain.image_count) {
        s.logged_swapchain = info;
        log::info("Presentation: {} with {} swapchain images",
                  info.present_mode == gpu::PresentMode::fifo      ? "FIFO (vsync)"
                  : info.present_mode == gpu::PresentMode::mailbox ? "mailbox"
                                                                   : "immediate",
                  info.image_count);
    }
    if (swap.extent.x != s.extent.width || swap.extent.y != s.extent.height)
        log::warn("Swapchain {}x{} does not match the frame targets {}x{}", swap.extent.x, swap.extent.y,
                  s.extent.width, s.extent.height);
    const auto prepare_start = std::chrono::steady_clock::now();

    const unsigned history_write = s.frame_index % 2;
    s.bind(Slot::history_a, s.frame_targets.history[history_write]);
    s.bind(Slot::history_b, s.frame_targets.history[1 - history_write]);
    const FrameData frame = s.build_frame(input);
    s.write_body_instances(input, frame);
    s.stats.frame.triangles = 0;
    s.stats.frame.draw_calls = 0;

    // CPU stages frame constants, culling parameters and body instances only.
    // The completed GPU scratch is read before preparing the next submission.
    const auto frame_address = reinterpret_cast<std::uint64_t>(s.buffers.data.range().gpu) + heap_layout.dynamic_offset;
    auto* dynamic = s.buffers.data.range().cpu + heap_layout.dynamic_offset;
    auto* scratch = reinterpret_cast<CullScratch*>(dynamic + heap_layout.cull_offset);
    if (s.frame_index)
        s.read_cull_counts(*reinterpret_cast<const CullScratch*>(s.buffers.cull_readback.range().cpu));
    std::memcpy(dynamic, &frame, sizeof frame);
    const auto cull_address = reinterpret_cast<std::uint64_t>(s.buffers.cull_device.range().gpu);
    s.cull_bodies(input, frame);
    s.write_cull_scratch(input, frame, *scratch,
                         cull_address + heap_layout.instance_offset + s.body_count * sizeof(Instance));
    std::memcpy(dynamic + heap_layout.instance_offset, s.instances.data(), s.instances.size() * sizeof(Instance));
    Root root{.frame = frame_address,
              .vertices = 0,
              .instances = cull_address + heap_layout.instance_offset,
              .base = 0,
              .mode = 0};
    const CullRoot cull_root{.frame = frame_address,
                             .rocks = s.rock_data,
                             .scratch = cull_address + heap_layout.cull_offset,
                             .pass = 0,
                             .unused = 0};
    const std::uint64_t args_address = cull_address + heap_layout.cull_offset + offsetof(CullScratch, args);

    auto* cmd = gpu::begin_commands(s.device);
    {
        GpuTimingFrame frame_timing(s.timings, cmd);
        {
            GpuTimingScope shadow_timing(s.timings, GpuPass::CullAndShadows);
            {
                GpuTimingScope cull_timing(s.timings, GpuPass::Culling);
                gpu::set_texture_descriptor_heap(cmd, gpu::gpu_range(s.buffers.texture_descriptors.get()));
                gpu::set_sampler_descriptor_heap(cmd, gpu::gpu_range(s.buffers.sampler_descriptors.get()));
                synchronize(cmd, frame_render_access, frame_render_access);
                // Transfer the small CPU inputs; generated instances and atomic counters
                // stay device-only. The submission makes preceding host writes available.
                gpu::copy_memory(
                    cmd, {reinterpret_cast<void*>(frame_address + heap_layout.cull_offset), sizeof(CullScratch)},
                    {s.buffers.cull_device.range().gpu + heap_layout.cull_offset, sizeof(CullScratch)});
                gpu::copy_memory(cmd,
                                 {reinterpret_cast<void*>(frame_address + heap_layout.instance_offset),
                                  s.instances.size() * sizeof(Instance)},
                                 {s.buffers.cull_device.range().gpu + heap_layout.instance_offset,
                                  s.instances.size() * sizeof(Instance)});
                synchronize(cmd, access::transfer_write, cull_input_access);
                s.record_cull_passes(cmd, cull_root);
                synchronize(cmd, access::compute_write, access::transfer_read);
                gpu::copy_memory(cmd,
                                 {s.buffers.cull_device.range().gpu + heap_layout.cull_offset, sizeof(CullScratch)},
                                 gpu::gpu_range(s.buffers.cull_readback.get()));
                synchronize(cmd, access::transfer_write, access::host_read);
            }
            {
                GpuTimingScope timing(s.timings, GpuPass::BodyShadows);
                s.record_shadow_pass(cmd, root);
            }
            s.record_belt_maps(cmd, cull_root, root, scratch->params.rock_limit, input.belt.light_map,
                               frame.belt_disc.y);
        }
        {
            GpuTimingScope timing(s.timings, GpuPass::Surface);
            s.record_galaxy_pass(cmd, root, frame); // counted in the group's remainder, not a child
            {
                GpuTimingScope child(s.timings, GpuPass::SurfacePrepass);
                s.record_depth_prepass(cmd, root);
            }
            s.record_scene_pass(cmd, root, input, frame, args_address);
        }
        {
            GpuTimingScope timing(s.timings, GpuPass::Atmosphere);
            {
                GpuTimingScope child(s.timings, GpuPass::Atmospheres);
                s.record_atmosphere_passes(cmd, root);
            }
            {
                GpuTimingScope child(s.timings, GpuPass::BeltDust);
                s.record_belt_dust_passes(cmd, root, input.belt_dust.enabled, frame.belt_disc.y);
            }
            // Coverage is also needed by sun visibility with TAA disabled.
            synchronize(cmd, access::fragment_sample, access::depth_read);
            {
                GpuTimingScope child(s.timings, GpuPass::SplatMask);
                s.record_splat_mask_pass(cmd, root, args_address);
            }
            synchronize(cmd, access::depth_read, access::fragment_sample);
            synchronize(cmd, access::color_write, access::fragment_sample);
        }
        {
            GpuTimingScope timing(s.timings, GpuPass::Post);
            s.record_post_passes(cmd, root, swap.render_view, input.aa.spatial_aa,
                                 input.post.bloom && input.post.bloom_intensity > 0, input.sun.lens_flare,
                                 frame.camera_cell.w > 0, input.ui, dynamic + heap_layout.ui_offset(),
                                 frame_address + heap_layout.ui_offset());
        }
    }
    s.stats.frame.prepare_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - prepare_start).count();
    s.submissions.submit_and_present(s.device, {cmd});

    s.frame_index++;
    s.previous_frame = frame;
    s.previous_camera = input.camera.position;
    s.previous_vertical_fov = input.camera.vertical_fov;
    s.previous_camera_cut = input.camera.cut_serial;
    s.history_valid = true;
    s.collect_memory_stats(); // a few dozen reads; cheaper than tracking when allocations change
    s.stats.frame.draw_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}

void Renderer::set_vsync(bool vsync) {
    gpu::set_vsync(impl_->device, vsync);
}

namespace {
// The capture's conversion of the HDR intermediate: IEEE half to float, and the sRGB transfer.
float half_to_float(std::uint16_t h) {
    const unsigned sign = h >> 15, exponent = (h >> 10) & 0x1f, mantissa = h & 0x3ff;
    float value;
    if (exponent == 0)
        value = std::ldexp(float(mantissa), -24);
    else if (exponent == 31)
        value = mantissa ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
    else
        value = std::ldexp(float(mantissa | 0x400), int(exponent) - 25);
    return sign ? -value : value;
}
float linear_to_srgb(float v) {
    v = std::clamp(v, 0.f, 1.f);
    return v <= .0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.f / 2.4f) - .055f;
}
} // namespace

bool Renderer::capture(const std::filesystem::path& path) {
    auto& s = *impl_;
    if (s.extent.empty())
        return false;
    s.submissions.wait_last();
    auto readback = UniqueGpuHeap::create(s.device, std::uint64_t(s.extent.width) * s.extent.height * (s.hdr() ? 8 : 4),
                                          gpu::MemoryType::readback);
    if (!readback.range().cpu) {
        log::error("screenshot readback allocation failed");
        return false;
    }
    auto* cmd = gpu::begin_commands(s.device);
    synchronize(cmd, access::color_write, access::transfer_read);
    gpu::copy_texture_to_memory(cmd, s.frame_targets.final_image.texture(), gpu::gpu_range(readback.get()));
    synchronize(cmd, access::transfer_write, access::host_read);
    s.submissions.submit_and_wait({cmd});
    const std::size_t pixels = std::size_t(s.extent.width) * s.extent.height;
    // The readback heap is host-visible but uncached, and reading it a value at a
    // time from the conversion loops below took three seconds for a 1080p frame;
    // one sequential copy into ordinary memory first, then the conversion.
    Bytes staged(readback.range().size);
    std::memcpy(staged.data(), readback.range().cpu, staged.size());
    readback.reset();
    Bytes rgb(pixels * 3);
    if (s.hdr()) {
        // The HDR intermediate holds the linear display value over the headroom;
        // the capture is an SDR image, so anything above the tone curve's white
        // clips, as it would on an SDR display.
        const auto* half = reinterpret_cast<const std::uint16_t*>(staged.data());
        const float headroom = std::max(s.previous_frame.display.z, 1.f);
        for (std::size_t i = 0; i < pixels; i++)
            for (unsigned channel = 0; channel < 3; channel++) {
                const float display = half_to_float(half[i * 4 + channel]) * headroom;
                rgb[i * 3 + channel] = std::uint8_t(std::lround(linear_to_srgb(std::min(display, 1.f)) * 255.f));
            }
    } else {
        const auto* rgba = staged.data();
        for (std::size_t i = 0; i < pixels; i++)
            for (unsigned channel = 0; channel < 3; channel++)
                rgb[i * 3 + channel] = rgba[i * 4 + channel];
    }
    return assets::save_png(path, {s.extent, assets::PixelLayout::Rgb8, rgb});
}

} // namespace space::render
