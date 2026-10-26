#pragma once
#include "core/math.hpp"

// The libraries use the host's math vocabulary under their own name.
namespace planet {

using space::Range;
using space::Vec3;
using space::Vec3d;
using space::Vec3f;

// Height shell every body shares until BodyShape makes it a per-body value: heights in
// reference radii lie within it, and a bake's alpha spans it.
inline constexpr float shell_height_min = -.05f, shell_height_max = .05f;

} // namespace planet
