#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace space::geometry {
namespace {
constexpr float pi = 3.14159265358979323846f;

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(Vec3 a) {
    return std::sqrt(dot(a, a));
}
Vec3 normalized(Vec3 a) {
    const float l = length(a);
    return l > 1e-20f ? a * (1.0f / l) : Vec3{0, 1, 0};
}

// SplitMix is small, deterministic across standard-library implementations,
// and avoids the implementation-defined distributions in <random>.
std::uint64_t splitmix64(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ull;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}
float unit(std::uint64_t& state) {
    return static_cast<float>((splitmix64(state) >> 40) * (1.0 / 16777216.0));
}
float signed_unit(std::uint64_t& state) {
    return unit(state) * 2.0f - 1.0f;
}

Mesh sphere_impl(std::uint32_t longitude, std::uint32_t latitude, std::uint64_t* rock_seed) {
    longitude = std::max<std::uint32_t>(3, longitude);
    latitude = std::max<std::uint32_t>(2, latitude);
    Mesh mesh;
    const float seed_phase = rock_seed ? static_cast<float>((*rock_seed % 1000003u) * 0.00037) : 0.0f;
    mesh.vertices.reserve(static_cast<std::size_t>(longitude + 1) * (latitude + 1));
    for (std::uint32_t y = 0; y <= latitude; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(latitude);
        const float phi = pi * v;
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (std::uint32_t x = 0; x <= longitude; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(longitude);
            const float theta = 2.0f * pi * u;
            Vec3 p{sp * std::cos(theta), cp, sp * std::sin(theta)};
            if (rock_seed) {
                // Several low-frequency terms give broad facets rather than
                // white noise, while preserving a bounded silhouette.
                const float n = 0.55f * std::sin(3.0f * p.x + 1.7f * p.y + seed_phase) +
                                0.30f * std::sin(7.0f * p.z - 2.1f * p.x + seed_phase * 1.7f) +
                                0.15f * std::sin(11.0f * p.x - 5.0f * p.y + 3.0f * p.z + seed_phase * 2.3f);
                p = p * (1.0f + 0.13f * n);
            }
            mesh.vertices.push_back({p, normalized(p), u, 1.0f - v});
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
    if (rock_seed) {
        // Area-weighted normals remain continuous over the indexed surface and
        // point outward even where displacement makes the radial normal wrong.
        std::vector<Vec3> sums(mesh.vertices.size());
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const auto ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
            const Vec3 n = cross(mesh.vertices[ib].position - mesh.vertices[ia].position,
                                 mesh.vertices[ic].position - mesh.vertices[ia].position);
            sums[ia] = sums[ia] + n;
            sums[ib] = sums[ib] + n;
            sums[ic] = sums[ic] + n;
        }
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
            mesh.vertices[i].normal = normalized(sums[i]);
    }
    return mesh;
}
} // namespace

Mesh generate_sphere(std::uint32_t longitude, std::uint32_t latitude) {
    return sphere_impl(longitude, latitude, nullptr);
}

Mesh generate_rock(std::uint32_t seed, std::uint32_t detail) {
    // An irregular, plane-cut icosahedron avoids latitude rings and smooth
    // potato silhouettes. Vertices are split per triangle for real hard faces.
    constexpr float t = 1.61803398875f;
    std::vector<Vec3> points{{-1, t, 0},  {1, t, 0},  {-1, -t, 0}, {1, -t, 0}, {0, -1, t},  {0, 1, t},
                             {0, -1, -t}, {0, 1, -t}, {t, 0, -1},  {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
    std::vector<std::uint32_t> faces{0, 11, 5,  0, 5,  1, 0, 1, 7, 0, 7,  10, 0, 10, 11, 1, 5, 9, 5, 11,
                                     4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3,  9,  4, 3,  4,  2, 3, 2, 6, 3,
                                     6, 8,  3,  8, 9,  4, 9, 5, 2, 4, 11, 6,  2, 10, 8,  6, 7, 9, 8, 1};
    for (auto& p : points)
        p = normalized(p);
    if (detail >= 3) {
        std::vector<std::uint32_t> refined;
        refined.reserve(faces.size() * 4);
        for (std::size_t i = 0; i < faces.size(); i += 3) {
            auto a = faces[i], b = faces[i + 1], c = faces[i + 2];
            auto ab = static_cast<std::uint32_t>(points.size());
            points.push_back(normalized(points[a] + points[b]));
            auto bc = static_cast<std::uint32_t>(points.size());
            points.push_back(normalized(points[b] + points[c]));
            auto ca = static_cast<std::uint32_t>(points.size());
            points.push_back(normalized(points[c] + points[a]));
            refined.insert(refined.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        faces = std::move(refined);
    }
    std::uint64_t state = seed;
    const Vec3 stretch{.58f + .42f * unit(state), .48f + .52f * unit(state), .58f + .42f * unit(state)};
    const float phase = unit(state) * 2 * pi;
    Vec3 cuts[5];
    float offsets[5];
    for (unsigned i = 0; i < 5; i++) {
        cuts[i] = normalized({signed_unit(state), signed_unit(state), signed_unit(state)});
        offsets[i] = .48f + .25f * unit(state);
    }
    float bound = 0;
    for (auto& p : points) {
        float radius = .9f + .17f * std::sin(3 * p.x + 2 * p.y + phase) +
                       .10f * std::sin(4 * p.z - 3 * p.y + phase * 1.7f);
        p = {p.x * stretch.x * radius, p.y * stretch.y * radius, p.z * stretch.z * radius};
        for (unsigned i = 0; i < 5; i++) {
            float distance = dot(p, cuts[i]);
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
        Vec3 a = points[faces[i]], b = points[faces[i + 1]], c = points[faces[i + 2]];
        Vec3 n = cross(b - a, c - a);
        if (dot(n, a) < 0) {
            std::swap(b, c);
            n = n * (-1.f);
        }
        n = normalized(n);
        for (Vec3 p : {a, b, c}) {
            Vec3 d = normalized(p);
            float u = std::atan2(d.z, d.x) / (2 * pi) + .5f;
            float v = std::asin(std::clamp(d.y, -1.f, 1.f)) / pi + .5f;
            mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size()));
            mesh.vertices.push_back({p, n, u, v});
        }
    }
    return mesh;
}

std::vector<AsteroidInstance> generate_belt(std::uint64_t seed, std::uint32_t count, float inner, float outer,
                                            float thickness) {
    inner = std::max(0.0f, inner);
    outer = std::max(inner, outer);
    thickness = std::max(0.0f, thickness);
    std::vector<AsteroidInstance> result;
    result.reserve(count);
    std::uint64_t state = seed;
    for (std::uint32_t i = 0; i < count; ++i) {
        const float r = std::sqrt(inner * inner + unit(state) * (outer * outer - inner * inner));
        const float angle = 2.0f * pi * unit(state);
        const float y = signed_unit(state) * thickness * 0.5f;
        const float s = 0.35f + 0.9f * unit(state);
        result.push_back(
            {{r * std::cos(angle), y, r * std::sin(angle)},
             {s * (0.75f + 0.5f * unit(state)), s * (0.75f + 0.5f * unit(state)), s * (0.75f + 0.5f * unit(state))},
             {2.0f * pi * unit(state), 2.0f * pi * unit(state), 2.0f * pi * unit(state)},
             static_cast<std::uint32_t>(splitmix64(state) & 3u)});
    }
    return result;
}

std::uint32_t select_lod(float projected_radius_pixels, std::uint32_t previous) {
    if (!std::isfinite(projected_radius_pixels))
        projected_radius_pixels = 0.0f;
    projected_radius_pixels = std::max(0.0f, projected_radius_pixels);
    previous = std::min<std::uint32_t>(previous, 3);
    constexpr float t[3] = {24.0f, 80.0f, 240.0f};
    std::uint32_t desired = projected_radius_pixels < t[0]   ? 0
                            : projected_radius_pixels < t[1] ? 1
                            : projected_radius_pixels < t[2] ? 2
                                                             : 3;
    if (desired > previous && previous < 3 && projected_radius_pixels < t[previous] * 1.15f)
        return previous;
    if (desired < previous && previous > 0 && projected_radius_pixels >= t[previous - 1] * 0.85f)
        return previous;
    return desired;
}

bool sphere_in_frustum(const Plane* planes, std::uint32_t plane_count, Vec3 center, float radius) {
    if (!planes)
        return false;
    radius = std::max(0.0f, radius);
    for (std::uint32_t i = 0; i < plane_count; ++i)
        if (dot(planes[i].normal, center) + planes[i].distance < -radius)
            return false;
    return true;
}
bool sphere_in_frustum(const Frustum& frustum, Vec3 center, float radius) {
    return sphere_in_frustum(frustum.planes, 6, center, radius);
}
} // namespace space::geometry
