#pragma once
#include "core/math.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// The minor planet's terrain, the one source of its shape and colour: the
// start-up bake of its maps reads it, and the near tier's patches will.
// Everything is a function of a direction from the centre, so any
// parameterisation (the equirectangular maps today, cube-sphere patches later)
// samples the same body. Heights are in radii, signed about the reference
// sphere. The look is after Ceres and Pluto: a dark grey ground with warm
// maculae, worn craters at every size, and a few bright faculae.
class MinorPlanetTerrain {
public:
    struct Crater {
        Vec3d centre;        // unit direction
        float radius = 0;    // angular, radians
        float depth = 0;     // radii, the floor below the reference sphere
        float age = 0;       // 0 fresh (dark floor, bright ejecta) to 1 worn to the ground's tone
        float facula = 0;    // a bright deposit at the floor's centre, 0 for most
        float cos_reach = 1; // cosine of the angle past which the crater no longer contributes
    };
    static constexpr float height_min = -.05f, height_max = .05f; // the range a bake's alpha spans, radii

    explicit MinorPlanetTerrain(std::uint64_t seed);

    // Height above the reference sphere, radii, within [height_min, height_max].
    float height(Vec3d direction) const;
    // Linear-light albedo at a point, given its height and slope (0 flat, 1 vertical).
    Vec3f albedo(Vec3d direction, float height, float slope) const;
    std::span<const Crater> craters() const { return craters_; }

private:
    std::uint64_t seed_;
    std::vector<Crater> craters_;
};

// The terrain sampled into equirectangular maps in the airless shader's layout,
// RGBA8 rows from the north: the albedo in linear light, and the tangent normal
// (x east, y south) with the height over [height_min, height_max] in alpha.
struct TerrainMaps {
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> albedo, normal;
};
TerrainMaps bake_terrain_maps(const MinorPlanetTerrain& terrain, unsigned width, unsigned height);

} // namespace space
