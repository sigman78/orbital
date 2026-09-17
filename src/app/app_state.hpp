#pragma once
#include "app/camera.hpp"
#include "core/math.hpp"
#include "render/settings.hpp"
#include "scene/system.hpp"

#include <filesystem>
#include <string_view>

// Interactive state that key presses, the control panel and the frame loop
// share, with the constants the controls are defined in terms of.
namespace space::app {

namespace exposure_keys {
inline constexpr Range<float> range{0.05f, 8.0f};
inline constexpr float step = 1.1f; // per +/- key press
} // namespace exposure_keys

namespace control {
inline constexpr double max_frame_seconds = 0.1; // a stall must not advance the simulation by more than this
inline constexpr float fast_speed_scale = 4.0f;  // Shift
inline constexpr float slow_speed_scale = 0.1f;  // Z toggles it: creeping up to a small body
inline constexpr double orbit_zoom_radii = 2.3;  // O key orbits at this many body radii
} // namespace control

inline constexpr std::string_view hotkey_capture_path =
    "captures/orbital.png"; // numbered per capture, see numbered_capture_path

struct AppState {
    Camera camera;
    BodyStates bodies;
    bool running = true, paused = false, high = false, overlay = true;
    bool show_ui = false; // F12
    bool vsync = true;
    bool slow_travel = false; // a tenth of the flight speed (Z)
    float pan = 0;            // lateral drift added to the move axis (--pan)
    render::ToneSettings tone;
    render::DisplaySettings display; // polled from the window once a second
    render::AntiAliasingSettings aa;
    render::BeltSettings belt;
    render::BeltDustSettings belt_dust;
    render::EarthSettings earth;
    render::SunSettings sun;
    render::PostSettings post;
    render::SkySettings sky;
    render::GasSettings gas;
    render::TerrainSettings terrain;
    unsigned selected_body = 0;
    std::filesystem::path capture_request;
};

// Startup, hotkeys and panel buttons must select the same camera and orbit target.
inline bool select_bookmark(AppState& app, std::size_t index) {
    if (index >= bookmark_count || Camera::bookmark_body(index) >= app.bodies.size())
        return false;
    app.camera.set_bookmark(index, app.bodies);
    app.selected_body = unsigned(Camera::bookmark_body(index));
    app.camera.set_mode(CameraMode::Free);
    return true;
}

} // namespace space::app
