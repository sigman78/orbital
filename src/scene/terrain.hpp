#pragma once
#include "core/math.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// Shared procedural shape and colour for baked maps and cube-sphere tiles.
// Inputs are body-local directions; heights are signed fractions of the reference radius.
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

    // Craters intersecting a spherical cap, to accelerate patch sampling.
    struct Region {
        std::vector<Crater> craters;
    };

    explicit MinorPlanetTerrain(std::uint64_t seed);

    // Slope-only micro-craters; geometry and LOD error are unaffected.
    // Octaves scale size/depth together and fade in as sampling resolution permits.
    // nyquist is half the texels per radian; strength scales the result, with 0 disabling it.
    static constexpr float detail_frequency = 110; // lattice cells per radian of the first octave
    static constexpr unsigned detail_octaves = 3;
    static constexpr float detail_radius = .34f;  // a crater's, of a cell
    static constexpr float detail_reach = 2.2f;   // ejecta out to this many radii
    static constexpr float detail_depth = .3f;    // of the crater's radius, as the population's are
    static constexpr float detail_density = .55f; // the share of cells carrying one
    static constexpr float detail_slope = .15f;   // rms tangent the full stack adds at strength 1
    float detail(Vec3d direction, double nyquist, float strength) const;

    // Height above the reference sphere, radii, within [height_min, height_max].
    float height(Vec3d direction) const { return height(direction, craters_); }
    float height(Vec3d direction, const Region& region) const { return height(direction, region.craters); }
    Region region(Vec3d centre, double angular_radius) const;
    // Linear-light albedo at a point, given its height and slope (0 flat, 1 vertical).
    Vec3f albedo(Vec3d direction, float height, float slope) const;
    std::span<const Crater> craters() const { return craters_; }

private:
    float height(Vec3d direction, std::span<const Crater> craters) const;

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
