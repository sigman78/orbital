#include "scene/terrain.hpp"

#include "core/noise.hpp"
#include "core/parallel.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace space {

namespace {

constexpr float crater_floor = .45f; // of the radius: flat inside, wall to the rim outside
constexpr float crater_reach = 3.f;  // radii out to which the ejecta and its brightening extend

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// Angular distance from the crater's centre over its radius: 0 at the centre,
// 1 at the rim; or nothing when the point is out of its reach.
std::optional<float> crater_x(const MinorPlanetTerrain::Crater& crater, Vec3d d) {
    const double c = dot(d, crater.centre);
    if (c < crater.cos_reach)
        return std::nullopt;
    return float(std::acos(std::min(c, 1.0))) / crater.radius;
}

} // namespace

// The population: sizes on the D^-2 law of a saturated surface (truncated, so
// many small and a few large), ages uniform, deep bowls for the small ones and
// shallower complex floors with a central peak for the large.
MinorPlanetTerrain::MinorPlanetTerrain(std::uint64_t seed) : seed_(seed) {
    constexpr unsigned count = 420;
    constexpr double radius_min = .018, radius_max = .17;
    constexpr double tail = 1 - (radius_min / radius_max) * (radius_min / radius_max);
    std::uint64_t state = mix64(seed ^ 0x43524154455253ull);
    craters_.reserve(count);
    for (unsigned i = 0; i < count; i++) {
        const double z = 2 * uniform(state) - 1, phi = 2 * pi<double> * uniform(state), s = std::sqrt(1 - z * z);
        const float radius = float(radius_min / std::sqrt(1 - uniform(state) * tail));
        const float age = float(uniform(state));
        const float complex = smoothstep(.04f, .14f, radius);
        const float facula = radius > .05f && uniform(state) < .15 ? .5f + .5f * float(uniform(state)) : 0;
        craters_.push_back({.centre = {s * std::cos(phi), z, s * std::sin(phi)},
                            .radius = radius,
                            .depth = radius * (.32f - .16f * complex) * (1 - .5f * age),
                            .age = age,
                            .facula = facula,
                            .cos_reach = float(std::cos(std::min(double(radius) * crater_reach, pi<double>)))});
    }
}

MinorPlanetTerrain::Region MinorPlanetTerrain::region(Vec3d centre, double angular_radius) const {
    Region region;
    const Vec3d c = normalized(centre);
    for (const Crater& crater : craters_) {
        const double reach = std::acos(std::clamp(double(crater.cos_reach), -1.0, 1.0)) + angular_radius;
        if (reach >= pi<double> || dot(c, crater.centre) >= std::cos(reach))
            region.craters.push_back(crater);
    }
    return region;
}

float MinorPlanetTerrain::height(Vec3d direction, std::span<const Crater> craters) const {
    const Vec3d d = normalized(direction);
    // Rolling ground and a sharper ridged term for the highlands.
    float h = .018f * fbm(d * 2.6, 5, seed_);
    const float ridged = ridged_fbm(d * 7.0, 4, seed_ + 77);
    h += .012f * ridged * ridged;
    // Craters: a flat floor, a wall rising to the rim, ejecta fading past it,
    // and a central peak on the large ones.
    for (const Crater& crater : craters) {
        const auto x = crater_x(crater, d);
        if (!x)
            continue;
        const float rim = .25f * crater.depth;
        if (*x < 1) {
            h += -crater.depth + (crater.depth + rim) * smoothstep(crater_floor, 1, *x);
            if (crater.radius > .07f)
                h += .4f * crater.depth * std::exp(-*x * *x * 60);
        } else {
            h += rim * std::exp(-(*x - 1) * 4);
        }
    }
    return std::clamp(h, height_min, height_max);
}

float MinorPlanetTerrain::detail(Vec3d direction, double nyquist, float strength) const {
    if (strength <= 0)
        return 0;
    const Vec3d d = normalized(direction);
    double h = 0;
    for (unsigned octave = 0; octave < detail_octaves; octave++) {
        const double frequency = double(detail_frequency) * (1u << octave);
        const double weight = std::clamp(std::log2(nyquist / frequency), 0.0, 1.0);
        if (weight <= 0)
            break; // and so is every octave above this one
        // Equal slope from each octave, adding in quadrature, so the amplitude falls
        // with the frequency. An octave of amplitude A at f cycles per radian has an
        // RMS slope of about 1.1 * A * f on this noise, measured against the tiles.
        const double amplitude = double(strength) * detail_slope /
                                 (std::sqrt(double(detail_octaves)) * 1.1 * frequency);
        h += weight * amplitude * gradient_noise(d * frequency, seed_ + 0xD1 + octave);
    }
    return float(h);
}

