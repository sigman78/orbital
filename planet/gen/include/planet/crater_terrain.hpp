#pragma once
#include "planet/body.hpp"
#include "planet/types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace planet {

// Shared map/tile terrain: body-local directions, heights in reference radii.
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
    static constexpr float height_min = shell_height_min,
                           height_max = shell_height_max; // the range a bake's alpha spans, radii
    // What this terrain hands the quadtree: its shell, and the seed-1007 calibration against 3D
    // triangles, 32 quads, Everitt warp (`planet_gen_tests --calibrate`: uniform samples followed
    // by a hill climb). Estimates, not certified bounds on every patch or intermediate morph.
    // The relief is the running sum, from each level down, of the measured parent-to-child steps
    // at twice for headroom: one step is not enough (at level 6 it is 0.000057 against a
    // subtree's 0.000128), and the gap culls patches whose children stand higher than they do.
    static constexpr BodyShape shape{{height_min, height_max}};
    static constexpr LevelTable levels{
        .error = {.0304741f, .0192836f, .00591285f, .00245085f, .00100089f, .000457055f, .000173189f, 8.54412e-5f,
                  4.493e-5f, 2.27982e-5f, 1.25439e-5f, 4.57292e-6f, 3.38214e-6f},
        .relief = {.0286f, .0136f, .0061f, .0036f, .0014f, .00056f, .00026f, .00015f, .000055f, .000016f, .000004f,
                   2e-6f, 2e-6f},
    };

    // Craters intersecting a spherical cap, to accelerate patch sampling.
    struct Region {
        std::vector<Crater> craters;
    };

    explicit MinorPlanetTerrain(std::uint64_t seed);

    // Slope-only micro-craters; nyquist = half texels/radian, strength = amplitude (0 disables).
    static constexpr float detail_frequency = 110; // lattice cells per radian of the first octave
    static constexpr unsigned detail_octaves = 3;
    static constexpr float detail_radius = .34f;  // a crater's, of a cell
    static constexpr float detail_reach = 2.2f;   // ejecta out to this many radii
    static constexpr float detail_depth = .3f;    // of the crater's radius, as the population's are
    static constexpr float detail_density = .55f; // the share of cells carrying one
    static constexpr float detail_slope = .15f;   // rms tangent the full stack adds at strength 1
    float detail(Vec3d direction, double nyquist, float strength) const;
    struct DetailCrater {
        Vec3d centre;
        double radius = 0, depth = 0; // lattice-cell units
    };
    struct DetailRegion {
        std::array<std::vector<DetailCrater>, detail_octaves> craters;
        std::array<double, detail_octaves> weights{};
    };
    // Complete projected crater support for a spherical cap, prepared once per tile.
    DetailRegion detail_region(Vec3d centre, double angular_radius, double nyquist) const;
    float detail(Vec3d direction, const DetailRegion& region, float strength) const;

    // Height above the reference sphere, radii, within [height_min, height_max].
    float height(Vec3d direction) const { return height(direction, craters_); }
    float height(Vec3d direction, const Region& region) const { return height(direction, region.craters); }
    Region region(Vec3d centre, double angular_radius) const;
    // Linear-light albedo at a point, given its height and slope (0 flat, 1 vertical).
    Vec3f albedo(Vec3d direction, float height, float slope) const {
        return albedo(direction, height, slope, craters_);
    }
    Vec3f albedo(Vec3d direction, float height, float slope, const Region& region) const {
        return albedo(direction, height, slope, region.craters);
    }
    std::span<const Crater> craters() const { return craters_; }
    std::uint64_t seed() const { return seed_; }

private:
    float height(Vec3d direction, std::span<const Crater> craters) const;
    Vec3f albedo(Vec3d direction, float height, float slope, std::span<const Crater> craters) const;

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

} // namespace planet
