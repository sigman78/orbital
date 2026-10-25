#include "render/frame_calculations.hpp"
#include "render/terrain_tier.hpp"
#include "scene/terrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace space;
using namespace space::render;

namespace {

TierView grazing_view(double altitude, double radius, bool away = false) {
    const Vec3d radial = normalized(Vec3d{.6, .3, 1});
    const Vec3d local = radial * (1 + altitude);
    const float tan_y = float(std::tan(CameraView{}.vertical_fov / 2));
    CameraView camera;
    camera.forward = away ? radial : normalized(normalized(cross(Vec3d{0, 1, 0}, radial)) - radial * .1);
    camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
    camera.up = cross(camera.right, camera.forward);
    return {.body_centre = local * -radius,
            .radius = radius,
            .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            .camera_local = local,
            .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
            .height_pixels = 900,
            .tan_y = tan_y,
            .lod_bias = 3};
}

void stationary_constrained_cache(unsigned capacity) {
    constexpr unsigned frames = 900;
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    TerrainTier tier(capacity);
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
        TierView view = grazing_view(.035, radius);
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
        for (const TerrainTier::Draw& draw : tier.draws())
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
        for (const TerrainTier::Draw& draw : tier.draws())
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

void invalidation_rejects_late_bounds() {
    constexpr double radius = 2.5 / 15;
    CameraView camera;
    camera.forward = {0, 0, -1};
    const float tan_y = float(std::tan(camera.vertical_fov / 2));
    TierView view{.body_centre = {0, 0, -.2},
                  .radius = radius,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = {0, 0, .2 / radius},
                  .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
                  .height_pixels = 900,
                  .tan_y = tan_y};
    TerrainTier tier;
    tier.update(view, 1);
    assert(!tier.generate().empty());
    const TerrainTier::Generation old = tier.generate().front();
    assert(old.key.level == 0);
    constexpr Range<float> measured{-0.001f, 0.001f};
    assert(tier.mark_resident(old.slot, old.key, old.stamp, measured));
    const Range<float> before = tier.reach_of(old.key);
    assert(before.min > TerrainTier::full_height_range.min);
    assert(before.max < TerrainTier::full_height_range.max);

    tier.invalidate();
    const Range<float> reset = tier.reach_of(old.key);
    assert(reset.min == TerrainTier::full_height_range.min - TerrainTier::relief_margin(0));
    assert(reset.max == TerrainTier::full_height_range.max + TerrainTier::relief_margin(0));
    // A completion racing with invalidation must not restore stale measurements.
    assert(!tier.mark_resident(old.slot, old.key, old.stamp, measured));
    const Range<float> after_late_upload = tier.reach_of(old.key);
    assert(after_late_upload.min == reset.min && after_late_upload.max == reset.max);
}

} // namespace

int main() {
    invalidation_rejects_late_bounds();
    stationary_constrained_cache(TerrainTier::slot_count);
    stationary_constrained_cache(TerrainTier::slot_count * 2);
}
