#include "scene/geometry.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>

using namespace space::geometry;

static float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static float length_squared(Vec3 value) {
    return dot(value, value);
}
static bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static void validate_flat_rock(const Mesh& rock) {
    constexpr float radius_limit_squared = 1.0001f * 1.0001f;
    constexpr float normal_tolerance = 1e-5f;
    constexpr float area_epsilon_squared = 1e-12f;
    assert(!rock.vertices.empty());
    assert(!rock.indices.empty() && rock.indices.size() % 3 == 0);
    for (const auto& vertex : rock.vertices) {
        assert(finite(vertex.position) && finite(vertex.normal));
        assert(length_squared(vertex.position) <= radius_limit_squared);
        assert(std::abs(length_squared(vertex.normal) - 1.0f) <= normal_tolerance);
    }
    for (std::size_t face = 0; face < rock.indices.size(); face += 3) {
        const std::uint32_t ia = rock.indices[face], ib = rock.indices[face + 1], ic = rock.indices[face + 2];
        assert(ia < rock.vertices.size() && ib < rock.vertices.size() && ic < rock.vertices.size());
        const auto& a = rock.vertices[ia];
        const auto& b = rock.vertices[ib];
        const auto& c = rock.vertices[ic];
        const Vec3 face_cross = cross(b.position - a.position, c.position - a.position);
        assert(finite(face_cross) && length_squared(face_cross) > area_epsilon_squared);
        const Vec3 centroid = (a.position + b.position + c.position) * (1.0f / 3.0f);
        assert(dot(face_cross, centroid) > 0.0f);
        assert(length_squared(a.normal - b.normal) <= normal_tolerance * normal_tolerance);
        assert(length_squared(a.normal - c.normal) <= normal_tolerance * normal_tolerance);
        assert(dot(face_cross, a.normal) > 0.0f);
    }
}

static bool distinct_silhouette(const Mesh& a, const Mesh& b) {
    if (a.vertices.size() != b.vertices.size())
        return true;
    for (std::size_t index = 0; index < a.vertices.size(); ++index) {
        if (!(a.vertices[index].position == b.vertices[index].position))
            return true;
    }
    return false;
}

int main() {
    const auto sphere = generate_sphere(16, 8);
    assert(sphere.vertices.size() == 17u * 9u);
    assert(sphere.indices.size() == 16u * 8u * 6u);
    for (std::uint32_t i : sphere.indices)
        assert(i < sphere.vertices.size());
    for (std::size_t i = 0; i < sphere.indices.size(); i += 3) {
        const auto& a = sphere.vertices[sphere.indices[i]];
        const auto& b = sphere.vertices[sphere.indices[i + 1]];
        const auto& c = sphere.vertices[sphere.indices[i + 2]];
        assert(dot(cross(b.position - a.position, c.position - a.position), a.position) > -1e-5f);
    }
    constexpr std::uint32_t rock_seeds[] = {1, 77, 0x12345678u, 0xffffffffu};
    for (std::uint32_t detail = 1; detail <= 5; ++detail) {
        for (std::uint32_t seed : rock_seeds) {
            const auto rock_a = generate_rock(seed, detail);
            const auto rock_b = generate_rock(seed, detail);
            assert(rock_a.vertices == rock_b.vertices); // exact deterministic generation
            assert(rock_a.indices == rock_b.indices);
            validate_flat_rock(rock_a);
        }
        const auto first_seed = generate_rock(rock_seeds[0], detail);
        const auto second_seed = generate_rock(rock_seeds[1], detail);
        assert(distinct_silhouette(first_seed, second_seed));
    }
    const auto belt_a = generate_belt(1234, 100, 10.0f, 20.0f, 4.0f);
    const auto belt_b = generate_belt(1234, 100, 10.0f, 20.0f, 4.0f);
    assert(belt_a.size() == 100 && belt_a[37].position.x == belt_b[37].position.x);
    for (const auto& i : belt_a) {
        const float r = std::sqrt(i.position.x * i.position.x + i.position.z * i.position.z);
        assert(r >= 10.0f && r <= 20.0f && std::abs(i.position.y) <= 2.0f);
    }
    assert(select_lod(220, 3) == 3); // hysteresis while shrinking
    assert(select_lod(1, 3) == 0);
    Frustum f{{{{1, 0, 0}, 0}, {{-1, 0, 0}, 10}, {{0, 1, 0}, 0}, {{0, -1, 0}, 10}, {{0, 0, 1}, 0}, {{0, 0, -1}, 10}}};
    assert(sphere_in_frustum(f, {5, 5, 5}, 1));
    assert(!sphere_in_frustum(f, {20, 5, 5}, 1));
    return 0;
}
