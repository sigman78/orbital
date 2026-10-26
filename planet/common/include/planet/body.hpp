#pragma once
#include "planet/patch.hpp"
#include "planet/types.hpp"

namespace planet {

// The height shell a body's terrain lives in, in reference radii.
struct BodyShape {
    Range<float> heights{shell_height_min, shell_height_max};
    // The sphere the horizon test hides patches behind: the shell's floor, never the reference
    // surface, since ground below that surface sets the horizon further out.
    constexpr double occluder() const { return 1 + double(heights.min); }
};

// What a tile source knows about its levels, both in radii and both estimates the source measured
// or calibrated rather than certified bounds: the error of a level's grid against the true
// surface, and how far a tile's whole subtree reaches outside its own heights, which culling a
// patch against its own tile must be padded by.
struct LevelTable {
    static constexpr unsigned count = patch_level_max + 2; // levels 0 .. patch_level_max, and one past
    float error[count] = {};
    float relief[count] = {};
    constexpr float error_at(unsigned level) const { return level < count ? error[level] : 0.f; }
    constexpr float relief_at(unsigned level) const { return level < count ? relief[level] : 0.f; }
};

} // namespace planet
