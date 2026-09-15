#include "platform/window.hpp"
#include "platform/win32/window_access.hpp"

#include "core/panic_if.hpp"
#include "core/small_vec.hpp"

#include <windows.h>

#include <dxgi1_6.h>

#include <string>
#include <vector>

namespace space::platform {
namespace {

constexpr const wchar_t* window_class_name = L"OrbitalWindow";
constexpr LPARAM key_repeat_bit = 1ll << 30;
constexpr std::size_t inline_press_count = 16;
constexpr COLORREF background_color = RGB(32, 32, 32), hatch_color = RGB(96, 96, 96);

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
    case VK_F3: return Key::f3;
    case VK_F4: return Key::f4;
    case VK_F5: return Key::f5;
    case VK_F6: return Key::f6;
    case VK_F7: return Key::f7;
    case VK_F8: return Key::f8;
    case VK_F9: return Key::f9;
    case VK_F10: return Key::f10;
    case VK_F11: return Key::f11;
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
    case Key::f3: return VK_F3;
    case Key::f4: return VK_F4;
    case Key::f5: return VK_F5;
    case Key::f6: return VK_F6;
    case Key::f7: return VK_F7;
    case Key::f8: return VK_F8;
    case Key::f9: return VK_F9;
    case Key::f10: return VK_F10;
    case Key::f11: return VK_F11;
    case Key::f12: return VK_F12;
    default: return int(key); // letters and digits
    }
}

} // namespace

struct Window::Impl {
    HWND handle = nullptr;
    HBRUSH background_brush = nullptr;
    bool closed = false;
    bool mouse_look = false, mouse_look_began = false;
    bool middle_button = false;
    POINT previous_cursor{};
    MouseDelta delta;
    SmallVec<Key, inline_press_count> presses;
    detail::MessageHook hook = nullptr; // the overlay backend sees every message first
    void* hook_user = nullptr;
    bool fullscreen = false; // borderless fullscreen (Alt+Enter); the placement below restores the window
    LONG saved_style = 0;
    RECT saved_rect{};
    bool saved_maximized = false;

    ~Impl() {
        if (handle)
            DestroyWindow(handle);
        if (background_brush)
            DeleteObject(background_brush);
    }

