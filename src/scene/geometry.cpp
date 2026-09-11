#include "scene/geometry.hpp"

#include <algorithm>
#include <cmath>

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

// Unit icosahedron, optionally subdivided once, as the base of every rock.
void icosahedron(std::vector<Vec3f>& points, std::vector<std::uint32_t>& faces, bool subdivide) {
    constexpr float t = 1.61803398875f; // golden ratio
    points = {{-1, t, 0},  {1, t, 0},  {-1, -t, 0}, {1, -t, 0}, {0, -1, t},  {0, 1, t},
              {0, -1, -t}, {0, 1, -t}, {t, 0, -1},  {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
    faces = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
             3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1};
    for (auto& p : points)
        p = normalized(p);
    if (!subdivide)
        return;
    std::vector<std::uint32_t> refined;
    refined.reserve(faces.size() * 4);
    for (std::size_t i = 0; i < faces.size(); i += 3) {
        const auto a = faces[i], b = faces[i + 1], c = faces[i + 2];
        const auto ab = static_cast<std::uint32_t>(points.size());
        points.push_back(normalized(points[a] + points[b]));
        const auto bc = static_cast<std::uint32_t>(points.size());
        points.push_back(normalized(points[b] + points[c]));
        const auto ca = static_cast<std::uint32_t>(points.size());
        points.push_back(normalized(points[c] + points[a]));
        refined.insert(refined.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
    }
    faces = std::move(refined);
}

} // namespace

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude) {
    return latitude_longitude_sphere(longitude, latitude, nullptr);
}

Mesh generate_rock(std::uint32_t seed, std::uint32_t detail) {
    // An irregular, plane-cut icosahedron avoids latitude rings and smooth
    // potato silhouettes. Vertices are split per triangle for real hard faces.
    constexpr unsigned cut_count = 5;
    std::vector<Vec3f> points;
    std::vector<std::uint32_t> faces;
    icosahedron(points, faces, detail >= 3);
    std::uint64_t state = seed;
    const Vec3f stretch{.58f + .42f * unit(state), .48f + .52f * unit(state), .58f + .42f * unit(state)};
    const float phase = unit(state) * 2 * pi<float>;
    Vec3f cuts[cut_count];
    float offsets[cut_count];
    for (unsigned i = 0; i < cut_count; i++) {
        cuts[i] = normalized(Vec3f{signed_unit(state), signed_unit(state), signed_unit(state)});
        offsets[i] = .48f + .25f * unit(state);
    }
    float bound = 0;
    for (auto& p : points) {
        const float radius = .9f + .17f * std::sin(3 * p.x + 2 * p.y + phase) +
                             .10f * std::sin(4 * p.z - 3 * p.y + phase * 1.7f);
        p = {p.x * stretch.x * radius, p.y * stretch.y * radius, p.z * stretch.z * radius};
        for (unsigned i = 0; i < cut_count; i++) {
            const float distance = dot(p, cuts[i]);
            if (distance > offsets[i])
                p = p * (offsets[i] / distance);
        }
        bound = std::max(bound, length(p));
    }
    for (auto& p : points)
        p = p * (1.f / bound);
    Mesh mesh;
    mesh.vertices.reserve(faces.size());
    mesh.indices.reserve(faces.size());
    for (std::size_t i = 0; i < faces.size(); i += 3) {
        Vec3f a = points[faces[i]], b = points[faces[i + 1]], c = points[faces[i + 2]];
        Vec3f n = cross(b - a, c - a);
        if (dot(n, a) < 0) {
            std::swap(b, c);
            n = -n;
        }
        n = normalized(n);
        for (Vec3f p : {a, b, c}) {
            const Vec3f d = normalized(p);
            const float u = std::atan2(d.z, d.x) / (2 * pi<float>)+.5f;
            const float v = std::asin(std::clamp(d.y, -1.f, 1.f)) / pi<float> + .5f;
            mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size()));
            mesh.vertices.push_back({.position = p, .normal = n, .u = u, .v = v});
        }
    }
    return mesh;
}

std::vector<AsteroidInstance> generate_belt(const BeltParams& params) {
    const float inner = std::max(0.0f, params.inner_radius);
    const float outer = std::max(inner, params.outer_radius);
    const float thickness = std::max(0.0f, params.thickness);
    std::vector<AsteroidInstance> result;
    result.reserve(params.count);
    std::uint64_t state = params.seed;
    for (std::uint32_t i = 0; i < params.count; ++i) {
        // Uniform in area over the annulus, then a random height in the slab.
        const float r = std::sqrt(inner * inner + unit(state) * (outer * outer - inner * inner));
        const float angle = 2.0f * pi<float> * unit(state);
        const float y = signed_unit(state) * thickness * 0.5f;
        const float s = 0.35f + 0.9f * unit(state);
        AsteroidInstance instance;
        instance.position = {r * std::cos(angle), y, r * std::sin(angle)};
        instance.scale = {s * (0.75f + 0.5f * unit(state)), s * (0.75f + 0.5f * unit(state)),
                          s * (0.75f + 0.5f * unit(state))};
        instance.rotation = {2.0f * pi<float> * unit(state), 2.0f * pi<float> * unit(state),
                             2.0f * pi<float> * unit(state)};
        instance.variant = static_cast<std::uint32_t>(splitmix64(state) & 3u);
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
