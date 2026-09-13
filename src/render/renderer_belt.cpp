#include "render/renderer_impl.hpp"

#include <algorithm>
#include <cmath>

namespace space::render {

// Uploads the static per-rock records the GPU culling pass places each frame.
// The seeded belt order defines quality tiers, so records keep their ids.
void Renderer::Impl::build_belt(const BeltDescription& description) {
    const float inner = float(description.inner_radius), outer = float(description.outer_radius);
    const auto belt = geometry::generate_belt({.seed = description.seed,
                                               .count = std::max(high_quality.belt_count, belt_count_override),
                                               .inner_radius = inner,
                                               .outer_radius = outer,
                                               .thickness = float(description.thickness)});
    std::vector<RockData> records;
    records.reserve(belt.size());
    for (unsigned id = 0; id < belt.size(); ++id) {
        const auto& rock = belt[id];
        const float radial = std::sqrt(rock.position.x * rock.position.x + rock.position.z * rock.position.z);
        const unsigned band = std::min(
            belt::radial_bands - 1,
            unsigned(std::clamp((radial - inner) / std::max(outer - inner, 1e-4f), 0.f, .999999f) *
                     belt::radial_bands));
        const float radius = rock.scale.x * belt::rock_radius_scale;
        records.push_back(
            {.position_radius = {rock.position.x, rock.position.y, rock.position.z, radius},
             .rotation_seed = {rock.rotation.x, rock.rotation.y, rock.rotation.z, 0},
             .spin_group = {rock.spin.x, rock.spin.y, rock.spin.z, float(rock.variant * belt::radial_bands + band)}});
    }
    rock_data = upload_static(bytes_of(records));
    rock_count = unsigned(records.size());
    std::vector<RockData> tail;
    for (unsigned id = 0; id < records.size(); ++id)
        if (records[id].position_radius.w >= belt::map_caster_min_radius) {
            tail.push_back(records[id]);
            rock_tail_ids.push_back(id);
        }
    rock_tail_data = tail.empty() ? rock_data : upload_static(bytes_of(tail));
}

// Statistics from the previous frame's culling, read back once its GPU work
// has completed. The triangle count is what those draws submitted.
void Renderer::Impl::read_cull_counts(const CullScratch& scratch) {
    unsigned rocks_visible = 0, triangles = 0, groups_drawn = 0;
    for (unsigned group = 0; group < rock_group_count; group++) {
        rocks_visible += scratch.counts[group];
        triangles += scratch.counts[group] * (scratch.params.index_counts[group] / 3);
        groups_drawn += scratch.counts[group] ? 1 : 0;
    }
    stats.rock_groups_drawn = groups_drawn; // equals scratch.draw_count once the prefix pass has run
    rocks_visible += scratch.counts[rock_group_count];
    triangles += scratch.counts[rock_group_count] * 2;
    stats.visible_asteroids = rocks_visible;
    stats.rock_triangles = triangles;
}

void Renderer::Impl::record_cull_passes(gpu::CommandBuffer* cmd, const CullRoot& root) {
    constexpr unsigned pass_count = 3, prefix_pass = 1;
    gpu::bind_pso(cmd, pso.belt.cull);
    const unsigned groups = (rock_count + ORBITAL_CULL_THREADS - 1) / ORBITAL_CULL_THREADS;
    for (unsigned pass = 0; pass < pass_count; pass++) {
        CullRoot pass_root = root;
        pass_root.pass = pass;
        gpu::dispatch(cmd, pass_root, {pass == prefix_pass ? 1u : groups, 1, 1});
        const bool last = pass + 1 == pass_count;
        gpu::barrier(cmd, gpu::Stage::compute, gpu::Access::shader_write,
                     last ? gpu::Stage::indirect | gpu::Stage::vertex | gpu::Stage::fragment : gpu::Stage::compute,
                     last ? gpu::Access::indirect_read | gpu::Access::shader_read
                          : gpu::Access::shader_read | gpu::Access::shader_write);
    }
}

void Renderer::Impl::record_belt_maps(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root,
                                      unsigned rock_limit, bool light_map, float far_weight) {
    if (light_map) {
        // Only the size-tail rocks splat; their ids are sorted, so the tier limit is a prefix.
        const auto tail_end = std::upper_bound(rock_tail_ids.begin(), rock_tail_ids.end(), rock_limit - 1);
        const CullRoot splat_root{.frame = root.frame,
                                  .rocks = rock_tail_data,
                                  .scratch = cull_root.scratch,
                                  .pass = unsigned(tail_end - rock_tail_ids.begin()),
                                  .unused = 0};
        record_belt_light_pass(cmd, splat_root, root, splat_root.pass);
    }
    {
        const CullRoot bake_root{
            .frame = root.frame, .rocks = rock_data, .scratch = cull_root.scratch, .pass = rock_limit, .unused = 0};
        record_belt_disc_bakes(cmd, bake_root, root, rock_limit, far_weight);
    }
}

// Belt transmittance map: splat every rock's coverage into its light-space
// column and depth slice, then blur in both directions.
void Renderer::Impl::record_belt_light_pass(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root,
                                            unsigned rock_limit) {
    // Coverage accumulates from zero in every slice, alpha included.
    gpu::ColorAttachment attachment{.render_view = belt_light.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
    gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
    gpu::bind_pso(cmd, pso.belt.splat);
    gpu::draw(cmd, cull_root, 6, rock_limit);
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    root.mode = 0;
    fullscreen_pass(cmd, belt_light_blur, pso.belt.blur, root);
    root.mode = 1;
    fullscreen_pass(cmd, belt_light, pso.belt.blur, root);
}

// Far-belt maps: the sunlight over the belt plane every few frames, and the
// rocks' coverage splatted every few dozen frames, both only while the far
// tier is in use (and once before it first shows).
void Renderer::Impl::record_belt_disc_bakes(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root,
                                            unsigned rock_limit, float far_weight) {
    if (far_weight <= 0)
        return;
    const bool bake_light = !belt_disc_baked || frame_index % targets::belt_disc_light_interval == 0;
    const bool bake_rocks = !belt_disc_baked || frame_index % targets::belt_disc_rock_interval == 0;
    if (bake_light) {
        root.mode = 0;
        fullscreen_pass(cmd, belt_disc_light, pso.belt.disc, root);
    }
    if (bake_rocks) {
        gpu::ColorAttachment attachment{
            .render_view = belt_disc_rocks.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
        gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
        gpu::bind_pso(cmd, pso.belt.disc_splat);
        gpu::draw(cmd, cull_root, 6, rock_limit);
        stats.draw_calls++;
        gpu::end_render_pass(cmd);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
    }
    belt_disc_baked = true;
}

void Renderer::Impl::draw_rock_batch(gpu::CommandBuffer* cmd, Root& root, std::uint64_t args_address) {
    gpu::bind_pso(cmd, pso.scene.surface_rock);
    // Rocks: one multi-draw over the pooled mesh; the culling pass wrote every
    // group's count, base and slice into the argument array.
    root.base = 0;
    root.vertices = rock_pool.vertices;
    gpu::draw_indexed_indirect_count(
        cmd, root, {reinterpret_cast<void*>(rock_pool.indices), std::uint64_t(rock_pool.index_count) * 4},
        gpu::IndexType::uint32, {reinterpret_cast<void*>(args_address), sizeof(DrawArgs) * rock_group_count},
        {reinterpret_cast<void*>(args_address - offsetof(CullScratch, args) + offsetof(CullScratch, draw_count)),
         sizeof(std::uint32_t)},
        rock_group_count, sizeof(DrawArgs));
    stats.draw_calls++;
    stats.triangles += stats.rock_triangles;
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
    gpu::bind_pso(cmd, pso.belt.billboard);
    root.mode = std::uint32_t(SurfaceMode::billboard);
    gpu::draw_indirect(cmd, root,
                       {reinterpret_cast<void*>(args_address + rock_group_count * sizeof(DrawArgs)), sizeof(DrawArgs)},
                       1, sizeof(DrawArgs));
    stats.draw_calls++;
}

// After the atmosphere has read the scene depth, the rock splats write their
// own depth and mark their pixels. The temporal pass then reprojects them
// exactly, as rocks at their distance rather than as sky, and keeps their
// history unclipped: a sub-pixel feature in motion never survives the
// neighbourhood clip, which faded the splats whenever the camera translated.
// The atmosphere must not see this depth, since a splat covers only a
// fraction of its pixels.
void Renderer::Impl::record_splat_mask_pass(gpu::CommandBuffer* cmd, Root root, std::uint64_t args_address) {
    root.mode = std::uint32_t(SurfaceMode::splat_mask);
    root.base = 0;
    gpu::ColorAttachment mask{.render_view = splat_mask.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
    gpu::begin_render_pass(cmd,
                           {.colors = {&mask, 1}, .depth = {.render_view = depth.view, .load = gpu::LoadOp::load}});
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.belt.splat_mask);
    gpu::draw_indirect(cmd, root,
                       {reinterpret_cast<void*>(args_address + rock_group_count * sizeof(DrawArgs)), sizeof(DrawArgs)},
                       1, sizeof(DrawArgs));
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
}

void Renderer::Impl::record_belt_dust_passes(gpu::CommandBuffer* cmd, Root& root, bool enabled, float far_weight) {
    // Belt dust scatters over everything the belt lies in front of, after the atmospheres.
    // Belt dust and the far-belt disc: the near march at half resolution, then
    // one composite that mixes it with the far tier by the LOD weight.
    const bool near_dust = enabled && far_weight < 1;
    if (near_dust) {
        root.mode = 0;
        fullscreen_pass(cmd, belt_dust, pso.belt.dust, root);
    }
    if (near_dust || far_weight > 0) {
        root.mode = 1;
        fullscreen_pass(cmd, hdr, pso.belt.dust_blend, root, true);
    }
}

} // namespace space::render
