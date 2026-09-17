#include "core/noise.hpp"

#include <cmath>

namespace space {

namespace {

// A unit gradient for a lattice point, one of the twelve edge directions of the cube.
Vec3d lattice_gradient(long x, long y, long z, std::uint64_t seed) {
    const std::uint64_t h = mix64(seed ^ (std::uint64_t(x) * 0x9e3779b97f4a7c15ull) ^
                                  (std::uint64_t(y) * 0xc2b2ae3d27d4eb4full) ^
                                  (std::uint64_t(z) * 0x165667b19e3779f9ull));
    constexpr double g[12][3] = {{1, 1, 0},  {-1, 1, 0},  {1, -1, 0}, {-1, -1, 0}, {1, 0, 1},  {-1, 0, 1},
                                 {1, 0, -1}, {-1, 0, -1}, {0, 1, 1},  {0, -1, 1},  {0, 1, -1}, {0, -1, -1}};
    const auto& v = g[h % 12];
    return {v[0], v[1], v[2]};
}

double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

} // namespace

std::uint64_t mix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

double uniform(std::uint64_t& state) {
    state = mix64(state);
    return double(state >> 11) * (1.0 / 9007199254740992.0);
}

float gradient_noise(Vec3d p, std::uint64_t seed) {
    const double fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    const long x0 = long(fx), y0 = long(fy), z0 = long(fz);
    const Vec3d f{p.x - fx, p.y - fy, p.z - fz};
    const double u = fade(f.x), v = fade(f.y), w = fade(f.z);
    double corner[2][2][2];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 2; k++) {
                const Vec3d g = lattice_gradient(x0 + i, y0 + j, z0 + k, seed);
                corner[i][j][k] = dot(g, Vec3d{f.x - i, f.y - j, f.z - k});
            }
    const auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
    const double x00 = lerp(corner[0][0][0], corner[1][0][0], u), x10 = lerp(corner[0][1][0], corner[1][1][0], u);
    const double x01 = lerp(corner[0][0][1], corner[1][0][1], u), x11 = lerp(corner[0][1][1], corner[1][1][1], u);
    return float(lerp(lerp(x00, x10, v), lerp(x01, x11, v), w));
}

float fbm(Vec3d p, unsigned octaves, std::uint64_t seed) {
    float sum = 0, amplitude = .5f, norm = 0;
    for (unsigned o = 0; o < octaves; o++) {
        sum += amplitude * gradient_noise(p, seed + o * 0x51ed27u);
        norm += amplitude;
        p = p * 2.03 + Vec3d{17.1, 31.7, 5.3};
        amplitude *= .5f;
    }
    return sum / norm;
}

float ridged_fbm(Vec3d p, unsigned octaves, std::uint64_t seed) {
    return 1 - std::abs(fbm(p, octaves, seed));
}

} // namespace space
