#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic_if.hpp"
#include "render/frame_calculations.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace space::render {

namespace {
// Copy offsets and entry strides must align to the four-byte albedo/slope texels.
constexpr const planet::TileLayout& tile_layout = ProceduralProvider::tile_layout;
constexpr std::uint64_t height_tile_bytes = tile_layout.planes[0].bytes();
constexpr std::uint64_t colour_tile_bytes = tile_layout.planes[1].bytes();
constexpr std::uint64_t albedo_offset = tile_layout.offset(1);
constexpr std::uint64_t slope_offset = tile_layout.offset(2);
constexpr std::uint64_t tile_total_bytes = tile_layout.bytes();
static_assert(height_tile_bytes == tile_side * tile_side * 2);
static_assert(colour_tile_bytes == tile_colour_side * tile_colour_side * 4);
static_assert(tile_layout.planes[2].bytes() == colour_tile_bytes);
static_assert(tile_total_bytes == 35980, "the staging heap size is unchanged");
// The low bits of PatchInstance::tile[3] carry six edge/quadrant flags plus four quadrant bits;
// its upper 16 bits carry the unmorphed-edge mask. Only the low flags enter debug packing, and the
// shaders' debug packing reads tile[2] and tile[3] through 12-bit fields (see patch.slang).
constexpr unsigned patch_tile_flag_bits = 10;
static_assert(patch_tile_flag_bits <= 12, "tile[3] is unpacked from a 12-bit field");
static_assert(terrain_cache_slots <= 0xfff, "tile[2] carries the parent slot in a 12-bit field");
static_assert(FrameStats::Terrain::cache_age_buckets == Clod::cache_age_buckets &&
              FrameStats::Terrain::cache_age_bucket_frames == Clod::cache_age_bucket_frames);
} // namespace

