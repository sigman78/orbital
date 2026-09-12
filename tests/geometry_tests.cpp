#include "scene/geometry.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>

using namespace space;
using namespace space::geometry;

static float length_squared(Vec3f value) {
    return dot(value, value);
}

static void validate_rock(const Mesh& rock) {
    constexpr float radius_limit_squared = 1.0001f * 1.0001f;
    constexpr float normal_tolerance = 1e-5f;
    constexpr float area_epsilon_squared = 1e-12f;
    assert(!rock.vertices.empty());
    assert(!rock.indices.empty() && rock.indices.size() % 3 == 0);
    for (const auto& vertex : rock.vertices) {
        assert(is_finite(vertex.position) && is_finite(vertex.normal));
        assert(length_squared(vertex.position) <= radius_limit_squared);
        assert(std::abs(length_squared(vertex.normal) - 1.0f) <= normal_tolerance);
    }
    for (std::size_t face = 0; face < rock.indices.size(); face += 3) {
        const std::uint32_t ia = rock.indices[face], ib = rock.indices[face + 1], ic = rock.indices[face + 2];
        assert(ia < rock.vertices.size() && ib < rock.vertices.size() && ic < rock.vertices.size());
        const auto& a = rock.vertices[ia];
        const auto& b = rock.vertices[ib];
        const auto& c = rock.vertices[ic];
        const Vec3f face_cross = cross(b.position - a.position, c.position - a.position);
        assert(is_finite(face_cross) && length_squared(face_cross) > area_epsilon_squared);
        const Vec3f centroid = (a.position + b.position + c.position) * (1.0f / 3.0f);
        assert(dot(face_cross, centroid) > 0.0f);
        // Shape-function normals face outward from their own vertex.
        assert(dot(a.normal, a.position) > 0.0f);
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
    for (std::uint32_t level = 0; level < rock_level_count; ++level) {
        for (std::uint32_t seed : rock_seeds) {
            const auto rock_a = generate_rock(seed, level);
            const auto rock_b = generate_rock(seed, level);
            assert(rock_a.vertices == rock_b.vertices); // exact deterministic generation
            assert(rock_a.indices == rock_b.indices);
            assert(rock_a.indices.size() == 60u << (2 * level)); // 20 * 4^level triangles
            validate_rock(rock_a);
        }
        const auto first_seed = generate_rock(rock_seeds[0], level);
        const auto second_seed = generate_rock(rock_seeds[1], level);
        assert(distinct_silhouette(first_seed, second_seed));
    }
    // Levels share one shape: the coarse level's vertices are the first ones of the finer level.
    {
        const auto coarse = generate_rock(77, 2), fine = generate_rock(77, 4);
        assert(coarse.vertices.size() < fine.vertices.size());
        for (std::size_t i = 0; i < coarse.vertices.size(); ++i)
            assert(coarse.vertices[i] == fine.vertices[i]);
    }
    assert(select_rock_level(1.f) == 0 && select_rock_level(9.f) == 2 &&
           select_rock_level(1000.f) == rock_level_count - 1);
    constexpr BeltParams belt_params{
        .seed = 1234, .count = 100, .inner_radius = 10, .outer_radius = 20, .thickness = 4};
    const auto belt_a = generate_belt(belt_params);
    const auto belt_b = generate_belt(belt_params);
    assert(belt_a.size() == 100 && belt_a[37].position.x == belt_b[37].position.x);
    for (const auto& i : belt_a) {
        const float r = std::sqrt(i.position.x * i.position.x + i.position.z * i.position.z);
        // Inside the belt or its tapered edges, and within three flared scale heights.
        assert(r >= 9.5f && r <= 10.0f + 10.0f * belt_outer_tail &&
               std::abs(i.position.y) <=
                   2.0f * 1.8f * 3.0f); // three scale heights, flared to 1.8 half thicknesses at the tail
        assert(std::abs(i.spin.y) >= 0.4f && std::abs(i.spin.y) <= 1.6f);
        assert(i.scale.x > 0.0f && i.scale.x <= 12.0f * 1.25f);
    }
    for (int step = 0; step <= 20; ++step) {
        const float density = belt_ring_density(float(step) / 20.f);
        assert(density > 0.0f && density <= 1.0f);
    }
    assert(belt_ring_density(.58f) < belt_ring_density(.15f)); // the gap is sparser than the rings
    assert(belt_ring_density(-.1f) == 0.0f &&
           belt_ring_density(1.3f) < belt_ring_density(1.0f) * .2f); // sharp inner edge, fading tail
    {
        // Most rocks stay inside the nominal belt and most are small.
        const auto big_belt = generate_belt(
            {.seed = 7, .count = 20000, .inner_radius = 10, .outer_radius = 20, .thickness = 1});
        unsigned outside = 0, large = 0;
        for (const auto& i : big_belt) {
            const float r = std::sqrt(i.position.x * i.position.x + i.position.z * i.position.z);
            outside += r > 20.0f ? 1 : 0;
            large += i.scale.x > 2.0f ? 1 : 0;
        }
        assert(outside < big_belt.size() / 5 && large < big_belt.size() / 10);
    }
    assert(select_lod(220, 3) == 3); // hysteresis while shrinking
    assert(select_lod(1, 3) == 0);
    Frustum f{{{{1, 0, 0}, 0}, {{-1, 0, 0}, 10}, {{0, 1, 0}, 0}, {{0, -1, 0}, 10}, {{0, 0, 1}, 0}, {{0, 0, -1}, 10}}};
    assert(sphere_in_frustum(f, {5, 5, 5}, 1));
    assert(!sphere_in_frustum(f, {20, 5, 5}, 1));
    return 0;
}
