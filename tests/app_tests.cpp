#include "app/actions.hpp"
#include "app/frame_history.hpp"
#include "app/frame_input.hpp"
#include "app/stats_smoothing.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string_view>

int main() {
    using namespace space;
    using namespace space::app;
    // The smoother: the first sample stands, then each reading eases toward the
    // latest by dt over the time constant; counts round, other fields pass through,
    // a camera cut restarts it.
    SmoothedStats smoother;
    const double half = SmoothedStats::time_constant_seconds * .5; // eases halfway
    render::FrameStats sample;
    sample.draw_ms = 250;
    sample.gpu[render::GpuPass::Frame] = 10;
    sample.visible_asteroids = 100;
    smoother.add(sample, 16, half, 1);
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 10 && smoother.frame_ms == 16); // primed, not eased from zero
    sample.gpu[render::GpuPass::Frame] = 30;
    sample.visible_asteroids = 103;
    sample.belt_lod = .5f;
    sample.draw_calls = 7;
    smoother.add(sample, 20, half, 1);
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 20 && smoother.frame_ms == 18);
    assert(smoother.frame.visible_asteroids == 102);                          // 101.5 rounds up
    assert(smoother.frame.belt_lod == .5f && smoother.frame.draw_calls == 7); // not smoothed
    assert(smoother.frame.draw_ms == 250);                                    // constant readings stay exact
    sample.gpu[render::GpuPass::Frame] = 50;
    smoother.add(sample, 20, half, 1);
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 35);
    smoother.add(sample, 20, 0, 1); // no time passed, nothing moves
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 35);
    smoother.add(sample, 20, 10, 1); // a long gap lands on the sample
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 50);
    sample.gpu[render::GpuPass::Frame] = 70;
    smoother.add(sample, 20, half, 2); // a cut: the new view's first sample stands alone
    assert(smoother.frame.gpu[render::GpuPass::Frame] == 70);
    // The frame history: a ring of the last samples, plotted from its offset, with a percentile on demand.
    FrameHistory history;
    assert(history.values().empty() && history.percentile(.95f) == 0 && history.peak() == 0);
    for (unsigned i = 1; i <= 10; i++)
        history.push(float(i));
    assert(history.values().size() == 10 && history.offset() == 0 && history.peak() == 10);
    assert(history.percentile(.5f) == 6 && history.percentile(.95f) == 10);
    for (unsigned i = 11; i <= FrameHistory::capacity + 3; i++)
        history.push(float(i));
    assert(history.values().size() == FrameHistory::capacity && history.offset() == 3);
    assert(history.values()[0] == FrameHistory::capacity + 1); // the newest three overwrote the oldest
    assert(history.values()[3] == 4 && history.peak() == FrameHistory::capacity + 3);
    // The pass table: one column per pass, all distinct, every child after its parent.
    for (std::size_t i = 0; i < render::gpu_pass_count; i++)
        for (std::size_t j = 0; j < i; j++)
            assert(std::string_view(render::gpu_pass_info[i].column) != render::gpu_pass_info[j].column);
    assert(std::string_view(render::pass_info(render::GpuPass::Frame).column) == "gpu_ms");
    AppState app;
    app.bodies = evaluate_system(generate_system(showcase_seed), 0);
    app.belt_dust = {
        .enabled = false, .density = 7, .brightness = 4, .far = 3, .saturation = .4f, .tint = {.2f, .3f, .6f}};
    app.tone.exposure = 2;
    app.aa.temporal_aa = false;
    app.belt.lod_scale = 3;
    app.earth.ocean_roughness = .7f;
    app.sun.glare_intensity = .2f;
    app.post.bloom = false;
    app.sky.star_brightness = 3;
    app.gas.haze = .8f;
    app.high = true;
    app.overlay = false;
    app.camera.look_at({19, 7, -3}, {-4, 2, 1});
    app.camera.vertical_fov = .73;
    const auto frame = make_frame_input(app, 123, nullptr);
    assert(frame.bodies.data() == app.bodies.data());
    assert(frame.camera.position == app.camera.position);
    assert(frame.camera.forward == app.camera.forward());
    assert(frame.camera.right == app.camera.right() && frame.camera.up == app.camera.up());
    assert(frame.camera.vertical_fov == .73 && frame.camera.cut_serial == app.camera.cut_serial());
    const auto snapshot_position = frame.camera.position;
    const auto snapshot_forward = frame.camera.forward;
    const auto snapshot_cut = frame.camera.cut_serial;
    app.camera.look_at({-8, 3, 2}, {0, 0, 0});
    app.camera.vertical_fov = 1.1;
    assert(frame.camera.position == snapshot_position && frame.camera.forward == snapshot_forward);
    assert(frame.camera.cut_serial == snapshot_cut && frame.camera.vertical_fov == .73);
    const auto next_frame = make_frame_input(app, 124, nullptr);
    assert(next_frame.camera.position == app.camera.position && next_frame.camera.forward == app.camera.forward());
    assert(next_frame.camera.cut_serial == app.camera.cut_serial() && next_frame.camera.cut_serial != snapshot_cut);
    assert(frame.time == 123 && frame.high_quality && !frame.overlay);
    assert(!frame.belt_dust.enabled && frame.belt_dust.density == 7 && frame.belt_dust.brightness == 4);
    assert(frame.belt_dust.far == 3 && frame.belt_dust.saturation == .4f);
    for (unsigned channel = 0; channel < 3; ++channel)
        assert(frame.belt_dust.tint[channel] == app.belt_dust.tint[channel]);
    assert(frame.tone.exposure == 2 && !frame.aa.temporal_aa && frame.belt.lod_scale == 3);
    assert(frame.earth.ocean_roughness == .7f && frame.sun.glare_intensity == .2f && !frame.post.bloom);
    assert(frame.sky.star_brightness == 3 && frame.gas.haze == .8f);

    for (std::size_t index = 0; index < bookmark_count; ++index) {
        assert(select_bookmark(app, index));
        assert(app.selected_body == Camera::bookmark_body(index));
        assert(app.camera.mode() == CameraMode::Free);
        app.camera.set_orbit_target(app.selected_body);
        assert(app.camera.orbit_target() == Camera::bookmark_body(index));
    }
    const auto selected = app.selected_body;
    assert(orbit_selected(app) && app.camera.orbit_target() == selected);
    free_camera(app);
    assert(app.camera.mode() == CameraMode::Free);
    request_capture(app);
    assert(app.capture_request == hotkey_capture_path);
    {
        // Captures are numbered past the highest already there; other files and a missing directory do not matter.
        const auto directory = std::filesystem::temp_directory_path() / "orbital-app-tests" / "captures";
        std::filesystem::remove_all(directory.parent_path());
        const auto request = directory / "orbital.png";
        assert(numbered_capture_path(request) == directory / "orbital-0001.png");
        assert(std::filesystem::is_directory(directory));
        for (const char* name :
             {"orbital-0003.png", "orbital-0012.png", "orbital-x.png", "orbital-0020.bmp", "other-0099.png"})
            std::ofstream(directory / name) << "x";
        assert(numbered_capture_path(request) == directory / "orbital-0013.png");
        std::filesystem::remove_all(directory.parent_path());
    }
    app.aa.spatial_aa = render::SpatialAA(unsigned(render::SpatialAA::Count) - 1);
    cycle_choice(app.aa.spatial_aa);
    assert(unsigned(app.aa.spatial_aa) == 0);
    assert(!select_bookmark(app, bookmark_count) && app.selected_body == selected);
    app.bodies.clear();
    assert(!orbit_selected(app));
    assert(!select_bookmark(app, 0));
}