void Renderer::Impl::create_terrain_tier() {
    if (!showcase.has_minor_planet())
        return;
    minor_planet_terrain.emplace(system.bodies[showcase.minor_planet()].material_seed);
    tile_height = GpuImage::create_array(device, {tile_side, tile_side}, terrain_cache_slots, gpu::Format::r16_unorm);
    tile_albedo = GpuImage::create_array(device, {tile_colour_side, tile_colour_side}, terrain_cache_slots,
                                         gpu::Format::rgba8_unorm);
    tile_slope = GpuImage::create_array(device, {tile_colour_side, tile_colour_side}, terrain_cache_slots,
                                        gpu::Format::rg16_unorm);
    bind(ArraySlot::terrain_height, tile_height);
    bind(ArraySlot::terrain_albedo, tile_albedo);
    bind(ArraySlot::terrain_slope, tile_slope);
    const auto upload_grid = [&](const PatchGrid& grid, std::uint64_t& vertices_address, std::uint64_t& indices_address,
                                 std::uint64_t& wire_address, unsigned& index_count) {
        std::vector<Vertex> vertices(grid.vertices.size());
        for (std::size_t i = 0; i < grid.vertices.size(); i++) {
            const PatchVertex& v = grid.vertices[i];
            // Position.w is zero for surfaces and identifies the canonical outer skirt edge otherwise.
            vertices[i] = {{float(v.x), float(v.y), v.skirt ? 1.f : 0.f, float(v.quadrant)}, {0, 0, 0, 0}};
        }
        vertices_address = upload_static(bytes_of(vertices));
        indices_address = upload_static(bytes_of(grid.indices));
        std::vector<Vertex> wire(grid.indices.size());
        for (std::size_t i = 0; i < wire.size(); i++) {
            wire[i] = vertices[grid.indices[i]];
            wire[i].normal = {i % 3 == 0 ? 1.f : 0.f, i % 3 == 1 ? 1.f : 0.f, i % 3 == 2 ? 1.f : 0.f, 0};
        }
        wire_address = upload_static(bytes_of(wire));
        index_count = unsigned(grid.indices.size());
    };
    upload_grid(patch_quadrant_surface_grid(), grid_vertices_address, grid_indices_address, grid_wire_address,
                grid_index_count);
    upload_grid(patch_quadrant_skirt_grid(), skirt_vertices_address, skirt_indices_address, skirt_wire_address,
                skirt_index_count);
    buffers.patch_records = UniqueGpuHeap::create(
        device, std::uint64_t(terrain_patch_record_capacity) * sizeof(PatchInstance), gpu::MemoryType::gpu_only);
    buffers.patch_records_staging = UniqueGpuHeap::create(device,
                                                          2ull * terrain_patch_record_capacity * sizeof(PatchInstance));
    buffers.tile_staging = UniqueGpuHeap::create(device, std::uint64_t(tile_ring_count) * tile_total_bytes);
    panic_if(!buffers.patch_records.range().gpu || !buffers.patch_records_staging.range().cpu ||
                 !buffers.tile_staging.range().cpu,
             "terrain tier allocation failed");
    tile_provider.emplace(*minor_planet_terrain);
    TileProvider* provider = &*tile_provider;
    if (!planet_store.empty()) {
        const planet::StoreStatus status = tile_store.open(planet_store.string().c_str());
        if (status == planet::StoreStatus::missing) {
            log::info("Near tier: no tile store at {}, tiles generated", planet_store.string());
        } else if (status != planet::StoreStatus::ok) {
            log::warn("Near tier: tile store {} not used: {}", planet_store.string(), store_status_name(status));
        } else {
            const planet::store::StoreHeader& header = tile_store.header();
            // The arrays are sized by the procedural layout, so the store must share it level by level.
            const auto same_layout = [&](const planet::TileLayout& stored) {
                bool same = stored.plane_count == tile_layout.plane_count;
                for (unsigned p = 0; same && p < tile_layout.plane_count; p++)
                    same = stored.planes[p].id == tile_layout.planes[p].id &&
                           stored.planes[p].format == tile_layout.planes[p].format &&
                           stored.planes[p].side == tile_layout.planes[p].side;
                return same;
            };
            // What MixedProvider would assert: the baked body's shell and level table are the terrain's.
            const auto same_body = [&] {
                bool same = header.shape.heights == MinorPlanetTerrain::shape.heights;
                for (unsigned i = 0; same && i < planet::LevelTable::count; i++)
                    same = header.levels.error[i] == MinorPlanetTerrain::levels.error[i] &&
                           header.levels.relief[i] == MinorPlanetTerrain::levels.relief[i];
                return same;
            };
            bool fits = true;
            for (unsigned level = header.level_min; level <= header.level_max; level++)
                fits &= same_layout(header.layouts[level]);
            const char* reject = header.seed != minor_planet_terrain->seed()         ? "another body's seed"
                                 : header.face_mask != 0x3f || header.level_min != 0 ? "faces or roots missing"
                                 : !same_body()                                      ? "shell or level table differs"
                                 : !fits                                             ? "tile layout differs"
                                                                                     : nullptr;
            if (reject) {
                log::warn("Near tier: tile store {} not used: {}", planet_store.string(), reject);
                tile_store.close();
            } else {
                baked_provider.emplace(tile_store);
                mixed_provider.emplace(*baked_provider, *tile_provider, header.level_max);
                provider = &*mixed_provider;
                baked_levels = header.level_max + 1;
                log::info("Near tier: levels 0..{} from the tile store {}", header.level_max, planet_store.string());
            }
        }
    }
    tile_streamer.emplace(TileStreamer::Config{.provider = provider, .sink = &tile_sink});
    log::info("Near tier: {} tile slots, {} workers, {} KiB height + {} KiB albedo + {} KiB slope arrays",
              terrain_cache_slots, tile_streamer->workers(), terrain_cache_slots * height_tile_bytes >> 10,
              terrain_cache_slots * colour_tile_bytes >> 10, terrain_cache_slots * colour_tile_bytes >> 10);
}

