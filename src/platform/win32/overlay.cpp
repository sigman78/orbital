#include "platform/overlay.hpp"

#include "platform/win32/window_access.hpp"

#include <windows.h>

#include "backends/imgui_impl_win32.h"
#include "imgui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace space::platform {

namespace {

// Feeds the message to the Dear ImGui backend first; consumes clicks, wheel
// and typed keys while the overlay wants them, so they do not also start a
// mouse look or fly the camera. Mouse moves and releases always pass through.
bool message_hook(void*, void* handle, unsigned message, std::uintptr_t w, std::intptr_t l, std::intptr_t* result) {
    if (!ImGui::GetCurrentContext())
        return false;
    if (const LRESULT handled = ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(handle), message, WPARAM(w),
                                                               LPARAM(l))) {
        *result = handled;
        return true;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool press = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN ||
                       message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK ||
                       message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
    const bool key = message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_CHAR;
    if ((press && io.WantCaptureMouse) || (key && io.WantCaptureKeyboard)) {
        *result = 0;
        return true;
    }
    return false;
}

} // namespace

void attach_overlay(Window& window) {
    ImGui_ImplWin32_Init(window.native_handle());
    detail::WindowAccess::set_message_hook(window, message_hook, nullptr);
}

void detach_overlay(Window& window) {
    detail::WindowAccess::set_message_hook(window, nullptr, nullptr);
    ImGui_ImplWin32_Shutdown();
}

void overlay_new_frame() {
    ImGui_ImplWin32_NewFrame();
}

} // namespace space::platform
