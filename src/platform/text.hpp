#pragma once
#include "core/math.hpp"
#include "core/types.hpp"

#include <span>
#include <string_view>

// Rasterizes a text overlay with the platform's font engine. The result is
// tightly packed RGBA8 with white RGB and the glyph coverage in alpha, ready
// to be composited by the renderer.
namespace space::platform {

struct TextItem {
    std::string_view text; // UTF-8
    int x = 0, y = 0;      // top-left, pixels
    int pixel_height = 14;
    bool bold = false;
};

// One-pixel horizontal separator from (x0, y) to (x1, y), drawn at partial coverage.
struct RuleItem {
    int x0 = 0, y = 0, x1 = 0;
};

struct OverlaySpec {
    Extent2D size;
    std::span<const TextItem> text;
    std::span<const RuleItem> rules;
};

Bytes rasterize_overlay(const OverlaySpec& spec);

} // namespace space::platform
