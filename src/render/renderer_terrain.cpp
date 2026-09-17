#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic_if.hpp"
#include "render/frame_calculations.hpp"
#include "scene/terrain_patch.hpp"

#include <cmath>
#include <cstring>

namespace space::render {

namespace {
constexpr std::uint64_t height_tile_bytes = tile_side * tile_side * 2;
constexpr std::uint64_t colour_tile_bytes = tile_side * tile_side * 4;
constexpr std::uint64_t tile_total_bytes = height_tile_bytes + 2 * colour_tile_bytes;
} // namespace

void Renderer::Impl::create_terrain_tier() {
    if (!showcase.has_minor_planet())
        return;
    minor_planet_terrain.emplace(system.bodies[showcase.minor_planet()].material_seed);
    tile_height = GpuImage::create_array(device, {tile_side, tile_side}, TerrainTier::slot_count,
                                         gpu::Format::r16_unorm);
    tile_albedo = GpuImage::create_array(device, {tile_side, tile_side}, TerrainTier::slot_count,
                                         gpu::Format::rgba8_unorm);
    tile_normal = GpuImage::create_array(device, {tile_side, tile_side}, TerrainTier::slot_count,
                                         gpu::Format::rgba8_unorm);
    bind(ArraySlot::terrain_height, tile_height);
    bind(ArraySlot::terrain_albedo, tile_albedo);
    bind(ArraySlot::terrain_normal, tile_normal);
    const auto grid = patch_grid_mesh();
    std::vector<Vertex> gpu_vertices(grid.vertices.size());
    for (std::size_t i = 0; i < grid.vertices.size(); i++)
        gpu_vertices[i] = {{grid.vertices[i].position.x, grid.vertices[i].position.y, grid.vertices[i].position.z, 0},
                           {0, 0, 0, 0}};
    grid_vertices_address = upload_static(bytes_of(gpu_vertices));
    grid_indices_address = upload_static(bytes_of(grid.indices));
    grid_index_count = unsigned(grid.indices.size());
    buffers.patch_records = UniqueGpuHeap::create(device, TerrainTier::slot_count * sizeof(PatchInstance),
                                                  gpu::MemoryType::gpu_only);
    buffers.patch_records_staging = UniqueGpuHeap::create(device,
                                                          2ull * TerrainTier::slot_count * sizeof(PatchInstance));
    buffers.tile_staging = UniqueGpuHeap::create(device, std::uint64_t(tile_ring_count) * tile_total_bytes);
    panic_if(!buffers.patch_records.range().gpu || !buffers.patch_records_staging.range().cpu ||
                 !buffers.tile_staging.range().cpu,
             "terrain tier allocation failed");
    const unsigned cores = std::thread::hardware_concurrency();
    const unsigned workers = std::max(1u, cores > 2 ? cores - 2 : 1u);
    tile_pool = std::make_unique<WorkerPool<TileResult>>(workers);
    log::info("Near tier: {} tile slots, {} workers, {} KiB height + {} KiB albedo + {} KiB normal arrays",
              TerrainTier::slot_count, workers, TerrainTier::slot_count * height_tile_bytes >> 10,
              TerrainTier::slot_count * colour_tile_bytes >> 10, TerrainTier::slot_count * colour_tile_bytes >> 10);
}

void Renderer::Impl::prepare_terrain_tier(const FrameInput& input) {
    patch_copies.clear();
    tile_copies.clear();
    if (!minor_planet_terrain)
        return;
    if (!input.terrain.near_tier) {
        terrain_tier.disable();
        stats.frame.patches_drawn = 0;
        return;
    }
    // Poll finished tiles from the worker pool and record their copies.
    if (tile_pool) {
        const float height_range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
        for (const TileResult& result : tile_pool->poll()) {
            terrain_tier.mark_resident(result.slot);
            const auto staging_gpu = reinterpret_cast<std::uint64_t>(buffers.tile_staging.range().gpu) +
                                     result.ring * tile_total_bytes;
            tile_copies.push_back({staging_gpu, height_tile_bytes, tile_height.texture(), result.slot});
            tile_copies.push_back(
                {staging_gpu + height_tile_bytes, colour_tile_bytes, tile_albedo.texture(), result.slot});
            tile_copies.push_back({staging_gpu + height_tile_bytes + colour_tile_bytes, colour_tile_bytes,
                                   tile_normal.texture(), result.slot});
            ring_used_frame[result.ring] = frame_index;
        }
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
    // Submit new generation requests to the worker pool.
    if (tile_pool) {
        const float height_range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
        for (const TerrainTier::Generation& generation : terrain_tier.generate()) {
            unsigned ring = tile_ring_count;
            for (unsigned i = 0; i < tile_ring_count; i++) {
                const unsigned idx = (ring_head + i) % tile_ring_count;
                if (frame_index >= ring_used_frame[idx] + 2 || ring_used_frame[idx] == 0) {
                    ring = idx;
                    ring_head = (idx + 1) % tile_ring_count;
                    break;
                }
            }
            if (ring == tile_ring_count)
                break;
            ring_used_frame[ring] = frame_index + 100;
            std::uint8_t* dst = buffers.tile_staging.range().cpu + ring * tile_total_bytes;
            const MinorPlanetTerrain* terrain = &*minor_planet_terrain;
            const PatchKey key = generation.key;
            const unsigned slot = generation.slot;
            tile_pool->submit([terrain, key, slot, ring, dst, height_range]() -> TileResult {
                std::vector<float> heights(tile_side * tile_side);
                std::vector<std::uint8_t> albedo(tile_side * tile_side * 4), normal(tile_side * tile_side * 4);
                generate_height_tile(*terrain, key, heights);
                generate_colour_tiles(*terrain, key, heights, albedo, normal);
                auto* h16 = reinterpret_cast<std::uint16_t*>(dst);
                for (unsigned i = 0; i < tile_side * tile_side; i++)
                    h16[i] = std::uint16_t(
                        std::clamp((heights[i] - MinorPlanetTerrain::height_min) / height_range, 0.0f, 1.0f) *
                            65535.0f +
                        .5f);
                std::memcpy(dst + height_tile_bytes, albedo.data(), colour_tile_bytes);
                std::memcpy(dst + height_tile_bytes + colour_tile_bytes, normal.data(), colour_tile_bytes);
                return {key, slot, ring};
            });
        }
    }
    // Write PatchInstance records for the drawn patches.
    const std::uint64_t records_slot = (frame_index & 1) * std::uint64_t(TerrainTier::slot_count) *
                                       sizeof(PatchInstance);
    auto* records = reinterpret_cast<PatchInstance*>(buffers.patch_records_staging.range().cpu + records_slot);
    for (std::size_t i = 0; i < terrain_tier.draws().size(); i++) {
        const TerrainTier::Draw& draw = terrain_tier.draws()[i];
        const auto cell = [&] {
            const double size = 2.0 / double(1u << draw.key.level);
            return ShaderFloat4{float(-1 + draw.key.x * size), float(-1 + draw.key.y * size), float(size),
                                float(draw.key.level)};
        }();
        const float morph_end = terrain_tier.range(draw.key.level);
        const float morph_start = 0.7f * morph_end;
        records[i] = {.cell = cell, .morph = {morph_start, morph_end, 0, 0}, .tile = {draw.key.face, draw.slot, 0, 0}};
    }
    patch_records_bytes = terrain_tier.draws().size() * sizeof(PatchInstance);
    if (patch_records_bytes)
        patch_copies.push_back(
            {.source = reinterpret_cast<std::uint64_t>(buffers.patch_records_staging.range().gpu) + records_slot,
             .destination = reinterpret_cast<std::uint64_t>(buffers.patch_records.range().gpu),
             .bytes = patch_records_bytes});
}

void Renderer::Impl::record_terrain_uploads(gpu::CommandBuffer* cmd) {
    for (const TileCopy& copy : tile_copies)
        gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(copy.source), copy.bytes}, copy.texture,
                                    {.base_slice = copy.layer, .slice_count = 1, .extent = {tile_side, tile_side, 1}});
    for (const PatchCopy& copy : patch_copies)
        gpu::copy_memory(cmd, {reinterpret_cast<void*>(copy.source), copy.bytes},
                         {reinterpret_cast<void*>(copy.destination), copy.bytes});
    if (!tile_copies.empty() || !patch_copies.empty())
        synchronize(cmd, access::transfer_write, {gpu::Stage::vertex | gpu::Stage::fragment, gpu::Access::shader_read});
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
    root.vertices = grid_vertices_address;
    root.patches = reinterpret_cast<std::uint64_t>(buffers.patch_records.range().gpu);
    root.flags = ORBITAL_ROOT_PATCHES | (wireframe ? ORBITAL_ROOT_WIREFRAME : 0);
    const gpu::GpuRange indices{reinterpret_cast<void*>(grid_indices_address), std::uint64_t(grid_index_count) * 4};
    const unsigned count = unsigned(terrain_tier.draws().size());
    stats.frame.draw_calls++;
    gpu::draw_indexed(cmd, root, indices, gpu::IndexType::uint32, grid_index_count, count);
    stats.frame.triangles += count * (grid_index_count / 3);
}

} // namespace space::render
