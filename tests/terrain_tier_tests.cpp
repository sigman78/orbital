#include "render/terrain_tier.hpp"
#include "core/parallel.hpp"
#include "render/frame_calculations.hpp"
#include "scene/terrain.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

// Camera above a chosen surface direction, looking at the body centre.
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

// Complete simulated uploads two updates later, before selection as in the renderer.
struct AsyncTiles {
    struct Pending {
        unsigned slot;
        PatchKey key;
        std::uint32_t stamp;
        unsigned due;
    };
    std::vector<Pending> pending;
    unsigned step = 0;
    // Set to hand the tier each tile's own height bounds, as the renderer does. Left null the
    // residency carries the global shell, which is cheaper and all most tests need.
    const MinorPlanetTerrain* terrain = nullptr;

    void update(TerrainTier& tier, const TierView& view, unsigned frame,
                unsigned budget = TerrainTier::generate_per_frame) {
        step++;
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->due <= step) {
                bool ok = false;
                if (terrain) {
                    std::vector<float> heights(tile_side * tile_side);
                    generate_height_tile(*terrain, it->key, heights);
                    ok = tier.mark_resident(it->slot, it->key, it->stamp, height_tile_range(heights));
                } else {
                    ok = tier.mark_resident(it->slot, it->key, it->stamp);
                }
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

// Check shared-edge continuity: fine morph must be 1, coarse morph must be 0.
struct MorphCheck {
    const MinorPlanetTerrain& terrain;
    std::unordered_map<std::uint32_t, std::vector<float>> heights; // by patch, kept across frames

    struct Surface {
        PatchKey key;
        float start = 0, end = 0, floor = 0;
        const std::vector<float>* height = nullptr;
    };

    // Match the vertex shader morph.
    float morph_at(const Surface& s, unsigned x, unsigned y, Vec3d camera_local) {
        constexpr unsigned quads = tile_side - 1;
        const double size = 2.0 / double(1u << s.key.level);
        const Vec3d d = cube_direction(s.key.face, -1 + s.key.x * size + double(x) * size / quads,
                                       -1 + s.key.y * size + double(y) * size / quads);
        const double dist = length(d * (1 + (*s.height)[y * tile_side + x]) - camera_local);
        return std::max(float(std::clamp((dist - s.start) / std::max(double(s.end) - s.start, 1e-6), 0.0, 1.0)),
                        s.floor);
    }

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
        // Find the covering ancestor; finer neighbors perform their own check.
        const auto coverer = [&](PatchKey cell) -> const Surface* {
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
                    // Map the adjacent cell onto its cube face.
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

// The deepest lowland and the highest summit of a seed, by a spiral sample of the whole body.
struct GroundExtremes {
    Vec3d lowest{0, 0, 1}, highest{0, 0, 1};
    float low = MinorPlanetTerrain::height_max, high = MinorPlanetTerrain::height_min;
};
GroundExtremes ground_extremes(const MinorPlanetTerrain& terrain) {
    constexpr unsigned samples = 4000;
    GroundExtremes found;
    for (unsigned i = 0; i < samples; i++) {
        const double y = 1 - 2 * (i + .5) / samples, a = i * 2.399963, r = std::sqrt(1 - y * y);
        const Vec3d d{r * std::cos(a), y, r * std::sin(a)};
        const float h = terrain.height(d);
        if (h < found.low)
            found.low = h, found.lowest = d;
        if (h > found.high)
            found.high = h, found.highest = d;
    }
    return found;
}

// No drawn patch may be one the frustum or the horizon could have rejected outright: every
// sample of it outside one shared plane, or every sample below the camera's horizon.
void test_cull_waste() {
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    const auto to_world = [](const TierView& v, Vec3d local) {
        return v.axes[0] * local.x + v.axes[1] * local.y + v.axes[2] * local.z;
    };
    // Over generic ground, and low over the deepest lowland, where the camera stands below the
    // reference sphere: the horizon test used to switch itself off there and keep the far side.
    const GroundExtremes ground = ground_extremes(terrain);
    const Vec3d lowland = ground.lowest;
    const double lowest = double(ground.low);
    assert(lowest < -.02);
    struct Case {
        Vec3d local; // the camera in the body's frame, radii
        double pitch;
    };
    const Vec3d generic = normalized(Vec3d{.6, .3, 1});
    const Case cases[] = {{generic * 1.05, 0.},
                          {generic * 1.15, 0.},
                          {generic * 1.05, .87},
                          {generic * 1.15, 1.22},
                          {lowland * (1 + lowest + .005), .1}};
    for (auto [over, pitch] : cases) {
        CameraView camera;
        const Vec3d down = normalized(over) * -1;
        const Vec3d east = normalized(cross(Vec3d{0, 1, 0}, down));
        camera.forward = normalized(down * std::cos(pitch) + east * std::sin(pitch));
        camera.right = normalized(cross(camera.forward, Vec3d{0, 1, 0}));
        camera.up = cross(camera.right, camera.forward);
        const float tan_y = float(std::tan(camera.vertical_fov / 2));
        const TierView view{.body_centre = over * -radius,
                            .radius = radius,
                            .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                            .camera_local = over,
                            .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
                            .height_pixels = 900,
                            .tan_y = tan_y};
        TerrainTier tier;
        AsyncTiles async;
        async.terrain = &terrain; // the tight bounds are the point of this test
        for (unsigned frame = 1; frame <= 300; frame++)
            async.update(tier, view, frame);
        unsigned drawn = 0, wasted = 0;
        double farthest = 0; // arc from the camera to the farthest patch centre drawn, radians
        for (const auto& draw : tier.draws()) {
            drawn++;
            const Range<float> h = tier.height_range(draw.key);
            const double size = 2.0 / double(1u << draw.key.level);
            const double s0 = -1 + draw.key.x * size, t0 = -1 + draw.key.y * size;
            const Vec3d centre = cube_direction(draw.key.face, s0 + size / 2, t0 + size / 2);
            farthest = std::max(farthest, std::acos(std::clamp(dot(centre, normalized(over)), -1.0, 1.0)));
            std::vector<Vec3f> points;
            bool any_visible_over_horizon = false;
            for (unsigned i = 0; i <= 4; i++)
                for (unsigned j = 0; j <= 4; j++) {
                    const Vec3d d = cube_direction(draw.key.face, s0 + i * size / 4, t0 + j * size / 4);
                    // Near side of the occluding sphere: dot(point, camera) >= radius squared,
                    // the occluder being the shell's floor as the tier takes it.
                    constexpr double occluder = TerrainTier::horizon_occluder;
                    if (dot(d * (1 + double(h.max)), view.camera_local) >= occluder * occluder)
                        any_visible_over_horizon = true;
                    // Down to the skirt ring, which is drawn and which the tier's bounds carry:
                    // 0.002 radii, a third of the altitude in the lowest view here.
                    for (float e : {h.min - patch_skirt_drop, h.max})
                        points.push_back(to_float(view.body_centre + to_world(view, d * (1 + double(e))) * radius));
                }
            bool outside = false;
            for (const auto& plane : view.frustum.planes) {
                bool all_out = true;
                for (const Vec3f& p : points)
                    if (dot(plane.normal, p) + plane.distance >= 0) {
                        all_out = false;
                        break;
                    }
                outside = outside || all_out;
            }
            wasted += outside || !any_visible_over_horizon;
        }
        std::printf("terrain tier: cull at %.3f radii pitch %.2f: %u drawn, %u provably invisible, farthest centre "
                    "%.0f deg\n",
                    length(over) - 1, pitch, drawn, wasted, farthest * 180 / pi<double>);
        assert(drawn > 0);
        // The defect this guards put two thirds of the set here, a sphere bound a third, and a
        // cap around the cell a sixth. Bounding the cell itself leaves almost nothing.
        assert(wasted * 8 < drawn);
        // Nothing beyond a quarter turn can be seen from this close, whatever the relief. The
        // horizon test is what rejects it, and comparing the camera distance with the reference
        // sphere rather than the occluder switched that test off below the sphere: the lowland
        // view then drew 99 patches instead of 42, out to 170 degrees of arc.
        assert(farthest < pi<double> / 2);
    }
}

// Parent morphing must start beyond the child handover distance.
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
    // Check settled boundaries across faces, edges, corners, and altitudes.
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

// Arrival fading should reduce mismatches beside missing-child fallback quadrants.
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

// Regression: loose height bounds fully collapsed adjacent finest levels, leaving a 2x border.
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
                // Grazing views expose LOD boundaries hidden by downward-facing frusta.
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

// The tree must stay reachable from its six roots -- a block that falls out of the walk is one
// no visit can collapse again -- and every drawn patch must be able to account for itself: a
// chain from its root to itself in which no test has a negative margin, since a negative one is
// a test that should have culled it. This is the invariant behind `--terrain-trace`.
void test_provenance_and_audit() {
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    TerrainTier tier;
    AsyncTiles async;
    async.terrain = &terrain;
    const TierView view = view_over({.6, .3, 1}, .05, radius);
    for (unsigned frame = 1; frame <= 300; frame++)
        async.update(tier, view, frame);
    const TerrainTier::Audit audit = tier.audit();
    std::vector<TerrainTier::Step> steps;
    unsigned checked = 0;
    for (const auto& draw : tier.draws()) {
        tier.explain(draw.key, view, steps);
        assert(steps.size() == std::size_t(draw.key.level) + 1);
        assert(steps.front().key.level == 0 && steps.back().key == draw.key);
        assert(steps.back().drawn && steps.back().quadrants == draw.quadrants);
        for (const TerrainTier::Step& step : steps) {
            assert(step.plane_margin >= 0);   // every ancestor passed the frustum
            assert(step.horizon_margin >= 0); // and the horizon, or stood inside the occluder
            assert(step.reach_level <= step.key.level && step.reach.min < step.reach.max);
            checked++;
        }
    }
    std::printf("terrain tier: %zu drawn patches explained over %u levels; nodes %u reachable of %u, tiles %u "
                "resident, %u undrawn, oldest %u frames\n",
                tier.draws().size(), checked, audit.reachable, audit.allocated, audit.resident, audit.resident_undrawn,
                audit.oldest_age);
    assert(checked > 0 && audit.reachable == audit.allocated);
    assert(audit.resident >= tier.draws().size());
}

// Ground truth for the cull: no terrain a viewer can actually see may be missing from the
// drawn set. Visibility is decided by ray-marching the real surface, not by the rule the tier
// applies, so this is the only check that can fail the horizon heuristic (terrain_tier.cpp
// explains what rests on it). Seconds a view on the worker pool, which is too slow for every
// ctest run and right for any change to the terrain's amplitude, height_min, descendant_relief
// or the horizon test: `terrain_tier_tests --truth`.
//
// What it catches, by injection: an occluder at 1.02, above the ground the sight lines graze,
// fails the first view outright. At 0.99, above the lowest ground but below a limb, it passes
// -- no view here looks along a basin that deep. It is a net over the geometry that occurs,
// not a proof, which is the whole reason the horizon rule needs one.
namespace truth {

struct View {
    const MinorPlanetTerrain* terrain = nullptr;
    CameraView camera;
    Vec3d camera_local; // radii
    double tan_x = 0, tan_y = 0;
    TierView view;
};

// A camera `altitude` radii over the ground at `over`, pitched down from the local horizontal.
View view_over_ground(const MinorPlanetTerrain& terrain, Vec3d over, double altitude, double pitch_down) {
    constexpr double radius = 2.5 / 15;
    const Vec3d up = normalized(over);
    View v{.terrain = &terrain, .camera_local = up * (1 + double(terrain.height(up)) + altitude)};
    const Vec3d east = normalized(cross(std::abs(up.y) < .9 ? Vec3d{0, 1, 0} : Vec3d{1, 0, 0}, up));
    v.camera.forward = normalized(east * std::cos(pitch_down) - up * std::sin(pitch_down));
    v.camera.right = normalized(cross(v.camera.forward, up));
    v.camera.up = cross(v.camera.right, v.camera.forward);
    v.tan_y = std::tan(v.camera.vertical_fov / 2);
    v.tan_x = v.tan_y * 16 / 9;
    v.view = {.body_centre = v.camera_local * -radius,
              .radius = radius,
              .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
              .camera_local = v.camera_local,
              .frustum = view_frustum(v.camera, float(v.tan_x), float(v.tan_y)),
              .height_pixels = 900,
              .tan_y = float(v.tan_y),
              .activate_pixels = 100};
    return v;
}

bool in_frustum(const View& v, Vec3d p) {
    const Vec3d d = p - v.camera_local;
    const double forward = dot(d, v.camera.forward);
    return forward > 0 && std::abs(dot(d, v.camera.right)) <= forward * v.tan_x &&
           std::abs(dot(d, v.camera.up)) <= forward * v.tan_y;
}

// The sight line from the camera to p clears the real terrain. Stepped at 0.0015 radii, about
// half a level-9 cell, with a tolerance at the far end so a point does not occlude itself.
bool unoccluded(const View& v, Vec3d p) {
    const Vec3d d = p - v.camera_local;
    const unsigned steps = unsigned(std::clamp(length(d) / .0015, 24.0, 1200.0));
    for (unsigned i = 1; i < steps; i++) {
        const Vec3d q = v.camera_local + d * (double(i) / steps);
        const double r = length(q);
        if (r > 1 + double(MinorPlanetTerrain::height_max))
            continue;
        if (r < 1 + double(v.terrain->height(q * (1 / r))) - 2e-4)
            return false;
    }
    return true;
}

// Run the tier to a standstill with the tile height ranges the renderer would supply.
void converge(TerrainTier& tier, const View& v) {
    unsigned quiet = 0;
    for (unsigned frame = 1; frame < 4000 && quiet < 30; frame++) {
        tier.update(v.view, frame);
        quiet = tier.generate().empty() ? quiet + 1 : 0;
        const std::vector<TerrainTier::Generation> made(tier.generate().begin(), tier.generate().end());
        std::vector<Range<float>> ranges(made.size());
        parallel_for(unsigned(made.size()), [&](unsigned i) {
            std::vector<float> heights(tile_side * tile_side);
            generate_height_tile(*v.terrain, made[i].key, heights);
            ranges[i] = height_tile_range(heights);
        });
        for (std::size_t i = 0; i < made.size(); i++)
            tier.mark_resident(made[i].slot, made[i].key, made[i].stamp, ranges[i]);
    }
}

} // namespace truth

void test_cull_soundness() {
    const MinorPlanetTerrain terrain(1007);
    const GroundExtremes ground = ground_extremes(terrain);
    struct Case {
        const char* name;
        Vec3d over;
        double altitude, pitch;
    };
    // A spread of altitudes over generic, high and low ground, looking ahead and at the limb.
    const Case cases[] = {
        {"limb from .30", {-1, .2, .3}, .30, .69},      {"limb from .20", {.6, .3, 1}, .20, .6},
        {"limb from .12", {.1, -1, .4}, .12, .46},      {"ahead from .05", {.6, .3, 1}, .05, .3},
        {"ahead from .01", {.6, .3, 1}, .01, .1},       {"highland from .01", ground.highest, .01, .1},
        {"lowland from .005", ground.lowest, .005, .5},
    };
    for (const Case& k : cases) {
        const truth::View v = truth::view_over_ground(terrain, k.over, k.altitude, k.pitch);
        TerrainTier tier;
        truth::converge(tier, v);
        // The drawn unit is a quadrant: what a patch draws is one level finer than the patch.
        std::unordered_set<std::uint32_t> units;
        for (const auto& draw : tier.draws())
            for (unsigned q = 0; q < 4; q++)
                if (draw.quadrants >> q & 1)
                    units.insert(draw.key.child(q).packed());
        // Probe the whole body on a level-9 lattice; every visible probe must lie in a unit.
        constexpr unsigned level = 9, cells = 1u << level;
        const double d = length(v.camera_local);
        std::atomic<unsigned> visible{0}, missing{0};
        parallel_for(6 * cells * cells, [&](unsigned index) {
            const unsigned face = index / (cells * cells), x = index / cells % cells, y = index % cells;
            const double s = -1 + (x + .5) * 2.0 / cells, t = -1 + (y + .5) * 2.0 / cells;
            const Vec3d direction = cube_direction(face, s, t);
            // Nothing is seen far past the flat horizon; skip the terrain lookup out there.
            const double reach = std::min(std::acos(std::min(1.0, .95 / d)) + .45, 3.1);
            if (dot(direction, v.camera_local) < d * std::cos(reach))
                return;
            const Vec3d p = direction * (1 + double(terrain.height(direction)));
            if (!truth::in_frustum(v, p) || !truth::unoccluded(v, p))
                return;
            visible++;
            // Walk up from the finest level: a unit may be finer than the probe lattice.
            const double finest = double(1u << patch_level_max);
            PatchKey cell{std::uint8_t(face), std::uint8_t(patch_level_max),
                          std::uint16_t(std::min(finest - 1, (s + 1) / 2 * finest)),
                          std::uint16_t(std::min(finest - 1, (t + 1) / 2 * finest))};
            for (;; cell = cell.parent()) {
                if (units.count(cell.packed()))
                    return;
                if (!cell.level)
                    break;
            }
            missing++;
        });
        std::printf("terrain tier: truth, %-20s %4zu units drawn, %6u visible probes, %u uncovered\n", k.name,
                    units.size(), visible.load(), missing.load());
        assert(visible > 0 && missing == 0);
    }
}

// The same question where the horizon heuristic is closest to failing: the highest summit of
// the body, placed a degree or two past the plane's own cut-off, seen from every side. A sweep
// of general views cannot find this -- the region is visible in a few of these arrangements and
// covers a fraction of a probe lattice -- and it is what the plane's error would break first.
void test_horizon_heuristic() {
    const MinorPlanetTerrain terrain(1007);
    Vec3d peak = ground_extremes(terrain).highest;
    // Climb to the local summit: the spiral sample lands near it, not on it.
    for (double step = .01; step > 1e-5; step *= .7)
        for (unsigned k = 0; k < 8; k++) {
            const Vec3d east = normalized(cross(std::abs(peak.y) < .9 ? Vec3d{0, 1, 0} : Vec3d{1, 0, 0}, peak));
            const Vec3d turned = east * std::cos(k * .785) + cross(peak, east) * std::sin(k * .785);
            if (const Vec3d trial = normalized(peak + turned * step); terrain.height(trial) > terrain.height(peak))
                peak = trial;
        }
    const double summit = 1 + double(terrain.height(peak));
    constexpr double occluder = TerrainTier::horizon_occluder;
    const Vec3d east = normalized(cross(std::abs(peak.y) < .9 ? Vec3d{0, 1, 0} : Vec3d{1, 0, 0}, peak));
    const Vec3d north = cross(peak, east);
    unsigned views = 0, seen_views = 0, probes = 0, uncovered = 0;
    for (double d : {1.15, 1.30})
        for (double back : {1.0, 2.5}) // degrees inside the plane's cut-off for this summit
            for (unsigned turn = 0; turn < 8; turn++) {
                const double arc = std::acos(occluder * occluder / (d * summit)) - back / 180 * pi<double>;
                const double phi = turn * pi<double> / 4;
                const Vec3d up = peak * std::cos(arc) + (east * std::cos(phi) + north * std::sin(phi)) * std::sin(arc);
                truth::View v{.terrain = &terrain, .camera_local = up * d};
                v.camera.forward = normalized(peak * summit - v.camera_local);
                v.camera.right = normalized(cross(v.camera.forward, up));
                v.camera.up = cross(v.camera.right, v.camera.forward);
                v.tan_y = std::tan(v.camera.vertical_fov / 2);
                v.tan_x = v.tan_y * 16 / 9;
                constexpr double radius = 2.5 / 15;
                v.view = {.body_centre = v.camera_local * -radius,
                          .radius = radius,
                          .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                          .camera_local = v.camera_local,
                          .frustum = view_frustum(v.camera, float(v.tan_x), float(v.tan_y)),
                          .height_pixels = 900,
                          .tan_y = float(v.tan_y),
                          .activate_pixels = 100};
                TerrainTier tier;
                truth::converge(tier, v);
                std::unordered_set<std::uint32_t> units;
                for (const auto& draw : tier.draws())
                    for (unsigned q = 0; q < 4; q++)
                        if (draw.quadrants >> q & 1)
                            units.insert(draw.key.child(q).packed());
                // A 6 degree square over the summit, finer than the lattice above by far.
                constexpr unsigned n = 120;
                std::atomic<unsigned> visible{0}, missing{0};
                parallel_for(n * n, [&](unsigned index) {
                    const double x = (index % n + .5) / n - .5, y = (index / n + .5) / n - .5;
                    const Vec3d direction = normalized(peak + east * (x * .105) + north * (y * .105));
                    const Vec3d p = direction * (1 + double(terrain.height(direction)));
                    if (!truth::in_frustum(v, p) || !truth::unoccluded(v, p))
                        return;
                    visible++;
                    const CubeCoord at = cube_coordinates(direction);
                    const double finest = double(1u << patch_level_max);
                    PatchKey cell{std::uint8_t(at.face), std::uint8_t(patch_level_max),
                                  std::uint16_t(std::min(finest - 1, (at.s + 1) / 2 * finest)),
                                  std::uint16_t(std::min(finest - 1, (at.t + 1) / 2 * finest))};
                    for (;; cell = cell.parent()) {
                        if (units.count(cell.packed()))
                            return;
                        if (!cell.level)
                            break;
                    }
                    missing++;
                });
                views++;
                seen_views += visible > 0;
                probes += visible;
                uncovered += missing;
                if (missing)
                    std::printf("terrain tier: truth, summit at %.1f deg past the cut-off, distance %.2f, turn %u: "
                                "%u visible, %u uncovered\n",
                                back, d, turn, visible.load(), missing.load());
            }
    std::printf("terrain tier: truth, summit %.4f radii: visible in %u of %u views, %u probes, %u uncovered\n", summit,
                seen_views, views, probes, uncovered);
    // The region has to be visible somewhere, or the search proves nothing.
    assert(seen_views > 0 && probes > 1000 && uncovered == 0);
}

// A flight rather than a view. Standing still, the tier converges in a second and every test
// above reads a settled tree; flying, it never converges, and the selection is always a mixture
// of what the camera wants now and what the cache happened to be holding a moment ago. That is
// the state a stray patch lives in, so this walks the body -- a ladder of altitudes, a great
// circle at each, the view swivelling as it goes -- and checks what must hold every single step.
void test_flight_sweep() {
    constexpr double radius = 2.5 / 15;
    const MinorPlanetTerrain terrain(1007);
    // The highest ground anywhere sets the cone horizon below, so it is the bound for everything.
    const GroundExtremes ground = ground_extremes(terrain);
    const double top = 1 + double(ground.high) + 1e-3;
    constexpr double occluder = TerrainTier::horizon_occluder;

    std::unordered_map<std::uint32_t, Range<float>> tile_bounds; // kept for the whole flight
    struct Pending {
        TerrainTier::Generation generation;
        unsigned due;
    };
    std::vector<Pending> pending;
    TerrainTier tier;
    unsigned frame = 0, steps = 0, anomalies = 0, parentless = 0;
    unsigned peak_drawn = 0, peak_nodes = 0, peak_resident = 0;
    double worst_excess = 0;

    const Vec3d axis = normalized(Vec3d{.3, 1, .2});
    const Vec3d start = normalized(cross(axis, Vec3d{0, 0, 1})), across = cross(axis, start);
    // Legs of a flight, not a ring of viewpoints: the step has to be small against the horizon
    // at that altitude or the camera teleports, the cache is never warm, and the tier is starved
    // the whole way -- which tests nothing it will meet in the app. A dive and a climb come last,
    // since a coarse patch left behind by a changing altitude is the case being hunted.
    struct Leg {
        const char* name;
        double from, to; // altitude at the start and the end of the leg
        double begin;    // where on the circle it starts, radians
        unsigned steps;
    };
    const Leg legs[] = {
        {"low pass", .004, .004, 0.0, 400}, {"skim", .01, .01, 1.7, 400}, {"cruise", .03, .03, 3.1, 400},
        {"high pass", .1, .1, 4.6, 300},    {"orbit", .3, .3, 5.6, 300},  {"dive", .3, .004, 2.2, 500},
        {"climb", .004, .3, 0.8, 500},
    };
    for (const Leg& leg : legs) {
        double along = leg.begin;
        // Per leg, what the cull actually kept: the sound bound below can only catch what is
        // provably invisible, and it is a loose one at low altitude, so the numbers matter too.
        unsigned leg_steps = 0, leg_quadrants = 0, past_flat = 0;
        double leg_worst_arc = 0;
        for (unsigned i = 0; i < leg.steps; i++) {
            const double t = double(i) / leg.steps;
            // Geometric between the ends, so a dive spends its steps evenly across the decades.
            const double altitude = leg.from * std::pow(leg.to / leg.from, t);
            // A fraction of the horizon at this altitude: acos(1 / (1 + h)) is how far it reaches.
            along += std::acos(1 / (1 + altitude)) / 6;
            const Vec3d up = start * std::cos(along) + across * std::sin(along);
            const Vec3d track = across * std::cos(along) - start * std::sin(along);
            const Vec3d local = up * (1 + double(terrain.height(up)) + altitude);
            // Swivel as it flies: the yaw turns by a fraction of a turn a step, so every heading
            // is met at every part of the path, and the pitch sweeps from the nadir to the sky.
            const double yaw = i * .05, pitch = .35 + .45 * std::sin(i * .021);
            const Vec3d east = cross(up, track);
            const Vec3d heading = track * std::cos(yaw) + east * std::sin(yaw);
            CameraView camera;
            camera.forward = normalized(heading * std::cos(pitch) - up * std::sin(pitch));
            camera.right = normalized(cross(camera.forward, up));
            camera.up = cross(camera.right, camera.forward);
            const float tan_y = float(std::tan(camera.vertical_fov / 2));
            const TierView view{.body_centre = local * -radius,
                                .radius = radius,
                                .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                                .camera_local = local,
                                .frustum = view_frustum(camera, tan_y * 16 / 9, tan_y),
                                .height_pixels = 900,
                                .tan_y = tan_y,
                                .activate_pixels = 100};

            // Tiles arrive two steps after they are asked for, as they do in the renderer.
            frame++;
            for (auto it = pending.begin(); it != pending.end();) {
                if (it->due > frame) {
                    ++it;
                    continue;
                }
                tier.mark_resident(it->generation.slot, it->generation.key, it->generation.stamp,
                                   tile_bounds[it->generation.key.packed()]);
                it = pending.erase(it);
            }
            tier.update(view, frame);
            const std::vector<TerrainTier::Generation> made(tier.generate().begin(), tier.generate().end());
            std::vector<Range<float>> fresh(made.size());
            parallel_for(unsigned(made.size()), [&](unsigned k) {
                if (tile_bounds.count(made[k].key.packed()))
                    return; // this tile was generated earlier in the flight
                std::vector<float> heights(tile_side * tile_side);
                generate_height_tile(terrain, made[k].key, heights);
                fresh[k] = height_tile_range(heights);
            });
            for (std::size_t k = 0; k < made.size(); k++) {
                tile_bounds.emplace(made[k].key.packed(), fresh[k]);
                pending.push_back({made[k], frame + 2});
            }
            if (!tier.active())
                continue;
            steps++;

            const double d = length(local);
            // Nothing past the cone horizon of the highest ground can be seen from here, whatever
            // the relief between: a sound bound, not the tier's own rule, so it can fail.
            const double reach = std::acos(std::min(1.0, occluder / d)) + std::acos(std::min(1.0, occluder / top));
            std::unordered_map<std::uint32_t, unsigned> drawn_slots;
            std::unordered_set<std::uint32_t> drawn_cells;
            for (const TerrainTier::Draw& draw : tier.draws()) {
                assert(draw.quadrants && draw.quadrants <= 0xf);
                assert(drawn_slots.emplace(draw.slot, draw.key.packed()).second); // a slot drawn twice
                parentless += draw.key.level && tier.resident_slot(draw.key.parent()) == TerrainTier::slot_count;
                for (unsigned q = 0; q < 4; q++) {
                    if (!(draw.quadrants >> q & 1))
                        continue;
                    const PatchKey cell = draw.key.child(q);
                    assert(drawn_cells.insert(cell.packed()).second);
                    const PatchBounds bounds = patch_bounds(cell);
                    const double arc = std::acos(std::clamp(dot(bounds.centre, local) / d, -1.0, 1.0));
                    // The nearest corner of the cell, not its centre: a coarse cell is wide.
                    leg_quadrants++;
                    leg_worst_arc = std::max(leg_worst_arc, arc);
                    past_flat += arc - bounds.angular_radius > std::acos(std::min(1.0, 1 / d));
                    const double excess = arc - bounds.angular_radius - reach;
                    if (excess <= 0)
                        continue;
                    if (++anomalies <= 4) {
                        std::printf("terrain tier: sweep anomaly on the %s at altitude %.4f step %u: face %u level "
                                    "%u (%u,%u) "
                                    "quadrant %u stands %.1f deg past the cone horizon (%.1f deg) from %.4f radii\n",
                                    leg.name, altitude, i, cell.face, cell.level, cell.x, cell.y, q,
                                    excess * 180 / pi<double>, reach * 180 / pi<double>, d);
                        std::vector<TerrainTier::Step> chain;
                        tier.explain(draw.key, view, chain);
                        for (const TerrainTier::Step& step : chain)
                            std::printf("    level %2u (%4u,%4u) %s reach %+.4f..%+.4f from level %u, horizon margin "
                                        "%+.5f, plane %u margin %+.5f, distance %.4f vs %.4f\n",
                                        step.key.level, step.key.x, step.key.y,
                                        step.slot < TerrainTier::slot_count ? "resident" : "no tile ",
                                        double(step.reach.min), double(step.reach.max), step.reach_level,
                                        step.horizon_margin, step.plane, step.plane_margin, step.distance,
                                        step.split_range);
                        std::fflush(stdout); // the assertion below aborts, and this is the diagnosis
                    }
                    worst_excess = std::max(worst_excess, excess);
                }
            }
            // No drawn cell may lie inside another: an ancestor covering a quadrant its own
            // descendant also draws is double cover, and the seam between them is a crack.
            for (std::uint32_t packed : drawn_cells) {
                PatchKey cell{std::uint8_t(packed & 7), std::uint8_t(packed >> 3 & 0x1f),
                              std::uint16_t(packed >> 8 & 0xfff), std::uint16_t(packed >> 20 & 0xfff)};
                while (cell.level) {
                    cell = cell.parent();
                    assert(!drawn_cells.count(cell.packed()));
                }
            }
            const TerrainTier::Audit audit = tier.audit();
            assert(audit.reachable == audit.allocated); // a block no walk can reach again
            assert(tier.nodes() <= TerrainTier::node_budget);
            leg_steps++;
            peak_drawn = std::max(peak_drawn, unsigned(tier.draws().size()));
            peak_nodes = std::max(peak_nodes, audit.allocated);
            peak_resident = std::max(peak_resident, audit.resident);
        }
        std::printf("terrain tier: sweep leg %-10s %4u steps, %6u quadrants drawn, %5.1f a step; farthest %5.1f deg "
                    "of arc, %5.2f%% of them past the flat horizon\n",
                    leg.name, leg_steps, leg_quadrants, leg_steps ? double(leg_quadrants) / leg_steps : 0.0,
                    leg_worst_arc * 180 / pi<double>, leg_quadrants ? 100. * past_flat / leg_quadrants : 0.0);
    }
    std::printf("terrain tier: flight sweep, %u steps over seven legs, %zu tiles generated; peak %u drawn, %u "
                "nodes, %u resident; %u quadrants drawn with no resident parent; %u past the cone horizon%s\n",
                steps, tile_bounds.size(), peak_drawn, peak_nodes, peak_resident, parentless, anomalies,
                anomalies ? "" : " (none)");
    assert(steps > 500);
    assert(anomalies == 0);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++)
        if (std::string_view(argv[i]) == "--truth") {
            test_cull_soundness(); // seconds a view of ray marching; see the note above it
            test_horizon_heuristic();
            return 0;
        } else if (std::string_view(argv[i]) == "--sweep") {
            test_flight_sweep();
            return 0;
        }
    test_resident_height_selection();
    test_height_range_lifetime();
    test_resident_terrain_morph();
    test_morph_bands();
    test_morph_continuity();
    test_morph_streaming();
    test_cull_waste();
    test_provenance_and_audit();
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
    unsigned roots = 0;
    for (const auto& g : tier.generate())
        roots += g.key.level == 0;
    assert(roots == 6);
    // Keep the sphere until all root uploads arrive.
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
    // Require disjoint ancestor/descendant coverage and valid child split ranges.
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

    TerrainTier budgeted;
    budgeted.update(view_at(.2, radius), 1, 0);
    assert(budgeted.generate().empty());
    budgeted.update(view_at(.2, radius), 2, 3);
    assert(budgeted.generate().size() <= 3);

    // Reject stale keys without establishing residency.
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

    // A released slot returns to the pool and its patch is asked for again.
    TerrainTier released;
    released.update(view_at(.2, radius), 1, 3);
    const TerrainTier::Generation unserved = released.generate()[0];
    released.release(unserved.slot, unserved.key, unserved.stamp);
    assert(released.pending() == 2); // the other two of the batch
    released.update(view_at(.2, radius), 2, 3);
    unsigned reissued_count = 0;
    for (const auto& g : released.generate())
        reissued_count += g.key == unserved.key;
    assert(reissued_count == 1);
    // Releasing twice, or against a stale stamp, must not free a slot that moved on.
    released.release(unserved.slot, unserved.key, unserved.stamp);
    const TerrainTier::Generation again = released.generate()[0];
    assert(released.mark_resident(again.slot, again.key, again.stamp));
    assert(released.resident() == 1);

    // A generation that never arrives is reclaimed, so the patch is not stuck forever.
    TerrainTier abandoned;
    abandoned.update(view_at(.2, radius), 1, 3);
    const TerrainTier::Generation dropped = abandoned.generate()[0];
    assert(abandoned.pending() == 3);
    // The sweep walks a window per frame, so a full pass takes slot_count / sweep_window frames.
    constexpr unsigned sweep_cycle = TerrainTier::slot_count / TerrainTier::sweep_window;
    for (unsigned f = 0; f <= sweep_cycle; f++)
        abandoned.update(view_at(.2, radius), 2 + TerrainTier::pending_timeout + f, 0);
    assert(abandoned.pending() == 0);
    assert(!abandoned.mark_resident(dropped.slot, dropped.key, dropped.stamp));
    abandoned.update(view_at(.2, radius), 3 + TerrainTier::pending_timeout + sweep_cycle, 3);
    unsigned re_requested = 0;
    for (const auto& g : abandoned.generate())
        re_requested += g.key == dropped.key;
    assert(re_requested == 1);

    // After invalidation, the same key/slot must reject the previous generation stamp.
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