Vec3f MinorPlanetTerrain::albedo(Vec3d direction, float height, float slope) const {
    const Vec3d d = normalized(direction);
    // Ceres's dark grey ground, with Pluto's warm dark maculae over parts of
    // it and a lighter frost where the lowlands pool; steep faces shed their
    // dust and read a little brighter.
    const float lows = std::clamp((height - height_min) / (height_max - height_min), 0.f, 1.f);
    const float macula = smoothstep(.05f, .5f, fbm(d * 1.3 + Vec3d{3.1, 0, 0}, 3, seed_ + 5));
    const float frost = smoothstep(.3f, .7f, fbm(d * 2.1 + Vec3d{0, 7.7, 0}, 3, seed_ + 9)) * (1 - macula);
    Vec3f albedo = lerp(Vec3f{.13f, .122f, .112f}, Vec3f{.085f, .066f, .05f}, macula);
    albedo = albedo * (.9f + .25f * (1 - lows) + .6f * frost) * (1 + .5f * slope);
    // Fresh craters: the excavated floor and walls dark, the ejecta blanket
    // bright and patchy, worn ones neither; a few carry a bright facula at the centre.
    const float patchy = .4f + .6f * (1 + fbm(d * 30.0, 2, seed_ + 13));
    for (const Crater& crater : craters_) {
        const auto x = crater_x(crater, d);
        if (!x)
            continue;
        const float fresh = (1 - crater.age) * (1 - crater.age);
        const float excavated = 1 - smoothstep(.9f, 1.1f, *x);
        const float blanket = smoothstep(.85f, 1.05f, *x) * std::exp(-(*x - 1) * 1.8f) * patchy;
        albedo = albedo * (1 - .4f * fresh * excavated + .55f * fresh * blanket);
        if (crater.facula > 0)
            albedo = lerp(albedo, Vec3f{.6f, .58f, .54f}, crater.facula * std::exp(-*x * *x * 16));
    }
    return {std::min(albedo.x, 1.f), std::min(albedo.y, 1.f), std::min(albedo.z, 1.f)};
}

// The heights are sampled first so the normals come from finite differences of
// the same values the alpha carries; rows go to the cores.
TerrainMaps bake_terrain_maps(const MinorPlanetTerrain& terrain, unsigned width, unsigned height) {
    TerrainMaps maps{.width = width, .height = height, .albedo = {}, .normal = {}};
    const std::size_t count = std::size_t(width) * height;
    maps.albedo.resize(count * 4);
    maps.normal.resize(count * 4);
    const auto direction = [](double u, double v) {
        const double angle = (u - .5) * 2 * pi<double>, latitude = (.5 - v) * pi<double>;
        const double c = std::cos(latitude);
        return Vec3d{c * std::cos(angle), std::sin(latitude), c * std::sin(angle)};
    };
    std::vector<float> heights(count);
    parallel_for(height, [&](unsigned y) {
        for (unsigned x = 0; x < width; x++)
            heights[std::size_t(y) * width + x] = terrain.height(direction((x + .5) / width, (y + .5) / height));
    });
    const float range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
    const auto at = [&](unsigned x, unsigned y) { return heights[std::size_t(y) * width + (x % width)]; };
    const auto byte = [](float v) { return std::uint8_t(std::clamp(v * 255.f + .5f, 0.f, 255.f)); };
    parallel_for(height, [&](unsigned y) {
        const double v = (y + .5) / height, latitude = (.5 - v) * pi<double>;
        const float cos_latitude = std::max(float(std::cos(latitude)), .05f);
        const float east_arc = 2 * float(pi<double>) / width * cos_latitude; // radii per texel along u
        const float south_arc = float(pi<double>) / height;                  // radii per texel along v
        for (unsigned x = 0; x < width; x++) {
            const float h = at(x, y);
            const float east = (at(x + 1, y) - at(x + width - 1, y)) / (2 * east_arc);
            const float south = (at(x, std::min(y + 1, height - 1)) - at(x, y == 0 ? 0 : y - 1)) / (2 * south_arc);
            const Vec3f n = normalized(Vec3f{-east, -south, 1});
            const std::size_t p = (std::size_t(y) * width + x) * 4;
            maps.normal[p] = byte(n.x * .5f + .5f);
            maps.normal[p + 1] = byte(n.y * .5f + .5f);
            maps.normal[p + 2] = byte(n.z * .5f + .5f);
            maps.normal[p + 3] = byte((h - MinorPlanetTerrain::height_min) / range);
            const Vec3f colour = terrain.albedo(direction((x + .5) / width, v), h, 1 - n.z);
            maps.albedo[p] = byte(colour.x);
            maps.albedo[p + 1] = byte(colour.y);
            maps.albedo[p + 2] = byte(colour.z);
            maps.albedo[p + 3] = 255;
        }
    });
    return maps;
}

} // namespace space
