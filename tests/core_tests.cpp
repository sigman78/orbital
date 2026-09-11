#include "core/file.hpp"
#include "core/math.hpp"
#include "core/small_vec.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

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

void test_small_types() {
    constexpr Range<float> range{-1.5f, 2.0f};
    static_assert(range.span() == 3.5f);
    static_assert(range.contains(0.0f) && !range.contains(3.0f));
    static_assert(range.clamp(9.0f) == 2.0f && range.clamp(-9.0f) == -1.5f && range.clamp(1.0f) == 1.0f);
    constexpr Extent2D extent{1920, 1080};
    static_assert(!extent.empty() && Extent2D{}.empty());
    assert(near(extent.aspect(), 1920.0f / 1080.0f));
}

void test_small_vec() {
    SmallVec<std::string, 2> names;
    assert(names.empty());
    static_assert(SmallVec<int, 4>::inline_capacity() == 4);
    names.push_back("alpha");
    names.emplace_back("beta");
    assert(names.size() == 2 && names[1] == "beta");
    names.push_back("gamma"); // spills to the heap, elements survive the move
    names.push_back("delta");
    assert(names.size() == 4 && names[0] == "alpha" && names.back() == "delta");
    const std::span<const std::string> view = names;
    assert(view.size() == 4 && view[2] == "gamma");
    SmallVec<std::string, 2> moved = std::move(names);
    assert(moved.size() == 4 && names.empty() && moved[3] == "delta");
    moved.clear();
    assert(moved.empty());
    SmallVec<int, 3> ints;
    for (int i = 0; i < 3; ++i)
        ints.push_back(i * 10);
    SmallVec<int, 3> inline_moved = std::move(ints);
    assert(inline_moved.size() == 3 && inline_moved[2] == 20 && ints.empty());
    const std::uint32_t words[] = {1, 2};
    const ByteView raw = bytes_of(std::span<const std::uint32_t>(words));
    assert(raw.size() == 8 && raw[0] == 1 && raw[4] == 2);
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
    test_small_types();
    test_small_vec();
    test_matrices();
    test_files();
    std::printf("core tests passed\n");
    return 0;
}
