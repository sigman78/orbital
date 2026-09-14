#pragma once
#include "app/app_state.hpp"

namespace space::app {

template <class Enum> constexpr void cycle_choice(Enum& value) {
    value = Enum((unsigned(value) + 1) % unsigned(Enum::Count));
}

inline bool orbit_selected(AppState& app) {
    if (app.selected_body >= app.bodies.size())
        return false;
    app.camera.set_orbit_target(app.selected_body, app.bodies[app.selected_body].radius * control::orbit_zoom_radii);
    return true;
}
inline void toggle_tour(AppState& app) {
    app.camera.toggle_tour();
}
inline void free_camera(AppState& app) {
    app.camera.set_mode(CameraMode::Free);
}
inline void request_capture(AppState& app) {
    app.capture_request = hotkey_capture_path;
}

} // namespace space::app
