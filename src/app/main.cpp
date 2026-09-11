#include <windows.h>
#include "app/camera.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/math.hpp"
#include "core/panic.hpp"
#include "render/renderer.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

using namespace space;

struct AppSettings {
    struct {
        Extent2D default_size{1600, 900};
        Range<unsigned> width{320, 7680}, height{200, 4320};
        double title_refresh_seconds = 0.5;
    } window;
    struct {
        Range<float> range{0.05f, 8.0f};
        float step = 1.1f; // per +/- key press
    } exposure;
    struct {
        double max_frame_seconds = 0.1; // a stall must not advance the simulation by more than this
        float fast_speed_scale = 4.0f;  // Shift
        double orbit_zoom_radii = 2.3;  // O key orbits at this many body radii
    } control;
    struct {
        std::size_t warmup_frames = 60;      // dropped before computing percentiles
        std::size_t expected_frames = 36000; // reserve for an open-ended run (10 minutes at 60 Hz)
    } benchmark;
};
constexpr AppSettings settings{};

constexpr std::string_view hotkey_capture_path = "captures/orbital.png";
constexpr const wchar_t* window_class_name = L"OrbitalWindow";
constexpr const wchar_t* window_title = L"ORBITAL  /  Procedural worlds";

constexpr std::string_view usage =
    "ORBITAL - NoGraphicsAPI space demo\n"
    "--seed N --frames N --duration seconds --width W --height H --time seconds --bookmark 0..4\n"
    "--capture file.png --benchmark file.csv --tour --high --no-hud --exposure scale\n"
    "Controls: RMB mouse look; WASD move; Q/E vertical; Shift fast; 1-5 bookmarks; O orbit; F free;\n"
    "T tour; Space pause; +/- exposure; X auto exposure; F1 HUD; F2 quality; F12 capture; Esc exit.";

struct Options {
    std::uint64_t seed = showcase_seed;
    unsigned frame_limit = 0;
    Extent2D size = settings.window.default_size;
    double fixed_time = -1; // >= 0 freezes simulation and exposure adaptation at this time
    double duration = 0;    // > 0 exits after this many wall-clock seconds
    int bookmark = -1;
    float exposure = 1;
    bool tour = false, high = false, no_hud = false, help = false;
    std::filesystem::path capture, benchmark;
};

template <class T> bool parse_number(std::optional<std::string_view> text, T& out) {
    if (!text)
        return false;
    const char* end = text->data() + text->size();
    const auto result = std::from_chars(text->data(), end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse_path(std::optional<std::string_view> text, std::filesystem::path& out) {
    if (!text || text->empty())
        return false;
    out = *text;
    return true;
}

bool options_valid(const Options& options) {
    const bool size_ok = settings.window.width.contains(options.size.width) &&
                         settings.window.height.contains(options.size.height);
    const bool time_ok = std::isfinite(options.fixed_time) && options.fixed_time >= -1 &&
                         std::isfinite(options.duration) && options.duration >= 0;
    const bool exposure_ok = std::isfinite(options.exposure) && options.exposure > 0;
    const bool bookmark_ok = options.bookmark >= -1 && options.bookmark < int(bookmark_count);
    return size_ok && time_ok && exposure_ok && bookmark_ok;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc)
                return std::nullopt;
            return std::string_view(argv[++i]);
        };
        bool ok = true;
        if (arg == "--help")
            options.help = true;
        else if (arg == "--tour")
            options.tour = true;
        else if (arg == "--high")
            options.high = true;
        else if (arg == "--no-hud")
            options.no_hud = true;
        else if (arg == "--seed")
            ok = parse_number(value(), options.seed);
        else if (arg == "--frames")
            ok = parse_number(value(), options.frame_limit);
        else if (arg == "--width")
            ok = parse_number(value(), options.size.width);
        else if (arg == "--height")
            ok = parse_number(value(), options.size.height);
        else if (arg == "--time")
            ok = parse_number(value(), options.fixed_time);
        else if (arg == "--duration")
            ok = parse_number(value(), options.duration);
        else if (arg == "--bookmark")
            ok = parse_number(value(), options.bookmark);
        else if (arg == "--exposure")
            ok = parse_number(value(), options.exposure);
        else if (arg == "--capture")
            ok = parse_path(value(), options.capture);
        else if (arg == "--benchmark")
            ok = parse_path(value(), options.benchmark);
        else {
            log::error("unknown option {}", arg);
            return std::nullopt;
        }
        if (!ok) {
            log::error("missing or invalid value for {}", arg);
            return std::nullopt;
        }
    }
    if (!options_valid(options)) {
        log::error("invalid dimensions, time, duration, exposure, or bookmark (expected 0..{})", bookmark_count - 1);
        return std::nullopt;
    }
    return options;
}

// Mutable state shared between the window procedure and the frame loop.
struct AppState {
    Camera camera;
    BodyStates bodies;
    bool running = true, paused = false, high = false, overlay = true, mouse_look = false, auto_exposure = true;
    float exposure = 1.0f;
    float mouse_dx = 0, mouse_dy = 0;
    POINT previous_cursor{};
    unsigned selected_body = 0;
    std::filesystem::path capture_request;
};

