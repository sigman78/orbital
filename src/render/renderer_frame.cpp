#include "assets/image_io.hpp"
#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic.hpp"
#include "core/timing.hpp"
#include <chrono>
#include <cstring>

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
    const auto* t = reinterpret_cast<const std::uint64_t*>(buffers.timestamps.range().cpu);
    const auto caps = gpu::get_device_caps(device);
    const float scale = float(caps.timestamp_period_ns * 1e-6);
    const auto elapsed = [&](unsigned begin, unsigned end) {
        return float(timestamp_ticks(t[begin], t[end], caps.timestamp_valid_bits)) * scale;
    };
    stats.gpu_ms = elapsed(0, 7);
    stats.shadow_ms = elapsed(0, 4);
    stats.cull_ms = elapsed(0, 1);
    stats.body_shadow_ms = elapsed(1, 2);
    stats.belt_light_ms = elapsed(2, 3);
    stats.belt_disc_ms = elapsed(3, 4);
    stats.surface_ms = elapsed(4, 5);
    stats.atmosphere_ms = elapsed(5, 6);
    stats.post_ms = elapsed(6, 7);
}

void Renderer::Impl::stamp(gpu::CommandBuffer* cmd, unsigned index) {
    ORBITAL_ASSERT(index < targets::timestamp_count);
    gpu::write_timestamp(
        cmd, reinterpret_cast<gpu::uint64*>(buffers.timestamps.range().gpu + index * sizeof(std::uint64_t)));
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
    s.apply_metering();
    const auto drawable = gpu::get_drawable_extent(s.device);
    if (!drawable.x || !drawable.y)
        return false;
    s.resize({drawable.x, drawable.y}, unsigned(input.sky.galaxy_resolution));
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
    s.stats.triangles = 0;
    s.stats.draw_calls = 0;

    // CPU stages frame constants, culling parameters and body instances only.
    // The completed GPU scratch is read before preparing the next submission.
    const auto frame_address = reinterpret_cast<std::uint64_t>(s.buffers.data.range().gpu) + heap_layout.dynamic_offset;
    auto* dynamic = s.buffers.data.range().cpu + heap_layout.dynamic_offset;
    auto* scratch = reinterpret_cast<CullScratch*>(dynamic + heap_layout.cull_offset);
    if (s.frame_index)
        s.read_cull_counts(*reinterpret_cast<const CullScratch*>(s.buffers.cull_readback.range().cpu));
    std::memcpy(dynamic, &frame, sizeof frame);
    const auto cull_address = reinterpret_cast<std::uint64_t>(s.buffers.cull_device.range().gpu);
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
    s.stamp(cmd, 0);
    gpu::set_texture_descriptor_heap(cmd, gpu::gpu_range(s.buffers.texture_descriptors.get()));
    gpu::set_sampler_descriptor_heap(cmd, gpu::gpu_range(s.buffers.sampler_descriptors.get()));
    synchronize(cmd, frame_render_access, frame_render_access);
    // Transfer the small CPU inputs; generated instances and atomic counters
    // stay device-only. The submission makes preceding host writes available.
    gpu::copy_memory(cmd, {reinterpret_cast<void*>(frame_address + heap_layout.cull_offset), sizeof(CullScratch)},
                     {s.buffers.cull_device.range().gpu + heap_layout.cull_offset, sizeof(CullScratch)});
    gpu::copy_memory(
        cmd,
        {reinterpret_cast<void*>(frame_address + heap_layout.instance_offset), s.instances.size() * sizeof(Instance)},
        {s.buffers.cull_device.range().gpu + heap_layout.instance_offset, s.instances.size() * sizeof(Instance)});
    synchronize(cmd, access::transfer_write, cull_input_access);
    s.record_cull_passes(cmd, cull_root);
    synchronize(cmd, access::compute_write, access::transfer_read);
    gpu::copy_memory(cmd, {s.buffers.cull_device.range().gpu + heap_layout.cull_offset, sizeof(CullScratch)},
                     gpu::gpu_range(s.buffers.cull_readback.get()));
    synchronize(cmd, access::transfer_write, access::host_read);
    s.stamp(cmd, 1);
    s.record_shadow_pass(cmd, root);
    s.stamp(cmd, 2);
    s.record_belt_maps(cmd, cull_root, root, scratch->params.rock_limit, input.belt.light_map, frame.belt_disc.y);
    s.stamp(cmd, 4);
    s.record_galaxy_pass(cmd, root, frame);
    s.record_scene_pass(cmd, root, input, frame, args_address);
    s.stamp(cmd, 5);
    s.record_atmosphere_passes(cmd, root);
    s.record_belt_dust_passes(cmd, root, input.belt_dust.enabled, frame.belt_disc.y);
    // Coverage is also needed by sun visibility with TAA disabled.
    synchronize(cmd, access::fragment_sample, access::depth_read);
    s.record_splat_mask_pass(cmd, root, args_address);
    synchronize(cmd, access::depth_read, access::fragment_sample);
    synchronize(cmd, access::color_write, access::fragment_sample);
    s.stamp(cmd, 6);
    s.record_post_passes(cmd, root, swap.render_view, input.aa.spatial_aa,
                         input.post.bloom && input.post.bloom_intensity > 0, frame.camera_cell.w > 0, input.ui,
                         dynamic + heap_layout.ui_offset(), frame_address + heap_layout.ui_offset());
    s.stamp(cmd, 7);
    s.stats.prepare_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - prepare_start).count();
    s.submissions.submit_and_present(s.device, {cmd});

    s.frame_index++;
    s.previous_frame = frame;
    s.previous_camera = input.camera.position;
    s.previous_vertical_fov = input.camera.vertical_fov;
    s.previous_camera_cut = input.camera.cut_serial;
    s.history_valid = true;
    s.stats.frame_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}

void Renderer::set_vsync(bool vsync) {
    gpu::set_vsync(impl_->device, vsync);
}

bool Renderer::capture(const std::filesystem::path& path) {
    auto& s = *impl_;
    if (s.extent.empty())
        return false;
    s.submissions.wait_last();
    auto readback = UniqueGpuHeap::create(s.device, std::uint64_t(s.extent.width) * s.extent.height * 4,
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
    const auto* rgba = reinterpret_cast<const std::uint8_t*>(readback.range().cpu);
    Bytes rgb(std::size_t(s.extent.width) * s.extent.height * 3);
    for (std::size_t i = 0; i < std::size_t(s.extent.width) * s.extent.height; i++)
        for (unsigned channel = 0; channel < 3; channel++)
            rgb[i * 3 + channel] = rgba[i * 4 + channel];
    readback.reset();
    return assets::save_png(path, {s.extent, assets::PixelLayout::Rgb8, rgb});
}

} // namespace space::render
