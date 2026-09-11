#include "app/camera.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/math.hpp"
#include "platform/process.hpp"
#include "platform/window.hpp"
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

namespace {

using namespace space;
using platform::Key;

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
constexpr std::string_view window_title = "ORBITAL  /  Procedural worlds";

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

// Interactive state that key presses and the frame loop share.
struct AppState {
    Camera camera;
    BodyStates bodies;
    bool running = true, paused = false, high = false, overlay = true, auto_exposure = true;
    float exposure = 1.0f;
    unsigned selected_body = 0;
    std::filesystem::path capture_request;
};

void handle_key(AppState& app, Key key) {
    const auto& exposure = settings.exposure;
    switch (key) {
    case Key::escape: app.running = false; break;
    case Key::space: app.paused = !app.paused; break;
    case Key::f1: app.overlay = !app.overlay; break;
    case Key::f2: app.high = !app.high; break;
    case Key::f12: app.capture_request = hotkey_capture_path; break;
    case Key::plus: app.exposure = exposure.range.clamp(app.exposure * exposure.step); break;
    case Key::minus: app.exposure = exposure.range.clamp(app.exposure / exposure.step); break;
    default: break;
    }
    if (key == platform::letter_key('T'))
        app.camera.toggle_tour();
    else if (key == platform::letter_key('O'))
        app.camera.set_orbit_target(app.selected_body,
                                    app.bodies[app.selected_body].radius * settings.control.orbit_zoom_radii);
    else if (key == platform::letter_key('F'))
        app.camera.set_mode(CameraMode::Free);
    else if (key == platform::letter_key('X'))
        app.auto_exposure = !app.auto_exposure;
    else if (const auto digit = platform::digit_of(key); digit && *digit >= 1 && *digit <= bookmark_count) {
        const std::size_t index = *digit - 1;
        app.camera.set_bookmark(index, app.bodies);
        app.selected_body = unsigned(std::min<std::size_t>(index, app.bodies.size() - 1));
        app.camera.set_mode(CameraMode::Free);
    }
}

Input gather_input(platform::Window& window, AppState& app) {
    const auto axis = [&](char positive, char negative) {
        return float(window.key_down(platform::letter_key(positive))) -
               float(window.key_down(platform::letter_key(negative)));
    };
    Input input;
    input.move_forward = axis('W', 'S');
    input.move_right = axis('D', 'A');
    input.move_up = axis('E', 'Q');
    input.speed_scale = window.key_down(Key::shift) ? settings.control.fast_speed_scale : 1.0f;
    const platform::MouseDelta mouse = window.take_mouse_look_delta();
    input.mouse_dx = mouse.dx;
    input.mouse_dy = mouse.dy;
    const bool moving = input.move_forward != 0 || input.move_right != 0 || input.move_up != 0;
    if ((moving || window.mouse_look_began()) && app.camera.mode() == CameraMode::Tour)
        app.camera.set_mode(CameraMode::Free);
    return input;
}

void update_title(platform::Window& window, const render::Stats& stats, bool high) {
    const int fps = int(1000 / std::max(stats.frame_ms, 0.1f));
    window.set_title(
        std::format("ORBITAL  |  {} FPS  |  {}  |  {} asteroids  |  RMB + WASD / T tour / 1-3 planets / F1 help", fps,
                    high ? "HIGH" : "BASELINE", stats.visible_asteroids));
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
    platform::Window& window;
    render::Renderer& renderer;
};

// Runs until the window closes or the frame/duration limit is reached; returns the frame count.
unsigned frame_loop(const Session& session, FrameTimes& times) {
    const Options& options = session.options;
    AppState& app = session.app;
    platform::Window& window = session.window;
    render::Renderer& renderer = session.renderer;
    auto previous = std::chrono::steady_clock::now();
    double simulation_time = 0, elapsed = 0, title_clock = 0;
    unsigned frames = 0;
    while (app.running && window.pump_events()) {
        for (const Key key : window.key_presses())
            handle_key(app, key);
        if (!app.running)
            break;
        if (window.minimized()) {
            window.wait_for_events();
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
        app.camera.step(dt, elapsed, gather_input(window, app), app.bodies);

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
            update_title(window, renderer.stats(), app.high);
        }
    }
    return frames;
}

int run(const Options& options) {
    const auto directory = platform::executable_directory();
    const auto system = generate_system(options.seed);
    if (const auto errors = validate_system(system); !errors.empty()) {
        log::error("invalid generated system: {}", errors.front());
        return 1;
    }
    platform::init_process();
    AppState app = initial_state(options, system);
    const auto window = platform::Window::create({.client_size = options.size, .title = window_title});
    render::Renderer renderer(window->native_handle(), system, directory);
    log::info(
        "Ready. RMB + WASD: fly | 1/2/3: planets | T: tour | F2: quality | F12: capture | --help for all controls");
    FrameTimes times;
    times.cpu_ms.reserve(options.frame_limit ? options.frame_limit : settings.benchmark.expected_frames);
    times.gpu_ms.reserve(times.cpu_ms.capacity());
    const unsigned frames = frame_loop({.options = options,
                                        .system = system,
                                        .directory = directory,
                                        .app = app,
                                        .window = *window,
                                        .renderer = renderer},
                                       times);
    if (!options.benchmark.empty())
        write_benchmark(options.benchmark, times);
    log::info("Completed {} frames.", frames);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parse_options(argc, argv);
    if (!options)
        return 1;
    if (options->help) {
        log::info("{}", usage);
        return 0;
    }
    return run(*options);
}
