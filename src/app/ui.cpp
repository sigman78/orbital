#include "app/ui.hpp"

#include "platform/overlay.hpp"
#include "platform/window.hpp"

#include "imgui.h"

namespace space::app {

Ui::Ui(platform::Window& window) : window_(window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no layout file next to the executable
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendRendererName = "orbital NoGraphicsAPI";
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.Colors[ImGuiCol_WindowBg].w = .88f;
    style.Colors[ImGuiCol_PopupBg] = {.06f, .06f, .07f, .98f}; // tooltips over a bright scene stay readable
    platform::attach_overlay(window);
}

Ui::~Ui() {
    platform::detach_overlay(window_);
    ImGui::DestroyContext();
}

Ui::FontAtlas Ui::font_atlas() const {
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    return {.rgba = pixels, .width = unsigned(width), .height = unsigned(height)};
}

void Ui::begin_frame() {
    platform::overlay_new_frame();
    ImGui::NewFrame();
}

const ImDrawData* Ui::end_frame() {
    ImGui::Render();
    return ImGui::GetDrawData();
}

bool Ui::wants_keyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Ui::wants_mouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

} // namespace space::app
