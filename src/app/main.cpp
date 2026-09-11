#include <windows.h>
#include "camera.hpp"
#include "render/renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
struct Application {
    space::Camera camera;
    std::vector<space::BodyState> bodies;
    bool running = true, paused = false, high = false, overlay = true, mouse = false, auto_exposure = true;
    float exposure = 1.0f;
    double mouse_dx = 0, mouse_dy = 0;
    POINT previous{};
    unsigned selected_body = 0;
    std::filesystem::path capture_request;
};
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app)
        return DefWindowProcW(window, message, w, l);
    switch (message) {
    case WM_CLOSE: app->running = false; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_KILLFOCUS:
        app->mouse = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        app->mouse = true;
        if (app->camera.mode() == space::CameraMode::Tour)
            app->camera.set_mode(space::CameraMode::Free);
        GetCursorPos(&app->previous);
        SetCapture(window);
        return 0;
    case WM_RBUTTONUP:
        app->mouse = false;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        if (app->mouse) {
            POINT p;
            GetCursorPos(&p);
            app->mouse_dx += p.x - app->previous.x;
            app->mouse_dy += p.y - app->previous.y;
            app->previous = p;
        }
        return 0;
    case WM_KEYDOWN:
        if (l & (1ll << 30))
            return 0;
        switch (w) {
        case VK_ESCAPE: app->running = false; break;
        case VK_SPACE: app->paused = !app->paused; break;
        case 'T': app->camera.toggle_tour(); break;
        case 'O': app->camera.set_orbit_target(app->selected_body, app->bodies[app->selected_body].radius * 2.3); break;
        case 'F': app->camera.set_mode(space::CameraMode::Free); break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
            app->camera.set_bookmark(w - '1', app->bodies);
            app->selected_body = unsigned(std::min<WPARAM>(w - '1', 2));
            app->camera.set_mode(space::CameraMode::Free);
            break;
        case 'X': app->auto_exposure = !app->auto_exposure; break;
        case VK_F1: app->overlay = !app->overlay; break;
        case VK_F2: app->high = !app->high; break;
        case VK_F12: app->capture_request = "captures/orbital.png"; break;
        case VK_OEM_PLUS: app->exposure = std::min(8.f, app->exposure * 1.1f); break;
        case VK_OEM_MINUS: app->exposure = std::max(.05f, app->exposure / 1.1f); break;
        }
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
std::filesystem::path executable_dir() {
    wchar_t p[32768];
    auto n = GetModuleFileNameW(nullptr, p, 32768);
    if (!n || n >= 32768)
        throw std::runtime_error("Cannot locate executable directory");
    return std::filesystem::path(p).parent_path();
}
struct Window {
    HWND handle{};
    ~Window() {
        if (handle)
            DestroyWindow(handle);
    }
};
} // namespace
int main(int argc, char** argv) {
#if defined(_MSC_VER) && !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        std::uint64_t seed = 20260911;
        unsigned frame_limit = 0, w = 1600, h = 900;
        double fixed_time = -1, duration = 0;
        int bookmark = -1;
        bool tour = false, high = false, no_hud = false;
        float exposure = 1;
        std::filesystem::path capture;
        std::filesystem::path benchmark;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            auto value = [&]() {
                if (++i >= argc)
                    throw std::runtime_error("Missing value for " + arg);
                return std::string(argv[i]);
            };
            if (arg == "--seed")
                seed = std::stoull(value());
            else if (arg == "--frames")
                frame_limit = std::stoul(value());
            else if (arg == "--width")
                w = std::stoul(value());
            else if (arg == "--height")
                h = std::stoul(value());
            else if (arg == "--time")
                fixed_time = std::stod(value());
            else if (arg == "--bookmark")
                bookmark = std::stoi(value());
            else if (arg == "--capture")
                capture = value();
            else if (arg == "--benchmark")
                benchmark = value();
            else if (arg == "--duration")
                duration = std::stod(value());
            else if (arg == "--no-hud")
                no_hud = true;
            else if (arg == "--exposure")
                exposure = std::stof(value());
            else if (arg == "--tour")
                tour = true;
            else if (arg == "--high")
                high = true;
            else if (arg == "--help") {
                std::cout << "ORBITAL - NoGraphicsAPI space demo\n--seed N --frames N --duration seconds --width W "
                             "--height H --time seconds --bookmark 0..4 --capture file.png --benchmark file.csv --tour "
                             "--high --no-hud --exposure scale\nControls: RMB mouse look; WASD move; "
                             "Q/E vertical; Shift fast; 1-5 bookmarks; O orbit; F free; T tour; Space pause; +/- "
                             "exposure; X auto exposure; F1 HUD; F2 quality; F12 capture; Esc exit.\n";
                return 0;
            } else
                throw std::runtime_error("Unknown option: " + arg);
        }
        if (w < 320 || h < 200 || w > 7680 || h > 4320 || !std::isfinite(fixed_time) || fixed_time < -1 ||
            !std::isfinite(duration) || duration < 0 || !std::isfinite(exposure) || exposure <= 0 || bookmark < -1 ||
            bookmark > 4)
            throw std::runtime_error("Invalid dimensions, time, duration, exposure, or bookmark (expected 0..4)");
        const auto dir = executable_dir();
        const auto system = space::generate_system(seed);
        auto errors = space::validate_system(system);
        if (!errors.empty())
            throw std::runtime_error("Invalid generated system: " + errors.front());
        SetProcessDPIAware();
        Application app;
        app.high = high;
        app.overlay = !no_hud;
        app.exposure = exposure;
        app.auto_exposure = fixed_time < 0;
        app.bodies = space::evaluate_system(system, std::max(0.0, fixed_time));
        if (bookmark >= 0)
            app.camera.set_bookmark(unsigned(bookmark), app.bodies);
        if (tour) {
            if (fixed_time >= 0)
                app.camera.set_tour_time(fixed_time);
            else
                app.camera.toggle_tour();
        }
        WNDCLASSW wc{};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"OrbitalWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register window class");
        RECT area{0, 0, LONG(w), LONG(h)};
        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
        Window window;
        window.handle = CreateWindowExW(0, wc.lpszClassName, L"ORBITAL  /  Procedural worlds", WS_OVERLAPPEDWINDOW,
                                        CW_USEDEFAULT, CW_USEDEFAULT, area.right - area.left, area.bottom - area.top,
                                        nullptr, nullptr, wc.hInstance, &app);
        if (!window.handle)
            throw std::runtime_error("Cannot create desktop window");
        ShowWindow(window.handle, SW_SHOW);
        space::render::Renderer renderer(window.handle, system, dir);
        std::cout << "Ready. RMB + WASD: fly | 1/2/3: planets | T: tour | F2: quality | F12: capture | --help for all "
                     "controls\n";
        auto previous = std::chrono::steady_clock::now();
        double sim = 0, elapsed = 0, title_clock = 0;
        unsigned frames = 0;
        std::vector<double> times, gpu_times;
        times.reserve(frame_limit ? frame_limit : 36000);
        while (app.running) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    app.running = false;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!app.running)
                break;
            if (IsIconic(window.handle)) {
                WaitMessage();
                previous = std::chrono::steady_clock::now();
                continue;
            }
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - previous).count();
            previous = now;
            dt = std::min(dt, .1);
            elapsed += dt;
            if (!app.paused)
                sim += dt;
            if (fixed_time >= 0)
                sim = fixed_time;
            app.bodies = space::evaluate_system(system, sim);
            auto down = [&](int key) {
                return GetForegroundWindow() == window.handle && (GetAsyncKeyState(key) & 0x8000) != 0;
            };
            space::Input input;
            input.move_forward = double(down('W')) - down('S');
            input.move_right = double(down('D')) - down('A');
            input.move_up = double(down('E')) - down('Q');
            input.speed_scale = down(VK_SHIFT) ? 4 : 1;
            input.mouse_dx = app.mouse_dx;
            input.mouse_dy = app.mouse_dy;
            app.mouse_dx = app.mouse_dy = 0;
            if (input.move_forward || input.move_right || input.move_up)
                if (app.camera.mode() == space::CameraMode::Tour)
                    app.camera.set_mode(space::CameraMode::Free);
            app.camera.step(dt, elapsed, input, app.bodies);
            if (renderer.draw(app.camera, app.bodies, sim, app.exposure, app.high, app.overlay, app.auto_exposure)) {
                frames++;
                times.push_back(renderer.stats().frame_ms);
                gpu_times.push_back(renderer.stats().gpu_ms);
                if (!app.capture_request.empty()) {
                    renderer.capture(dir / app.capture_request);
                    std::cout << "Saved " << (dir / app.capture_request) << '\n';
                    app.capture_request.clear();
                }
                if ((frame_limit && frames >= frame_limit) || (duration > 0 && elapsed >= duration)) {
                    if (!capture.empty())
                        renderer.capture(capture);
                    break;
                }
            }
            if (elapsed - title_clock > .5) {
                title_clock = elapsed;
                auto st = renderer.stats();
                std::wstring title = L"ORBITAL  |  " + std::to_wstring(int(1000 / std::max(st.frame_ms, .1))) +
                                     L" FPS  |  " + (app.high ? L"HIGH" : L"BASELINE") + L"  |  " +
                                     std::to_wstring(st.visible_asteroids) +
                                     L" asteroids  |  RMB + WASD / T tour / 1-3 planets / F1 help";
                SetWindowTextW(window.handle, title.c_str());
            }
        }
        if (!benchmark.empty()) {
            if (!benchmark.parent_path().empty())
                std::filesystem::create_directories(benchmark.parent_path());
            std::ofstream f(benchmark);
            f << "frame,cpu_submit_and_wait_ms,gpu_ms\n";
            for (unsigned i = 0; i < times.size(); i++)
                f << i << ',' << times[i] << ',' << gpu_times[i] << '\n';
            auto sorted = times;
            if (sorted.size() > 60)
                sorted.erase(sorted.begin(), sorted.begin() + 60);
            std::sort(sorted.begin(), sorted.end());
            if (!sorted.empty())
                std::cout << "Frame timing median " << sorted[sorted.size() / 2] << " ms, p95 "
                          << sorted[std::min(sorted.size() - 1, std::size_t(sorted.size() * .95))]
                          << " ms (CPU including GPU wait; not isolated GPU timing).\n";
        }
        std::cout << "Completed " << frames << " frames.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ORBITAL: " << e.what() << '\n';
        return 1;
    }
}
