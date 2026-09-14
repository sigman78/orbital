#pragma once
#include "app/app_state.hpp"
#include "render/renderer.hpp"

namespace space::app {

inline render::FrameInput make_frame_input(const AppState& app, double time, const ImDrawData* ui) {
    return {.camera = app.camera,
            .bodies = app.bodies,
            .time = time,
            .high_quality = app.high,
            .overlay = app.overlay,
            .ui = ui,
            .tone = app.tone,
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
