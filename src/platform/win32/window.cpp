#include "platform/window.hpp"

#include "core/panic.hpp"
#include "core/small_vec.hpp"

#include <windows.h>

#include <string>

namespace space::platform {
namespace {

constexpr const wchar_t* window_class_name = L"OrbitalWindow";
constexpr LPARAM key_repeat_bit = 1ll << 30;
constexpr std::size_t inline_press_count = 16;

std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), wide.data(), length);
    return wide;
}

Key key_from_virtual(WPARAM code) {
    switch (code) {
    case VK_ESCAPE: return Key::escape;
    case VK_SPACE: return Key::space;
    case VK_SHIFT: return Key::shift;
    case VK_OEM_PLUS: return Key::plus;
    case VK_OEM_MINUS: return Key::minus;
    case VK_F1: return Key::f1;
    case VK_F2: return Key::f2;
    case VK_F12: return Key::f12;
    default:
        if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9'))
            return Key(code); // virtual-key codes for letters and digits equal ASCII
        return Key::none;
    }
}

int virtual_from_key(Key key) {
    switch (key) {
    case Key::escape: return VK_ESCAPE;
    case Key::space: return VK_SPACE;
    case Key::shift: return VK_SHIFT;
    case Key::plus: return VK_OEM_PLUS;
    case Key::minus: return VK_OEM_MINUS;
    case Key::f1: return VK_F1;
    case Key::f2: return VK_F2;
    case Key::f12: return VK_F12;
    default: return int(key); // letters and digits
    }
}

} // namespace

struct Window::Impl {
    HWND handle = nullptr;
    bool closed = false;
    bool mouse_look = false, mouse_look_began = false;
    POINT previous_cursor{};
    MouseDelta delta;
    SmallVec<Key, inline_press_count> presses;

    ~Impl() {
        if (handle)
            DestroyWindow(handle);
    }

    LRESULT handle_message(UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CLOSE: closed = true; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_KILLFOCUS:
        case WM_RBUTTONUP:
            mouse_look = false;
            ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            mouse_look = mouse_look_began = true;
            GetCursorPos(&previous_cursor);
            SetCapture(handle);
            return 0;
        case WM_MOUSEMOVE:
            if (mouse_look) {
                POINT cursor;
                GetCursorPos(&cursor);
                delta.dx += float(cursor.x - previous_cursor.x);
                delta.dy += float(cursor.y - previous_cursor.y);
                previous_cursor = cursor;
            }
            return 0;
        case WM_KEYDOWN:
            if (!(l & key_repeat_bit))
                if (const Key key = key_from_virtual(w); key != Key::none)
                    presses.try_push_back(key); // beyond capacity, extra presses are dropped
            return 0;
        }
        return DefWindowProcW(handle, message, w, l);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
        auto* impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            impl = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            impl->handle = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
        }
        return impl ? impl->handle_message(message, w, l) : DefWindowProcW(window, message, w, l);
    }
};

std::unique_ptr<Window> Window::create(const WindowDesc& desc) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = Impl::window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = window_class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    panic_if(!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS, "cannot register the window class");
    RECT area{0, 0, LONG(desc.client_size.width), LONG(desc.client_size.height)};
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    auto impl = std::make_unique<Impl>();
    const std::wstring title = to_wide(desc.title);
    HWND handle = CreateWindowExW(0, window_class_name, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                  CW_USEDEFAULT, area.right - area.left, area.bottom - area.top, nullptr, nullptr,
                                  wc.hInstance, impl.get());
    panic_if(!handle, "cannot create the desktop window");
    impl->handle = handle;
    ShowWindow(handle, SW_SHOW);
    return std::unique_ptr<Window>(new Window(std::move(impl)));
}

Window::Window(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Window::~Window() = default;

void* Window::native_handle() const {
    return impl_->handle;
}

bool Window::pump_events() {
    impl_->presses.clear();
    impl_->mouse_look_began = false;
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            impl_->closed = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !impl_->closed;
}

std::span<const Key> Window::key_presses() const {
    return impl_->presses;
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
    return GetForegroundWindow() == impl_->handle && (GetAsyncKeyState(virtual_from_key(key)) & 0x8000) != 0;
}

bool Window::minimized() const {
    return IsIconic(impl_->handle) != 0;
}

void Window::wait_for_events() const {
    WaitMessage();
}

void Window::set_title(std::string_view utf8) {
    SetWindowTextW(impl_->handle, to_wide(utf8).c_str());
}

} // namespace space::platform
