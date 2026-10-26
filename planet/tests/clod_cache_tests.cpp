#include "planet/clod.hpp"
#include "planet/crater_terrain.hpp"
#include "planet/tile_sampling.hpp"
#include "render/frame_calculations.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace space;
using namespace planet;

namespace {

// The CPU regression baseline, independent of the renderer's arrays.
constexpr unsigned baseline_slots = 1024;

Clod make_clod(unsigned capacity = baseline_slots) {
    return Clod{ClodConfig{
        .shape = MinorPlanetTerrain::shape, .levels = MinorPlanetTerrain::levels, .cache_capacity = capacity}};
}

} // namespace

// The tier takes the four side planes and the near plane, in the frustum's order.
void set_planes(ClodView& view, const geometry::Frustum& frustum) {
    for (unsigned i = 0; i < 5; i++)
        view.planes[i] = ViewPlane{.normal = frustum.planes[i].normal, .distance = frustum.planes[i].distance};
}

namespace {

ClodView grazing_view(double altitude, double radius, bool away = false) {
    const Vec3d radial = normalized(Vec3d{.6, .3, 1});
    const Vec3d local = radial * (1 + altitude);
    const float tan_y = float(std::tan(render::CameraView{}.vertical_fov / 2));
    render::CameraView camera;
    camera.forward = away ? radial : normalized(normalized(cross(Vec3d{0, 1, 0}, radial)) - radial * .1);
    camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
    camera.up = cross(camera.right, camera.forward);
    ClodView view{.body_centre = local * -radius,
                  .radius = radius,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = local,
                  .height_pixels = 900,
                  .tan_y = tan_y,
                  .lod_bias = 3};
    set_planes(view, render::view_frustum(camera, tan_y * 16 / 9, tan_y));
    return view;
}

void stationary_constrained_cache(unsigned capacity) {
    constexpr unsigned frames = 900;
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    Clod tier = make_clod(capacity);
    struct Pending {
        unsigned slot;
        PatchKey key;
        std::uint32_t stamp;
        unsigned due;
    };
    std::vector<Pending> pending;
    std::unordered_map<std::uint32_t, unsigned> generated;
    unsigned regenerated = 0, warm_regenerated = 0, resident_undrawn_peak = 0, tail_coverage_changes = 0;
    unsigned evictions = 0, warm_evictions = 0, tail_generated = 0;
    std::vector<std::uint64_t> previous_coverage;

    for (unsigned frame = 1; frame <= frames; ++frame) {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due <= frame) {
                std::vector<float> heights(tile_side * tile_side);
                generate_height_tile(terrain, it->key, heights);
                assert(tier.mark_resident(it->slot, it->key, it->stamp, height_tile_range(heights)));
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        ClodView view = grazing_view(.035, radius);
        tier.update(view, frame, 6);
        for (const auto& g : tier.generate()) {
            if (const auto old = generated.find(g.key.packed()); old != generated.end()) {
                regenerated++;
                warm_regenerated += frame - old->second <= 60;
            }
            generated[g.key.packed()] = frame;
            pending.push_back({g.slot, g.key, g.stamp, frame + 2});
        }
        if (frame > frames - 120)
            tail_generated += unsigned(tier.generate().size());
        std::vector<std::uint64_t> coverage;
        for (const Clod::Draw& draw : tier.draws())
            coverage.push_back((std::uint64_t(draw.key.packed()) << 4) | draw.quadrants);
        std::sort(coverage.begin(), coverage.end());
        if (frame > frames - 120 && !previous_coverage.empty() && coverage != previous_coverage)
            tail_coverage_changes++;
        previous_coverage = std::move(coverage);
        const auto audit = tier.audit();
        resident_undrawn_peak = std::max(resident_undrawn_peak, audit.resident_undrawn);
        evictions += tier.pressure().evictions;
        warm_evictions += tier.pressure().evicted_recent;
        assert(audit.reachable == audit.allocated);
        assert(tier.resident() + tier.pending() <= capacity);
    }

    std::printf("stationary cache: capacity=%u frames=%u generated_keys=%zu regenerated=%u "
                "regenerated_within_60=%u evictions=%u warm_evictions=%u peak_resident_undrawn=%u "
                "tail_generated=%u tail_coverage_changes=%u\n",
                capacity, frames, generated.size(), regenerated, warm_regenerated, evictions, warm_evictions,
                resident_undrawn_peak, tail_generated, tail_coverage_changes);
    std::fflush(stdout);
    // Once a fixed view settles, its prepared set should not churn tile assignments.
    assert(tail_generated == 0);
    assert(tail_coverage_changes == 0);

    // A temporary look-away clears the refinement tree while preserving the tile cache. Returning
    // to the same grazing view may refill missing tiles, but must settle without repeating work.
    const auto advance = [&](unsigned frame, bool away) {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due <= frame) {
                std::vector<float> heights(tile_side * tile_side);
                generate_height_tile(terrain, it->key, heights);
                assert(tier.mark_resident(it->slot, it->key, it->stamp, height_tile_range(heights)));
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        tier.update(grazing_view(.035, radius, away), frame, 6);
        const unsigned generated_now = unsigned(tier.generate().size());
        for (const auto& g : tier.generate())
            pending.push_back({g.slot, g.key, g.stamp, frame + 2});
        std::vector<std::uint64_t> coverage;
        for (const Clod::Draw& draw : tier.draws())
            coverage.push_back((std::uint64_t(draw.key.packed()) << 4) | draw.quadrants);
        std::sort(coverage.begin(), coverage.end());
        return std::pair{generated_now, coverage};
    };
    unsigned frame = frames;
    std::vector<std::uint64_t> coverage;
    for (unsigned i = 0; i < 90; ++i)
        coverage = advance(++frame, true).second;
    unsigned recovery_tail_generated = 0, recovery_tail_changes = 0;
    for (unsigned i = 0; i < 240; ++i) {
        auto [made, now] = advance(++frame, false);
        if (i >= 120) {
            recovery_tail_generated += made;
            recovery_tail_changes += now != coverage;
        }
        coverage = std::move(now);
    }
    assert(recovery_tail_generated == 0);
    assert(recovery_tail_changes == 0);
}

// Deterministic low-altitude cruise; delayed uploads and a bounded 32-entry ring.
void cruise(unsigned capacity, unsigned delay, float bias, double speed, double altitude, bool report = true,
            bool verify = false) {
    const MinorPlanetTerrain terrain(1007);
    Clod tier = make_clod(capacity);
    struct Pending {
        Clod::Generation g;
        unsigned due;
    };
    std::vector<Pending> pending;
    std::unordered_map<std::uint32_t, Range<float>> heights;
    unsigned rollbacks = 0, evictions = 0, tail_generation = 0;
    ClodView last_view;
    if (report)
        std::puts("frame,drawn,resident,pending,evictions,warm,balanced,rollback_quadrants,rollback_levels,selection,"
                  "balance,missing_previous,slots_blocked,nodes,splits_blocked,timeouts");
    for (unsigned frame = 1; frame <= 2100; ++frame) {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due > frame) {
                ++it;
                continue;
            }
            auto [h, inserted] = heights.try_emplace(it->g.key.packed());
            if (inserted) {
                std::vector<float> tile(tile_side * tile_side);
                generate_height_tile(terrain, it->g.key, tile);
                h->second = height_tile_range(tile);
            }
            assert(tier.mark_resident(it->g.slot, it->g.key, it->g.stamp, h->second));
            it = pending.erase(it);
        }
        const double angle = speed * std::clamp(int(frame) - 500, 0, 1100);
        const Vec3d radial = rotate_about(normalized(Vec3d{.6, .3, 1}), Vec3d{0, 1, 0}, angle);
        const Vec3d local = radial * (1 + altitude);
        render::CameraView camera;
        camera.forward = normalized(normalized(cross(Vec3d{0, 1, 0}, radial)) - radial * .1);
        camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
        camera.up = cross(camera.right, camera.forward);
        const float tan_y = float(std::tan(camera.vertical_fov / 2));
        ClodView view{.body_centre = local * -1.0,
                      .radius = 1,
                      .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                      .camera_local = local,
                      .height_pixels = 1440,
                      .tan_y = tan_y,
                      .lod_bias = bias,
                      .mesh_pixels = 8};
        set_planes(view, render::view_frustum(camera, tan_y * 16 / 9, tan_y));
        last_view = view;
        tier.update(view, frame, 32 - unsigned(pending.size()));
        for (const auto& g : tier.generate())
            pending.push_back({g, frame + delay + (g.key.packed() % 5)});
        const auto& p = tier.pressure();
        const auto& h = tier.health();
        if (frame > 500)
            rollbacks += h.rollback_quadrants > 0;
        evictions += p.evictions;
        if (frame > 2000)
            tail_generation += unsigned(tier.generate().size());
        if (report)
            std::printf("%u,%zu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", frame, tier.draws().size(),
                        tier.resident(), tier.pending(), p.evictions, p.evicted_recent, p.balanced,
                        h.rollback_quadrants, h.rollback_levels, h.selection_rollbacks, h.balance_rollbacks,
                        h.missing_previous, p.slots_blocked, tier.nodes(), p.splits_blocked, h.timeouts);
        assert(pending.size() <= 32);
        assert(tier.audit().allocated == tier.audit().reachable);
        for (const auto& draw : tier.draws()) {
            assert(tier.resident_slot(draw.key) == draw.slot);
            for (const auto& g : tier.generate())
                assert(g.slot != draw.slot);
        }
    }
    if (verify) {
        std::printf("cruise: %u slots, %u evictions, %u rollback frames, %u tail generations\n", capacity, evictions,
                    rollbacks, tail_generation);
        assert(evictions > 0 && rollbacks == 0 && tail_generation == 0);
        assert(tier.health().invalidations == 0 && tier.health().timeouts == 0);
        // An explicit quality change is a selection rollback, not a residency failure.
        last_view.lod_bias = -4;
        last_view.mesh_pixels = 64;
        tier.update(last_view, 2101, 0);
        assert(tier.health().selection_rollbacks > 0);
        assert(tier.health().missing_previous == 0);
        const unsigned recorded = tier.health().last_quadrants;
        tier.update(last_view, 2102, 0);
        assert(tier.health().rollback_quadrants == 0);
        assert(recorded > 0 && tier.health().last_quadrants == recorded);
        tier.disable();
        assert(tier.health().rollback_quadrants == 0 && tier.health().last_quadrants == recorded);
    }
}

