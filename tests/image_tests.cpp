#include "assets/image_io.hpp"
#include "core/file.hpp"
#include "core/types.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <vector>

namespace {
constexpr std::array<std::uint8_t, 10> view_pixels{1, 2, 3, 4, 99, 99, 5, 6, 7, 8};
constexpr space::assets::ImageView static_view({2, 2}, space::assets::PixelLayout::GrayAlpha8, view_pixels, 6);
static_assert(static_view.extent() == space::Extent2D{2, 2} && static_view.channels() == 2);
static_assert(static_view.row_stride() == 6 && static_view.row(1)[0] == 5 && static_view.row(1).size() == 4);
static_assert(static_view.bytes().size() == 10);
static_assert(!space::assets::ImageView::try_make({2, 3}, space::assets::PixelLayout::GrayAlpha8, view_pixels, 6));
static_assert(space::assets::valid_image_extent({16384, 16384}));
static_assert(!space::assets::valid_image_extent({16385, 1}));
static_assert(!space::assets::valid_image_extent({1, 16385}));
} // namespace

int main() {
    using namespace space::assets;
    assert(!Rgba8Image{}.valid());
    Rgba8Image malformed{{4, 4}, {}};
    assert(!malformed.valid());
    malformed.pixels.resize(4 * 4 * 3);
    assert(!malformed.valid());
    malformed.pixels.resize(4 * 4 * 4);
    assert(malformed.valid());
    malformed.pixels.push_back(0);
    assert(!malformed.valid());
    const std::uint32_t width = 7, height = 5;
    const space::Extent2D extent{width, height};
    space::Bytes rgba(width * height * 4);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        rgba[i * 4] = static_cast<std::uint8_t>(i * 3);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(255 - i);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(i * 7);
        rgba[i * 4 + 3] = i % 2 ? 255 : 64;
    }
    const auto path = std::filesystem::temp_directory_path() / "orbital_image_test.png";

    // RGBA round trip is lossless.
    assert(save_png(path, {extent, PixelLayout::Rgba8, rgba}));
    const auto loaded = load_png(path);
    assert(loaded.extent == extent);
    assert(loaded.pixels == rgba);

    // RGB input is expanded to opaque RGBA on load.
    space::Bytes rgb(width * height * 3);
    for (std::uint32_t i = 0; i < width * height; ++i)
        for (unsigned c = 0; c < 3; ++c)
            rgb[i * 3 + c] = rgba[i * 4 + c];
    assert(save_png(path, {extent, PixelLayout::Rgb8, rgb}));
    const auto expanded = load_png(path);
    assert(expanded.extent == extent);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        for (unsigned c = 0; c < 3; ++c)
            assert(expanded.pixels[i * 4 + c] == rgba[i * 4 + c]);
        assert(expanded.pixels[i * 4 + 3] == 255);
    }

    // Every layout accepts row padding, without requiring padding after the last row.
    for (const auto layout : {PixelLayout::Gray8, PixelLayout::GrayAlpha8, PixelLayout::Rgb8, PixelLayout::Rgba8}) {
        const unsigned channels = unsigned(layout);
        const std::size_t row_bytes = width * channels, stride = row_bytes + 3;
        space::Bytes padded((height - 1) * stride + row_bytes, 231);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
                for (unsigned c = 0; c < channels; ++c)
                    padded[y * stride + x * channels + c] = std::uint8_t(y * 17 + x * 3 + c * 41);
        const ImageView view(extent, layout, padded, stride);
        assert(save_png(path, view));
        const auto decoded = load_png(path);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto offset = (y * width + x) * 4;
                const auto source = y * stride + x * channels;
                for (unsigned c = 0; c < 3; ++c)
                    assert(decoded.pixels[offset + c] == padded[source + (channels < 3 ? 0 : c)]);
                assert(decoded.pixels[offset + 3] ==
                       (channels == 2 || channels == 4 ? padded[source + channels - 1] : 255));
            }
        assert(!ImageView::try_make(extent, layout, padded, row_bytes - 1));
        padded.pop_back();
        assert(!ImageView::try_make(extent, layout, padded, stride));
    }
    assert(!ImageView::try_make({}, PixelLayout::Rgba8, rgba));
    assert(!ImageView::try_make(extent, PixelLayout(0), rgba));
    assert(!ImageView::try_make(extent, PixelLayout(5), rgba));
    assert(!ImageView::try_make({1, 3}, PixelLayout::Gray8, rgba, std::numeric_limits<std::size_t>::max()));
    assert(!ImageView::try_make({UINT32_MAX, UINT32_MAX}, PixelLayout::Rgba8, rgba));

    // Check each dimension independently, even for thin images with modest byte counts.
    space::Bytes strip(max_image_dimension + 1);
    assert(ImageView::try_make({max_image_dimension, 1}, PixelLayout::Gray8, strip));
    assert(ImageView::try_make({1, max_image_dimension}, PixelLayout::Gray8, strip));
    assert(!ImageView::try_make({max_image_dimension + 1, 1}, PixelLayout::Gray8, strip));
    assert(!ImageView::try_make({1, max_image_dimension + 1}, PixelLayout::Gray8, strip));

    // Valid PNG headers beyond either supported axis must be rejected before allocation.
    assert(save_png(path, {extent, PixelLayout::Rgba8, rgba}));
    const auto png = space::file::read(path);
    assert(png && png->size() > 33);
    for (const unsigned offset : {16u, 20u}) {
        auto oversized = *png;
        const auto put_word = [&](unsigned at, std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i)
                oversized[at + i] = std::uint8_t(value >> (24 - 8 * i));
        };
        put_word(offset, max_image_dimension + 1);
        std::uint32_t crc = UINT32_MAX;
        for (unsigned i = 12; i < 29; ++i) {
            crc ^= oversized[i];
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
        }
        put_word(29, ~crc);
        assert(space::file::write(path, oversized));
        assert(!try_load_png(path));
    }

    // A missing file and a non-PNG file are reported, not fatal, on the try_ path.
    std::filesystem::remove(path);
    assert(!try_load_png(path));
    assert(space::file::write_text(path, "not a png"));
    assert(!try_load_png(path));
    std::filesystem::remove(path);
    std::printf("image tests passed\n");
    return 0;
}
