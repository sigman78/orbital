#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic_if.hpp"
#include "render/frame_calculations.hpp"
#include "scene/terrain_patch.hpp"

#include <cmath>

// The minor planet's near tier: the CPU picks the cube-sphere patches to draw
// and generates the new ones into a staging slot before the previous frame's
// wait, the command buffer copies them into the device pool ahead of the
// passes, and the body's draws go through the pool instead of the sphere
// levels while the tier is active. See render/terrain_tier.hpp.

namespace space::render {

namespace {
constexpr std::uint64_t patch_bytes = patch_vertex_count * sizeof(Vertex);
}

void Renderer::Impl::create_terrain_tier() {
    if (!showcase.has_minor_planet())
        return;
    minor_planet_terrain.emplace(system.bodies[showcase.minor_planet()].material_seed);
    buffers.patch_pool = UniqueGpuHeap::create(device, TerrainTier::slot_count * patch_bytes,
                                               gpu::MemoryType::gpu_only);
    buffers.patch_staging = UniqueGpuHeap::create(device, 2ull * TerrainTier::generate_per_frame * patch_bytes);
    panic_if(!buffers.patch_pool.range().gpu || !buffers.patch_staging.range().cpu, "patch pool allocation failed");
    patch_indices_address = upload_static(bytes_of(patch_indices()));
    patch_scratch.resize(patch_vertex_count);
    log::info("Near tier: {} patch slots of {} vertices, {} KiB device pool", TerrainTier::slot_count,
              patch_vertex_count, TerrainTier::slot_count * patch_bytes >> 10);
}

void Renderer::Impl::prepare_terrain_tier(const FrameInput& input) {
    patch_copies.clear();
    if (!minor_planet_terrain)
        return;
    if (!input.terrain.near_tier) {
        terrain_tier.disable();
        stats.frame.patches_drawn = 0;
        return;
    }
    const unsigned body = showcase.minor_planet();
    const CameraView& camera = frozen_cull ? frozen_cull->camera : input.camera;
    const float tan_y = frozen_cull ? frozen_cull->tan_y : float(std::tan(camera.vertical_fov * .5));
    const float tan_x = tan_y * float(extent.width) / float(std::max(extent.height, 1u));
    const BodyState& state = input.bodies[body];
    TierView view{.body_centre = state.position - camera.position,
                  .radius = state.radius,
                  .frustum = view_frustum(camera, tan_x, tan_y),
                  .height_pixels = float(extent.height),
                  .tan_y = tan_y};
    // The body's frame as the shader's rotation(): the spin about y, then the tilt about z.
    const double tilt = system.bodies[body].axial_tilt;
    for (unsigned axis = 0; axis < 3; axis++) {
        const Vec3d e{axis == 0 ? 1. : 0., axis == 1 ? 1. : 0., axis == 2 ? 1. : 0.};
        view.axes[axis] = rotate_about(rotate_about(e, Vec3d{0, 1, 0}, state.rotation_angle), Vec3d{0, 0, 1}, tilt);
    }
    const Vec3d camera_relative = camera.position - state.position;
    view.camera_local = Vec3d{dot(view.axes[0], camera_relative), dot(view.axes[1], camera_relative),
                              dot(view.axes[2], camera_relative)} *
                        (1 / state.radius);
    const bool was_active = terrain_tier.active();
    terrain_tier.update(view, frame_index);
    if (terrain_tier.active() != was_active)
        log::info("Near tier {} at frame {}, {} patches resident", terrain_tier.active() ? "on" : "off", frame_index,
                  terrain_tier.resident());
    stats.frame.patches_drawn = unsigned(terrain_tier.draws().size());
    stats.frame.patches_resident = terrain_tier.resident();
    // The new patches go to this frame's staging slot; the previous frame may still copy the other.
    const std::uint64_t slot_offset = (frame_index & 1) * std::uint64_t(TerrainTier::generate_per_frame) * patch_bytes;
    std::uint8_t* staging = buffers.patch_staging.range().cpu + slot_offset;
    unsigned n = 0;
    for (TerrainTier::Generation& generation : terrain_tier.generate()) {
        generation.error = generate_patch(*minor_planet_terrain, generation.key, patch_scratch);
        auto* out = reinterpret_cast<Vertex*>(staging + n * patch_bytes);
        for (unsigned i = 0; i < patch_vertex_count; i++) {
            const geometry::Vertex& v = patch_scratch[i];
            out[i] = {{v.position.x, v.position.y, v.position.z, 1}, {v.normal.x, v.normal.y, v.normal.z, 0}};
        }
        patch_copies.push_back({.source = reinterpret_cast<std::uint64_t>(buffers.patch_staging.range().gpu) +
                                          slot_offset + n * patch_bytes,
                                .destination = reinterpret_cast<std::uint64_t>(buffers.patch_pool.range().gpu) +
                                               generation.slot * patch_bytes});
        n++;
    }
}

void Renderer::Impl::record_terrain_uploads(gpu::CommandBuffer* cmd) {
    for (const PatchCopy& copy : patch_copies)
        gpu::copy_memory(cmd, {reinterpret_cast<void*>(copy.source), patch_bytes},
                         {reinterpret_cast<void*>(copy.destination), patch_bytes});
}

bool Renderer::Impl::terrain_tier_draws(unsigned body) const {
    return minor_planet_terrain && body == showcase.minor_planet() && terrain_tier.active();
}

void Renderer::Impl::draw_body(gpu::CommandBuffer* cmd, Root& root, unsigned body, bool wireframe) {
    if (!terrain_tier_draws(body)) {
        draw_mesh(cmd, root, body_mesh(body, body_level[body]), body, 1);
        return;
    }
    root.base = body;
    root.vertices = reinterpret_cast<std::uint64_t>(buffers.patch_pool.range().gpu);
    const gpu::GpuRange indices{reinterpret_cast<void*>(patch_indices_address), std::uint64_t(patch_index_count) * 4};
    for (const TerrainTier::Draw& draw : terrain_tier.draws()) {
        root.flags = wireframe ? ORBITAL_ROOT_WIREFRAME | draw.level << 8 : 0;
        stats.frame.draw_calls++;
        gpu::draw_indexed(cmd, root, indices, gpu::IndexType::uint32, patch_index_count, 1, 0,
                          std::int32_t(draw.slot * patch_vertex_count));
        stats.frame.triangles += patch_index_count / 3;
    }
}

} // namespace space::render
