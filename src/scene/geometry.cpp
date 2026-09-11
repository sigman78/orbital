#include "scene/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace space::geometry {
namespace {

// SplitMix is small, deterministic across standard-library implementations,
// and avoids the implementation-defined distributions in <random>.
std::uint64_t splitmix64(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ull;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

// Uniform in [0, 1) from the top 24 bits, which a float holds exactly.
float unit(std::uint64_t& state) {
    return static_cast<float>(splitmix64(state) >> 40) * (1.0f / 16777216.0f);
}

float signed_unit(std::uint64_t& state) {
    return unit(state) * 2.0f - 1.0f;
}

// Integer-lattice value noise: exact across compilers, unlike sin-based hashes.
float lattice(int x, int y, int z, std::uint64_t seed) {
    std::uint64_t state = seed ^ (std::uint64_t(std::uint32_t(x)) * 0x9E3779B1ull) ^
                          (std::uint64_t(std::uint32_t(y)) * 0x85EBCA77ull) ^
                          (std::uint64_t(std::uint32_t(z)) * 0xC2B2AE3Dull);
    return unit(state);
}

float value_noise(Vec3f p, std::uint64_t seed) {
    const float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    const int x = int(fx), y = int(fy), z = int(fz);
    Vec3f t{p.x - fx, p.y - fy, p.z - fz};
    t = {t.x * t.x * (3 - 2 * t.x), t.y * t.y * (3 - 2 * t.y), t.z * t.z * (3 - 2 * t.z)};
    const auto mix = [](float a, float b, float f) { return a + (b - a) * f; };
    const float c00 = mix(lattice(x, y, z, seed), lattice(x + 1, y, z, seed), t.x);
    const float c10 = mix(lattice(x, y + 1, z, seed), lattice(x + 1, y + 1, z, seed), t.x);
    const float c01 = mix(lattice(x, y, z + 1, seed), lattice(x + 1, y, z + 1, seed), t.x);
    const float c11 = mix(lattice(x, y + 1, z + 1, seed), lattice(x + 1, y + 1, z + 1, seed), t.x);
    return mix(mix(c00, c10, t.y), mix(c01, c11, t.y), t.z);
}

float fbm(Vec3f p, std::uint64_t seed, unsigned octaves) {
    float sum = 0, amplitude = .5f;
    for (unsigned i = 0; i < octaves; ++i) {
        sum += amplitude * value_noise(p, seed + i);
        p = p * 2.03f + Vec3f{1.7f, 9.2f, 4.1f};
        amplitude *= .5f;
    }
    return sum;
}

// Everything a rock's shape function needs, drawn once from the seed.
struct RockShape {
    struct Crater {
        Vec3f centre; // unit direction
        float radius; // angular radius in radians
        float depth;  // fraction of the local radius
    };
    struct Cut {
        Vec3f normal;
        float offset;
    };
    static constexpr unsigned cut_count = 5, max_craters = 24;
    Vec3f stretch;
    float phase;
    std::uint64_t noise_seed;
    Cut cuts[cut_count];
    Crater craters[max_craters];
    unsigned crater_count;
    float scale; // normalises the finest level into the unit sphere
};

// Broad lumps, a ridged noise skin and bowl craters with raised rims.
float rock_radius(const RockShape& shape, Vec3f dir) {
    float r = .9f + .17f * std::sin(3 * dir.x + 2 * dir.y + shape.phase) +
              .10f * std::sin(4 * dir.z - 3 * dir.y + shape.phase * 1.7f);
    const float ridged = 1 - std::abs(2 * fbm(dir * 2.5f, shape.noise_seed, 3) - 1);
    r += .07f * (ridged - .6f) + .05f * (fbm(dir * 9.f, shape.noise_seed + 11, 3) - .5f) +
         .02f * (fbm(dir * 22.f, shape.noise_seed + 23, 2) - .5f);
    for (unsigned i = 0; i < shape.crater_count; ++i) {
        const auto& crater = shape.craters[i];
        const float cosine = dot(dir, crater.centre);
        if (cosine < .5f)
            continue;
        const float d = std::acos(std::min(cosine, 1.f)) / crater.radius; // 0 at the centre, 1 at the rim
        if (d >= 1.5f)
            continue;
        const float bowl = d < 1 ? (d * d - 1) : 0.f;                  // sunk floor
        const float rim = .3f * std::exp(-(d - 1) * (d - 1) * 25.f);   // raised, rounded rim
        const float fade = 1 - std::clamp((d - 1.1f) / .4f, 0.f, 1.f); // rim blends back out
        r += crater.depth * (bowl + rim * fade);
    }
    return r;
}

// Displaced point for a direction, including the plane cuts (which are radial
// scalings, so the result is still a function of direction alone).
Vec3f rock_point(const RockShape& shape, Vec3f dir) {
    const float r = rock_radius(shape, dir);
    Vec3f p{dir.x * shape.stretch.x * r, dir.y * shape.stretch.y * r, dir.z * shape.stretch.z * r};
    for (const auto& cut : shape.cuts) {
        const float distance = dot(p, cut.normal);
        if (distance > cut.offset)
            p = p * (cut.offset / distance);
    }
    return p * shape.scale;
}

// Normal from the shape function itself (central differences on the sphere),
// so every level lights the same way and the finest level is not needed.
Vec3f rock_normal(const RockShape& shape, Vec3f dir) {
    constexpr float epsilon = .008f; // radians; below the finest level's edge length
    const Vec3f helper = std::abs(dir.y) < .9f ? Vec3f{0, 1, 0} : Vec3f{1, 0, 0};
    const Vec3f t1 = normalized(cross(helper, dir)), t2 = cross(dir, t1);
    const Vec3f du = rock_point(shape, normalized(dir + t1 * epsilon)) -
                     rock_point(shape, normalized(dir - t1 * epsilon));
    const Vec3f dv = rock_point(shape, normalized(dir + t2 * epsilon)) -
                     rock_point(shape, normalized(dir - t2 * epsilon));
    const Vec3f n = normalized(cross(du, dv));
    return dot(n, dir) < 0 ? -n : n;
}

RockShape make_rock_shape(std::uint32_t seed) {
    std::uint64_t state = seed;
    RockShape shape{};
    shape.stretch = {.58f + .42f * unit(state), .48f + .52f * unit(state), .58f + .42f * unit(state)};
    shape.phase = unit(state) * 2 * pi<float>;
    shape.noise_seed = splitmix64(state);
    for (auto& cut : shape.cuts) {
        cut.normal = normalized(Vec3f{signed_unit(state), signed_unit(state), signed_unit(state)});
        cut.offset = .48f + .25f * unit(state);
    }
    shape.crater_count = 12 + unsigned(unit(state) * 12);
    for (unsigned i = 0; i < shape.crater_count; ++i) {
        auto& crater = shape.craters[i];
        crater.centre = normalized(Vec3f{signed_unit(state), signed_unit(state), signed_unit(state)});
        crater.radius = .06f + .3f * unit(state) * unit(state); // many small, few large
        crater.depth = .04f + .08f * unit(state);
    }
    shape.scale = 1;
    return shape;
}

// Icosphere with shared vertices: each subdivision splits every edge once,
// so coarser levels' vertices are subsets of finer ones.
void icosphere(std::vector<Vec3f>& points, std::vector<std::uint32_t>& faces, std::uint32_t level) {
    constexpr float t = 1.61803398875f; // golden ratio
    points = {{-1, t, 0},  {1, t, 0},  {-1, -t, 0}, {1, -t, 0}, {0, -1, t},  {0, 1, t},
              {0, -1, -t}, {0, 1, -t}, {t, 0, -1},  {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
    faces = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
             3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1};
    for (auto& p : points)
        p = normalized(p);
    for (std::uint32_t pass = 0; pass < level; ++pass) {
        std::unordered_map<std::uint64_t, std::uint32_t> midpoints;
        midpoints.reserve(faces.size());
        const auto midpoint = [&](std::uint32_t a, std::uint32_t b) {
            const std::uint64_t key = (std::uint64_t(std::min(a, b)) << 32) | std::max(a, b);
            const auto found = midpoints.find(key);
            if (found != midpoints.end())
                return found->second;
            const auto index = static_cast<std::uint32_t>(points.size());
            points.push_back(normalized(points[a] + points[b]));
            midpoints.emplace(key, index);
            return index;
        };
        std::vector<std::uint32_t> refined;
        refined.reserve(faces.size() * 4);
        for (std::size_t i = 0; i < faces.size(); i += 3) {
            const auto a = faces[i], b = faces[i + 1], c = faces[i + 2];
            const auto ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            refined.insert(refined.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        faces = std::move(refined);
    }
}

// Several low-frequency terms give broad facets rather than white noise,
// while preserving a bounded silhouette.
float rock_displacement(Vec3f p, float seed_phase) {
    return 0.55f * std::sin(3.0f * p.x + 1.7f * p.y + seed_phase) +
           0.30f * std::sin(7.0f * p.z - 2.1f * p.x + seed_phase * 1.7f) +
           0.15f * std::sin(11.0f * p.x - 5.0f * p.y + 3.0f * p.z + seed_phase * 2.3f);
}

// Area-weighted normals remain continuous over the indexed surface and point
// outward even where displacement makes the radial normal wrong.
void recompute_smooth_normals(Mesh& mesh) {
    std::vector<Vec3f> sums(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        const Vec3f n = cross(mesh.vertices[ib].position - mesh.vertices[ia].position,
                              mesh.vertices[ic].position - mesh.vertices[ia].position);
        sums[ia] += n;
        sums[ib] += n;
        sums[ic] += n;
    }
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
        mesh.vertices[i].normal = normalized(sums[i]);
}

Mesh latitude_longitude_sphere(std::uint32_t longitude, std::uint32_t latitude, const std::uint64_t* rock_seed) {
    longitude = std::max<std::uint32_t>(3, longitude);
    latitude = std::max<std::uint32_t>(2, latitude);
    Mesh mesh;
    const float seed_phase = rock_seed ? static_cast<float>((*rock_seed % 1000003u) * 0.00037) : 0.0f;
    mesh.vertices.reserve(static_cast<std::size_t>(longitude + 1) * (latitude + 1));
    for (std::uint32_t y = 0; y <= latitude; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(latitude);
        const float phi = pi<float> * v;
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (std::uint32_t x = 0; x <= longitude; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(longitude);
            const float theta = 2.0f * pi<float> * u;
            Vec3f p{sp * std::cos(theta), cp, sp * std::sin(theta)};
            if (rock_seed)
                p = p * (1.0f + 0.13f * rock_displacement(p, seed_phase));
            mesh.vertices.push_back({.position = p, .normal = normalized(p), .u = u, .v = 1.0f - v});
        }
    }
    mesh.indices.reserve(static_cast<std::size_t>(longitude) * latitude * 6);
    const std::uint32_t row = longitude + 1;
    for (std::uint32_t y = 0; y < latitude; ++y) {
        for (std::uint32_t x = 0; x < longitude; ++x) {
            const std::uint32_t a = y * row + x, b = a + 1, c = a + row, d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    if (rock_seed)
        recompute_smooth_normals(mesh);
    return mesh;
}

} // namespace

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude) {
    return latitude_longitude_sphere(longitude, latitude, nullptr);
}

Mesh generate_rock(std::uint32_t seed, std::uint32_t level) {
    level = std::min(level, rock_level_count - 1);
    RockShape shape = make_rock_shape(seed);
    // Normalise against the finest level; coarser levels' directions are a
    // subset of it, so no level ever leaves the unit sphere.
    std::vector<Vec3f> directions;
    std::vector<std::uint32_t> faces;
    icosphere(directions, faces, rock_level_count - 1);
    float bound = 0;
    for (const auto& dir : directions)
        bound = std::max(bound, length(rock_point(shape, dir)));
    shape.scale = 1.f / bound;
    icosphere(directions, faces, level);
    Mesh mesh;
    mesh.vertices.reserve(directions.size());
    for (const auto& dir : directions) {
        const float u = std::atan2(dir.z, dir.x) / (2 * pi<float>)+.5f;
        const float v = std::asin(std::clamp(dir.y, -1.f, 1.f)) / pi<float> + .5f;
        mesh.vertices.push_back(
            {.position = rock_point(shape, dir), .normal = rock_normal(shape, dir), .u = u, .v = v});
    }
    mesh.indices.reserve(faces.size());
    for (std::size_t i = 0; i < faces.size(); i += 3) {
        std::uint32_t a = faces[i], b = faces[i + 1], c = faces[i + 2];
        const Vec3f pa = mesh.vertices[a].position, pb = mesh.vertices[b].position, pc = mesh.vertices[c].position;
        if (dot(cross(pb - pa, pc - pa), pa + pb + pc) < 0)
            std::swap(b, c);
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }
    return mesh;
}

std::uint32_t select_rock_level(float projected_radius_pixels) {
    constexpr float thresholds[rock_level_count - 1] = {3.f, 8.f, 20.f, 50.f, 130.f}; // projected radius in pixels
    if (!std::isfinite(projected_radius_pixels))
        return 0;
    std::uint32_t level = 0;
    while (level < rock_level_count - 1 && projected_radius_pixels >= thresholds[level])
        ++level;
    return level;
}

float belt_ring_density(float t) {
    struct Rings {
        float count = 3.f, phase = .15f;            // ring pattern across the belt width
        float gap_center = .58f, gap_width = .035f; // one thin sparse lane
        float floor = .5f, gap_depth = .7f;         // density never drops below floor * (1 - gap_depth)
    };
    constexpr Rings rings{};
    const float bands = .5f + .5f * std::cos(2 * pi<float> * (t * rings.count + rings.phase));
    const float gap = (t - rings.gap_center) / rings.gap_width;
    return (rings.floor + (1 - rings.floor) * bands) * (1 - rings.gap_depth * std::exp(-gap * gap));
}

std::vector<AsteroidInstance> generate_belt(const BeltParams& params) {
    const float inner = std::max(0.0f, params.inner_radius);
    const float outer = std::max(inner, params.outer_radius);
    const float thickness = std::max(0.0f, params.thickness);
    constexpr unsigned ring_attempts = 16; // rejection sampling against belt_ring_density
    std::vector<AsteroidInstance> result;
    result.reserve(params.count);
    std::uint64_t state = params.seed;
    for (std::uint32_t i = 0; i < params.count; ++i) {
        // Uniform in area over the annulus, thinned into rings, then a random height in the slab.
        float r = inner;
        for (unsigned attempt = 0; attempt < ring_attempts; ++attempt) {
            r = std::sqrt(inner * inner + unit(state) * (outer * outer - inner * inner));
            if (unit(state) <= belt_ring_density((r - inner) / std::max(outer - inner, 1e-6f)))
                break;
        }
        const float angle = 2.0f * pi<float> * unit(state);
        const float y = signed_unit(state) * thickness * 0.5f;
        const float s = 0.35f + 0.9f * unit(state);
        AsteroidInstance instance;
        instance.position = {r * std::cos(angle), y, r * std::sin(angle)};
        instance.scale = {s * (0.75f + 0.5f * unit(state)), s * (0.75f + 0.5f * unit(state)),
                          s * (0.75f + 0.5f * unit(state))};
        instance.rotation = {2.0f * pi<float> * unit(state), 2.0f * pi<float> * unit(state),
                             2.0f * pi<float> * unit(state)};
        // Mostly a spin about the local Y axis at an individual rate and sense,
        // with a slower tumble about the others.
        const float sense = unit(state) < .5f ? -1.f : 1.f;
        instance.spin = {.35f * signed_unit(state), sense * (.4f + 1.2f * unit(state)), .35f * signed_unit(state)};
        instance.variant = static_cast<std::uint32_t>(splitmix64(state) % rock_shape_count);
        result.push_back(instance);
    }
    return result;
}

std::uint32_t select_lod(float projected_radius_pixels, std::uint32_t previous) {
    struct LodSettings {
        float thresholds[lod_count - 1] = {24.0f, 80.0f, 240.0f}; // projected radius in pixels
        float grow_hysteresis = 1.15f, shrink_hysteresis = 0.85f;
    };
    constexpr LodSettings lod{};
    if (!std::isfinite(projected_radius_pixels))
        projected_radius_pixels = 0.0f;
    projected_radius_pixels = std::max(0.0f, projected_radius_pixels);
    previous = std::min(previous, lod_count - 1);
    std::uint32_t desired = 0;
    while (desired < lod_count - 1 && projected_radius_pixels >= lod.thresholds[desired])
        ++desired;
    if (desired > previous && previous < lod_count - 1 &&
        projected_radius_pixels < lod.thresholds[previous] * lod.grow_hysteresis)
        return previous;
    if (desired < previous && previous > 0 &&
        projected_radius_pixels >= lod.thresholds[previous - 1] * lod.shrink_hysteresis)
        return previous;
    return desired;
}

bool sphere_in_frustum(std::span<const Plane> planes, Vec3f center, float radius) {
    radius = std::max(0.0f, radius);
    for (const auto& plane : planes)
        if (dot(plane.normal, center) + plane.distance < -radius)
            return false;
    return true;
}

} // namespace space::geometry
