#include "app/actions.hpp"
#include "app/app_state.hpp"
#include "app/frame_input.hpp"
#include "app/hud.hpp"
#include "app/options.hpp"
#include "app/timing_average.hpp"
#include "app/ui.hpp"
#include "assets/image.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/math.hpp"
#include "platform/process.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace space;
using namespace space::app;
using platform::Key;

namespace window_limits {
inline constexpr double title_refresh_seconds = 0.5;
inline constexpr std::size_t timing_history_frames = 240; // recent CPU samples for the panel and title
} // namespace window_limits

namespace benchmark {
inline constexpr std::size_t warmup_frames = 60;      // dropped before computing percentiles
inline constexpr std::size_t expected_frames = 36000; // reserve for an open-ended run (10 minutes at 60 Hz)
} // namespace benchmark

constexpr std::string_view window_title = "ORBITAL  /  Procedural worlds";

void handle_key(AppState& app, Key key) {
    switch (key) {
    case Key::escape: app.running = false; break;
    case Key::space: app.paused = !app.paused; break;
    case Key::f1: app.overlay = !app.overlay; break;
    case Key::f2: app.high = !app.high; break;
    case Key::f3: app.belt.light_map = !app.belt.light_map; break;
    case Key::f4: app.belt.extinction = !app.belt.extinction; break;
    case Key::f5: app.aa.temporal_aa = !app.aa.temporal_aa; break;
    case Key::f9: cycle_choice(app.aa.spatial_aa); break;
    case Key::f6: cycle_choice(app.belt.splat_mode); break;
    case Key::f7: app.belt.splat_light_twice = !app.belt.splat_light_twice; break;
    case Key::f8: cycle_choice(app.tone.tone_curve); break;
    case Key::f10: request_capture(app); break;
    case Key::f11: app.belt_dust.enabled = !app.belt_dust.enabled; break;
    case Key::f12: app.show_ui = !app.show_ui; break;
    case Key::plus: app.tone.exposure = exposure_keys::range.clamp(app.tone.exposure * exposure_keys::step); break;
    case Key::minus: app.tone.exposure = exposure_keys::range.clamp(app.tone.exposure / exposure_keys::step); break;
    default: break;
    }
    if (key == platform::letter_key('T'))
        toggle_tour(app);
    else if (key == platform::letter_key('O'))
        orbit_selected(app);
    else if (key == platform::letter_key('F'))
        free_camera(app);
    else if (key == platform::letter_key('X'))
        app.tone.auto_exposure = !app.tone.auto_exposure;
    else if (const auto digit = platform::digit_of(key); digit && *digit >= 1 && *digit <= bookmark_count) {
        const std::size_t index = *digit - 1;
        select_bookmark(app, index);
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
    input.telescope = window.middle_button_down();
    const platform::MouseDelta mouse = window.take_mouse_look_delta();
    input.mouse_dx = mouse.dx;
    input.mouse_dy = mouse.dy;
    const bool moving = input.move_forward != 0 || input.move_right != 0 || input.move_up != 0;
    if ((moving || window.mouse_look_began()) && app.camera.mode() == CameraMode::Tour)
        free_camera(app);
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
        stats.triangles / 1e6, app.high ? "HIGH" : "BASELINE", app.belt.light_map ? "on" : "off",
        app.belt.extinction ? "on" : "off", app.belt_dust.enabled ? "on" : "off", app.aa.temporal_aa ? "on" : "off",
        app.aa.spatial_aa == render::SpatialAA::Off    ? "none"
        : app.aa.spatial_aa == render::SpatialAA::FXAA ? "FXAA"
                                                       : "SMAA",
        app.belt.billboard_radius() > 0 ? std::format("< {:.1f} px", app.belt.billboard_radius()) : "off",
        app.belt.splat_light_twice ? "2x" : "1x",
        app.tone.tone_curve == render::ToneCurve::ACES  ? "ACES"
        : app.tone.tone_curve == render::ToneCurve::AgX ? "AgX"
                                                        : "Neutral"));
}

struct FrameTimes {
    std::vector<float> cpu_ms, gpu_ms, prepare_ms;
    std::vector<float> cull_ms, body_shadow_ms, belt_light_ms, belt_disc_ms, meter_ms;
    std::vector<float> cull_shadow_ms, surface_ms, atmosphere_ms, post_ms; // GPU pass timings
    std::vector<float> children[15]; // the groups' children, in the CSV's column order
};

