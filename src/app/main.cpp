#include "app/app_state.hpp"
#include "app/ui.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/math.hpp"
#include "platform/process.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"

#include <algorithm>
#include <array>
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

namespace {

using namespace space;
using namespace space::app;
using platform::Key;

namespace window_limits {
inline constexpr Extent2D default_size{1600, 900};
inline constexpr Range<unsigned> width{320, 7680}, height{200, 4320};
inline constexpr double title_refresh_seconds = 0.5;
} // namespace window_limits

namespace benchmark {
inline constexpr std::size_t warmup_frames = 60;      // dropped before computing percentiles
inline constexpr std::size_t expected_frames = 36000; // reserve for an open-ended run (10 minutes at 60 Hz)
} // namespace benchmark

constexpr std::string_view window_title = "ORBITAL  /  Procedural worlds";

constexpr std::string_view usage =
    "ORBITAL - NoGraphicsAPI space demo\n"
    "--seed N --frames N --duration seconds --width W --height H --time seconds --bookmark 0..5\n"
    "--capture file.png --benchmark file.csv --tour --high --no-hud --exposure scale --pan axis --rocks N\n"
    "--taa 0|1 --spatial 0|1|2 (off, FXAA, SMAA) --dust 0|1 --disc 0|1 --lod-scale X --vsync 0|1 --splat 0..3 --tone "
    "0|1|2 "
    "--maximize-at N "
    "--fullscreen-at "
    "N\n"
    "Controls: RMB mouse look; WASD move; Q/E vertical; Shift fast; 1-6 bookmarks; O orbit; F free;\n"
    "T tour; Space pause; +/- exposure; X auto exposure; F1 HUD; F2 quality; F3 belt light map; F4 belt extinction;\n"
    "F5 temporal AA; F6 rock splat cut-off; F7 splat lighting in both cull passes; F8 tone curve;\n"
    "F9 spatial AA (off, FXAA, SMAA); Alt+Enter borderless fullscreen;\n"
    "F10 capture; F11 belt dust; F12 control panel (--ui shows it at start); Esc exit.";

struct Options {
    std::uint64_t seed = showcase_seed;
    unsigned frame_limit = 0;
    Extent2D size = window_limits::default_size;
    double fixed_time = -1; // >= 0 freezes simulation and exposure adaptation at this time
    double duration = 0;    // > 0 exits after this many wall-clock seconds
    int bookmark = -1;
    float exposure = 1;
    unsigned rocks = 0;       // belt override for benchmarks; 0 keeps the quality tiers
    int vsync = -1;           // -1 default: on, except off for benchmarks
    unsigned taa = 1;         // temporal anti-aliasing on
    unsigned dust = 1;        // volumetric belt dust
    unsigned disc = 1;        // far-belt disc LOD
    float lod_scale = 1;      // far-belt fade distance scale
    unsigned spatial = 2;     // spatial pass: 0 off, 1 FXAA, 2 SMAA
    unsigned splat = 2;       // initial splat cut-off mode, an index into splat_radii
    unsigned tone = 2;        // initial tone curve (PBR Neutral)
    float pan = 0;            // lateral drift as a fraction of the flight speed, stepped at a fixed 60 Hz for captures
    unsigned maximize_at = 0; // > 0 maximizes the window after this many frames, to test resizing in captures
    unsigned fullscreen_at = 0; // > 0 enters borderless fullscreen after this many frames
    bool tour = false, high = false, no_hud = false, help = false;
    bool ui = false; // start with the control panel shown
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
    const bool size_ok = window_limits::width.contains(options.size.width) &&
                         window_limits::height.contains(options.size.height);
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
        else if (arg == "--ui")
            options.ui = true;
        else if (arg == "--high")
            options.high = true;
        else if (arg == "--no-hud")
            options.no_hud = true;
        else if (arg == "--seed")
            ok = parse_number(value(), options.seed);
        else if (arg == "--frames")
            ok = parse_number(value(), options.frame_limit);
        else if (arg == "--rocks")
            ok = parse_number(value(), options.rocks);
        else if (arg == "--lod-scale")
            ok = parse_number(value(), options.lod_scale) && options.lod_scale > 0;
        else if (arg == "--disc")
            ok = parse_number(value(), options.disc) && options.disc <= 1;
        else if (arg == "--dust")
            ok = parse_number(value(), options.dust) && options.dust <= 1;
        else if (arg == "--vsync")
            ok = parse_number(value(), options.vsync) && options.vsync >= 0 && options.vsync <= 1;
        else if (arg == "--taa")
            ok = parse_number(value(), options.taa) && options.taa <= 1;
        else if (arg == "--spatial")
            ok = parse_number(value(), options.spatial) && options.spatial <= 2;
        else if (arg == "--pan")
            ok = parse_number(value(), options.pan);
        else if (arg == "--maximize-at")
            ok = parse_number(value(), options.maximize_at);
        else if (arg == "--fullscreen-at")
            ok = parse_number(value(), options.fullscreen_at);
        else if (arg == "--tone")
            ok = parse_number(value(), options.tone) && options.tone <= 2;
        else if (arg == "--splat")
            ok = parse_number(value(), options.splat) && options.splat < splat_radii.size();
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

void handle_key(AppState& app, Key key) {
    switch (key) {
    case Key::escape: app.running = false; break;
    case Key::space: app.paused = !app.paused; break;
    case Key::f1: app.overlay = !app.overlay; break;
    case Key::f2: app.high = !app.high; break;
    case Key::f3: app.belt_light_map = !app.belt_light_map; break;
    case Key::f4: app.belt_extinction = !app.belt_extinction; break;
    case Key::f5: app.temporal_aa = !app.temporal_aa; break;
    case Key::f9: app.spatial_aa = (app.spatial_aa + 1) % 3; break;
    case Key::f6: app.splat_mode = (app.splat_mode + 1) % splat_radii.size(); break;
    case Key::f7: app.splat_light_twice = !app.splat_light_twice; break;
    case Key::f8: app.tone_curve = (app.tone_curve + 1) % 3; break;
    case Key::f10: app.capture_request = hotkey_capture_path; break;
    case Key::f11: app.belt_dust = !app.belt_dust; break;
    case Key::f12: app.show_ui = !app.show_ui; break;
    case Key::plus: app.exposure = exposure_keys::range.clamp(app.exposure * exposure_keys::step); break;
    case Key::minus: app.exposure = exposure_keys::range.clamp(app.exposure / exposure_keys::step); break;
    default: break;
    }
    if (key == platform::letter_key('T'))
        app.camera.toggle_tour();
    else if (key == platform::letter_key('O'))
        app.camera.set_orbit_target(app.selected_body,
                                    app.bodies[app.selected_body].radius * control::orbit_zoom_radii);
    else if (key == platform::letter_key('F'))
        app.camera.set_mode(CameraMode::Free);
    else if (key == platform::letter_key('X'))
        app.auto_exposure = !app.auto_exposure;
    else if (const auto digit = platform::digit_of(key); digit && *digit >= 1 && *digit <= bookmark_count) {
        const std::size_t index = *digit - 1;
        app.camera.set_bookmark(index, app.bodies);
        app.selected_body = unsigned(std::min(Camera::bookmark_body(index), app.bodies.size() - 1));
        app.camera.set_mode(CameraMode::Free);
    }
}

Input gather_input(platform::Window& window, AppState& app, bool keyboard_free) {
    const auto axis = [&](char positive, char negative) {
        return float(window.key_down(platform::letter_key(positive))) -
               float(window.key_down(platform::letter_key(negative)));
    };
    Input input;
    if (keyboard_free) { // a focused text field in the panel keeps the flight keys idle
        input.move_forward = axis('W', 'S');
        input.move_right = axis('D', 'A');
        input.move_up = axis('E', 'Q');
    }
    input.move_right += app.pan;
    input.speed_scale = window.key_down(Key::shift) ? control::fast_speed_scale : 1.0f;
    const platform::MouseDelta mouse = window.take_mouse_look_delta();
    input.mouse_dx = mouse.dx;
    input.mouse_dy = mouse.dy;
    const bool moving = input.move_forward != 0 || input.move_right != 0 || input.move_up != 0;
    if ((moving || window.mouse_look_began()) && app.camera.mode() == CameraMode::Tour)
        app.camera.set_mode(CameraMode::Free);
    return input;
}

// 95th percentile of the most recent frame times, for the title bar.
float recent_p95_ms(std::span<const float> frame_ms) {
    constexpr std::size_t window = 120;
    if (frame_ms.empty())
        return 0;
    const auto begin = frame_ms.size() > window ? frame_ms.end() - window : frame_ms.begin();
    std::vector<float> recent(begin, frame_ms.end());
    const auto nth = recent.begin() + std::ptrdiff_t(recent.size() * 95 / 100);
    std::nth_element(recent.begin(), nth, recent.end());
    return *nth;
}

void update_title(platform::Window& window, const render::Stats& stats, const AppState& app, float p95_ms) {
    const int fps = int(1000 / std::max(stats.frame_ms, 0.1f));
    window.set_title(std::format(
        "ORBITAL  |  {} FPS  |  {:.1f} ms (p95 {:.1f})  |  GPU {:.1f} ms  |  {} draws ({} rock groups)  |  "
        "{} rocks  |  {:.2f} M tris  |  {}  |  belt map {} ext {} dust {}  |  TAA {} + {}  |  splats {} lit {}  |  "
        "tone {}",
        fps, stats.frame_ms, p95_ms, stats.gpu_ms, stats.draw_calls, stats.rock_groups_drawn, stats.visible_asteroids,
        stats.triangles / 1e6, app.high ? "HIGH" : "BASELINE", app.belt_light_map ? "on" : "off",
        app.belt_extinction ? "on" : "off", app.belt_dust ? "on" : "off", app.temporal_aa ? "on" : "off",
        app.spatial_aa == 0   ? "none"
        : app.spatial_aa == 1 ? "FXAA"
                              : "SMAA",
        splat_radii[app.splat_mode] > 0 ? std::format("< {:.1f} px", splat_radii[app.splat_mode]) : "off",
        app.splat_light_twice ? "2x" : "1x",
        app.tone_curve == 0   ? "ACES"
        : app.tone_curve == 1 ? "AgX"
                              : "Neutral"));
}

struct FrameTimes {
    std::vector<float> cpu_ms, gpu_ms, prepare_ms;
    std::vector<float> cull_shadow_ms, surface_ms, atmosphere_ms, post_ms; // GPU pass timings
};

void write_benchmark(const std::filesystem::path& path, const FrameTimes& times) {
    std::string csv = "frame,cpu_submit_and_wait_ms,gpu_ms,cpu_prepare_ms,gpu_cull_shadow_ms,gpu_surface_ms,"
                      "gpu_atmosphere_ms,gpu_post_ms\n";
    for (std::size_t i = 0; i < times.cpu_ms.size(); i++)
        std::format_to(std::back_inserter(csv), "{},{},{},{},{},{},{},{}\n", i, times.cpu_ms[i], times.gpu_ms[i],
                       times.prepare_ms[i], times.cull_shadow_ms[i], times.surface_ms[i], times.atmosphere_ms[i],
                       times.post_ms[i]);
    if (!file::write_text(path, csv))
        log::error("cannot write benchmark {}", path.string());
    auto sorted = times.cpu_ms;
    if (sorted.size() > benchmark::warmup_frames)
        sorted.erase(sorted.begin(), sorted.begin() + benchmark::warmup_frames);
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
    app.temporal_aa = options.taa != 0;
    app.spatial_aa = options.spatial;
    app.splat_mode = options.splat;
    app.tone_curve = options.tone;
    app.pan = options.pan;
    app.overlay = !options.no_hud;
    app.show_ui = options.ui;
    app.belt_dust = options.dust != 0;
    app.belt_disc = options.disc != 0;
    app.belt_lod_scale = options.lod_scale;
    app.vsync = options.vsync < 0 ? options.benchmark.empty() : options.vsync != 0;
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
    Ui& ui;
};

// Runs until the window closes or the frame/duration limit is reached; returns the frame count.
unsigned frame_loop(const Session& session, FrameTimes& times) {
    const Options& options = session.options;
    AppState& app = session.app;
    platform::Window& window = session.window;
    render::Renderer& renderer = session.renderer;
    Ui& ui = session.ui;
    auto previous = std::chrono::steady_clock::now();
    double simulation_time = 0, elapsed = 0, title_clock = 0;
    unsigned frames = 0;
    while (app.running && window.pump_events()) {
        for (const Key key : window.key_presses()) {
            if (key == Key::alt_enter)
                window.toggle_fullscreen();
            else
                handle_key(app, key);
        }
        if (!app.running)
            break;
        if (window.minimized()) {
            window.wait_for_events();
            previous = std::chrono::steady_clock::now();
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::min(std::chrono::duration<double>(now - previous).count(), control::max_frame_seconds);
        previous = now;
        elapsed += dt;
        if (!app.paused)
            simulation_time += dt;
        if (options.fixed_time >= 0)
            simulation_time = options.fixed_time;
        app.bodies = evaluate_system(session.system, simulation_time);
        // A panning capture steps the camera at a fixed rate so runs are comparable.
        app.camera.step(app.pan != 0 ? 1.0 / 60 : dt, elapsed, gather_input(window, app, !ui.wants_keyboard()),
                        app.bodies);
        // The overlay is built every frame so its input state stays current; the panel itself is optional.
        ui.begin_frame();
        if (app.show_ui) {
            const std::size_t recent = std::min<std::size_t>(times.cpu_ms.size(), 240);
            draw_panel(app, renderer.stats(), std::span<const float>(times.cpu_ms).last(recent));
        }
        const ImDrawData* ui_draw = ui.end_frame();

        const render::FrameInput frame_input{.camera = app.camera,
                                             .bodies = app.bodies,
                                             .time = simulation_time,
                                             .exposure = app.exposure,
                                             .high_quality = app.high,
                                             .overlay = app.overlay,
                                             .auto_exposure = app.auto_exposure,
                                             .belt_light_map = app.belt_light_map,
                                             .belt_extinction = app.belt_extinction,
                                             .belt_dust = app.belt_dust,
                                             .dust_density = app.dust_density,
                                             .dust_brightness = app.dust_brightness,
                                             .dust_far = app.dust_far,
                                             .dust_saturation = app.dust_saturation,
                                             .dust_tint = {app.dust_tint[0], app.dust_tint[1], app.dust_tint[2]},
                                             .belt_disc = app.belt_disc,
                                             .belt_lod_scale = app.belt_lod_scale,
                                             .temporal_aa = app.temporal_aa,
                                             .spatial_aa = app.spatial_aa,
                                             .billboard_radius = splat_radii[app.splat_mode],
                                             .splat_light_twice = app.splat_light_twice,
                                             .tone_curve = app.tone_curve,
                                             .ui = ui_draw,
                                             .ocean_roughness = app.ocean_roughness,
                                             .glint_intensity = app.glint_intensity,
                                             .sea_patchiness = app.sea_patchiness,
                                             .cloud_shadow = app.cloud_shadow,
                                             .cloud_shadow_softness = app.cloud_shadow_softness,
                                             .cloud_opacity = app.cloud_opacity,
                                             .glare_intensity = app.glare_intensity,
                                             .ghost_strength = app.ghost_strength,
                                             .starburst_strength = app.starburst_strength,
                                             .starburst_blades = unsigned(std::max(app.starburst_blades, 0)),
                                             .sun_disc_radius = app.sun_disc_radius,
                                             .sun_limb_darkening = app.sun_limb_darkening,
                                             .bloom_intensity = app.bloom ? app.bloom_intensity : 0.f,
                                             .bloom_threshold = app.bloom_threshold,
                                             .bloom_knee = app.bloom_knee,
                                             .aberration = app.aberration,
                                             .vignette = app.vignette,
                                             .grain = app.grain,
                                             .motion_streaks = app.motion_streaks,
                                             .motion_streak_intensity = app.motion_streak_intensity,
                                             .gas_flow = app.gas_flow,
                                             .gas_time_scale = app.gas_time_scale,
                                             .gas_cycle = app.gas_cycle,
                                             .gas_turbulence = app.gas_turbulence,
                                             .gas_haze = app.gas_haze,
                                             .gas_terminator = app.gas_terminator,
                                             .gas_relief = app.gas_relief,
                                             .gas_lightning_rate = app.gas_lightning_rate,
                                             .gas_lightning = app.gas_lightning,
                                             .gas_polar = app.gas_polar,
                                             .gas_cap_size = app.gas_cap_size,
                                             .gas_cap_blend = app.gas_cap_blend,
                                             .gas_cap_opacity = app.gas_cap_opacity,
                                             .gas_layers = app.gas_layers,
                                             .gas_layer_lift = app.gas_layer_lift,
                                             .gas_layer_shadow = app.gas_layer_shadow};
        renderer.set_vsync(app.vsync);
        if (renderer.draw(frame_input)) {
            frames++;
            if (options.maximize_at && frames == options.maximize_at)
                window.maximize();
            if (options.fullscreen_at && frames == options.fullscreen_at)
                window.toggle_fullscreen();
            const auto stats = renderer.stats();
            times.cpu_ms.push_back(stats.frame_ms);
            times.gpu_ms.push_back(stats.gpu_ms);
            times.prepare_ms.push_back(stats.prepare_ms);
            times.cull_shadow_ms.push_back(stats.shadow_ms);
            times.surface_ms.push_back(stats.surface_ms);
            times.atmosphere_ms.push_back(stats.atmosphere_ms);
            times.post_ms.push_back(stats.post_ms);
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
        if (elapsed - title_clock > window_limits::title_refresh_seconds) {
            title_clock = elapsed;
            update_title(window, renderer.stats(), app, recent_p95_ms(times.cpu_ms));
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
    render::Renderer renderer(window->native_handle(), system, directory, {.belt_count = options.rocks});
    Ui ui(*window);
    const auto atlas = ui.font_atlas();
    renderer.set_ui_font(atlas.rgba, atlas.width, atlas.height);
    log::info(
        "Ready. RMB + WASD: fly | 1-6: views | T: tour | F12: control panel | F10: capture | --help for all controls");
    FrameTimes times;
    times.cpu_ms.reserve(options.frame_limit ? options.frame_limit : benchmark::expected_frames);
    times.gpu_ms.reserve(times.cpu_ms.capacity());
    const unsigned frames = frame_loop({.options = options,
                                        .system = system,
                                        .directory = directory,
                                        .app = app,
                                        .window = *window,
                                        .renderer = renderer,
                                        .ui = ui},
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
