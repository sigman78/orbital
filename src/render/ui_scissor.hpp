#pragma once

#include "core/math.hpp"

#include "imgui.h"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>

#include <algorithm>
#include <optional>

namespace space::render {

inline std::optional<gpu::Scissor> ui_scissor(ImVec4 clip, ImVec2 display_pos, ImVec2 framebuffer_scale,
                                              Extent2D extent) {
    // ImGui clips are logical coordinates; Vulkan scissors are framebuffer pixels.
    const float x0 = std::max((clip.x - display_pos.x) * framebuffer_scale.x, 0.f);
    const float y0 = std::max((clip.y - display_pos.y) * framebuffer_scale.y, 0.f);
    const float x1 = std::min((clip.z - display_pos.x) * framebuffer_scale.x, float(extent.width));
    const float y1 = std::min((clip.w - display_pos.y) * framebuffer_scale.y, float(extent.height));
    if (x1 <= x0 || y1 <= y0)
        return std::nullopt;
    return gpu::Scissor{.x = std::int32_t(x0),
                        .y = std::int32_t(y0),
                        .width = std::uint32_t(x1 - x0),
                        .height = std::uint32_t(y1 - y0)};
}

} // namespace space::render
