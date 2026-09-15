#include "platform/window.hpp"

#include "core/panic_if.hpp"
#include "core/small_vec.hpp"

#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include <SDL.h>

#include <string>

namespace space::platform {
namespace {

Key key_from_sdl(SDL_Keycode code) {
    if (code >= SDLK_a && code <= SDLK_z)
        return letter_key(char('A' + code - SDLK_a));
    if (code >= SDLK_0 && code <= SDLK_9)
        return Key(code);
    if (code >= SDLK_F1 && code <= SDLK_F12)
        return Key(unsigned(Key::f1) + unsigned(code - SDLK_F1));
    switch (code) {
    case SDLK_ESCAPE: return Key::escape;
    case SDLK_SPACE: return Key::space;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return Key::shift;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS: return Key::plus;
    case SDLK_MINUS:
    case SDLK_KP_MINUS: return Key::minus;
    default: return Key::none;
    }
}

} // namespace

struct Window::Impl {
    SDL_Window* handle = nullptr;
    bool closed = false;
    bool mouse_look = false, mouse_look_began = false;
    bool middle_button = false;
    MouseDelta delta;
    SmallVec<Key, 16> presses;

    ~Impl() {
        if (handle)
            SDL_DestroyWindow(handle);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void stop_mouse_look() {
        mouse_look = false;
        delta = {};
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
};

std::unique_ptr<Window> Window::create(const WindowDesc& desc) {
    panic_if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0, "cannot initialize SDL video: {}", SDL_GetError());
    auto impl = std::make_unique<Impl>();
    impl->handle = SDL_CreateWindow(std::string(desc.title).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    int(desc.client_size.width), int(desc.client_size.height),
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    panic_if(!impl->handle, "cannot create the desktop window: {}", SDL_GetError());
    return std::unique_ptr<Window>(new Window(std::move(impl)));
}

Window::Window(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Window::~Window() = default;

void* Window::native_handle() const {
    return impl_->handle;
}

bool Window::pump_events() {
    auto& state = *impl_;
    state.presses.clear();
    state.mouse_look_began = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool overlay = ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData;
        if (overlay)
            ImGui_ImplSDL2_ProcessEvent(&event);
        const bool capture_mouse = overlay && ImGui::GetIO().WantCaptureMouse;
        const bool capture_keyboard = overlay && ImGui::GetIO().WantCaptureKeyboard;
        switch (event.type) {
        case SDL_QUIT: state.closed = true; break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                state.closed = true;
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                state.stop_mouse_look();
                state.middle_button = false;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_RIGHT && !capture_mouse) {
                state.mouse_look = state.mouse_look_began = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (event.button.button == SDL_BUTTON_MIDDLE)
                state.middle_button = true;
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_RIGHT)
                state.stop_mouse_look();
            if (event.button.button == SDL_BUTTON_MIDDLE)
                state.middle_button = false;
            break;
        case SDL_MOUSEMOTION:
            if (state.mouse_look) {
                state.delta.dx += float(event.motion.xrel);
                state.delta.dy += float(event.motion.yrel);
            }
            break;
        case SDL_KEYDOWN:
            if (!event.key.repeat && !capture_keyboard) {
                const Key key = event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)
                                    ? Key::alt_enter
                                    : key_from_sdl(event.key.keysym.sym);
                if (key != Key::none)
                    state.presses.try_push_back(key);
            }
            break;
        default: break;
        }
    }
    return !state.closed;
}

std::span<const Key> Window::key_presses() const {
    return impl_->presses;
}
bool Window::middle_button_down() const {
    return impl_->middle_button;
}

bool Window::mouse_look_active() const {
    return impl_->mouse_look;
}
bool Window::mouse_look_began() const {
    return impl_->mouse_look_began;
}

MouseDelta Window::take_mouse_look_delta() {
    const MouseDelta delta = impl_->delta;
    impl_->delta = {};
    return delta;
}

bool Window::key_down(Key key) const {
    if (SDL_GetKeyboardFocus() != impl_->handle)
        return false;
    int count = 0;
    const auto* keys = SDL_GetKeyboardState(&count);
    for (int index = 0; index < count; ++index)
        if (keys[index] && key_from_sdl(SDL_GetKeyFromScancode(SDL_Scancode(index))) == key)
            return true;
    return false;
}

bool Window::minimized() const {
    return (SDL_GetWindowFlags(impl_->handle) & SDL_WINDOW_MINIMIZED) != 0;
}
void Window::wait_for_events() const {
    SDL_WaitEvent(nullptr);
}
void Window::set_title(std::string_view utf8) {
    SDL_SetWindowTitle(impl_->handle, std::string(utf8).c_str());
}
void Window::maximize() {
    SDL_MaximizeWindow(impl_->handle);
}

void Window::toggle_fullscreen() {
    panic_if(SDL_SetWindowFullscreen(impl_->handle, fullscreen() ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0,
             "cannot toggle fullscreen: {}", SDL_GetError());
}

DisplayInfo Window::display_info() const {
    return {}; // SDL2 reports no HDR state; the desktop's HDR output is not queried on Linux
}

bool Window::fullscreen() const {
    return (SDL_GetWindowFlags(impl_->handle) & SDL_WINDOW_FULLSCREEN) != 0;
}

} // namespace space::platform
