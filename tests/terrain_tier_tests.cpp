#include "render/terrain_tier.hpp"
#include "render/frame_calculations.hpp"
#include "scene/terrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <unordered_map>
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

// A camera hovering over a given point of the body, aimed at its centre: the
// morph check needs the camera over a cube edge or a corner, where the levels
// meeting there are not the mirror image of each other.
TierView view_over(Vec3d over, double altitude, double radius) {
    const Vec3d local = normalized(over) * (1 + altitude);
    CameraView camera;
    camera.forward = local * -1;
    camera.right = normalized(cross(camera.forward, std::abs(camera.forward.y) < .9 ? Vec3d{0, 1, 0} : Vec3d{1, 0, 0}));
    camera.up = cross(camera.right, camera.forward);
    const float tan_y = float(std::tan(camera.vertical_fov / 2));
    return {.body_centre = local * -radius,
            .radius = radius,
            .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            .camera_local = local,
            .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
            .height_pixels = 900,
            .tan_y = tan_y};
}

// Models asynchronous generation, like the renderer's worker threads: a slot
// handed out by generate() this update is only marked resident two updates
// later (and the renderer polls before it calls update(), so resolution here
// happens before the wrapped update() call too).
struct AsyncTiles {
    struct Pending {
        unsigned slot;
        PatchKey key;
        std::uint32_t stamp;
        unsigned due;
    };
    std::vector<Pending> pending;
    unsigned step = 0;

    void update(TerrainTier& tier, const TierView& view, unsigned frame,
                unsigned budget = TerrainTier::generate_per_frame) {
        step++;
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due <= step) {
                const bool ok = tier.mark_resident(it->slot, it->key, it->stamp);
                assert(ok);
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        tier.update(view, frame, budget);
        for (const auto& g : tier.generate())
            pending.push_back({g.slot, g.key, g.stamp, step + 2});
    }
};

// True if b is a (strict) ancestor of a: same face, coarser level, containing cell.
bool is_ancestor(PatchKey a, PatchKey b) {
    if (a.face != b.face || b.level >= a.level)
        return false;
    const unsigned shift = a.level - b.level;
    return (a.x >> shift) == b.x && (a.y >> shift) == b.y;
}

// The cross-level morph, checked the way the vertex shader computes it. A drawn
// patch's grid is morphed by the vertex's own distance to the camera, so two
// drawn surfaces of different level meet without a crack only where the finer
// one is fully morphed to the coarser one's shape (m == 1) and the coarser one
// has not begun morphing toward its own parent (m == 0). This walks every such
// boundary and counts the vertices that break it.
struct MorphCheck {
    const MinorPlanetTerrain& terrain;
    std::unordered_map<std::uint32_t, std::vector<float>> heights; // by patch, kept across frames

    struct Surface {
        PatchKey key;
        float start = 0, end = 0, floor = 0;
        const std::vector<float>* height = nullptr;
    };

    // shaders/surface/patch.slang: saturate((dist - morph.x) / (morph.y - morph.x)).
    float morph_at(const Surface& s, unsigned x, unsigned y, Vec3d camera_local) {
        constexpr unsigned quads = tile_side - 1;
        const double size = 2.0 / double(1u << s.key.level);
        const Vec3d d = cube_direction(s.key.face, -1 + s.key.x * size + double(x) * size / quads,
                                       -1 + s.key.y * size + double(y) * size / quads);
        const double dist = length(d * (1 + (*s.height)[y * tile_side + x]) - camera_local);
        return std::max(float(std::clamp((dist - s.start) / std::max(double(s.end) - s.start, 1e-6), 0.0, 1.0)),
                        s.floor);
    }

