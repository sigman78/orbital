#include "render/terrain_tier.hpp"
#include "render/frame_calculations.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

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

int main() {
    constexpr double radius = 2.5 / 15;
    TerrainTier tier;
    // Far away the tier stays off and asks for nothing.
    tier.update(view_at(20, radius), 1);
    assert(!tier.active() && tier.generate().empty() && tier.draws().empty());
    // Close in it wants the six faces first, and takes over once they are resident.
    const TierView close = view_at(.2, radius);
    tier.update(close, 2);
    assert(!tier.active() && tier.generate().size() == TerrainTier::generate_per_frame);
    for (unsigned i = 0; i < 6; i++)
        assert(tier.generate()[i].key.level == 0);
    unsigned frame = 3, converged_at = 0;
    for (; frame < 200; frame++) {
        tier.update(close, frame);
        assert(tier.generate().size() <= TerrainTier::generate_per_frame);
        if (tier.active() && tier.generate().empty()) {
            converged_at = frame;
            break;
        }
    }
    assert(converged_at && tier.active() && !tier.draws().empty());
    // Every drawn slot is drawn once, and no slot is both drawn and regenerated in one frame.
    std::vector<unsigned> drawn;
    for (const auto& draw : tier.draws())
        drawn.push_back(draw.slot);
    std::sort(drawn.begin(), drawn.end());
    assert(std::adjacent_find(drawn.begin(), drawn.end()) == drawn.end());
    std::printf("terrain tier: converged at frame %u with %zu patches drawn, %u resident, %u nodes\n", converged_at,
                tier.draws().size(), tier.resident(), tier.nodes());
    assert(tier.draws().size() > 6 && tier.draws().size() < 400);
    // A still view stays converged; a turn away draws fewer and generates the newly seen side.
    tier.update(close, frame + 1);
    assert(tier.generate().empty());
    const std::size_t facing = tier.draws().size();
    tier.update(view_at(.2, radius, normalized(Vec3d{1, 0, -.4})), frame + 2);
    assert(tier.draws().size() < facing);
    for (unsigned f = frame + 3; f < frame + 60; f++) {
        tier.update(view_at(.2, radius), f);
        for (const auto& g : tier.generate())
            for (const auto& draw : tier.draws())
                assert(draw.slot != g.slot);
    }
    // Backing off past the threshold switches the tier off again.
    tier.update(view_at(20, radius), frame + 61);
    assert(!tier.active() && tier.draws().empty());
    return 0;
}