void handle_key(AppState& app, WPARAM key) {
    const auto& exposure = settings.exposure;
    switch (key) {
    case VK_ESCAPE: app.running = false; break;
    case VK_SPACE: app.paused = !app.paused; break;
    case 'T': app.camera.toggle_tour(); break;
    case 'O':
        app.camera.set_orbit_target(app.selected_body,
                                    app.bodies[app.selected_body].radius * settings.control.orbit_zoom_radii);
        break;
    case 'F': app.camera.set_mode(CameraMode::Free); break;
    case 'X': app.auto_exposure = !app.auto_exposure; break;
    case VK_F1: app.overlay = !app.overlay; break;
    case VK_F2: app.high = !app.high; break;
    case VK_F12: app.capture_request = hotkey_capture_path; break;
    case VK_OEM_PLUS: app.exposure = exposure.range.clamp(app.exposure * exposure.step); break;
    case VK_OEM_MINUS: app.exposure = exposure.range.clamp(app.exposure / exposure.step); break;
    default:
        if (key >= '1' && key < '1' + bookmark_count) {
            const auto index = static_cast<std::size_t>(key - '1');
            app.camera.set_bookmark(index, app.bodies);
            app.selected_body = unsigned(std::min<std::size_t>(index, app.bodies.size() - 1));
            app.camera.set_mode(CameraMode::Free);
        }
        break;
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<AppState*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app)
        return DefWindowProcW(window, message, w, l);
    constexpr LPARAM key_repeat_bit = 1ll << 30;
    switch (message) {
    case WM_CLOSE: app->running = false; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_KILLFOCUS:
    case WM_RBUTTONUP:
        app->mouse_look = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        app->mouse_look = true;
        if (app->camera.mode() == CameraMode::Tour)
            app->camera.set_mode(CameraMode::Free);
        GetCursorPos(&app->previous_cursor);
        SetCapture(window);
        return 0;
    case WM_MOUSEMOVE:
        if (app->mouse_look) {
            POINT cursor;
            GetCursorPos(&cursor);
            app->mouse_dx += float(cursor.x - app->previous_cursor.x);
            app->mouse_dy += float(cursor.y - app->previous_cursor.y);
            app->previous_cursor = cursor;
        }
        return 0;
    case WM_KEYDOWN:
        if (!(l & key_repeat_bit))
            handle_key(*app, w);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

std::filesystem::path executable_directory() {
    wchar_t buffer[32768];
    const auto length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!length || length >= std::size(buffer))
        panic("cannot locate the executable directory");
    return std::filesystem::path(buffer).parent_path();
}

struct Window {
    HWND handle = nullptr;
    Window(Extent2D size, AppState& app) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = window_class_name;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            panic("cannot register the window class");
        RECT area{0, 0, LONG(size.width), LONG(size.height)};
        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
        handle = CreateWindowExW(0, window_class_name, window_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 area.right - area.left, area.bottom - area.top, nullptr, nullptr, wc.hInstance, &app);
        if (!handle)
            panic("cannot create the desktop window");
        ShowWindow(handle, SW_SHOW);
    }
    ~Window() {
        if (handle)
            DestroyWindow(handle);
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

// Pumps pending messages; false once the application should exit.
bool pump_messages(AppState& app) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            app.running = false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return app.running;
}

Input gather_input(HWND window, AppState& app) {
    const auto down = [&](int key) { return GetForegroundWindow() == window && (GetAsyncKeyState(key) & 0x8000) != 0; };
    Input input;
    input.move_forward = float(down('W')) - float(down('S'));
    input.move_right = float(down('D')) - float(down('A'));
    input.move_up = float(down('E')) - float(down('Q'));
    input.speed_scale = down(VK_SHIFT) ? settings.control.fast_speed_scale : 1.0f;
    input.mouse_dx = app.mouse_dx;
    input.mouse_dy = app.mouse_dy;
    app.mouse_dx = app.mouse_dy = 0;
    const bool moving = input.move_forward != 0 || input.move_right != 0 || input.move_up != 0;
    if (moving && app.camera.mode() == CameraMode::Tour)
        app.camera.set_mode(CameraMode::Free);
    return input;
}

void update_title(HWND window, const render::Stats& stats, bool high) {
    const int fps = int(1000 / std::max(stats.frame_ms, 0.1f));
    const auto title = std::format(
        L"ORBITAL  |  {} FPS  |  {}  |  {} asteroids  |  RMB + WASD / T tour / 1-3 planets / F1 help", fps,
        high ? L"HIGH" : L"BASELINE", stats.visible_asteroids);
    SetWindowTextW(window, title.c_str());
}

struct FrameTimes {
    std::vector<float> cpu_ms, gpu_ms;
};

void write_benchmark(const std::filesystem::path& path, const FrameTimes& times) {
    std::string csv = "frame,cpu_submit_and_wait_ms,gpu_ms\n";
    for (std::size_t i = 0; i < times.cpu_ms.size(); i++)
        std::format_to(std::back_inserter(csv), "{},{},{}\n", i, times.cpu_ms[i], times.gpu_ms[i]);
    if (!file::write_text(path, csv))
        log::error("cannot write benchmark {}", path.string());
    auto sorted = times.cpu_ms;
    if (sorted.size() > settings.benchmark.warmup_frames)
        sorted.erase(sorted.begin(), sorted.begin() + settings.benchmark.warmup_frames);
    std::sort(sorted.begin(), sorted.end());
    if (!sorted.empty()) {
        const auto p95 = sorted[std::min(sorted.size() - 1, std::size_t(double(sorted.size()) * .95))];
        log::info("Frame timing median {} ms, p95 {} ms (CPU including GPU wait; not isolated GPU timing).",
                  sorted[sorted.size() / 2], p95);
    }
}

AppState initial_state(const Options& options, const SystemDescription& system) {
    AppState app;
    app.high = options.high;
    app.overlay = !options.no_hud;
    app.exposure = options.exposure;
    app.auto_exposure = options.fixed_time < 0;
    app.bodies = evaluate_system(system, std::max(0.0, options.fixed_time));
    if (options.bookmark >= 0)
        app.camera.set_bookmark(unsigned(options.bookmark), app.bodies);
    if (options.tour) {
        if (options.fixed_time >= 0)
            app.camera.set_tour_time(options.fixed_time);
        else
            app.camera.toggle_tour();
    }
    return app;
}

struct Session {
    const Options& options;
    const SystemDescription& system;
    std::filesystem::path directory;
    AppState& app;
    HWND window;
    render::Renderer& renderer;
};

// Runs until the window closes or the frame/duration limit is reached; returns the frame count.
unsigned frame_loop(const Session& session, FrameTimes& times) {
    const Options& options = session.options;
    AppState& app = session.app;
    render::Renderer& renderer = session.renderer;
    auto previous = std::chrono::steady_clock::now();
    double simulation_time = 0, elapsed = 0, title_clock = 0;
    unsigned frames = 0;
    while (pump_messages(app)) {
        if (IsIconic(session.window)) {
            WaitMessage();
            previous = std::chrono::steady_clock::now();
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::min(std::chrono::duration<double>(now - previous).count(),
                                   settings.control.max_frame_seconds);
        previous = now;
        elapsed += dt;
        if (!app.paused)
            simulation_time += dt;
        if (options.fixed_time >= 0)
            simulation_time = options.fixed_time;
        app.bodies = evaluate_system(session.system, simulation_time);
        app.camera.step(dt, elapsed, gather_input(session.window, app), app.bodies);

        const render::FrameInput frame_input{.camera = app.camera,
                                             .bodies = app.bodies,
                                             .time = simulation_time,
                                             .exposure = app.exposure,
                                             .high_quality = app.high,
                                             .overlay = app.overlay,
                                             .auto_exposure = app.auto_exposure};
        if (renderer.draw(frame_input)) {
            frames++;
            const auto stats = renderer.stats();
            times.cpu_ms.push_back(stats.frame_ms);
            times.gpu_ms.push_back(stats.gpu_ms);
            if (!app.capture_request.empty()) {
                const auto path = session.directory / app.capture_request;
                if (renderer.capture(path))
                    log::info("Saved {}", path.string());
                app.capture_request.clear();
            }
            const bool frames_done = options.frame_limit && frames >= options.frame_limit;
            const bool duration_done = options.duration > 0 && elapsed >= options.duration;
            if (frames_done || duration_done) {
                if (!options.capture.empty())
                    renderer.capture(options.capture);
                break;
            }
        }
        if (elapsed - title_clock > settings.window.title_refresh_seconds) {
            title_clock = elapsed;
            update_title(session.window, renderer.stats(), app.high);
        }
    }
    return frames;
}

int run(const Options& options) {
    const auto directory = executable_directory();
    const auto system = generate_system(options.seed);
    if (const auto errors = validate_system(system); !errors.empty()) {
        log::error("invalid generated system: {}", errors.front());
        return 1;
    }
    SetProcessDPIAware();
    AppState app = initial_state(options, system);
    Window window(options.size, app);
    render::Renderer renderer(window.handle, system, directory);
    log::info(
        "Ready. RMB + WASD: fly | 1/2/3: planets | T: tour | F2: quality | F12: capture | --help for all controls");
    FrameTimes times;
    times.cpu_ms.reserve(options.frame_limit ? options.frame_limit : settings.benchmark.expected_frames);
    times.gpu_ms.reserve(times.cpu_ms.capacity());
    const unsigned frames = frame_loop({.options = options,
                                        .system = system,
                                        .directory = directory,
                                        .app = app,
                                        .window = window.handle,
                                        .renderer = renderer},
                                       times);
    if (!options.benchmark.empty())
        write_benchmark(options.benchmark, times);
    log::info("Completed {} frames.", frames);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    const auto options = parse_options(argc, argv);
    if (!options)
        return 1;
    if (options->help) {
        log::info("{}", usage);
        return 0;
    }
    return run(*options);
}
