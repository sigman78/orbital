#include "render/terrain_tier.hpp"
#include "render/frame_calculations.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace space;
using namespace space::render;

// A camera looking down -z at the body some distance ahead, unspun.
TierView view_at(double distance, double radius, Vec3d forward = {0, 0, -1}) {
    CameraView camera;
    camera.forward = forward;
    const float tan_y = float(std::tan(camera.vertical_fov / 2));
    TierView view{.body_centre = Vec3d{0, 0, -distance},
                  .radius = radius,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = Vec3d{0, 0, distance} * (1 / radius),
                  .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
                  .height_pixels = 900,
                  .tan_y = tan_y};
    return view;
}

// Models asynchronous generation, like the renderer's worker threads: a slot
// handed out by generate() this update is only marked resident two updates
// later (and the renderer polls before it calls update(), so resolution here
// happens before the wrapped update() call too).
struct AsyncTiles {
    struct Pending {
        unsigned slot;
        PatchKey key;
        unsigned due;
    };
    std::vector<Pending> pending;
    unsigned step = 0;

    void update(TerrainTier& tier, const TierView& view, unsigned frame,
                unsigned budget = TerrainTier::generate_per_frame) {
        step++;
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due <= step) {
                const bool ok = tier.mark_resident(it->slot, it->key);
                assert(ok);
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        tier.update(view, frame, budget);
        for (const auto& g : tier.generate())
            pending.push_back({g.slot, g.key, step + 2});
    }
};

// True if b is a (strict) ancestor of a: same face, coarser level, containing cell.
bool is_ancestor(PatchKey a, PatchKey b) {
    if (a.face != b.face || b.level >= a.level)
        return false;
    const unsigned shift = a.level - b.level;
    return (a.x >> shift) == b.x && (a.y >> shift) == b.y;
}

int main() {
    constexpr double radius = 2.5 / 15;
    TerrainTier tier;
    AsyncTiles async;
    // Far away the tier stays off and asks for nothing.
    tier.update(view_at(20, radius), 1);
    assert(!tier.active() && tier.generate().empty() && tier.draws().empty());
    // Close in it wants the six faces first, and takes over once they are resident.
    const TierView close = view_at(.2, radius);
    async.update(tier, close, 2);
    assert(!tier.active());
    assert(tier.generate().size() > 0 && tier.generate().size() <= TerrainTier::generate_per_frame);
    // Coarse first: the six face roots are the first six.
    unsigned roots = 0;
    for (const auto& g : tier.generate())
        roots += g.key.level == 0;
    assert(roots == 6);
    // The roots haven't landed yet: the tier stays off and draws nothing (not a
    // slot standing in for a tile that hasn't arrived).
    async.update(tier, close, 3);
    assert(!tier.active() && tier.draws().empty());
    unsigned frame = 4, converged_at = 0;
    for (; frame < 2000; frame++) {
        async.update(tier, close, frame);
        assert(tier.generate().size() <= TerrainTier::generate_per_frame);
        if (tier.active())
            assert(!tier.draws().empty());
        if (tier.active() && tier.generate().empty() && async.pending.empty()) {
            converged_at = frame;
            break;
        }
    }
    assert(converged_at && tier.active() && !tier.draws().empty());
    std::printf("terrain tier: converged at frame %u with %zu patches drawn, %u resident, %u nodes\n", converged_at,
                tier.draws().size(), tier.resident(), tier.nodes());
    assert(tier.draws().size() > 6 && tier.draws().size() < 400);
    // Every drawn slot is drawn once, and no slot is both drawn and regenerated in one frame.
    std::vector<unsigned> drawn;
    for (const auto& draw : tier.draws())
        drawn.push_back(draw.slot);
    std::sort(drawn.begin(), drawn.end());
    assert(std::adjacent_find(drawn.begin(), drawn.end()) == drawn.end());
    for (const auto& g : tier.generate())
        for (const auto& draw : tier.draws())
            assert(draw.slot != g.slot);
    // No drawn patch has a drawn ancestor, and every drawn child is still within
    // its parent's own split range (the condition that let the parent descend).
    for (const auto& a : tier.draws()) {
        for (const auto& b : tier.draws())
            assert(!is_ancestor(a.key, b.key));
        if (a.key.level == 0)
            continue;
        const PatchKey parent{a.key.face, std::uint8_t(a.key.level - 1), std::uint16_t(a.key.x / 2),
                              std::uint16_t(a.key.y / 2)};
        const double dist = length(close.camera_local - patch_bounds(parent).centre);
        assert(dist < double(tier.range(a.key.level)) / TerrainTier::hysteresis);
    }
    // A still view stays converged; a turn away draws fewer and generates the newly seen side.
    async.update(tier, close, frame + 1);
    assert(tier.generate().empty());
    const std::size_t facing = tier.draws().size();
    async.update(tier, view_at(.2, radius, normalized(Vec3d{1, 0, -.4})), frame + 2);
    assert(tier.draws().size() < facing);
    for (unsigned f = frame + 3; f < frame + 60; f++) {
        async.update(tier, view_at(.2, radius), f);
        for (const auto& g : tier.generate())
            for (const auto& draw : tier.draws())
                assert(draw.slot != g.slot);
    }
    // Backing off past the threshold switches the tier off again.
    tier.update(view_at(20, radius), frame + 61);
    assert(!tier.active() && tier.draws().empty());

    // Budget caps how many entries a single update may generate.
    TerrainTier budgeted;
    budgeted.update(view_at(.2, radius), 1, 0);
    assert(budgeted.generate().empty());
    budgeted.update(view_at(.2, radius), 2, 3);
    assert(budgeted.generate().size() <= 3);

    // A stale mark_resident (wrong key: the slot was recycled) is refused and
    // leaves the slot non-resident; the right key still marks it.
    TerrainTier stale;
    stale.update(view_at(.2, radius), 1, 3);
    assert(!stale.generate().empty());
    const TerrainTier::Generation first = stale.generate()[0];
    const PatchKey wrong_key{std::uint8_t((first.key.face + 1) % 6), first.key.level, first.key.x, first.key.y};
    assert(stale.resident() == 0);
    assert(!stale.mark_resident(first.slot, wrong_key));
    assert(stale.resident() == 0);
    assert(stale.mark_resident(first.slot, first.key));
    assert(stale.resident() == 1);
    return 0;
}
