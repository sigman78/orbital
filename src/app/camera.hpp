#pragma once

#include "core/math.hpp"
#include "scene/system.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace space {

enum class CameraMode { Free, Orbit, Tour };

// Per-step navigation input. Axes are -1, 0 or 1; mouse deltas are in pixels.
struct Input {
    float move_forward = 0.0f;
    float move_right = 0.0f;
    float move_up = 0.0f;
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    float speed_scale = 1.0f;
};

// Number of preset views reachable with the number keys.
constexpr std::size_t bookmark_count = 6;

struct Camera {
    Vec3d position{0.0, 0.8, 4.8};
    double vertical_fov = pi<double> / 3; // 60 degrees

    Camera();
    CameraMode mode() const { return mode_; }
    Vec3d forward() const { return forward_; }
    Vec3d right() const { return right_; }
    Vec3d up() const { return up_; }

    // Advances navigation by dt. Simulation time is supplied separately so a
    // paused simulation does not pause camera input or the tour clock.
    void step(double dt, double time, const Input& input, std::span<const BodyState> bodies);
    void set_bookmark(std::size_t index, std::span<const BodyState> bodies);
    // Body a bookmark is anchored to; used to pick the orbit target.
    static std::size_t bookmark_body(std::size_t index);
    void toggle_tour();
    void look_at(Vec3d eye, Vec3d target);
    void set_tour_time(double seconds);
    void set_orbit_target(std::size_t index, double zoom = 0.0);
    void set_mode(CameraMode mode);
    std::size_t orbit_target() const { return orbit_target_; }

private:
    struct Bookmark {
        Vec3d offset, forward;
        bool valid = false;
    };

    Vec3d forward_{0.188144f, -0.125429f, -0.97493f};
    Vec3d right_{0.981f, 0.0f, 0.188f};
    Vec3d up_{0.0236f, 0.992f, -0.123f};
    CameraMode mode_ = CameraMode::Free;
    std::size_t orbit_target_ = 0;
    double orbit_zoom_ = 8.0, orbit_yaw_ = 0.0, orbit_pitch_ = 0.35;
    double tour_start_ = 0.0;
    bool tour_clock_valid_ = false;
    bool tour_override_ = false;
    double tour_override_time_ = 0.0;
    Bookmark bookmarks_[bookmark_count]{};

    void step_tour(double time, std::span<const BodyState> bodies);
    void step_orbit(double dt, double speed, const Input& input, const BodyState& body);
    void step_free(double dt, double speed, const Input& input);
    void rebuild_basis();
    void collision_clamp(std::span<const BodyState> bodies);
};

} // namespace space
