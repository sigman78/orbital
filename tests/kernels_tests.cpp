#include "assets/kernels.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace {

using namespace space::assets;

std::vector<std::uint8_t> random_pixels(std::uint32_t width, std::uint32_t height, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
    for (auto& value : pixels)
        value = static_cast<std::uint8_t>(rng());
    return pixels;
}

void check_downsample_matches_reference(std::uint32_t width, std::uint32_t height, bool normal_map) {
    const auto source = random_pixels(width, height, width * 131 + height * 7 + normal_map);
    const std::size_t out_size = std::size_t(kernels::half_extent(width)) * kernels::half_extent(height) * 4;
    std::vector<std::uint8_t> expected(out_size), actual(out_size, 0xCD);
    kernels::downsample_reference(source.data(), width, height, expected.data(), normal_map);
    (normal_map ? kernels::downsample_normals : kernels::downsample_rgba8)(source.data(), width, height, actual.data());
    if (!normal_map) {
        assert(actual == expected); // integer box filter is exact
        return;
    }
    // The vector normal path evaluates the same single-precision math in a
    // different instruction order, so allow one code of rounding slack.
    for (std::size_t i = 0; i < actual.size(); ++i)
        assert(std::abs(int(actual[i]) - int(expected[i])) <= 1);
}

void test_downsample() {
    constexpr std::uint32_t sizes[][2] = {{1, 1},   {2, 1},    {1, 2},   {2, 2},   {3, 3},    {5, 2},
                                          {7, 7},   {8, 8},    {9, 4},   {16, 8},  {17, 5},   {33, 17},
                                          {64, 64}, {100, 37}, {257, 3}, {4, 300}, {1024, 3}, {130, 66}};
    for (const auto& size : sizes) {
        check_downsample_matches_reference(size[0], size[1], false);
        check_downsample_matches_reference(size[0], size[1], true);
    }
    // Chains from a real texture shape all the way down to 1x1.
    for (std::uint32_t w = 512, h = 256; w > 1 || h > 1; w = kernels::half_extent(w), h = kernels::half_extent(h)) {
        check_downsample_matches_reference(w, h, false);
        check_downsample_matches_reference(w, h, true);
    }
}

void test_transforms() {
    auto pixels = random_pixels(97, 3, 42);
    auto srgb = pixels;
    kernels::srgb_to_linear(srgb.data(), 97 * 3);
    for (std::size_t q = 0; q < pixels.size(); q += 4) {
        for (unsigned c = 0; c < 3; ++c) {
            const double s = pixels[q + c] / 255.0;
            const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
            assert(srgb[q + c] == static_cast<std::uint8_t>(std::lround(linear * 255.0)));
        }
        assert(srgb[q + 3] == pixels[q + 3]);
    }
    auto mask = pixels;
    kernels::luminance_to_alpha(mask.data(), 97 * 3);
    for (std::size_t q = 0; q < pixels.size(); q += 4) {
        const unsigned luma = 54u * pixels[q] + 183u * pixels[q + 1] + 19u * pixels[q + 2];
        assert(mask[q] == 255 && mask[q + 1] == 255 && mask[q + 2] == 255);
        assert(mask[q + 3] == static_cast<std::uint8_t>((luma + 128) >> 8));
    }
}

template <class F> double time_ms(F&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void report_timings() {
    constexpr std::uint32_t width = 4096, height = 2048;
    auto source = random_pixels(width, height, 7);
    std::vector<std::uint8_t> out(std::size_t(width / 2) * (height / 2) * 4);
    const double reference = time_ms(
        [&] { kernels::downsample_reference(source.data(), width, height, out.data(), false); });
    const double box = time_ms([&] { kernels::downsample_rgba8(source.data(), width, height, out.data()); });
    const double reference_normals = time_ms(
        [&] { kernels::downsample_reference(source.data(), width, height, out.data(), true); });
    const double normals = time_ms([&] { kernels::downsample_normals(source.data(), width, height, out.data()); });
    const double srgb = time_ms([&] { kernels::srgb_to_linear(source.data(), std::size_t(width) * height); });
    const double luma = time_ms([&] { kernels::luminance_to_alpha(source.data(), std::size_t(width) * height); });
    std::printf("kernels backend=%s, %ux%u -> one level: box %.1f ms (reference %.1f) | normals %.1f ms (reference "
                "%.1f) | srgb %.1f ms | luma %.1f ms\n",
                kernels::backend(), width, height, box, reference, normals, reference_normals, srgb, luma);
}

} // namespace

int main() {
    test_downsample();
    test_transforms();
    report_timings();
    return 0;
}
