#include "app/actions.hpp"
#include "app/app_state.hpp"
#include "app/benchmark_log.hpp"
#include "app/frame_history.hpp"
#include "app/frame_input.hpp"
#include "app/hud.hpp"
#include "app/options.hpp"
#include "app/stats_smoothing.hpp"
#include "app/ui.hpp"
#include "assets/image.hpp"
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
#include <fstream>
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
inline constexpr double title_refresh_seconds = 0.5;
} // namespace window_limits

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

void update_title(platform::Window& window, const SmoothedStats& smoothed, const AppState& app, float p95_ms) {
    const render::FrameStats& stats = smoothed.frame;
    const int fps = int(1000 / std::max(smoothed.frame_ms, 0.1f));
    window.set_title(std::format(
        "ORBITAL  |  {} FPS  |  {:.1f} ms (p95 {:.1f})  |  GPU {:.1f} ms  |  {} draws ({} rock groups)  |  "
        "{} rocks  |  {:.2f} M tris  |  {}  |  belt map {} ext {} dust {}  |  TAA {} + {}  |  splats {} lit {}  |  "
        "tone {}",
        fps, smoothed.frame_ms, p95_ms, stats.gpu[render::GpuPass::Frame], stats.draw_calls, stats.rock_groups_drawn,
        stats.visible_asteroids, stats.triangles / 1e6, app.high ? "HIGH" : "BASELINE",
        app.belt.light_map ? "on" : "off", app.belt.extinction ? "on" : "off", app.belt_dust.enabled ? "on" : "off",
        app.aa.temporal_aa ? "on" : "off",
        app.aa.spatial_aa == render::SpatialAA::Off    ? "none"
        : app.aa.spatial_aa == render::SpatialAA::FXAA ? "FXAA"
                                                       : "SMAA",
        app.belt.billboard_radius() > 0 ? std::format("< {:.1f} px", app.belt.billboard_radius()) : "off",
        app.belt.splat_light_twice ? "2x" : "1x",
        app.tone.tone_curve == render::ToneCurve::ACES  ? "ACES"
        : app.tone.tone_curve == render::ToneCurve::AgX ? "AgX"
                                                        : "Neutral"));
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
    app.belt.point_cutoff = options.point_cutoff;
    app.belt.speckle = options.speckle != 0;
    app.belt.veil = options.veil != 0;
    app.belt.lod_scale = options.lod_scale;
    app.vsync = options.vsync < 0 ? options.benchmark.empty() : options.vsync != 0;
    app.tone.exposure = options.exposure;
    app.tone.hdr_output = render::HdrOutput(options.hdr);
    app.tone.auto_exposure = options.fixed_time < 0;
    app.bodies = evaluate_system(system, std::max(0.0, options.fixed_time));
    if (options.bookmark >= 0)
        select_bookmark(app, unsigned(options.bookmark));
    // Away from the bookmark's body along its line, aimed at its centre: the far
    // and zoomed views the size-dependent checks need, without a bookmark each.
    if (const auto body = Camera::bookmark_body(options.bookmark >= 0 ? std::size_t(options.bookmark) : 0);
        options.back > 0 && body < app.bodies.size()) {
        const Vec3d centre = app.bodies[body].position, away = normalized(app.camera.position - centre);
        app.camera.look_at(centre + away * (length(app.camera.position - centre) + options.back), centre);
        free_camera(app);
    }
    if (options.fov_div > 1)
        app.camera.vertical_fov /= options.fov_div;
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

// What a scripted run reports per shot: the frames drawn, the median of each GPU
// pass over the frames after the warmup, and the meter's last reading.
struct ShotReadings {
    unsigned frames = 0;
    std::array<float, render::gpu_pass_count> pass_ms{};
    float draw_ms = 0;
    render::ExposureStats exposure;
};

// Runs until the window closes or the frame/duration limit is reached.
ShotReadings frame_loop(const Session& session, FrameHistory& history, BenchmarkLog* benchmark) {
    const Options& options = session.options;
    AppState& app = session.app;
    platform::Window& window = session.window;
    render::Renderer& renderer = session.renderer;
    Ui& ui = session.ui;
    auto previous = std::chrono::steady_clock::now();
    double simulation_time = 0, elapsed = 0, title_clock = 0;
    unsigned frames = 0;
    SmoothedStats smoother;
    std::vector<render::PassTimings> timings; // per frame, for the readings of a frame-limited run
    std::vector<float> draw_times;
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
        if (app.show_ui)
            draw_panel(app, renderer.stats(), smoother, history);
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
            // The frame time is the loop's delta: everything between two frames, not only draw().
            const float frame_ms = float(dt * 1000);
            const render::FrameStats& frame = renderer.stats().frame;
            history.push(frame_ms);
            smoother.add(frame, frame_ms, dt, app.camera.cut_serial());
            if (options.frame_limit) {
                timings.push_back(frame.gpu);
                draw_times.push_back(frame.draw_ms);
            }
            if (benchmark)
                benchmark->record(frame_ms, frame);
            if (!app.capture_request.empty()) {
                const auto path = numbered_capture_path(session.directory / app.capture_request);
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
            update_title(window, smoother, app, history.percentile(.95f));
        }
    }
    ShotReadings readings{.frames = frames, .exposure = renderer.stats().exposure};
    // Medians over the frames after a warmup of the first half, at most 60 frames.
    const std::size_t warmup = std::min<std::size_t>(timings.size() / 2, 60);
    const auto median = [&](auto value_of) {
        std::vector<float> values;
        for (std::size_t i = warmup; i < timings.size(); i++)
            values.push_back(value_of(i));
        if (values.empty())
            return 0.f;
        std::nth_element(values.begin(), values.begin() + std::ptrdiff_t(values.size() / 2), values.end());
        return values[values.size() / 2];
    };
    for (std::size_t pass = 0; pass < render::gpu_pass_count; pass++)
        readings.pass_ms[pass] = median([&](std::size_t i) { return timings[i].ms[pass]; });
    readings.draw_ms = median([&](std::size_t i) { return draw_times[i]; });
    return readings;
}

