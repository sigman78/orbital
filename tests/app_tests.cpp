#include "app/frame_input.hpp"
#include <cassert>

int main() {
    using namespace space;
    using namespace space::app;
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
    assert(!select_bookmark(app, bookmark_count) && app.selected_body == selected);
    app.bodies.clear();
    assert(!select_bookmark(app, 0));
}