void Renderer::Impl::prepare_terrain_tier(const FrameInput& input) {
    patch_copies.clear();
    stats.frame.terrain.uploaded = 0;
    if (!minor_planet_terrain)
        return;
    // Pump even while disabled; upload recording commits residency and releases staging ownership.
    if (tile_streamer)
        tile_streamer->pump(terrain_tier);
    if (!input.terrain.near_tier) {
        terrain_tier.disable();
        // Disabling only stops selection; it deliberately keeps the cache warm. Preserve a
        // truthful pressure strip instead of showing an empty cache while its tiles remain.
        const Clod::CacheStats cache = terrain_tier.cache_stats(frame_index);
        auto& ts = stats.frame.terrain;
        const Clod::Pressure& p = terrain_tier.pressure();
        unsigned rings_free = 0;
        for (unsigned i = 0; i < tile_ring_count; i++)
            rings_free += ring_available(i);
        ts.active = false;
        ts.drawn = 0;
        ts.submitted_triangles = ts.covered_quadrants = 0;
        ts.resident = cache.resident;
        ts.pending = cache.pending;
        ts.pending_age = cache.pending_age;
        ts.queued = tile_streamer ? tile_streamer->in_flight() : 0;
        ts.rings_free = rings_free;
        ts.rings = tile_ring_count;
        ts.slots = cache.capacity;
        ts.slots_free = cache.free;
        ts.oldest_age = cache.oldest_age;
        ts.cache_age = cache.age;
        ts.nodes = terrain_tier.nodes();
        ts.node_budget = Clod::node_budget;
        ts.workers = tile_streamer ? tile_streamer->workers() : 0;
        ts.baked_levels = baked_levels;
        ts.splits_blocked = ts.requested = ts.served = ts.evictions = ts.evicted_recent = 0;
        ts.generation_budget = ts.generation_budget_limited = ts.slots_blocked = 0;
        ts.starved = ts.out_of_range = ts.deepest = ts.behind_one = 0;
        ts.flat_near = ts.flat_far = ts.graded = ts.balanced = 0;
        ts.behind_mean = 0;
        // Totals survive disable/invalidate and remain useful after the current-frame counters clear.
        ts.eviction_total = p.eviction_total;
        ts.warm_eviction_total = p.warm_eviction_total;
        ts.health = terrain_tier.health();
        patch_views.clear();
        stats.patch_map = {};
        return;
    }
    const unsigned body = showcase.minor_planet();
    const BodyState& state = input.bodies[body];
    const double tilt = system.bodies[body].axial_tilt;
    Vec3d axes[3];
    for (unsigned axis = 0; axis < 3; axis++) {
        const Vec3d e{axis == 0 ? 1. : 0., axis == 1 ? 1. : 0., axis == 2 ? 1. : 0.};
        axes[axis] = rotate_about(rotate_about(e, Vec3d{0, 1, 0}, state.rotation_angle), Vec3d{0, 0, 1}, tilt);
    }
    const auto into_body = [&](Vec3d v) { return Vec3d{dot(axes[0], v), dot(axes[1], v), dot(axes[2], v)}; };
    CameraView camera = frozen_cull ? frozen_cull->camera : input.camera;
    // The belt freezes its cull camera in world space, which is right for a ring the body does
    // not carry. This body turns once in 700 s and orbits besides, about one percent of its
    // radius a second against a world-fixed point, which is most of a low flight's altitude: a
    // set frozen that way slides over the ground, and into it, while it is being looked at. Hold
    // the viewpoint in the body's frame instead, so the frozen set stays the frozen set.
    if (frozen_cull) {
        const auto from_body = [&](Vec3d v) { return axes[0] * v.x + axes[1] * v.y + axes[2] * v.z; };
        if (!frozen_cull->terrain)
            frozen_cull->terrain = {.position = into_body(camera.position - state.position) * (1 / state.radius),
                                    .forward = into_body(camera.forward),
                                    .right = into_body(camera.right),
                                    .up = into_body(camera.up)};
        const FrozenCull::BodyFrame& held = *frozen_cull->terrain;
        camera.position = state.position + from_body(held.position) * state.radius;
        camera.forward = from_body(held.forward);
        camera.right = from_body(held.right);
        camera.up = from_body(held.up);
    }
    const float tan_y = frozen_cull ? frozen_cull->tan_y : float(std::tan(camera.vertical_fov * .5));
    const float tan_x = tan_y * float(extent.width) / float(std::max(extent.height, 1u));
    ClodView view{.body_centre = state.position - camera.position,
                  .radius = state.radius,
                  .axes = {axes[0], axes[1], axes[2]},
                  .camera_local = into_body(camera.position - state.position) * (1 / state.radius),
                  .height_pixels = float(extent.height),
                  .tan_y = tan_y,
                  .activate_pixels = input.terrain.activate_pixels,
                  .lod_bias = input.terrain.lod_bias,
                  .mesh_pixels = input.terrain.mesh_pixels};
    const geometry::Frustum view_planes = view_frustum(camera, tan_x, tan_y);
    for (unsigned i = 0; i < 5; i++)
        view.planes[i] = {.normal = view_planes.planes[i].normal, .distance = view_planes.planes[i].distance};
    unsigned free_rings = 0;
    for (unsigned i = 0; i < tile_ring_count; i++)
        free_rings += ring_available(i);
    if (input.terrain.detail != tile_detail) {
        tile_detail = input.terrain.detail;
        terrain_tier.invalidate();
    }
    const bool was_active = terrain_tier.active();
    terrain_tier.update(view, frame_index, tile_streamer ? free_rings : 0);
    // The tier's generation budget is the free ring count, so every generation below finds a
    // ring. Nothing else enforces it, and a generation left unserved strands its slot.
    ORBITAL_ASSERT(terrain_tier.generate().size() <= free_rings);
    if (terrain_tier.active() != was_active)
        log::info("Near tier {} at frame {}, {} patches resident", terrain_tier.active() ? "on" : "off", frame_index,
                  terrain_tier.resident());
    // The draw list for the panel's patch map, kept until the next frame replaces it.
    patch_views.clear();
    patch_views.reserve(terrain_tier.draws().size());
    for (const Clod::Draw& draw : terrain_tier.draws()) {
        const float end = terrain_tier.range(draw.key.level), start = terrain_tier.morph_start(draw.key.level);
        const float morph = draw.key.level && end > start
                                ? std::clamp((draw.near_distance - start) / (end - start), 0.f, 1.f)
                                : 0.f;
        patch_views.push_back({.face = draw.key.face,
                               .level = draw.key.level,
                               .quadrants = std::uint8_t(draw.quadrants),
                               .x = draw.key.x,
                               .y = draw.key.y,
                               .morph = morph,
                               .recovery = draw.fade});
    }
    const CubeCoord under = cube_coordinates(view.camera_local);
    stats.patch_map = {.patches = patch_views,
                       .camera_face = std::uint8_t(under.face),
                       .camera_s = float(under.s),
                       .camera_t = float(under.t)};
    auto& ts = stats.frame.terrain;
    const Clod::Pressure& p = terrain_tier.pressure();
    ts.active = terrain_tier.active();
    ts.drawn = unsigned(terrain_tier.draws().size());
    ts.submitted_triangles = ts.covered_quadrants = 0;
    for (const Clod::Draw& draw : terrain_tier.draws()) {
        ts.covered_quadrants += std::popcount(unsigned(draw.quadrants));
    }
    ts.submitted_triangles = ts.covered_quadrants * ((grid_index_count + skirt_index_count) / 3);
    ts.resident = p.cache_resident;
    ts.pending = p.cache_pending;
    ts.pending_age = p.cache_pending_age;
    ts.queued = tile_streamer ? tile_streamer->in_flight() : 0;
    ts.rings_free = free_rings;
    ts.rings = tile_ring_count;
    ts.slots = terrain_tier.capacity();
    ts.nodes = terrain_tier.nodes();
    ts.workers = tile_streamer ? tile_streamer->workers() : 0;
    ts.baked_levels = baked_levels;
    ts.node_budget = p.node_budget;
    ts.splits_blocked = p.splits_blocked;
    ts.requested = p.requested;
    ts.served = p.served;
    ts.evictions = p.evictions;
    ts.evicted_recent = p.evicted_recent;
    ts.eviction_total = p.eviction_total;
    ts.warm_eviction_total = p.warm_eviction_total;
    ts.slots_free = p.cache_free;
    ts.oldest_age = p.cache_oldest_age;
    ts.cache_age = p.cache_age;
    ts.generation_budget = p.generation_budget;
    ts.generation_budget_limited = p.generation_budget_limited;
    ts.slots_blocked = p.slots_blocked;
    ts.starved = p.starved;
    ts.out_of_range = p.out_of_range;
    ts.deepest = p.deepest;
    ts.behind_one = p.behind_one;
    ts.behind_mean = p.behind_mean;
    ts.flat_near = p.flat_near;
    ts.flat_far = p.flat_far;
    ts.graded = p.graded;
    ts.balanced = p.balanced;
    ts.health = terrain_tier.health();
    if (ts.health.rollback_quadrants)
        log::warn("Terrain rollback: frame {}, {} quadrants, up to {} levels; selection {}, balance {}, "
                  "previous tiles missing {}, evictions {}, free slots {}",
                  frame_index, ts.health.rollback_quadrants, ts.health.rollback_levels, ts.health.selection_rollbacks,
                  ts.health.balance_rollbacks, ts.health.missing_previous, p.evictions, p.cache_free);
    if (tile_streamer)
        tile_streamer->dispatch(terrain_tier, tile_detail);
    const std::uint64_t records_slot = (frame_index & 1) * std::uint64_t(terrain_patch_record_capacity) *
                                       sizeof(PatchInstance);
    auto* records = reinterpret_cast<PatchInstance*>(buffers.patch_records_staging.range().cpu + records_slot);
    std::size_t expected_records = 0;
    for (const Clod::Draw& draw : terrain_tier.draws())
        expected_records += std::popcount(unsigned(draw.quadrants));
    ORBITAL_ASSERT(expected_records <= terrain_patch_record_capacity);
    std::size_t record_index = 0;
    for (const Clod::Draw& draw : terrain_tier.draws()) {
        const auto cell = [&] {
            const double size = 2.0 / double(1u << draw.key.level);
            return ShaderFloat4{float(-1 + draw.key.x * size), float(-1 + draw.key.y * size), float(size),
                                float(draw.key.level)};
        }();
        const float morph_end = terrain_tier.range(draw.key.level);
        const float morph_start = terrain_tier.morph_start(draw.key.level);
        const PatchKey parent{draw.key.face, std::uint8_t(draw.key.level ? draw.key.level - 1 : 0),
                              std::uint16_t(draw.key.x / 2), std::uint16_t(draw.key.y / 2)};
        // The parent tile supplies material at the coarse end of the distance band.
        const unsigned resident_parent = draw.key.level ? terrain_tier.resident_slot(parent) : Clod::no_slot;
        // The shader's parent-layer absence marker is the fixed GPU array size, while the CPU
        // cache uses no_slot so deterministic tests can safely exceed that array's capacity.
        const unsigned parent_slot = resident_parent == Clod::no_slot ? terrain_cache_slots : resident_parent;
        const unsigned edge_flags = (draw.key.x & 1u) | (draw.key.y & 1u) << 1 | (draw.key.x == 0) << 2 |
                                    (unsigned(draw.key.x) + 1 == 1u << draw.key.level) << 3 | (draw.key.y == 0) << 4 |
                                    (unsigned(draw.key.y) + 1 == 1u << draw.key.level) << 5;
        for (unsigned quadrant = 0; quadrant < 4; quadrant++) {
            if (!(draw.quadrants >> quadrant & 1))
                continue;
            records[record_index++] = {.cell = cell,
                                       .morph = {morph_start, morph_end, draw.fade, float(draw.coarse_edges)},
                                       .tile = {draw.key.face, draw.slot, parent_slot,
                                                edge_flags | (1u << (6 + quadrant)) | draw.fine_edges << 16}};
        }
    }
    ORBITAL_ASSERT(record_index == expected_records);
    patch_record_count = unsigned(record_index);
    patch_records_bytes = record_index * sizeof(PatchInstance);
    if (patch_records_bytes)
        patch_copies.push_back(
            {.source = reinterpret_cast<std::uint64_t>(buffers.patch_records_staging.range().gpu) + records_slot,
             .destination = reinterpret_cast<std::uint64_t>(buffers.patch_records.range().gpu),
             .bytes = patch_records_bytes});
}

