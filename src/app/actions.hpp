#pragma once
#include "app/app_state.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <system_error>

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

// The file a capture request lands in: the request's stem numbered past the
// highest such file already in its directory (orbital-0001.png, -0002 and on),
// so one session's shots never overwrite each other or an earlier session's.
// The directory is created if it is missing.
inline std::filesystem::path numbered_capture_path(const std::filesystem::path& request) {
    const std::string stem = request.stem().string() + "-";
    const std::string extension = request.extension().string();
    std::error_code error;
    std::filesystem::create_directories(request.parent_path(), error);
    unsigned highest = 0;
    for (const auto& entry : std::filesystem::directory_iterator(request.parent_path(), error)) {
        const std::string name = entry.path().filename().string();
        if (name.size() <= stem.size() + extension.size() || !name.starts_with(stem) || !name.ends_with(extension))
            continue;
        unsigned number = 0;
        const char* first = name.data() + stem.size();
        const char* last = name.data() + name.size() - extension.size();
        if (const auto [end, code] = std::from_chars(first, last, number); code == std::errc{} && end == last)
            highest = std::max(highest, number);
    }
    return request.parent_path() / std::format("{}{:04}{}", stem, highest + 1, extension);
}

} // namespace space::app
