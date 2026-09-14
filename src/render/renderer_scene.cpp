#include "render/renderer_impl.hpp"

#include <cmath>

namespace space::render {

void Renderer::Impl::draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base,
                               unsigned instance_count) {
    root.base = base;
    root.vertices = mesh.vertices;
    stats.draw_calls++;
    gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(mesh.indices), std::uint64_t(mesh.index_count) * 4},
                      gpu::IndexType::uint32, mesh.index_count, instance_count);
    stats.triangles += mesh.index_count / 3 * instance_count;
}

void Renderer::Impl::record_shadow_pass(gpu::CommandBuffer* cmd, Root root) {
    root.mode = std::uint32_t(SurfaceMode::shadow);
    {
        RenderPassScope pass(cmd,
                             {.depth = {.render_view = fixed_targets.shadow_map.view(), .load = gpu::LoadOp::clear}});
        gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
        gpu::bind_pso(cmd, pso.scene.shadow);
        const unsigned triangles_before = stats.triangles;
        for (unsigned i = 0; i < body_count; i++)
            draw_mesh(cmd, root, body_mesh(i, 2), i, 1);
        stats.triangles = triangles_before; // shadow geometry is not counted in the frame statistic
    }
    synchronize(cmd, access::depth_write, access::fragment_sample);
}

void Renderer::Impl::record_galaxy_pass(gpu::CommandBuffer* cmd, Root root, const FrameData& frame) {
    if (splat_count && frame.galaxy.w < .5f &&
        frame.stars.w > 0) { // the Milky Way's splats summed at a quarter of the frame for the background
        Root galaxy_root = root;
        galaxy_root.vertices = splat_data;
        galaxy_root.instances = splat_data + std::uint64_t(splat_count) * 64; // the cell table after the records
        galaxy_root.base = std::uint32_t(frame.galaxy.x);                     // the splats drawn, the first n by energy
        fullscreen_pass(cmd, frame_targets.galaxy, pso.scene.galaxy, galaxy_root);
    }
}

void Renderer::Impl::record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input,
                                       const FrameData& frame, std::uint64_t args_address) {
    root.mode = std::uint32_t(SurfaceMode::opaque);
    gpu::ColorAttachment color{.render_view = frame_targets.hdr.view(), .load = gpu::LoadOp::clear};
    {
        RenderPassScope pass(cmd, {.colors = {&color, 1},
                                   .depth = {.render_view = frame_targets.depth.view(), .load = gpu::LoadOp::clear}});
        gpu::bind_pso(cmd, pso.scene.background);
        gpu::draw(cmd, root, 3);
        stats.draw_calls++;
        // The catalogue stars over the background, before the bodies paint over them.
        if (star_count && frame.stars.y > 0) {
            gpu::bind_pso(cmd, pso.scene.stars);
            Root star_root = root;
            star_root.vertices = star_data;
            gpu::draw(cmd, star_root, 6, star_count);
            stats.draw_calls++;
        }
        gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
        for (unsigned i = 0; i < body_count; i++) {
            const float distance = float(length(input.bodies[i].position - input.camera.position));
            const float projected = float(input.bodies[i].radius) * float(extent.height) /
                                    (distance * frame.right_tan.w);
            gpu::bind_pso(cmd, surface_pso(surface_kind(system.bodies[i].body_class)));
            draw_mesh(cmd, root, body_mesh(i, geometry::select_lod(projected, 2)), i, 1);
        }
        draw_rock_batch(cmd, root, args_address);
        gpu::bind_pso(cmd, pso.scene.cloud);
        root.mode = std::uint32_t(SurfaceMode::cloud);
        draw_mesh(cmd, root, spheres[geometry::lod_count - 1], showcase.earth(), 1);
    }
}

void Renderer::Impl::record_atmosphere_passes(gpu::CommandBuffer* cmd, Root& root) {
    synchronize(cmd, access::color_write, access::color_blend);
    // The atmosphere passes and the temporal pass read the scene depth.
    synchronize(cmd, access::depth_write, access::fragment_sample);
    // Atmosphere is composited per body; the depth clamp orders them against
    // geometry, and Earth's goes last so it stays on top where shells overlap.
    root.mode = 0;
    for (unsigned body : {showcase.giant(), showcase.desert(), showcase.earth()}) {
        root.base = body;
        fullscreen_pass(cmd, frame_targets.hdr, pso.scene.atmosphere, root, true);
    }
}

void Renderer::Impl::record_motion_streaks(gpu::CommandBuffer* cmd, Root& root) {
    // TAA has consumed scene HDR. Reuse it for this frame's transient streaks,
    // retaining scene depth but keeping the effect out of temporal history.
    synchronize(cmd, access::fragment_sample, access::color_write);
    synchronize(cmd, access::fragment_sample, access::depth_read);
    gpu::ColorAttachment color{
        .render_view = frame_targets.hdr.view(), .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
    {
        RenderPassScope pass(cmd, {.colors = {&color, 1},
                                   .depth = {.render_view = frame_targets.depth.view(), .load = gpu::LoadOp::load}});
        gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
        gpu::bind_pso(cmd, pso.scene.motes);
        root.mode = 0;
        gpu::draw(cmd, root, 6, targets::mote_count);
        stats.draw_calls++;
    }
    synchronize(cmd, access::depth_read, access::fragment_sample);
    synchronize(cmd, access::color_write, access::fragment_sample);
}

} // namespace space::render
