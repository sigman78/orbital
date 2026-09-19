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
// Copy offsets and entry strides must align to the four-byte albedo/slope texels.
constexpr std::uint64_t height_tile_bytes = tile_side * tile_side * 2;
constexpr std::uint64_t colour_tile_bytes = tile_colour_side * tile_colour_side * 4;
constexpr std::uint64_t albedo_offset = align4(height_tile_bytes);
constexpr std::uint64_t slope_offset = albedo_offset + colour_tile_bytes;
constexpr std::uint64_t tile_total_bytes = align4(slope_offset + colour_tile_bytes);
// PatchInstance::tile[3] carries six edge/quadrant flags plus the four quadrant bits, and the
// shaders' debug packing reads tile[2] and tile[3] through 12-bit fields (see patch.slang).
constexpr unsigned patch_tile_flag_bits = 10;
static_assert(patch_tile_flag_bits <= 12, "tile[3] is unpacked from a 12-bit field");
static_assert(TerrainTier::slot_count <= 0xfff, "tile[2] carries the parent slot in a 12-bit field");
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
    stats.frame.terrain.uploaded = 0;
    if (!minor_planet_terrain)
        return;
    // Poll even while disabled; upload recording commits residency and releases staging ownership.
    if (tile_pool)
        for (const TileResult& result : tile_pool->poll()) {
            ring_state[result.ring] = RingState::upload;
            tile_uploads.push_back({result.key, result.slot, result.ring, result.stamp, result.heights});
            stats.frame.terrain.generate_ms = result.ms;
        }
    if (!input.terrain.near_tier) {
        terrain_tier.disable();
        stats.frame.terrain = {.slots = TerrainTier::slot_count, .workers = tile_workers};
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
    TierView view{.body_centre = state.position - camera.position,
                  .radius = state.radius,
                  .axes = {axes[0], axes[1], axes[2]},
                  .camera_local = into_body(camera.position - state.position) * (1 / state.radius),
                  .frustum = view_frustum(camera, tan_x, tan_y),
                  .height_pixels = float(extent.height),
                  .tan_y = tan_y,
                  .activate_pixels = input.terrain.activate_pixels,
                  .lod_bias = input.terrain.lod_bias};
    unsigned free_rings = 0;
    for (unsigned i = 0; i < tile_ring_count; i++)
        free_rings += ring_available(i);
    if (input.terrain.detail != tile_detail) {
        tile_detail = input.terrain.detail;
        terrain_tier.invalidate();
    }
    const bool was_active = terrain_tier.active();
    terrain_tier.update(view, frame_index, tile_pool ? free_rings : 0);
    // The tier's generation budget is the free ring count, so every generation below finds a
    // ring. Nothing else enforces it, and a generation left unserved strands its slot.
    ORBITAL_ASSERT(terrain_tier.generate().size() <= free_rings);
    if (terrain_tier.active() != was_active)
        log::info("Near tier {} at frame {}, {} patches resident", terrain_tier.active() ? "on" : "off", frame_index,
                  terrain_tier.resident());
    if (input.terrain.trace_arc > 0)
        trace_far_patches(view, input.terrain.trace_arc);
    else if (!traced_patches.empty())
        traced_patches.clear();
    auto& ts = stats.frame.terrain;
    ts.active = terrain_tier.active();
    ts.drawn = unsigned(terrain_tier.draws().size());
    ts.resident = terrain_tier.resident();
    ts.pending = terrain_tier.pending();
    ts.pending_age = terrain_tier.pending_age();
    ts.queued = tile_pool ? tile_pool->in_flight() : 0;
    ts.rings_free = free_rings;
    ts.rings = tile_ring_count;
    ts.slots = TerrainTier::slot_count;
    ts.nodes = terrain_tier.nodes();
    ts.workers = tile_workers;
    const TerrainTier::Pressure& p = terrain_tier.pressure();
    ts.node_budget = p.node_budget;
    ts.splits_blocked = p.splits_blocked;
    ts.requested = p.requested;
    ts.served = p.served;
    ts.evictions = p.evictions;
    ts.evicted_recent = p.evicted_recent;
    ts.starved = p.starved;
    ts.out_of_range = p.out_of_range;
    ts.deepest = p.deepest;
    ts.behind_one = p.behind_one;
    ts.behind_mean = p.behind_mean;
    ts.flat_near = p.flat_near;
    ts.flat_far = p.flat_far;
    ts.graded = p.graded;
    ts.fade_mean = p.fade_mean;
    if (tile_pool) {
        const float height_range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
        for (const TerrainTier::Generation& generation : terrain_tier.generate()) {
            unsigned ring = tile_ring_count;
            for (unsigned i = 0; i < tile_ring_count; i++) {
                const unsigned idx = (ring_head + i) % tile_ring_count;
                if (ring_available(idx)) {
                    ring = idx;
                    ring_head = (idx + 1) % tile_ring_count;
                    break;
                }
            }
            if (ring == tile_ring_count) {
                // The assert above states the invariant; recover anyway rather than keep a slot
                // no tile will ever fill, which would hold its key forever: visit() re-requests
                // only keys with no slot, and eviction passes over anything not resident.
                log::error("Near tier: no staging ring for {}, slot {} released", generation.key.packed(),
                           generation.slot);
                terrain_tier.release(generation.slot, generation.key, generation.stamp);
                continue;
            }
            ring_state[ring] = RingState::worker; // held until its result is polled, however long that takes
            std::uint8_t* dst = buffers.tile_staging.range().cpu + ring * tile_total_bytes;
            const MinorPlanetTerrain* terrain = &*minor_planet_terrain;
            const PatchKey key = generation.key;
            const unsigned slot = generation.slot;
            const std::uint32_t stamp = generation.stamp;
            const float detail = tile_detail;
            tile_pool->submit([terrain, key, slot, ring, stamp, dst, height_range, detail]() -> TileResult {
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
                return {key,
                        slot,
                        ring,
                        stamp,
                        height_tile_range(heights),
                        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count()};
            });
        }
    }
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
        // Arrival fading needs the parent tile for matching geometry and material transitions.
        const unsigned parent_slot = draw.key.level ? terrain_tier.resident_slot(parent) : TerrainTier::slot_count;
        records[i] = {.cell = cell,
                      .morph = {morph_start, morph_end, parent_slot < TerrainTier::slot_count ? draw.fade : 0, 0},
                      .tile = {draw.key.face, draw.slot, parent_slot,
                               (draw.key.x & 1u) | (draw.key.y & 1u) << 1 | (draw.key.x == 0) << 2 |
                                   (unsigned(draw.key.x) + 1 == 1u << draw.key.level) << 3 | (draw.key.y == 0) << 4 |
                                   (unsigned(draw.key.y) + 1 == 1u << draw.key.level) << 5 | draw.quadrants << 6}};
    }
    patch_records_bytes = terrain_tier.draws().size() * sizeof(PatchInstance);
    if (patch_records_bytes)
        patch_copies.push_back(
            {.source = reinterpret_cast<std::uint64_t>(buffers.patch_records_staging.range().gpu) + records_slot,
             .destination = reinterpret_cast<std::uint64_t>(buffers.patch_records.range().gpu),
             .bytes = patch_records_bytes});
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