    // Violating boundary vertices over the tier's current draws.
    unsigned violations(const TerrainTier& tier, const TierView& view, unsigned* checked = nullptr,
                        bool use_fade = true) {
        constexpr unsigned quads = tile_side - 1;
        std::vector<Surface> surfaces;
        std::unordered_map<std::uint32_t, unsigned> cover; // a drawn quadrant's cell -> its surface
        for (const auto& draw : tier.draws()) {
            auto& height = heights[draw.key.packed()];
            if (height.empty()) {
                height.resize(tile_side * tile_side);
                generate_height_tile(terrain, draw.key, height);
            }
            const float end = tier.range(draw.key.level);
            for (unsigned i = 0; i < 4; i++)
                if (draw.quadrants >> i & 1)
                    cover[draw.key.child(i).packed()] = unsigned(surfaces.size());
            surfaces.push_back({draw.key, .7f * end, end, use_fade ? draw.fade : 0.f, &height});
        }
        // The surface covering a cell: itself, or the nearest ancestor drawn as a
        // quadrant. None means finer cells cover it, and they run the check instead.
        const auto coverer = [&](PatchKey cell) -> const Surface* {
            // Up to the root, not halfway: the counter must not shrink with the level.
            for (;;) {
                if (const auto found = cover.find(cell.packed()); found != cover.end())
                    return &surfaces[found->second];
                if (!cell.level)
                    return static_cast<const Surface*>(nullptr);
                cell = {cell.face, std::uint8_t(cell.level - 1), std::uint16_t(cell.x / 2), std::uint16_t(cell.y / 2)};
            }
        };
        unsigned bad = 0;
        for (const auto& [packed, index] : cover) {
            const Surface& s = surfaces[index];
            const PatchKey cell{std::uint8_t(packed & 7), std::uint8_t(packed >> 3 & 0x1f),
                                std::uint16_t(packed >> 8 & 0xfff), std::uint16_t(packed >> 20 & 0xfff)};
            const unsigned cells = 1u << cell.level, qx = cell.x & 1, qy = cell.y & 1;
            for (int side = 0; side < 4; side++) {
                const int dx = side == 0 ? -1 : side == 1 ? 1 : 0, dy = side == 2 ? -1 : side == 3 ? 1 : 0;
                const int nx = int(cell.x) + dx, ny = int(cell.y) + dy;
                PatchKey neighbour{cell.face, cell.level, std::uint16_t(nx), std::uint16_t(ny)};
                if (nx < 0 || ny < 0 || nx >= int(cells) || ny >= int(cells)) {
                    // Over a cube edge: step past it and ask which face and cell that is.
                    const double size = 2.0 / cells;
                    const CubeCoord c = cube_coordinates(cube_direction(
                        cell.face, -1 + (cell.x + .5) * size + dx * size, -1 + (cell.y + .5) * size + dy * size));
                    const auto index_of = [&](double v) {
                        return std::uint16_t(std::clamp(int((v + 1) / size), 0, int(cells) - 1));
                    };
                    neighbour = {std::uint8_t(c.face), cell.level, index_of(c.s), index_of(c.t)};
                }
                const Surface* other = coverer(neighbour);
                if (!other || other->key.level == s.key.level)
                    continue;
                const bool finer = s.key.level > other->key.level, vertical = side < 2;
                const unsigned fixed = vertical ? qx * (quads / 2) + (side == 1) * quads / 2
                                                : qy * (quads / 2) + (side == 3) * quads / 2;
                const unsigned from = vertical ? qy * (quads / 2) : qx * (quads / 2);
                for (unsigned i = 0; i <= quads / 2; i++) {
                    const unsigned x = vertical ? fixed : from + i, y = vertical ? from + i : fixed;
                    const float m = morph_at(s, x, y, view.camera_local);
                    if (checked)
                        (*checked)++;
                    bad += finer ? m < .999f : m > .001f;
                }
            }
        }
        return bad;
    }
};

// A level's morph band must not reach below where its child hands over, or the
// coarser side is already morphing where the finer one meets it.
void test_morph_bands() {
    double worst = 1e9;
    for (unsigned level = 0; level + 1 < std::size(TerrainTier::level_error); level++)
        worst = std::min(worst, .7 * TerrainTier::level_error[level] / TerrainTier::level_error[level + 1]);
    std::printf("terrain tier: worst morph-band headroom over the child's hand-over %.3fx\n", worst);
    assert(worst > 1);
}

void test_morph_continuity() {
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    MorphCheck check{terrain};
    // Settled, over a face centre, a cube edge, a corner and off-axis: the levels
    // that meet must meet cleanly wherever the camera stands.
    const Vec3d spots[] = {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {.9, .1, 1}, {.3, .2, 1}};
    unsigned checked = 0, bad = 0, partial = 0, drawn = 0;
    for (const Vec3d& spot : spots)
        for (double altitude : {.03, .08, .2, .5}) {
            TerrainTier tier;
            const TierView view = view_over(spot, altitude, radius);
            for (unsigned frame = 1; frame <= 96; frame++) {
                tier.update(view, frame, TerrainTier::slot_count);
                for (const auto& g : tier.generate())
                    tier.mark_resident(g.slot, g.key, g.stamp);
            }
            tier.update(view, 200, TerrainTier::slot_count);
            bad += check.violations(tier, view, &checked);
            for (const auto& draw : tier.draws())
                drawn++, partial += draw.quadrants != 0xf;
        }
    std::printf("terrain tier: %u boundary vertices at settled level boundaries, %u break the morph, "
                "%u of %u draws partial\n",
                checked, bad, partial, drawn);
    assert(checked > 20000 && bad == 0);
}

