#pragma once

#include "../scene/system.hpp"

#include <cstddef>
#include <vector>

namespace space {

enum class CameraMode { Free, Orbit, Tour };

struct Input {
    double move_forward = 0.0;
    double move_right = 0.0;
    double move_up = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double speed_scale = 1.0;
};

struct Camera {
    Vec3d position{0.0, 0.8, 4.8};
    double vertical_fov = 1.0471975511965976; // 60 degrees, radians

    Camera();
    CameraMode mode() const { return mode_; }
    Vec3d forward() const { return forward_; }
    Vec3d right() const { return right_; }
    Vec3d up() const { return up_; }

    // Advances navigation by dt. Simulation time is supplied separately so a
    // paused simulation does not pause camera input or the tour clock.
    void step(double dt, double time, const Input& input, const std::vector<BodyState>& bodies);
    void set_bookmark(std::size_t index, const std::vector<BodyState>& bodies);
    void toggle_tour();
    void look_at(Vec3d eye, Vec3d target);
    void set_tour_time(double seconds);
    void set_orbit_target(std::size_t index, double zoom = 0.0);
    void set_mode(CameraMode mode);
    std::size_t orbit_target() const { return orbit_target_; }

private:
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
    struct Bookmark {
        Vec3d offset, forward;
        bool valid = false;
    };
    std::vector<Bookmark> bookmarks_;

    void rebuild_basis();
    void collision_clamp(const std::vector<BodyState>& bodies);
};

} // namespace space