// Nothing much past the horizon should be in the draw list, and a patch that is there anyway has
// a reason: every ancestor of it passed the same tests. Print the chain, so the level and the test
// whose margin let it through name themselves rather than being guessed at. Each patch is reported
// once, since a standing one would otherwise fill the log.
void Renderer::Impl::trace_far_patches(const TierView& view, float min_arc) {
    const double d = length(view.camera_local);
    if (!terrain_tier.active() || d < 1e-9)
        return;
    const double limit = std::cos(std::min(double(min_arc) * pi<double> / 180, pi<double>));
    const auto arc_of = [&](Vec3d direction) {
        return std::acos(std::clamp(dot(direction, view.camera_local) / d, -1.0, 1.0)) * 180 / pi<double>;
    };
    unsigned standing = 0;
    double worst = 0;
    for (const TerrainTier::Draw& draw : terrain_tier.draws()) {
        // The quadrant mask is what is drawn, not the whole cell: a root covering one quadrant
        // has its centre 90 degrees away by construction, and that is not the defect. Judge the
        // nearest quadrant actually drawn, and print both so the two cases read apart.
        double nearest = pi<double>;
        for (unsigned q = 0; q < 4; q++)
            if (draw.quadrants >> q & 1)
                nearest = std::min(nearest, arc_of(patch_bounds(draw.key.child(q)).centre) * pi<double> / 180);
        if (std::cos(nearest) > limit)
            continue;
        standing++;
        worst = std::max(worst, nearest * 180 / pi<double>);
        if (!traced_patches.insert(draw.key.packed()).second)
            continue;
        const double cosine = dot(patch_bounds(draw.key).centre, view.camera_local) / d;
        const TerrainTier::Audit audit = terrain_tier.audit();
        log::info("Near tier: patch face {} level {} ({},{}) quadrants {:x}: nearest drawn quadrant {:.0f} deg of "
                  "arc, cell centre {:.0f} deg, frame {}; camera at {:.4f} radii, occluder {:.4f}; nodes {} "
                  "reachable of {} allocated, tiles {} resident, {} undrawn, oldest {} frames, {} pending the "
                  "oldest {} frames",
                  draw.key.face, draw.key.level, draw.key.x, draw.key.y, draw.quadrants, nearest * 180 / pi<double>,
                  std::acos(std::clamp(cosine, -1.0, 1.0)) * 180 / pi<double>, frame_index, d,
                  TerrainTier::horizon_occluder, audit.reachable, audit.allocated, audit.resident,
                  audit.resident_undrawn, audit.oldest_age, audit.pending, audit.pending_age);
        terrain_tier.explain(draw.key, view, trace_steps);
        for (const TerrainTier::Step& step : trace_steps)
            log::info("  level {:2} ({:4},{:4}) {} reach {:+.4f}..{:+.4f} from level {} | horizon {} | plane {} "
                      "margin {:+.5f} | distance {:.4f} vs split range {:.4f} {} {}",
                      step.key.level, step.key.x, step.key.y,
                      step.slot < TerrainTier::slot_count ? "resident" : "no tile ", double(step.reach.min),
                      double(step.reach.max), step.reach_level,
                      std::isinf(step.horizon_margin) ? std::string("not applied, camera inside the occluder")
                                                      : std::format("margin {:+.5f}", step.horizon_margin),
                      step.plane, step.plane_margin, step.distance, step.split_range, step.split ? "split" : "leaf",
                      step.drawn ? std::format("DRAWN quadrants {:x}", step.quadrants) : std::string());
    }
    // A patch is explained once; this says whether any are still there, which is what tells a
    // convergence artefact apart from one that stands.
    if (standing && frame_index % 120 == 0)
        log::info("Near tier: {} patches still drawn past {:.0f} deg of arc at frame {}, farthest quadrant {:.0f} deg",
                  standing, double(min_arc), frame_index, worst);
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