// While tiles stream in, a node draws the quadrants whose children have not
// arrived. A resident child beside one of those is inside its own range, so it is
// not fully morphed, and the two do not meet: the transient crack the skirt
// covers. The fade closes it by holding a newly drawn child at its parent's shape.
void test_morph_streaming() {
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    MorphCheck check{terrain};
    const TierView view = view_over({.6, .3, 1}, .05, radius);
    struct Count {
        unsigned frames = 0, worst = 0, total = 0;
    };
    Count faded, plain;
    TerrainTier tier;
    AsyncTiles async;
    for (unsigned frame = 1; frame <= 80; frame++) {
        async.update(tier, view, frame);
        if (tier.draws().empty())
            continue;
        const unsigned with = check.violations(tier, view, nullptr, true);
        const unsigned without = check.violations(tier, view, nullptr, false);
        faded.frames += with > 0, faded.worst = std::max(faded.worst, with), faded.total += with;
        plain.frames += without > 0, plain.worst = std::max(plain.worst, without), plain.total += without;
    }
    std::printf("terrain tier: streaming, boundary vertices breaking the morph: without the fade %u over %u frames "
                "(worst %u), with it %u over %u frames (worst %u)\n",
                plain.total, plain.frames, plain.worst, faded.total, faded.frames, faded.worst);
    assert(faded.total <= plain.total);
}

// A flat sphere at the camera collision altitude used to select level 7 from
// the global +.05 height ceiling, although every vertex of levels 6 and 7 was
// beyond its morph end. Both sides collapsed, leaving the persistent 2x border.
void test_resident_height_selection() {
    constexpr double radius = 2.5 / 15;
    for (bool grazing : {false, true}) {
        TierView view = view_over({.6, .3, 1}, .08, radius);
        if (grazing) {
            CameraView camera;
            const Vec3d radial = normalized(view.camera_local);
            camera.forward = normalized(normalized(cross(Vec3d{0, 1, 0}, radial)) - radial * .1);
            camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
            camera.up = cross(camera.right, camera.forward);
            view.frustum = view_frustum(camera, view.tan_y * 16 / 9, view.tan_y);
        }
        unsigned deepest[2]{};
        for (unsigned tight = 0; tight < 2; tight++) {
            TerrainTier tier;
            for (unsigned frame = 1; frame <= 120; frame++) {
                tier.update(view, frame);
                for (const auto& g : tier.generate())
                    assert(tier.mark_resident(g.slot, g.key, g.stamp,
                                              tight ? Range<float>{0, 0} : TerrainTier::full_height_range));
            }
            tier.update(view, 140);
            assert(tier.active() && tier.generate().empty() && tier.pending() == 0);
            assert(tier.pressure().splits_blocked == 0);
            deepest[tight] = tier.pressure().deepest;
            double least_morph = 1;
            for (const auto& draw : tier.draws()) {
                if (draw.key.level != deepest[tight])
                    continue;
                assert(draw.fade == 0);
                const double size = 2.0 / (1u << draw.key.level);
                const double end = tier.range(draw.key.level), start = .7 * end;
                for (unsigned y = 0; y < tile_side; y++)
                    for (unsigned x = 0; x < tile_side; x++) {
                        const Vec3d d = cube_direction(draw.key.face,
                                                       -1 + (draw.key.x + double(x) / (tile_side - 1)) * size,
                                                       -1 + (draw.key.y + double(y) / (tile_side - 1)) * size);
                        const double m = std::clamp((length(d - view.camera_local) - start) / (end - start), 0.0, 1.0);
                        least_morph = std::min(least_morph, m);
                    }
            }
            if (tight)
                assert(least_morph < .01); // the finest grid is actually present
            else
                assert(least_morph == 1); // reproduce the previous all-collapsed selection
        }
        std::printf("terrain tier: %s flat sphere, finest level %u -> %u with resident height bounds\n",
                    grazing ? "grazing" : "overhead", deepest[0], deepest[1]);
        assert(deepest[1] < deepest[0]);
    }
}

void test_height_range_lifetime() {
    TerrainTier tier;
    const TierView view = view_at(.2, 2.5 / 15);
    tier.update(view, 1);
    const auto first = tier.generate()[0];
    const Range<float> heights{-.012f, .018f};
    assert(tier.height_range(first.key) == TerrainTier::full_height_range);
    assert(tier.mark_resident(first.slot, first.key, first.stamp, heights));
    assert(tier.height_range(first.key) == heights);
    tier.invalidate();
    assert(tier.height_range(first.key) == TerrainTier::full_height_range);
    tier.update(view, 2);
    assert(!tier.mark_resident(first.slot, first.key, first.stamp, heights));
    assert(tier.height_range(first.key) == TerrainTier::full_height_range);
}

