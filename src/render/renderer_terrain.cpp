#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic_if.hpp"
#include "render/frame_calculations.hpp"
#include "scene/terrain_patch.hpp"

#include <chrono>
#include <cmath>
#include <cstring>

namespace space::render {

namespace {
constexpr std::uint64_t align4(std::uint64_t bytes) {
    return (bytes + 3) & ~3ull;
}
// A buffer-to-image copy's offset must be a multiple of the texel size: the rgba8
// and rg16 planes are both four bytes a texel, so they start at 4-byte offsets
// within an entry, and entries are 4-byte strides.
constexpr std::uint64_t height_tile_bytes = tile_side * tile_side * 2;
constexpr std::uint64_t colour_tile_bytes = tile_colour_side * tile_colour_side * 4;
constexpr std::uint64_t albedo_offset = align4(height_tile_bytes);
constexpr std::uint64_t slope_offset = albedo_offset + colour_tile_bytes;
constexpr std::uint64_t tile_total_bytes = align4(slope_offset + colour_tile_bytes);
} // namespace

void Renderer::Impl::create_terrain_tier() {
    if (!showcase.has_minor_planet())
        return;
    minor_planet_terrain.emplace(system.bodies[showcase.minor_planet()].material_seed);
    tile_height = GpuImage::create_array(device, {tile_side, tile_side}, TerrainTier::slot_count,
                                         gpu::Format::r16_unorm);
    tile_albedo = GpuImage::create_array(device, {tile_colour_side, tile_colour_side}, TerrainTier::slot_count,
                                         gpu::Format::rgba8_unorm);
    tile_slope = GpuImage::create_array(device, {tile_colour_side, tile_colour_side}, TerrainTier::slot_count,
                                        gpu::Format::rg16_unorm);
    bind(ArraySlot::terrain_height, tile_height);
    bind(ArraySlot::terrain_albedo, tile_albedo);
    bind(ArraySlot::terrain_slope, tile_slope);
    const auto grid = patch_grid_mesh();
    std::vector<Vertex> gpu_vertices(grid.vertices.size());
    for (std::size_t i = 0; i < grid.vertices.size(); i++)
        gpu_vertices[i] = {{grid.vertices[i].position.x, grid.vertices[i].position.y, grid.vertices[i].position.z, 0},
                           {0, 0, 0, 0}};
    grid_vertices_address = upload_static(bytes_of(gpu_vertices));
    grid_indices_address = upload_static(bytes_of(grid.indices));
    std::vector<Vertex> wire(grid.indices.size());
    for (std::size_t i = 0; i < wire.size(); i++) {
        wire[i] = gpu_vertices[grid.indices[i]];
        wire[i].normal = {i % 3 == 0 ? 1.f : 0.f, i % 3 == 1 ? 1.f : 0.f, i % 3 == 2 ? 1.f : 0.f, 0};
    }
    grid_wire_address = upload_static(bytes_of(wire));
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
    tile_workers = std::max(1u, cores > 2 ? cores - 2 : 1u);
    tile_pool = std::make_unique<WorkerPool<TileResult>>(tile_workers);
    log::info("Near tier: {} tile slots, {} workers, {} KiB height + {} KiB albedo + {} KiB slope arrays",
              TerrainTier::slot_count, tile_workers, TerrainTier::slot_count * height_tile_bytes >> 10,
              TerrainTier::slot_count * colour_tile_bytes >> 10, TerrainTier::slot_count * colour_tile_bytes >> 10);
}

void Renderer::Impl::prepare_terrain_tier(const FrameInput& input) {
    patch_copies.clear();
    tile_copies.clear();
    stats.frame.terrain.uploaded = 0;
    if (!minor_planet_terrain)
        return;
    if (!input.terrain.near_tier) {
        terrain_tier.disable();
        stats.frame.terrain = {.slots = TerrainTier::slot_count, .workers = tile_workers};
        return;
    }
    // Poll finished tiles from the worker pool and record their copies.
    if (tile_pool) {
        for (const TileResult& result : tile_pool->poll()) {
            terrain_tier.mark_resident(result.slot, result.key);
            const auto staging_gpu = reinterpret_cast<std::uint64_t>(buffers.tile_staging.range().gpu) +
                                     result.ring * tile_total_bytes;
            tile_copies.push_back({staging_gpu, height_tile_bytes, tile_height.texture(), result.slot, tile_side});
            tile_copies.push_back(
                {staging_gpu + albedo_offset, colour_tile_bytes, tile_albedo.texture(), result.slot, tile_colour_side});
            tile_copies.push_back(
                {staging_gpu + slope_offset, colour_tile_bytes, tile_slope.texture(), result.slot, tile_colour_side});
            ring_used_frame[result.ring] = frame_index;
            stats.frame.terrain.uploaded++;
            stats.frame.terrain.generate_ms = result.ms;
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
                  .tan_y = tan_y,
                  .activate_pixels = input.terrain.activate_pixels,
                  .lod_bias = input.terrain.lod_bias,
                  .seam = input.terrain.seam};
    const double tilt = system.bodies[body].axial_tilt;
    for (unsigned axis = 0; axis < 3; axis++) {
        const Vec3d e{axis == 0 ? 1. : 0., axis == 1 ? 1. : 0., axis == 2 ? 1. : 0.};
        view.axes[axis] = rotate_about(rotate_about(e, Vec3d{0, 1, 0}, state.rotation_angle), Vec3d{0, 0, 1}, tilt);
    }
    const Vec3d camera_relative = camera.position - state.position;
    view.camera_local = Vec3d{dot(view.axes[0], camera_relative), dot(view.axes[1], camera_relative),
                              dot(view.axes[2], camera_relative)} *
                        (1 / state.radius);
    // Only as many requests as there are ring entries to generate them into.
    unsigned free_rings = 0;
    for (unsigned i = 0; i < tile_ring_count; i++)
        free_rings += frame_index >= ring_used_frame[i] + 2 || ring_used_frame[i] == 0;
    // A change to how the tiles are generated makes the resident ones wrong.
    if (input.terrain.detail != tile_detail) {
        tile_detail = input.terrain.detail;
        terrain_tier.invalidate();
    }
    const bool was_active = terrain_tier.active();
    terrain_tier.update(view, frame_index, tile_pool ? free_rings : 0);
    if (terrain_tier.active() != was_active)
        log::info("Near tier {} at frame {}, {} patches resident", terrain_tier.active() ? "on" : "off", frame_index,
                  terrain_tier.resident());
    auto& ts = stats.frame.terrain;
    ts.active = terrain_tier.active();
    ts.drawn = unsigned(terrain_tier.draws().size());
    ts.resident = terrain_tier.resident();
    ts.pending = terrain_tier.pending();
    ts.queued = tile_pool ? tile_pool->in_flight() : 0;
    ts.rings_free = free_rings;
    ts.rings = tile_ring_count;
    ts.slots = TerrainTier::slot_count;
    ts.nodes = terrain_tier.nodes();
    ts.workers = tile_workers;
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
            const float detail = tile_detail;
            tile_pool->submit([terrain, key, slot, ring, dst, height_range, detail]() -> TileResult {
                const auto start = std::chrono::steady_clock::now();
                std::vector<float> heights(tile_side * tile_side);
                std::vector<std::uint8_t> albedo(tile_colour_side * tile_colour_side * 4);
                std::vector<std::uint16_t> slope(tile_colour_side * tile_colour_side * 2);
                generate_height_tile(*terrain, key, heights);
                generate_colour_tiles(*terrain, key, albedo, slope, detail);
                auto* h16 = reinterpret_cast<std::uint16_t*>(dst);
                for (unsigned i = 0; i < tile_side * tile_side; i++)
                    h16[i] = std::uint16_t(
                        std::clamp((heights[i] - MinorPlanetTerrain::height_min) / height_range, 0.0f, 1.0f) *
                            65535.0f +
                        .5f);
                std::memcpy(dst + albedo_offset, albedo.data(), colour_tile_bytes);
                std::memcpy(dst + slope_offset, slope.data(), colour_tile_bytes);
                return {key, slot, ring,
                        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count()};
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
        const PatchKey parent{draw.key.face, std::uint8_t(draw.key.level ? draw.key.level - 1 : 0),
                              std::uint16_t(draw.key.x / 2), std::uint16_t(draw.key.y / 2)};
        // The fade wants the parent's shape, and the fragment path blends toward the
        // parent's tile: without it resident the two would disagree, so it is dropped.
        const unsigned parent_slot = draw.key.level ? terrain_tier.resident_slot(parent) : TerrainTier::slot_count;
        records[i] = {
            .cell = cell,
            .morph = {morph_start, morph_end, parent_slot < TerrainTier::slot_count ? draw.fade : 0, float(draw.finer)},
            .tile = {draw.key.face, draw.slot, parent_slot,
                     (draw.key.x & 1u) | (draw.key.y & 1u) << 1 | (draw.key.x == 0) << 2 |
                         (draw.key.x + 1 == 1u << draw.key.level) << 3 | (draw.key.y == 0) << 4 |
                         (draw.key.y + 1 == 1u << draw.key.level) << 5 | draw.quadrants << 6}};
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
                                    {.base_slice = copy.layer, .slice_count = 1, .extent = {copy.side, copy.side, 1}});
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
    root.patches = reinterpret_cast<std::uint64_t>(buffers.patch_records.range().gpu);
    root.flags = ORBITAL_ROOT_PATCHES | (wireframe ? ORBITAL_ROOT_WIREFRAME : 0);
    const unsigned count = unsigned(terrain_tier.draws().size());
    stats.frame.draw_calls++;
    if (wireframe) { // the unindexed grid: the corner barycentrics the overlay needs
        root.vertices = grid_wire_address;
        gpu::draw(cmd, root, grid_index_count, count);
    } else {
        root.vertices = grid_vertices_address;
        const gpu::GpuRange indices{reinterpret_cast<void*>(grid_indices_address), std::uint64_t(grid_index_count) * 4};
        gpu::draw_indexed(cmd, root, indices, gpu::IndexType::uint32, grid_index_count, count);
    }
    stats.frame.triangles += count * (grid_index_count / 3);
}

} // namespace space::render
