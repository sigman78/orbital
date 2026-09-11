#pragma once
#include <cmath>
#include <cstddef>

// Small vector and matrix helpers shared by the scene, camera and renderer.
// Vec3<double> carries simulation-space positions; Vec3<float> is used for
// everything camera-relative and GPU-facing. Mat4 is column-major like the
// shader-side float4x4.
namespace space {

template <class T> inline constexpr T pi = static_cast<T>(3.14159265358979323846L);

template <class T> struct Vec3 {
    T x{}, y{}, z{};

    constexpr Vec3 operator+(Vec3 other) const { return {x + other.x, y + other.y, z + other.z}; }
    constexpr Vec3 operator-(Vec3 other) const { return {x - other.x, y - other.y, z - other.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(T scale) const { return {x * scale, y * scale, z * scale}; }
    constexpr Vec3& operator+=(Vec3 other) { return *this = *this + other; }
    constexpr Vec3& operator-=(Vec3 other) { return *this = *this - other; }
    constexpr bool operator==(const Vec3&) const = default;
};

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;

template <class T> constexpr T dot(Vec3<T> a, Vec3<T> b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <class T> constexpr Vec3<T> cross(Vec3<T> a, Vec3<T> b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

template <class T> T length(Vec3<T> a) {
    return std::sqrt(dot(a, a));
}

// Unit vector, or +Y when the input is too short to have a direction.
template <class T> Vec3<T> normalized(Vec3<T> a) {
    constexpr T min_length = sizeof(T) == sizeof(float) ? static_cast<T>(1e-20) : static_cast<T>(1e-9);
    const T n = length(a);
    return n > min_length ? a * (static_cast<T>(1) / n) : Vec3<T>{0, 1, 0};
}

template <class T> constexpr Vec3<T> lerp(Vec3<T> a, Vec3<T> b, T t) {
    return a + (b - a) * t;
}

// Rodrigues rotation of v about a unit axis.
template <class T> Vec3<T> rotate_about(Vec3<T> v, Vec3<T> axis, T radians) {
    axis = normalized(axis);
    const T c = std::cos(radians), s = std::sin(radians);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (static_cast<T>(1) - c));
}

template <class T> bool is_finite(Vec3<T> v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

constexpr Vec3f to_float(Vec3d v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

constexpr Vec3d to_double(Vec3f v) {
    return {v.x, v.y, v.z};
}

// Column-major 4x4 matrix; element (row, column) lives at m[column * 4 + row].
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    constexpr float& at(std::size_t row, std::size_t column) { return m[column * 4 + row]; }
    constexpr float at(std::size_t row, std::size_t column) const { return m[column * 4 + row]; }
};

constexpr Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result;
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row) {
            float sum = 0;
            for (std::size_t k = 0; k < 4; ++k)
                sum += a.at(row, k) * b.at(k, column);
            result.at(row, column) = sum;
        }
    return result;
}

// View matrix from an orthonormal camera basis; the translation is zero
// because positions handed to the GPU are already camera-relative.
constexpr Mat4 view_matrix(Vec3f right, Vec3f up, Vec3f forward) {
    Mat4 view;
    view.at(0, 0) = right.x, view.at(0, 1) = right.y, view.at(0, 2) = right.z;
    view.at(1, 0) = up.x, view.at(1, 1) = up.y, view.at(1, 2) = up.z;
    view.at(2, 0) = -forward.x, view.at(2, 1) = -forward.y, view.at(2, 2) = -forward.z;
    return view;
}

// Vulkan clip space: Y down, depth in [0, 1].
constexpr Mat4 perspective_matrix(float tan_half_fov, float aspect, float near_z, float far_z) {
    Mat4 projection{};
    for (float& value : projection.m)
        value = 0;
    projection.at(0, 0) = 1 / (aspect * tan_half_fov);
    projection.at(1, 1) = -1 / tan_half_fov;
    projection.at(2, 2) = far_z / (near_z - far_z);
    projection.at(2, 3) = far_z * near_z / (near_z - far_z);
    projection.at(3, 2) = -1;
    return projection;
}

} // namespace space
