#pragma once
#include "core/math.hpp"

#include <cstdint>

namespace space {

// Seeded hashing and noise, exact across compilers, for anything generated
// from a seed on the CPU (and for a GPU port to check against).

// SplitMix64's finaliser: a well-mixed word from any integer.
std::uint64_t mix64(std::uint64_t x);
// Advances a SplitMix64 state and returns a uniform double in [0, 1).
double uniform(std::uint64_t& state);

// Gradient noise over 3D (Perlin's twelve edge gradients), in [-1, 1], peaking near 0.9.
float gradient_noise(Vec3d p, std::uint64_t seed);
// Fractional Brownian motion of gradient_noise: octaves doubling in frequency
// and halving in amplitude, normalised to [-1, 1].
float fbm(Vec3d p, unsigned octaves, std::uint64_t seed);
// Crests where fbm crosses zero, in [0, 1]; the highland and ridge term.
float ridged_fbm(Vec3d p, unsigned octaves, std::uint64_t seed);

} // namespace space