void write_benchmark(const std::filesystem::path& path, const FrameTimes& times) {
    std::string csv =
        "frame,cpu_submit_and_wait_ms,gpu_ms,cpu_prepare_ms,gpu_cull_shadow_ms,gpu_surface_ms,"
        "gpu_atmosphere_ms,gpu_post_ms,gpu_cull_ms,gpu_body_shadow_ms,gpu_belt_light_ms,gpu_belt_disc_ms,"
        "gpu_meter_ms,gpu_surface_sky_ms,gpu_surface_bodies_ms,gpu_surface_rocks_ms,gpu_surface_clouds_ms,"
        "gpu_atmospheres_ms,gpu_belt_dust_ms,gpu_splat_mask_ms,gpu_temporal_ms,gpu_streaks_ms,gpu_bloom_ms,"
        "gpu_sun_visibility_ms,gpu_flare_ms,gpu_composite_ms,gpu_spatial_aa_ms,gpu_present_ms\n";
    for (std::size_t i = 0; i < times.cpu_ms.size(); i++) {
        std::format_to(std::back_inserter(csv), "{},{},{},{},{},{},{},{},{},{},{},{},{}", i, times.cpu_ms[i],
                       times.gpu_ms[i], times.prepare_ms[i], times.cull_shadow_ms[i], times.surface_ms[i],
                       times.atmosphere_ms[i], times.post_ms[i], times.cull_ms[i], times.body_shadow_ms[i],
                       times.belt_light_ms[i], times.belt_disc_ms[i], times.meter_ms[i]);
        for (const auto& child : times.children)
            std::format_to(std::back_inserter(csv), ",{}", child[i]);
        csv += '\n';
    }
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
    app.aa.temporal_aa = options.taa != 0;
    app.aa.spatial_aa = render::SpatialAA(options.spatial);
    app.sky.galaxy_mode = render::GalaxyMode(options.galaxy);
    app.belt.splat_mode = render::SplatMode(options.splat);
    app.tone.tone_curve = render::ToneCurve(options.tone);
    app.pan = options.pan;
    app.overlay = !options.no_hud;
    app.show_ui = options.ui;
    app.belt_dust.enabled = options.dust != 0;
    app.belt.disc = options.disc != 0;
    app.belt.lod_scale = options.lod_scale;
    app.vsync = options.vsync < 0 ? options.benchmark.empty() : options.vsync != 0;
    app.tone.exposure = options.exposure;
    app.tone.hdr_output = render::HdrOutput(options.hdr);
    app.tone.auto_exposure = options.fixed_time < 0;
    app.bodies = evaluate_system(system, std::max(0.0, options.fixed_time));
    if (options.bookmark >= 0)
        select_bookmark(app, unsigned(options.bookmark));
    if (options.tour) {
        if (options.fixed_time >= 0)
            app.camera.set_tour_time(options.fixed_time);
        else
            toggle_tour(app);
    }
    if (options.galaxy_view) {
        const double longitude = *options.galaxy_view * pi<double> / 180;
        const double c = std::cos(longitude), s = std::sin(longitude);
        const Vec3d direction{-.054876 * c + .494109 * s, -.483835 * c + .746982 * s, -.873437 * c - .444830 * s};
        app.camera.look_at({0, 1000, 0}, Vec3d{0, 1000, 0} + direction);
        free_camera(app);
    }
    if (options.sun_at) {
        app.camera.aim(system.star.position, (*options.sun_at)[0], (*options.sun_at)[1], options.size.aspect());
        free_camera(app);
    }
    if (options.belt_sun_view && app.bodies.size() > 1 && !system.belts.empty()) {
        const auto& belt = system.belts.front();
        const auto parent = std::find_if(app.bodies.begin(), app.bodies.end(),
                                         [&](const BodyState& body) { return body.id == belt.parent_id; });
        ORBITAL_ASSERT(parent != app.bodies.end());
        // Same tilted ring plane as the belt renderer (normal 0, .933, .36).
        const Vec3d ring_point = parent->position +
                                 normalized(Vec3d{0, -.36, .933}) * ((belt.inner_radius + belt.outer_radius) * .5);
        const Vec3d toward_sun = normalized(system.star.position - ring_point);
        // Behind the belt plane: the centre ray crosses its middle, clear of the planet.
        app.camera.look_at(ring_point - toward_sun * 12.0, system.star.position);
        free_camera(app);
    }
    return app;
}

