#include "scene/terrain.hpp"

#include <algorithm>
#include <cmath>

namespace space {

namespace {

std::uint64_t mix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

double unit(std::uint64_t& state) {
    state = mix64(state);
    return double(state >> 11) * (1.0 / 9007199254740992.0);
}

// A unit gradient for a lattice point, one of twelve edge directions of the cube (Perlin's set).
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
    return float(lerp(lerp(x00, x10, v), lerp(x01, x11, v), w)); // gradient noise peaks near 0.9, not 1
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

// The population of craters: many small, few large, over the sphere; the same seed
// gives the same body.
PlanetoidTerrain::PlanetoidTerrain(std::uint64_t seed) : seed_(seed) {
    constexpr unsigned count = 160;
    std::uint64_t state = mix64(seed ^ 0x43524154455253ull);
    craters_.reserve(count);
    for (unsigned i = 0; i < count; i++) {
        const double z = 2 * unit(state) - 1, phi = 2 * pi<double> * unit(state), s = std::sqrt(1 - z * z);
        const float radius = float(.025 * std::pow(10.0, 0.8 * unit(state) * unit(state))); // 0.025 to 0.16 radians
        craters_.push_back({.centre = {s * std::cos(phi), z, s * std::sin(phi)},
                            .radius = radius,
                            .depth = .12f * radius,
                            .bright = float(unit(state) < .2 ? unit(state) : 0)});
    }
}

float PlanetoidTerrain::height(Vec3d direction) const {
    const Vec3d d = normalized(direction);
    // Rolling ground and a sharper ridged term for the highlands.
    float h = .018f * fbm(d * 2.6, 5, seed_);
    const float ridged = 1 - std::abs(fbm(d * 7.0, 4, seed_ + 77));
    h += .012f * ridged * ridged;
    // Craters: a bowl to the floor, a raised rim, ejecta fading past it.
    for (const Crater& crater : craters_) {
        const double c = dot(d, crater.centre);
        if (c < std::cos(crater.radius * 1.8))
            continue;
        const float x = float(std::acos(std::min(c, 1.0))) / crater.radius; // 0 centre, 1 rim
        if (x < 1)
            h += crater.depth * (1.25f * x * x - 1);
        else
            h += .25f * crater.depth * std::exp(-(x - 1) * 5);
    }
    return std::clamp(h, height_min, height_max);
}

Vec3f PlanetoidTerrain::albedo(Vec3d direction, float height, float slope) const {
    const Vec3d d = normalized(direction);
    // Dark regolith, warmer where the lowlands pool, brighter on steep faces
    // that shed their dust, and around the fresh craters.
    const float lows = std::clamp((height - height_min) / (height_max - height_min), 0.f, 1.f);
    const float tint = .5f + .5f * fbm(d * 1.7 + Vec3d{3.1, 0, 0}, 3, seed_ + 5);
    Vec3f albedo{.19f, .175f, .16f};
    albedo = albedo * (.85f + .3f * lows) + Vec3f{.03f, .01f, 0} * tint;
    albedo = albedo * (1 + .8f * slope);
    for (const Crater& crater : craters_) {
        if (crater.bright <= 0)
            continue;
        const double c = dot(d, crater.centre);
        if (c < std::cos(crater.radius * 3))
            continue;
        const float x = float(std::acos(std::min(c, 1.0))) / crater.radius;
        albedo = albedo * (1 + crater.bright * std::exp(-x * x * .6f));
    }
    return {std::min(albedo.x, 1.f), std::min(albedo.y, 1.f), std::min(albedo.z, 1.f)};
}

} // namespace space
