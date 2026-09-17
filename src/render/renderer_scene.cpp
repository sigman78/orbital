#include "render/renderer_impl.hpp"

#include "core/small_vec.hpp"

#include <cmath>

namespace space::render {

void Renderer::Impl::draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base,
                               unsigned instance_count) {
    root.base = base;
    root.vertices = mesh.vertices;
    stats.frame.draw_calls++;
    gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(mesh.indices), std::uint64_t(mesh.index_count) * 4},
                      gpu::IndexType::uint32, mesh.index_count, instance_count);
    stats.frame.triangles += mesh.index_count / 3 * instance_count;
}

void Renderer::Impl::record_shadow_pass(gpu::CommandBuffer* cmd, Root root) {
    root.mode = std::uint32_t(SurfaceMode::shadow);
    {
        RenderPassScope pass(cmd,
                             {.depth = {.render_view = fixed_targets.shadow_map.view(), .load = gpu::LoadOp::clear}});
        gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
        gpu::bind_pso(cmd, pso.scene.shadow);
        const unsigned triangles_before = stats.frame.triangles;
        for (unsigned i = 0; i < body_count; i++)
            draw_mesh(cmd, root, body_mesh(i, 2), i, 1);
        stats.frame.triangles = triangles_before; // shadow geometry is not counted in the frame statistic
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

// The bodies' depth ahead of the scene pass, so the surface shaders run once
// per pixel: rocks in front of a body reject the body's pixels and a body in
// front of another rejects those, whichever order they are drawn in. The same
// vertex shader and matrices give the same depth, so the scene pass passes its
// own pixels on equal depth.
void Renderer::Impl::record_depth_prepass(gpu::CommandBuffer* cmd, Root root) {
    root.mode = std::uint32_t(SurfaceMode::opaque);
    {
        RenderPassScope pass(cmd, {.depth = {.render_view = frame_targets.depth.view(),
                                             .load = gpu::LoadOp::clear,
                                             .clear = 0}}); // reversed-Z: far is 0
        gpu::set_depth_stencil(
            cmd, {.depth_test = true, .depth_write = true, .depth_compare = gpu::CompareOp::greater_equal});
        gpu::bind_pso(cmd, pso.scene.depth_prepass);
        const unsigned triangles_before = stats.frame.triangles;
        for (unsigned i = 0; i < body_count; i++)
            if (body_visible[i])
                draw_mesh(cmd, root, body_mesh(i, body_level[i]), i, 1);
        stats.frame.triangles = triangles_before; // counted once, in the scene pass
    }
    synchronize(cmd, access::depth_write, access::depth_read);
}

// Opaque geometry first, then the sky depth-tested behind it, then everything
// that blends: the rock splats, the cloud shell and the stars. The sky and the
// blends never write depth. Bodies outside the view frustum are skipped, with
// their pre-pass, atmosphere and clouds.
void Renderer::Impl::record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input,
                                       const FrameData& frame, std::uint64_t args_address) {
    (void)input;
    root.mode = std::uint32_t(SurfaceMode::opaque);
    gpu::ColorAttachment color{.render_view = frame_targets.hdr.view(), .load = gpu::LoadOp::clear};
    {
        RenderPassScope pass(cmd, {.colors = {&color, 1},
                                   .depth = {.render_view = frame_targets.depth.view(), .load = gpu::LoadOp::load}});
        gpu::set_depth_stencil(
            cmd, {.depth_test = true, .depth_write = true, .depth_compare = gpu::CompareOp::greater_equal});
        // The timing children sit inside the pass; draws overlap in the pipeline, so
        // each child is where its commands were issued rather than an exact cost.
        {
            GpuTimingScope rock_batch(timings, GpuPass::SurfaceRocks);
            draw_rock_meshes(cmd, root, args_address, pso.scene.surface_rock);
        }
        {
            GpuTimingScope bodies(timings, GpuPass::SurfaceBodies);
            for (unsigned i = 0; i < body_count; i++) {
                if (!body_visible[i])
                    continue;
                gpu::bind_pso(cmd, surface_pso(surface_kind(system.bodies[i].body_class)));
                root.detail = body_detail[i];
                root.flags = i == showcase.earth() && !body_shell[i] ? ORBITAL_ROOT_FOLD_CLOUDS : 0;
                draw_mesh(cmd, root, body_mesh(i, body_level[i]), i, 1);
            }
            root.detail = 0;
            root.flags = 0;
        }
        gpu::set_depth_stencil(
            cmd, {.depth_test = true, .depth_write = false, .depth_compare = gpu::CompareOp::greater_equal});
        {
            GpuTimingScope sky(timings, GpuPass::SurfaceSky);
            gpu::bind_pso(cmd, pso.scene.background); // at the far plane: only where nothing was drawn
            gpu::draw(cmd, root, 3);
            stats.frame.draw_calls++;
            if (star_count && frame.stars.y > 0) {
                gpu::bind_pso(cmd, pso.scene.stars);
                Root star_root = root;
                star_root.vertices = star_data;
                gpu::draw(cmd, star_root, 6, star_count);
                stats.frame.draw_calls++;
            }
        }
        {
            GpuTimingScope splats(timings, GpuPass::SurfaceSplats);
            draw_rock_splats(cmd, root, args_address);
        }
        // The cloud shell at the Earth's own mesh level (the same subdivision, so it
        // stays a uniform 0.9 percent above the ground), while the tier draws it.
        if (const unsigned earth = showcase.earth(); body_visible[earth] && body_shell[earth]) {
            GpuTimingScope clouds(timings, GpuPass::SurfaceClouds);
            gpu::bind_pso(cmd, pso.scene.cloud);
            root.mode = std::uint32_t(SurfaceMode::cloud);
            root.detail = body_detail[earth];
            draw_mesh(cmd, root, spheres[body_level[earth]], earth, 1);
            root.detail = 0;
        }
    }
}

// The opaque meshes again at equal depth, each vertex placed where it is and
// where it was a frame ago, into the motion target the temporal pass reads. The
// same vertex placement as the scene pass, so the depth test passes exactly the
// visible surface; a triangle at the far plane then fills the rest with the sky's.
void Renderer::Impl::record_motion_pass(gpu::CommandBuffer* cmd, Root root, std::uint64_t args_address) {
    synchronize(cmd, access::depth_write, access::depth_read);
    root.mode = std::uint32_t(SurfaceMode::opaque);
    gpu::ColorAttachment color{.render_view = frame_targets.motion.view(), .load = gpu::LoadOp::clear};
    RenderPassScope pass(
        cmd, {.colors = {&color, 1}, .depth = {.render_view = frame_targets.depth.view(), .load = gpu::LoadOp::load}});
    gpu::set_depth_stencil(cmd,
                           {.depth_test = true, .depth_write = false, .depth_compare = gpu::CompareOp::greater_equal});
    const unsigned triangles_before = stats.frame.triangles, draws_before = stats.frame.draw_calls;
    draw_rock_meshes(cmd, root, args_address, pso.scene.motion);
    gpu::bind_pso(cmd, pso.scene.motion);
    for (unsigned i = 0; i < body_count; i++)
        if (body_visible[i])
            draw_mesh(cmd, root, body_mesh(i, body_level[i]), i, 1);
    gpu::bind_pso(cmd, pso.scene.motion_sky);
    root.mode = std::uint32_t(SurfaceMode::motion_sky);
    gpu::draw(cmd, root, 3);
    stats.frame.triangles = triangles_before; // counted once, in the scene pass
    stats.frame.draw_calls = draws_before;
}

void Renderer::Impl::record_atmosphere_passes(gpu::CommandBuffer* cmd, Root& root) {
    synchronize(cmd, access::color_write, access::color_blend);
    // The atmosphere passes and the temporal pass read the scene depth.
    synchronize(cmd, access::depth_write, access::fragment_sample);
    // Atmosphere is composited per body; the depth clamp orders them against
    // geometry, and Earth's goes last so it stays on top where shells overlap.
    root.mode = 0;
    SmallVec<unsigned, 4> bodies;
    if (showcase.has_minor_planet())
        bodies.push_back(showcase.minor_planet());
    for (unsigned body : {showcase.giant(), showcase.desert(), showcase.earth()})
        bodies.push_back(body);
    for (unsigned body : bodies) {
        if (!body_visible[body])
            continue; // the shell is inside the culled sphere
        root.base = body;
        fullscreen_pass(cmd, frame_targets.hdr, pso.scene.atmosphere, root, true);
    }
}

void Renderer::Impl::record_motion_streaks(gpu::CommandBuffer* cmd, Root& root, bool temporal_aa) {
    // This frame's transient streaks go into a target the composite does not read as the
    // scene, retaining scene depth but keeping the effect out of temporal history: with
    // TAA on the scene HDR, which the temporal pass has consumed; off, the scene HDR is
    // the resolved image itself, so the idle history target takes them (sampleHDR reads
    // the one the frame bound as history B).
    synchronize(cmd, access::fragment_sample, access::color_write);
    synchronize(cmd, access::fragment_sample, access::depth_read);
    gpu::ColorAttachment color{
        .render_view = (temporal_aa ? frame_targets.hdr : frame_targets.history[1 - frame_index % 2]).view(),
        .load = gpu::LoadOp::clear,
        .clear = {0, 0, 0, 0}};
    {
        RenderPassScope pass(cmd, {.colors = {&color, 1},
                                   .depth = {.render_view = frame_targets.depth.view(), .load = gpu::LoadOp::load}});
        gpu::set_depth_stencil(
            cmd, {.depth_test = true, .depth_write = false, .depth_compare = gpu::CompareOp::greater_equal});
        gpu::bind_pso(cmd, pso.scene.motes);
        root.mode = 0;
        gpu::draw(cmd, root, 6, targets::mote_count);
        stats.frame.draw_calls++;
    }
    synchronize(cmd, access::depth_read, access::fragment_sample);
    synchronize(cmd, access::color_write, access::fragment_sample);
}

} // namespace space::render
