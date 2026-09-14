#pragma once

namespace space {

// Width and height of an image, window or render target.
struct Extent2D {
    unsigned width = 0, height = 0;

    constexpr float aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    constexpr bool empty() const { return width == 0 || height == 0; }
    constexpr bool operator==(const Extent2D&) const = default;
};

} // namespace space