void invalidation_rejects_late_bounds() {
    constexpr double radius = 2.5 / 15;
    render::CameraView camera;
    camera.forward = {0, 0, -1};
    const float tan_y = float(std::tan(camera.vertical_fov / 2));
    ClodView view{.body_centre = {0, 0, -.2},
                  .radius = radius,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = {0, 0, .2 / radius},
                  .height_pixels = 900,
                  .tan_y = tan_y};
    set_planes(view, render::view_frustum(camera, tan_y * 16 / 9, tan_y));
    Clod tier = make_clod();
    tier.update(view, 1);
    assert(!tier.generate().empty());
    const Clod::Generation old = tier.generate().front();
    assert(old.key.level == 0);
    constexpr Range<float> measured{-0.001f, 0.001f};
    assert(tier.mark_resident(old.slot, old.key, old.stamp, measured));
    const Range<float> before = tier.reach_of(old.key);
    assert(before.min > MinorPlanetTerrain::shape.heights.min);
    assert(before.max < MinorPlanetTerrain::shape.heights.max);

    tier.invalidate();
    const Range<float> reset = tier.reach_of(old.key);
    assert(reset.min == MinorPlanetTerrain::shape.heights.min - MinorPlanetTerrain::levels.relief_at(0));
    assert(reset.max == MinorPlanetTerrain::shape.heights.max + MinorPlanetTerrain::levels.relief_at(0));
    // A completion racing with invalidation must not restore stale measurements.
    assert(!tier.mark_resident(old.slot, old.key, old.stamp, measured));
    assert(tier.health().invalidations == 1 && tier.health().stale_completions == 1);
    const Range<float> after_late_upload = tier.reach_of(old.key);
    assert(after_late_upload.min == reset.min && after_late_upload.max == reset.max);
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--cruise") {
        cruise(argc > 2 ? unsigned(std::atoi(argv[2])) : 2048, argc > 3 ? unsigned(std::atoi(argv[3])) : 12,
               argc > 4 ? float(std::atof(argv[4])) : 0, argc > 5 ? std::atof(argv[5]) : .00012,
               argc > 6 ? std::atof(argv[6]) : .035);
        return 0;
    }
    invalidation_rejects_late_bounds();
    stationary_constrained_cache(baseline_slots);
    stationary_constrained_cache(baseline_slots * 2);
    cruise(512, 24, 0, .0012, .08, false, true);
}