bool Renderer::Impl::TileRings::acquire(unsigned bytes, TileStaging& staging) {
    ORBITAL_ASSERT(bytes <= tile_total_bytes);
    for (unsigned i = 0; i < tile_ring_count; i++) {
        const unsigned ring = (head + i) % tile_ring_count;
        if (!owner.ring_available(ring))
            continue;
        head = (ring + 1) % tile_ring_count;
        owner.ring_state[ring] = RingState::worker; // held until its result is pumped, however long that takes
        auto* base = reinterpret_cast<std::byte*>(owner.buffers.tile_staging.range().cpu + ring * tile_total_bytes);
        staging = {.bytes = {base, tile_total_bytes}, .token = ring};
        return true;
    }
    // The update's budget is the free ring count, so this is a broken invariant, not load.
    log::error("Near tier: no staging ring for a {} byte tile", bytes);
    return false;
}

void Renderer::Impl::TileRings::complete(const TileRequest& request, const TileStaging& staging, Range<float> heights,
                                         float milliseconds) {
    owner.ring_state[staging.token] = RingState::upload;
    owner.tile_uploads.push_back({request.key, request.slot, staging.token, request.stamp, heights});
    owner.stats.frame.terrain.generate_ms = milliseconds;
}

void Renderer::Impl::TileRings::abandon(const TileStaging& staging) {
    owner.ring_state[staging.token] = RingState::free; // never copied, so no GPU reuse to wait for
}

