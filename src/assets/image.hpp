#pragma once
#include "core/extent.hpp"
#include "core/panic.hpp"
#include "core/types.hpp"

#include <optional>
#include <vector>

namespace space::assets {

// Project asset limit, shared by decoded pixels, views and compressed textures.
// This is a supported-input policy, not a claim about every GPU's hardware limit.
inline constexpr unsigned max_image_dimension = 16384;
constexpr bool valid_image_extent(Extent2D extent) {
    return !extent.empty() && extent.width <= max_image_dimension && extent.height <= max_image_dimension;
}

// Pixel layout only; material color encoding is a separate processing policy.
enum class PixelLayout { Gray8 = 1, GrayAlpha8, Rgb8, Rgba8 };

// Borrowed read-only pixels. Padding between rows and after the final row is allowed.
// Construction checks the last addressed byte; the caller keeps the storage alive.
class ImageView {
public:
    static constexpr std::optional<ImageView> try_make(Extent2D extent, PixelLayout layout, ByteView bytes,
                                                       std::size_t row_stride = 0) {
        const auto channels = unsigned(layout);
        if (!valid_image_extent(extent) || channels < 1 || channels > 4)
            return std::nullopt;
        const std::size_t row_bytes = std::size_t(extent.width) * channels;
        if (!row_stride)
            row_stride = row_bytes;
        if (row_stride < row_bytes || bytes.size() < row_bytes ||
            extent.height - 1 > (bytes.size() - row_bytes) / row_stride)
            return std::nullopt;
        return ImageView(extent, layout, bytes, row_stride, Checked{});
    }
    constexpr ImageView(Extent2D extent, PixelLayout layout, ByteView bytes, std::size_t row_stride = 0)
        : ImageView(require(extent, layout, bytes, row_stride)) {}
    constexpr Extent2D extent() const { return extent_; }
    constexpr PixelLayout layout() const { return layout_; }
    constexpr unsigned channels() const { return unsigned(layout_); }
    constexpr std::size_t row_stride() const { return row_stride_; }
    constexpr ByteView bytes() const { return bytes_; }
    constexpr ByteView row(unsigned y) const {
        ORBITAL_ASSERT(y < extent_.height);
        return bytes_.subspan(std::size_t(y) * row_stride_, std::size_t(extent_.width) * channels());
    }

private:
    struct Checked {};
    constexpr ImageView(Extent2D extent, PixelLayout layout, ByteView bytes, std::size_t stride, Checked)
        : extent_(extent), layout_(layout), row_stride_(stride), bytes_(bytes) {}
    static constexpr ImageView require(Extent2D extent, PixelLayout layout, ByteView bytes, std::size_t stride) {
        auto view = try_make(extent, layout, bytes, stride);
        ORBITAL_ASSERT(view.has_value());
        return *view;
    }
    Extent2D extent_;
    PixelLayout layout_;
    std::size_t row_stride_;
    ByteView bytes_;
};

// Owning tightly packed RGBA8 pixels; views borrow this storage.
struct Rgba8Image {
    Extent2D extent{};
    Bytes pixels;

    constexpr std::size_t pixel_count() const { return static_cast<std::size_t>(extent.width) * extent.height; }
    constexpr ByteView bytes() const { return pixels; }
    constexpr ImageView view() const& {
        ORBITAL_ASSERT(valid());
        return {extent, PixelLayout::Rgba8, pixels};
    }
    constexpr ImageView view() const&& = delete;
    constexpr bool valid() const {
        return valid_image_extent(extent) && pixels.size() % 4 == 0 && pixel_count() == pixels.size() / 4;
    }
};

// Base level first, each level half the previous, down to 1x1.
using MipChain = std::vector<Rgba8Image>;

} // namespace space::assets
