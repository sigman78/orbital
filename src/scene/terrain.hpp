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

    // The craters within reach of a cap of the sphere, so a patch's vertices test
    // a handful instead of the whole population.
    struct Region {
        std::vector<Crater> craters;
    };

    explicit MinorPlanetTerrain(std::uint64_t seed);

    // Micro-relief past where the crater population stops, for the tiles' slope only.
    // It continues the body's own construction rather than laying noise over it: a
    // lattice of cells at each octave's frequency, at most one crater to a cell at a
    // hashed place and size, with the flat floor, rising wall, raised rim and ejecta
    // the big ones have. Everything scales with the frequency, so each octave adds
    // the same slope and the stack is self-similar. The field is a function of the
    // direction alone, so two tiles sharing a texel agree on it, and each octave is
    // weighted in only once the sampling grid can carry it, so a tile never sees one
    // it would alias and a level boundary fades it in through the morph rather than
    // switching it on. `nyquist` is half the grid's texels per radian; `strength`
    // scales the stack, 0 off. The geometry does not take it, so the quadtree's
    // error table is untouched.
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
