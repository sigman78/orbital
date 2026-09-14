#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic.hpp"
#include "core/timing.hpp"
#include <chrono>
#include <cstring>

namespace space::render {

void Renderer::Impl::read_gpu_timings() {
    if (!frame_index)
        return;
    const auto* t = reinterpret_cast<const std::uint64_t*>(timestamps.range.cpu);
    const auto caps = gpu::get_device_caps(device);
    const float scale = float(caps.timestamp_period_ns * 1e-6);
    const auto elapsed = [&](unsigned begin, unsigned end) {
        return float(timestamp_ticks(t[begin], t[end], caps.timestamp_valid_bits)) * scale;
    };
    stats.gpu_ms = elapsed(0, 4);
    stats.shadow_ms = elapsed(0, 1);
    stats.surface_ms = elapsed(1, 2);
    stats.atmosphere_ms = elapsed(2, 3);
    stats.post_ms = elapsed(3, 4);
}

bool Renderer::draw(const FrameInput& input) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = *impl_;
    ORBITAL_ASSERT(input.bodies.size() == s.body_count);
    gpu::wait_timeline({s.timeline, s.serial});
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
    s.bind(Slot::history_a, s.history[history_write]);
    s.bind(Slot::history_b, s.history[1 - history_write]);
    const FrameData frame = s.build_frame(input);
    s.write_body_instances(input, frame);
    s.stats.triangles = 0;
    s.stats.draw_calls = 0;

    // Per-frame constants, culling scratch and instances live in the dynamic
    // half of the static heap; the previous frame's culling counts are read
    // before the scratch is reset.
    const auto frame_address = reinterpret_cast<std::uint64_t>(s.data.range.gpu) + heap_layout.dynamic_offset;
    auto* dynamic = s.data.range.cpu + heap_layout.dynamic_offset;
    auto* scratch = reinterpret_cast<CullScratch*>(dynamic + heap_layout.cull_offset);
    s.read_cull_counts(*scratch);
    std::memcpy(dynamic, &frame, sizeof frame);
    s.write_cull_scratch(input, frame, *scratch, frame_address + heap_layout.instance_offset);
    std::memcpy(dynamic + heap_layout.instance_offset, s.instances.data(), s.instances.size() * sizeof(Instance));
    Root root{.frame = frame_address,
              .vertices = 0,
              .instances = frame_address + heap_layout.instance_offset,
              .base = 0,
              .mode = 0};
    const CullRoot cull_root{.frame = frame_address,
                             .rocks = s.rock_data,
                             .scratch = frame_address + heap_layout.cull_offset,
                             .pass = 0,
                             .unused = 0};
    const std::uint64_t args_address = frame_address + heap_layout.cull_offset + offsetof(CullScratch, args);

    auto* cmd = gpu::begin_commands(s.device);
    const auto stamp = [&](unsigned index) {
        ORBITAL_ASSERT(index < targets::timestamp_count);
        gpu::write_timestamp(cmd,
                             reinterpret_cast<gpu::uint64*>(s.timestamps.range.gpu + index * sizeof(std::uint64_t)));
    };
    stamp(0);
    gpu::set_texture_descriptor_heap(cmd, gpu::gpu_range(s.texture_descriptors));
    gpu::set_sampler_descriptor_heap(cmd, gpu::gpu_range(s.sampler_descriptors));
    gpu::barrier(cmd, gpu::Stage::all_commands,
                 gpu::Access::shader_read | gpu::Access::color_write | gpu::Access::depth_stencil_write,
                 gpu::Stage::all_commands,
                 gpu::Access::color_write | gpu::Access::depth_stencil_write | gpu::Access::shader_read);
    s.record_cull_passes(cmd, cull_root);
    s.record_shadow_pass(cmd, root);
    s.record_belt_maps(cmd, cull_root, root, scratch->params.rock_limit, input.belt.light_map, frame.belt_disc.y);
    stamp(1);
    s.record_galaxy_pass(cmd, root, frame);
    s.record_scene_pass(cmd, root, input, frame, args_address);
    stamp(2);
    s.record_atmosphere_passes(cmd, root);
    s.record_belt_dust_passes(cmd, root, input.belt_dust.enabled, frame.belt_disc.y);
    s.record_motion_streaks(cmd, root, input.post);
    // Coverage is also needed by sun visibility with TAA disabled.
    gpu::barrier(cmd, gpu::Stage::fragment, gpu::Access::shader_read, gpu::Stage::depth_stencil_tests,
                 gpu::Access::depth_stencil_read);
    s.record_splat_mask_pass(cmd, root, args_address);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_read, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    stamp(3);
    s.record_post_passes(cmd, root, swap.render_view, input.aa.spatial_aa,
                         input.post.bloom && input.post.bloom_intensity > 0, input.ui,
                         dynamic + heap_layout.ui_offset(), frame_address + heap_layout.ui_offset());
    stamp(4);
    s.stats.prepare_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - prepare_start).count();
    gpu::submit_and_present(s.device, {cmd}, {s.timeline, ++s.serial});

    s.frame_index++;
    s.previous_frame = frame;
    s.previous_camera = input.camera.position;
    s.previous_vertical_fov = input.camera.vertical_fov;
    s.previous_camera_cut = input.camera.cut_serial();
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
    gpu::wait_timeline({s.timeline, s.serial});
    auto readback = gpu::create_gpu_heap(s.device, std::uint64_t(s.extent.width) * s.extent.height * 4,
                                         gpu::MemoryType::readback);
    if (!readback.range.cpu) {
        log::error("screenshot readback allocation failed");
        return false;
    }
    auto* cmd = gpu::begin_commands(s.device);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                 gpu::Access::transfer_read);
    gpu::copy_texture_to_memory(cmd, s.final_image.texture, gpu::gpu_range(readback));
    gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
    gpu::submit({cmd}, {s.timeline, ++s.serial});
    gpu::wait_timeline({s.timeline, s.serial});
    const auto* rgba = reinterpret_cast<const std::uint8_t*>(readback.range.cpu);
    Bytes rgb(std::size_t(s.extent.width) * s.extent.height * 3);
    for (std::size_t i = 0; i < std::size_t(s.extent.width) * s.extent.height; i++)
        for (unsigned channel = 0; channel < 3; channel++)
            rgb[i * 3 + channel] = rgba[i * 4 + channel];
    gpu::destroy_gpu_heap(readback);
    return assets::save_png(path, s.extent, 3, rgb);
}

} // namespace space::render