render::DisplaySettings display_settings(const platform::DisplayInfo& info) {
    return {.hdr = info.hdr,
            .min_nits = info.min_nits,
            .max_nits = info.max_nits,
            .max_full_frame_nits = info.max_full_frame_nits,
            .sdr_white_nits = info.sdr_white_nits};
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
    TimingAverage timing_average;
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
        if (options.pan_stop_frame && frames >= options.pan_stop_frame)
            app.pan = 0;
        // A panning capture steps the camera at a fixed rate so runs are comparable.
        app.camera.step(options.pan != 0 ? 1.0 / 60 : dt, elapsed, gather_input(window, app, !ui.wants_keyboard()),
                        app.bodies);
        // The overlay is built every frame so its input state stays current; the panel itself is optional.
        ui.begin_frame();
        if (app.show_ui) {
            const std::size_t recent = std::min(times.cpu_ms.size(), window_limits::timing_history_frames);
            draw_panel(app, timing_average.apply(renderer.stats()), std::span<const float>(times.cpu_ms).last(recent));
        }
        const ImDrawData* ui_draw = ui.end_frame();

        const auto frame_input = make_frame_input(app, simulation_time, ui_draw);
        renderer.set_vsync(app.vsync);
        if (frames % 60 == 0) // the window may move to another display, or the OS switch HDR
            app.display = display_settings(window.display_info());
        if (renderer.draw(frame_input)) {
            frames++;
            if (options.maximize_at && frames == options.maximize_at)
                window.maximize();
            if (options.fullscreen_at && frames == options.fullscreen_at)
                window.toggle_fullscreen();
            const auto stats = renderer.stats();
            timing_average.add(stats);
            if (options.benchmark.empty() && times.cpu_ms.size() == window_limits::timing_history_frames)
                times.cpu_ms.erase(times.cpu_ms.begin());
            times.cpu_ms.push_back(stats.frame_ms);
            if (!options.benchmark.empty()) {
                times.gpu_ms.push_back(stats.gpu_ms);
                times.prepare_ms.push_back(stats.prepare_ms);
                times.cull_shadow_ms.push_back(stats.shadow_ms);
                times.cull_ms.push_back(stats.cull_ms);
                times.body_shadow_ms.push_back(stats.body_shadow_ms);
                times.belt_light_ms.push_back(stats.belt_light_ms);
                times.belt_disc_ms.push_back(stats.belt_disc_ms);
                times.meter_ms.push_back(stats.meter_ms);
                const float children[15] = {stats.surface_sky_ms,    stats.surface_bodies_ms, stats.surface_rocks_ms,
                                            stats.surface_clouds_ms, stats.atmospheres_ms,    stats.belt_dust_ms,
                                            stats.splat_mask_ms,     stats.temporal_ms,       stats.streaks_ms,
                                            stats.bloom_ms,          stats.sun_visibility_ms, stats.flare_ms,
                                            stats.composite_ms,      stats.spatial_aa_ms,     stats.present_ms};
                for (std::size_t c = 0; c < 15; c++)
                    times.children[c].push_back(children[c]);
                times.surface_ms.push_back(stats.surface_ms);
                times.atmosphere_ms.push_back(stats.atmosphere_ms);
                times.post_ms.push_back(stats.post_ms);
            }
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
    // The display's HDR range seeds the tone defaults once; the panel can change them after.
    app.display = display_settings(window->display_info());
    if (app.display.hdr) {
        if (app.display.sdr_white_nits > 0)
            app.tone.paper_white_nits = app.display.sdr_white_nits;
        if (app.display.max_nits > 0)
            app.tone.peak_nits = app.display.max_nits;
        log::info("Display: HDR on, {:.0f} to {:.0f} nits, SDR white {:.0f} nits", app.display.min_nits,
                  app.display.max_nits, app.display.sdr_white_nits);
    }
    auto hud = make_hud();
    render::Renderer renderer(window->native_handle(), system, directory, hud.view(), {.belt_count = options.rocks});
    hud = {}; // The renderer has copied/uploaded the pixels.
    Ui ui(*window);
    const auto atlas = ui.font_atlas();
    renderer.set_ui_font({{atlas.width, atlas.height},
                          assets::PixelLayout::Rgba8,
                          {atlas.rgba, std::size_t(atlas.width) * atlas.height * 4}});
    log::info(
        "Ready. RMB + WASD: fly | 1-6: views | T: tour | F12: control panel | F10: capture | --help for all controls");
    FrameTimes times;
    times.cpu_ms.reserve(options.benchmark.empty() ? window_limits::timing_history_frames
                         : options.frame_limit     ? options.frame_limit
                                                   : benchmark::expected_frames);
    if (!options.benchmark.empty()) {
        times.gpu_ms.reserve(times.cpu_ms.capacity());
        times.prepare_ms.reserve(times.cpu_ms.capacity());
        times.cull_shadow_ms.reserve(times.cpu_ms.capacity());
        times.cull_ms.reserve(times.cpu_ms.capacity());
        times.body_shadow_ms.reserve(times.cpu_ms.capacity());
        times.belt_light_ms.reserve(times.cpu_ms.capacity());
        times.belt_disc_ms.reserve(times.cpu_ms.capacity());
        times.meter_ms.reserve(times.cpu_ms.capacity());
        for (auto& child : times.children)
            child.reserve(times.cpu_ms.capacity());
        times.surface_ms.reserve(times.cpu_ms.capacity());
        times.atmosphere_ms.reserve(times.cpu_ms.capacity());
        times.post_ms.reserve(times.cpu_ms.capacity());
    }
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
