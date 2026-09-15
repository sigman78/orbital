#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstring>

namespace space::render {
namespace {

// Overlay vertex and push constants, mirrored in ui.slang.
struct UiVertex {
    float position_uv[4];
    std::uint32_t colour[4];
};
struct UiRoot {
    float scale[2], translate[2];
    std::uint64_t vertices;
    std::uint32_t texture, display; // display: the output encoding, mode in the low byte and paper white above
};
static_assert(sizeof(UiVertex) == 32 && sizeof(UiRoot) == 32);
static_assert(sizeof(ImDrawIdx) == 2, "The overlay pipeline requires 16-bit ImGui indices");

} // namespace

// Dear ImGui draw lists into the swapchain pass: vertices repacked into the
// overlay region of the dynamic heap, one indexed draw per command with its
// clip rectangle as the scissor.
void Renderer::Impl::record_ui(gpu::CommandBuffer* cmd, const ImDrawData* ui, std::uint8_t* cpu, std::uint64_t gpu) {
    if (!ui || ui->CmdListsCount == 0 || ui->TotalVtxCount == 0)
        return;
    const std::uint64_t vertex_bytes = std::uint64_t(ui->TotalVtxCount) * sizeof(UiVertex);
    const std::uint64_t index_bytes = std::uint64_t(ui->TotalIdxCount) * sizeof(ImDrawIdx);
    if (vertex_bytes + index_bytes > heap_layout.ui_bytes) {
        if (!ui_overflow_logged)
            log::warn("Overlay draw data ({} vertices) exceeds its {} MiB region; skipped", ui->TotalVtxCount,
                      heap_layout.ui_bytes >> 20);
        ui_overflow_logged = true;
        return;
    }
    auto* vertices = reinterpret_cast<UiVertex*>(cpu);
    auto* indices = reinterpret_cast<ImDrawIdx*>(cpu + vertex_bytes);
    const std::uint64_t index_address = gpu + vertex_bytes;
    gpu::bind_pso(cmd, ui_pso());
    UiRoot ui_root{.scale = {2.f / ui->DisplaySize.x, 2.f / ui->DisplaySize.y},
                   .translate = {},
                   .vertices = gpu,
                   .texture = std::uint32_t(Slot::ui_font),
                   .display = ui_display};
    ui_root.translate[0] = -1 - ui->DisplayPos.x * ui_root.scale[0];
    ui_root.translate[1] = -1 - ui->DisplayPos.y * ui_root.scale[1];
    unsigned vertex_base = 0, index_base = 0;
    for (int n = 0; n < ui->CmdListsCount; n++) {
        const ImDrawList* list = ui->CmdLists[n];
        for (int i = 0; i < list->VtxBuffer.Size; i++) {
            const ImDrawVert& v = list->VtxBuffer[i];
            vertices[vertex_base + i] = {{v.pos.x, v.pos.y, v.uv.x, v.uv.y}, {v.col, 0, 0, 0}};
        }
        std::memcpy(indices + index_base, list->IdxBuffer.Data, std::size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
        for (const ImDrawCmd& draw : list->CmdBuffer) {
            if (draw.UserCallback || draw.ElemCount == 0)
                continue;
            const float x0 = std::max(draw.ClipRect.x - ui->DisplayPos.x, 0.f);
            const float y0 = std::max(draw.ClipRect.y - ui->DisplayPos.y, 0.f);
            const float x1 = std::min(draw.ClipRect.z - ui->DisplayPos.x, float(extent.width));
            const float y1 = std::min(draw.ClipRect.w - ui->DisplayPos.y, float(extent.height));
            if (x1 <= x0 || y1 <= y0)
                continue;
            gpu::set_scissor(cmd, {.x = std::int32_t(x0),
                                   .y = std::int32_t(y0),
                                   .width = std::uint32_t(x1 - x0),
                                   .height = std::uint32_t(y1 - y0)});
            ui_root.vertices = gpu + std::uint64_t(vertex_base + draw.VtxOffset) * sizeof(UiVertex);
            gpu::draw_indexed(cmd, gpu::ByteSpan(&ui_root, sizeof ui_root),
                              {reinterpret_cast<void*>(index_address +
                                                       std::uint64_t(index_base + draw.IdxOffset) * sizeof(ImDrawIdx)),
                               std::uint64_t(draw.ElemCount) * sizeof(ImDrawIdx)},
                              gpu::IndexType::uint16, draw.ElemCount);
            stats.frame.draw_calls++;
        }
        vertex_base += unsigned(list->VtxBuffer.Size);
        index_base += unsigned(list->IdxBuffer.Size);
    }
}

void Renderer::set_ui_font(assets::ImageView pixels) {
    ORBITAL_ASSERT(!impl_->ui_font_uploaded);
    impl_->upload_rgba(Slot::ui_font, pixels);
    impl_->ui_font_uploaded = true;
}

} // namespace space::render
