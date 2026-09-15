#pragma once
#include "app/app_state.hpp"
#include "render/renderer.hpp"

namespace space::app {

constexpr render::CameraView make_camera_view(const Camera& camera) {
    return {.position = camera.position,
            .forward = camera.forward(),
            .right = camera.right(),
            .up = camera.up(),
            .vertical_fov = camera.vertical_fov,
            .cut_serial = camera.cut_serial()};
}

inline render::FrameInput make_frame_input(const AppState& app, double time, const ImDrawData* ui) {
    return {.camera = make_camera_view(app.camera),
            .bodies = app.bodies,
            .time = time,
            .high_quality = app.high,
            .overlay = app.overlay,
            .ui = ui,
            .tone = app.tone,
            .display = app.display,
            .aa = app.aa,
            .belt = app.belt,
            .belt_dust = app.belt_dust,
            .earth = app.earth,
            .sun = app.sun,
            .post = app.post,
            .sky = app.sky,
            .gas = app.gas};
}

} // namespace space::app