// A JSON string with the two characters that need it escaped.
std::string json_string(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '\\' || c == '"')
            out += '\\';
        out += c;
    }
    return out + "\"";
}

// One shot's entry of the report: its name and outputs, the frame count, the
// GPU pass medians by their benchmark column names, and the meter's reading.
std::string report_entry(const Shot& shot, const ShotReadings& readings) {
    std::string entry = std::format(
        "  {{\n    \"name\": {}, \"capture\": {}, \"frames\": {}, \"width\": {}, \"height\": {},\n",
        json_string(shot.name), json_string(shot.options.capture.string()), readings.frames, shot.options.size.width,
        shot.options.size.height);
    entry += std::format("    \"draw_ms\": {:.4f},\n    \"gpu\": {{", readings.draw_ms);
    for (std::size_t pass = 0; pass < render::gpu_pass_count; pass++)
        entry += std::format("{}\"{}\": {:.4f}", pass ? ", " : "", render::gpu_pass_info[pass].column,
                             readings.pass_ms[pass]);
    const auto& e = readings.exposure;
    entry += std::format(
        "}},\n    \"exposure\": {{\"automatic\": {}, \"ready\": {}, \"metered\": {:.5f}, \"peak\": {:.4f}, "
        "\"target\": {:.4f}, \"adapted\": {:.4f}}}\n  }}",
        e.automatic ? "true" : "false", e.ready ? "true" : "false", e.luminance, e.peak_luminance, e.target, e.adapted);
    return entry;
}

