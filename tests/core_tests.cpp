#include "core/file.hpp"
#include "core/math.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

using namespace space;

bool near(float a, float b) {
    return std::abs(a - b) < 1e-5f;
}

void test_vectors() {
    constexpr Vec3f a{1, 2, 3}, b{4, 5, 6};
    static_assert(a + b == Vec3f{5, 7, 9});
    static_assert(b - a == Vec3f{3, 3, 3});
    static_assert(a * 2 == Vec3f{2, 4, 6});
    static_assert(dot(a, b) == 32);
    static_assert(cross(Vec3f{1, 0, 0}, Vec3f{0, 1, 0}) == Vec3f{0, 0, 1});
    static_assert(to_float(Vec3d{1.5, 2.5, 3.5}) == Vec3f{1.5f, 2.5f, 3.5f});
    const Vec3f three_four{3, 4, 0}, z_two{0, 0, 2}, z_one{0, 0, 1}, y_one{0, 1, 0};
    assert(near(length(three_four), 5));
    assert(normalized(z_two) == z_one);
    assert(normalized(Vec3d{}) == to_double(y_one)); // degenerate input falls back to +Y
    const Vec3d x_axis{1, 0, 0}, y_axis{0, 1, 0};
    const auto rotated = rotate_about(x_axis, y_axis, pi<double> / 2);
    assert(std::abs(rotated.x) < 1e-12 && std::abs(rotated.z + 1) < 1e-12);
}

void test_matrices() {
    constexpr Mat4 identity;
    constexpr Mat4 view = view_matrix({1, 0, 0}, {0, 1, 0}, {0, 0, -1});
    static_assert((identity * view).m[10] == 1); // forward -Z maps to +Z rows
    Mat4 translate;
    translate.at(0, 3) = 5;
    Mat4 scale;
    scale.at(0, 0) = 2;
    const Mat4 product = translate * scale;
    assert(product.at(0, 0) == 2 && product.at(0, 3) == 5);
    const Mat4 projection = perspective_matrix(1.0f, 2.0f, 0.1f, 100.0f);
    assert(near(projection.at(0, 0), 0.5f) && near(projection.at(1, 1), -1.0f) && projection.at(3, 2) == -1);
}

void test_files() {
    const auto path = std::filesystem::temp_directory_path() / "orbital_core_test" / "nested" / "file.bin";
    std::filesystem::remove_all(path.parent_path().parent_path());
    const std::uint8_t payload[] = {1, 2, 3, 250, 0, 7};
    assert(file::write(path, payload)); // creates the nested directories
    const auto bytes = file::read(path);
    assert(bytes && bytes->size() == sizeof payload);
    assert(std::equal(bytes->begin(), bytes->end(), payload));
    assert(file::write_text(path, "hello"));
    const auto text = file::read(path);
    assert(text && std::string_view(reinterpret_cast<const char*>(text->data()), text->size()) == "hello");
    assert(!file::read(path.parent_path() / "missing.bin"));
    std::filesystem::remove_all(path.parent_path().parent_path());
}

} // namespace

int main() {
    test_vectors();
    test_matrices();
    test_files();
    std::printf("core tests passed\n");
    return 0;
}