void Renderer::Impl::record_terrain_uploads(gpu::CommandBuffer* cmd) {
    const auto plane = [&](std::uint64_t source, std::uint64_t bytes, gpu::Texture* texture, unsigned layer,
                           unsigned side) {
        gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(source), bytes}, texture,
                                    {.base_slice = layer, .slice_count = 1, .extent = {side, side, 1}});
    };
    for (const TileUpload& upload : tile_uploads) {
        // Reject stale assignments before either uploading or establishing residency.
        if (terrain_tier.mark_resident(upload.slot, upload.key, upload.stamp, upload.heights)) {
            const auto staging = reinterpret_cast<std::uint64_t>(buffers.tile_staging.range().gpu) +
                                 upload.ring * tile_total_bytes;
            plane(staging, height_tile_bytes, tile_height.texture(), upload.slot, tile_side);
            plane(staging + albedo_offset, colour_tile_bytes, tile_albedo.texture(), upload.slot, tile_colour_side);
            plane(staging + slope_offset, colour_tile_bytes, tile_slope.texture(), upload.slot, tile_colour_side);
            stats.frame.terrain.uploaded++;
        }
        ring_state[upload.ring] = RingState::free;
        ring_used_frame[upload.ring] = frame_index;
    }
    for (const PatchCopy& copy : patch_copies)
        gpu::copy_memory(cmd, {reinterpret_cast<void*>(copy.source), copy.bytes},
                         {reinterpret_cast<void*>(copy.destination), copy.bytes});
    if (!tile_uploads.empty() || !patch_copies.empty())
        synchronize(cmd, access::transfer_write, {gpu::Stage::vertex | gpu::Stage::fragment, gpu::Access::shader_read});
    tile_uploads.clear();
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
    if (wireframe) {
        root.vertices = grid_wire_address;
        gpu::draw(cmd, root, grid_index_count, patch_record_count);
        root.vertices = skirt_wire_address;
        gpu::draw(cmd, root, skirt_index_count, patch_record_count);
    } else {
        const gpu::GpuRange surface_indices{reinterpret_cast<void*>(grid_indices_address),
                                            std::uint64_t(grid_index_count) * 4};
        root.vertices = grid_vertices_address;
        gpu::draw_indexed(cmd, root, surface_indices, gpu::IndexType::uint32, grid_index_count, patch_record_count);
        const gpu::GpuRange skirt_indices{reinterpret_cast<void*>(skirt_indices_address),
                                          std::uint64_t(skirt_index_count) * 4};
        root.vertices = skirt_vertices_address;
        gpu::draw_indexed(cmd, root, skirt_indices, gpu::IndexType::uint32, skirt_index_count, patch_record_count);
    }
    stats.frame.draw_calls += 2;
    stats.frame.triangles += patch_record_count * ((grid_index_count + skirt_index_count) / 3);
}

} // namespace space::render