    LRESULT handle_message(UINT message, WPARAM w, LPARAM l) {
        if (hook) {
            std::intptr_t result = 0;
            if (hook(hook_user, handle, message, std::uintptr_t(w), std::intptr_t(l), &result))
                return LRESULT(result);
        }
        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(handle, &paint);
            const COLORREF previous_color = SetBkColor(dc, background_color);
            const int previous_mode = SetBkMode(dc, OPAQUE);
            FillRect(dc, &paint.rcPaint, background_brush);
            SetBkMode(dc, previous_mode);
            SetBkColor(dc, previous_color);
            EndPaint(handle, &paint);
            return 0;
        }
        case WM_CLOSE: closed = true; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_KILLFOCUS: middle_button = false; [[fallthrough]];
        case WM_RBUTTONUP:
            mouse_look = false;
            ReleaseCapture();
            return 0;
        case WM_MBUTTONDOWN: middle_button = true; return 0;
        case WM_MBUTTONUP: middle_button = false; return 0;
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
        case WM_SYSKEYDOWN:
            // Alt+Enter and F10 arrive as system keys; everything else (Alt+F4, menu keys) stays default.
            if (w == VK_RETURN && (l & (1 << 29))) {
                if (!(l & key_repeat_bit))
                    presses.try_push_back(Key::alt_enter);
                return 0;
            }
            if (w == VK_F10) {
                if (!(l & key_repeat_bit))
                    presses.try_push_back(Key::f10);
                return 0;
            }
            break;
        case WM_SYSCHAR:
            if (w == VK_RETURN)
                return 0; // no beep for Alt+Enter
            break;
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
    impl->background_brush = CreateHatchBrush(HS_BDIAGONAL, hatch_color);
    panic_if(!impl->background_brush, "cannot create the window background brush");
    const std::wstring title = to_wide(desc.title);
    HWND handle = CreateWindowExW(0, window_class_name, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                  CW_USEDEFAULT, area.right - area.left, area.bottom - area.top, nullptr, nullptr,
                                  wc.hInstance, impl.get());
    panic_if(!handle, "cannot create the desktop window");
    impl->handle = handle;
    if (!desc.hidden) {
        ShowWindow(handle, SW_SHOW);
        UpdateWindow(handle); // Paint now, before synchronous renderer and asset initialization.
    }
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

void Window::resize(Extent2D client_size) {
    RECT area{0, 0, LONG(client_size.width), LONG(client_size.height)};
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(impl_->handle, nullptr, 0, 0, area.right - area.left, area.bottom - area.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::maximize() {
    ShowWindow(impl_->handle, SW_MAXIMIZE);
}

void Window::toggle_fullscreen() {
    auto& i = *impl_;
    const HWND handle = i.handle;
    if (!i.fullscreen) {
        i.saved_style = GetWindowLongW(handle, GWL_STYLE);
        i.saved_maximized = IsZoomed(handle) != 0;
        GetWindowRect(handle, &i.saved_rect);
        MONITORINFO monitor{.cbSize = sizeof monitor};
        GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor);
        const RECT& m = monitor.rcMonitor;
        SetWindowLongW(handle, GWL_STYLE, (i.saved_style & ~LONG(WS_OVERLAPPEDWINDOW)) | WS_POPUP | WS_VISIBLE);
        SetWindowPos(handle, HWND_TOP, m.left, m.top, m.right - m.left, m.bottom - m.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        i.fullscreen = true;
    } else {
        SetWindowLongW(handle, GWL_STYLE, i.saved_style);
        const RECT& r = i.saved_rect;
        SetWindowPos(handle, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        if (i.saved_maximized)
            ShowWindow(handle, SW_MAXIMIZE);
        i.fullscreen = false;
    }
}

// DXGI describes the output whose monitor holds the window: its colour space
// (HDR10 when the desktop presents in HDR) and its luminance range. The SDR
// white level comes from the display configuration, in 1/1000 of 80 nits.
DisplayInfo Window::display_info() const {
    DisplayInfo info;
    const HMONITOR monitor = MonitorFromWindow(impl_->handle, MONITOR_DEFAULTTONEAREST);
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || !factory)
        return info;
    bool found = false;
    for (UINT a = 0; !found; ++a) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(a, &adapter) != S_OK)
            break;
        for (UINT o = 0; !found; ++o) {
            IDXGIOutput* output = nullptr;
            if (adapter->EnumOutputs(o, &output) != S_OK)
                break;
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == monitor) {
                IDXGIOutput6* output6 = nullptr;
                if (SUCCEEDED(output->QueryInterface(IID_PPV_ARGS(&output6))) && output6) {
                    DXGI_OUTPUT_DESC1 desc1{};
                    if (SUCCEEDED(output6->GetDesc1(&desc1))) {
                        info.hdr = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                        info.min_nits = desc1.MinLuminance;
                        info.max_nits = desc1.MaxLuminance;
                        info.max_full_frame_nits = desc1.MaxFullFrameLuminance;
                        found = true;
                    }
                    output6->Release();
                }
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    if (!found)
        return info;
    // The SDR white level: the display configuration path whose source is this monitor's device.
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info))
        return info;
    UINT32 path_count = 0, mode_count = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS)
        return info;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr) !=
        ERROR_SUCCESS)
        return info;
    for (UINT32 i = 0; i < path_count; ++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header = {.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,
                         .size = sizeof(source),
                         .adapterId = paths[i].sourceInfo.adapterId,
                         .id = paths[i].sourceInfo.id};
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
            wcscmp(source.viewGdiDeviceName, monitor_info.szDevice) != 0)
            continue;
        DISPLAYCONFIG_SDR_WHITE_LEVEL white{};
        white.header = {.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL,
                        .size = sizeof(white),
                        .adapterId = paths[i].targetInfo.adapterId,
                        .id = paths[i].targetInfo.id};
        if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS)
            info.sdr_white_nits = float(white.SDRWhiteLevel) / 1000.f * 80.f;
        break;
    }
    return info;
}

bool Window::fullscreen() const {
    return impl_->fullscreen;
}

void detail::WindowAccess::set_message_hook(Window& window, detail::MessageHook hook, void* user) {
    window.impl_->hook = hook;
    window.impl_->hook_user = user;
}

} // namespace space::platform
