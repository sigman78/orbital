#include "platform/overlay.hpp"

#include "core/panic_if.hpp"
#include "platform/window.hpp"

#include "backends/imgui_impl_sdl2.h"

namespace space::platform {

void attach_overlay(Window& window) {
    panic_if(!ImGui_ImplSDL2_InitForVulkan(static_cast<SDL_Window*>(window.native_handle())),
             "cannot initialize the SDL overlay");
}

void detach_overlay(Window&) {
    ImGui_ImplSDL2_Shutdown();
}

void overlay_new_frame() {
    ImGui_ImplSDL2_NewFrame();
}

} // namespace space::platform