int run(const Options& options) {
    const auto start = std::chrono::steady_clock::now();
    const auto since_start = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    };
    const auto directory = platform::executable_directory();
    const auto system = generate_system(options.seed);
    if (const auto errors = validate_system(system); !errors.empty()) {
        log::error("invalid generated system: {}", errors.front());
        return 1;
    }
    // A shot list runs several scripted views in this one process; the command
    // line alone is a single shot with its own outputs.
    std::vector<Shot> shots;
    if (options.shots.empty())
        shots.push_back({.name = "", .options = options});
    else if (auto loaded = load_shots(options, options.shots))
        shots = std::move(*loaded);
    else
        return 1;
    platform::init_process();
    log::info("System generated in {} ms", since_start());
    const auto window = platform::Window::create(
        {.client_size = shots.front().options.size, .title = window_title, .hidden = options.headless});
    log::info("Window at {} ms", since_start());
    Extent2D window_size = shots.front().options.size;
    const auto display = window->display_info();
    if (display.hdr)
        log::info("Display: HDR on, {:.0f} to {:.0f} nits, SDR white {:.0f} nits", display.min_nits, display.max_nits,
                  display.sdr_white_nits);
    auto hud = make_hud();
    render::Renderer renderer(window->native_handle(), system, directory, hud.view(), {.belt_count = options.rocks});
    hud = {}; // The renderer has copied/uploaded the pixels.
    Ui ui(*window);
    const auto atlas = ui.font_atlas();
    renderer.set_ui_font({{atlas.width, atlas.height},
                          assets::PixelLayout::Rgba8,
                          {atlas.rgba, std::size_t(atlas.width) * atlas.height * 4}});
    log::info("Ready at {} ms. RMB + WASD: fly | 1-8: views | T: tour | F12: control panel | F10: capture | --help "
              "for all controls",
              since_start());
    std::string report;
    std::size_t last_cut = 0;
    for (std::size_t index = 0; index < shots.size(); index++) {
        const Shot& shot = shots[index];
        if (!shot.name.empty())
            log::info("Shot {} of {}: {}", index + 1, shots.size(), shot.name);
        AppState app = initial_state(shot.options, system);
        // The camera's cut count continues from the previous shot's, so the new
        // view reads as a cut and no history crosses over.
        app.camera.resume_cuts(last_cut);
        // The display's HDR range seeds the tone defaults once; the panel can change them after.
        app.display = display_settings(window->display_info());
        if (app.display.hdr) {
            if (app.display.sdr_white_nits > 0)
                app.tone.paper_white_nits = app.display.sdr_white_nits;
            if (app.display.max_nits > 0)
                app.tone.peak_nits = app.display.max_nits;
        }
        if (window->fullscreen())
            window->toggle_fullscreen(); // a previous shot's fullscreen-at; each shot starts windowed
        if (shot.options.size != window_size) {
            window->resize(shot.options.size);
            window_size = shot.options.size;
        }
        FrameHistory history;
        std::optional<BenchmarkLog> benchmark;
        if (!shot.options.benchmark.empty())
            benchmark.emplace(shot.options.benchmark);
        const ShotReadings readings = frame_loop({.options = shot.options,
                                                  .system = system,
                                                  .directory = directory,
                                                  .app = app,
                                                  .window = *window,
                                                  .renderer = renderer,
                                                  .ui = ui},
                                                 history, benchmark ? &*benchmark : nullptr);
        if (benchmark)
            benchmark->finish();
        last_cut = app.camera.cut_serial();
        // The meter's last reading, so a scripted run can check where each view lands.
        if (const auto& exposure = readings.exposure; app.tone.auto_exposure && exposure.ready)
            log::info("Exposure: metered {:.4f}, peak {:.2f}, target {:.2f}x, adapted {:.2f}x", exposure.luminance,
                      exposure.peak_luminance, exposure.target, exposure.adapted);
        log::info("Completed {} frames.", readings.frames);
        report += (report.empty() ? "" : ",\n") + report_entry(shot, readings);
        if (!app.running)
            break; // closed or Esc: the remaining shots are skipped
    }
    if (!options.report.empty()) {
        std::ofstream file(options.report, std::ios::binary | std::ios::trunc);
        file << "{\n\"shots\": [\n" << report << "\n]\n}\n";
        if (file)
            log::info("Saved {}", options.report.string());
        else
            log::error("cannot write the report {}", options.report.string());
    }
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