void test_resident_terrain_morph() {
    const MinorPlanetTerrain terrain(1007);
    MorphCheck check{terrain};
    unsigned checked = 0, bad = 0;
    for (Vec3d spot : {Vec3d{.6, .3, 1}, Vec3d{1, .2, 1}})
        for (double altitude : {.004, .02, .08})
            for (bool grazing : {false, true}) {
                TerrainTier tier;
                TierView view = view_over(spot, altitude, 2.5 / 15);
                // Looking down culls the grazing boundaries where a level meets the next, which
                // are the ones a tighter height bound could part. Aim along the surface too.
                if (grazing) {
                    CameraView camera;
                    const Vec3d radial = normalized(view.camera_local);
                    camera.forward = normalized(normalized(cross(Vec3d{0, 1, 0}, radial)) - radial * .1);
                    camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
                    camera.up = cross(camera.right, camera.forward);
                    view.frustum = view_frustum(camera, view.tan_y * 16 / 9, view.tan_y);
                }
                for (unsigned frame = 1; frame <= 120; frame++) {
                    tier.update(view, frame);
                    for (const auto& g : tier.generate()) {
                        auto& heights = check.heights[g.key.packed()];
                        if (heights.empty()) {
                            heights.resize(tile_side * tile_side);
                            generate_height_tile(terrain, g.key, heights);
                        }
                        assert(tier.mark_resident(g.slot, g.key, g.stamp, height_tile_range(heights)));
                    }
                }
                tier.update(view, 140);
                assert(tier.generate().empty() && tier.pressure().splits_blocked == 0);
                bad += check.violations(tier, view, &checked);
            }
    std::printf("terrain tier: resident terrain bounds, %u boundary vertices checked, %u morph violations\n", checked,
                bad);
    assert(checked > 0 && bad == 0);
}

int main() {
    test_resident_height_selection();
    test_height_range_lifetime();
    test_resident_terrain_morph();
    test_morph_bands();
    test_morph_continuity();
    test_morph_streaming();
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
    // A drawn ancestor covers only quadrants no drawn descendant lies in, and
    // every drawn child is still within its parent's own split range (the
    // condition that let the parent descend).
    unsigned parents_resident = 0, children_drawn = 0, partial = 0;
    for (const auto& a : tier.draws()) {
        assert(a.quadrants && a.quadrants <= 0xf);
        partial += a.quadrants != 0xf;
        for (const auto& b : tier.draws())
            if (is_ancestor(a.key, b.key)) {
                const unsigned shift = a.key.level - b.key.level - 1;
                const unsigned quadrant = ((a.key.x >> shift) & 1) | ((a.key.y >> shift) & 1) << 1;
                assert(!((b.quadrants >> quadrant) & 1));
            }
        if (a.key.level == 0)
            continue;
        const PatchKey parent{a.key.face, std::uint8_t(a.key.level - 1), std::uint16_t(a.key.x / 2),
                              std::uint16_t(a.key.y / 2)};
        const double dist = TerrainTier::nearest_distance(close.camera_local, patch_bounds(parent));
        parents_resident += tier.resident_slot(parent) != TerrainTier::slot_count;
        children_drawn++;
        assert(dist < double(tier.range(a.key.level)) / TerrainTier::hysteresis);
    }
    std::printf("terrain tier: %u of %u drawn children have a resident parent, %u partial draws\n", parents_resident,
                children_drawn, partial);
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
    assert(!stale.mark_resident(first.slot, wrong_key, first.stamp));
    assert(stale.resident() == 0);
    assert(stale.mark_resident(first.slot, first.key, first.stamp));
    assert(stale.resident() == 1);

    // Slot and key repeat; the stamp does not. A result in flight when the tiles were
    // invalidated names a generation that no longer exists, even though the same patch
    // is asked for again and is handed back the same slot.
    TerrainTier reissued;
    reissued.update(view_at(.2, radius), 1, 3);
    const TerrainTier::Generation before = reissued.generate()[0];
    reissued.invalidate();
    reissued.update(view_at(.2, radius), 2, 3);
    const TerrainTier::Generation after = reissued.generate()[0];
    assert(after.key == before.key && after.slot == before.slot);
    assert(after.stamp != before.stamp);
    assert(!reissued.mark_resident(before.slot, before.key, before.stamp));
    assert(reissued.resident() == 0);
    assert(reissued.mark_resident(after.slot, after.key, after.stamp));
    assert(reissued.resident() == 1);
    return 0;
}
