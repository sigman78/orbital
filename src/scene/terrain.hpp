#pragma once
#include "core/math.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// The planetoid's terrain, the one source of its shape and colour: the start-up
// bake of its maps reads it, and the near tier's patches will. Everything is a
// function of a direction from the centre, so any parameterisation (the
// equirectangular maps today, cube-sphere patches later) samples the same body.
// Heights are in radii, signed about the reference sphere.
class PlanetoidTerrain {
public:
    struct Crater {
        Vec3d centre;     // unit direction
        float radius = 0; // angular, radians
        float depth = 0;  // radii, at the floor
        float bright = 0; // 0 old and dark to 1 fresh and bright, for the albedo
    };
    static constexpr float height_min = -.04f, height_max = .06f; // the range a bake's alpha spans, radii

    explicit PlanetoidTerrain(std::uint64_t seed);

    // Height above the reference sphere, radii, within [height_min, height_max].
    float height(Vec3d direction) const;
    // Linear-light albedo at a point, given its height and slope (0 flat, 1 vertical).
    Vec3f albedo(Vec3d direction, float height, float slope) const;
    std::span<const Crater> craters() const { return craters_; }

private:
    std::uint64_t seed_;
    std::vector<Crater> craters_;
};

// Deterministic gradient noise over 3D, in [-1, 1]; the terrain's building block,
// public for tests and for a GPU port to check against.
float gradient_noise(Vec3d p, std::uint64_t seed);
// Fractional Brownian motion of gradient_noise: octaves doubling in frequency, halving in amplitude.
float fbm(Vec3d p, unsigned octaves, std::uint64_t seed);

} // namespace space
