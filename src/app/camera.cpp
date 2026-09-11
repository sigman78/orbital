#include "camera.hpp"

#include <algorithm>
#include <cmath>

namespace space {
namespace {
constexpr double pi = 3.14159265358979323846;
Vec3d cross(Vec3d a, Vec3d b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3d rotate_y(Vec3d p, double a) {
    const double c = std::cos(a), s = std::sin(a);
    return {c * p.x - s * p.z, p.y, s * p.x + c * p.z};
}
double clampd(double x, double a, double b) {
    return std::max(a, std::min(b, x));
}
Vec3d catmull(Vec3d a, Vec3d b, Vec3d c, Vec3d d, double t) {
    const double t2 = t * t, t3 = t2 * t;
    return (b * 2.0 + (c - a) * t + (a * 2.0 - b * 5.0 + c * 4.0 - d) * t2 + (a * -1.0 + b * 3.0 - c * 3.0 + d) * t3) *
           0.5;
}
} // namespace

Camera::Camera() {
    forward_ = normalized(Vec3d{0.8, 1.2, -3.0} - position);
    rebuild_basis();
}

void Camera::rebuild_basis() {
    forward_ = normalized(forward_);
    // Keep world Y as the stable up direction except when looking nearly up.
    Vec3d world_up{0, 1, 0};
    right_ = normalized(cross(forward_, world_up));
    if (length(right_) < 1e-8)
        right_ = {1, 0, 0};
    up_ = normalized(cross(right_, forward_));
}

void Camera::collision_clamp(const std::vector<BodyState>& bodies) {
    for (const auto& body : bodies) {
        const Vec3d delta = position - body.position;
        const double distance = length(delta), minimum = std::max(0.0, body.radius * 1.08);
        if (minimum > 0.0 && distance < minimum) {
            const Vec3d outward = distance > 1e-9 ? delta * (1.0 / distance) : Vec3d{0, 1, 0};
            position = body.position + outward * minimum;
        }
    }
}

void Camera::step(double dt, double time, const Input& input, const std::vector<BodyState>& bodies) {
    if (!std::isfinite(dt) || dt <= 0.0)
        return;
    dt = std::min(dt, 0.25);
    const double speed = 8.0 * clampd(std::isfinite(input.speed_scale) ? input.speed_scale : 1.0, 0.0, 100.0);

    if (mode_ == CameraMode::Tour && !bodies.empty()) {
        if (!tour_clock_valid_) {
            tour_start_ = time;
            tour_clock_valid_ = true;
        }
        const double tour_time = tour_override_ ? tour_override_time_ : (time - tour_start_);
        const double phase = std::fmod(std::max(0.0, tour_time), 90.0) / 90.0;
        Vec3d p[5], q[5];
        const auto& earth = bodies[0];
        p[0] = earth.position + Vec3d{0, earth.radius * .45, earth.radius * 2.1};
        q[0] = earth.position;
        p[1] = earth.position + Vec3d{4.4, -1.0, -1.8};
        q[1] = earth.position;
        const auto& giant = bodies[std::min<std::size_t>(1, bodies.size() - 1)];
        p[2] = giant.position + Vec3d{-giant.radius * .6, giant.radius * .25, giant.radius * 2.2};
        q[2] = giant.position;
        const auto& moon = bodies[std::min<std::size_t>(2, bodies.size() - 1)];
        p[3] = moon.position + Vec3d{moon.radius, .4 * moon.radius, 2 * moon.radius};
        q[3] = moon.position;
        p[4] = giant.position + Vec3d{45, -8, 22};
        q[4] = giant.position;
        const double scaled = phase * 5.0, segf = std::floor(scaled);
        const int seg = static_cast<int>(segf) % 5;
        const double local = scaled - segf;
        const int a = (seg + 4) % 5, b = seg, c = (seg + 1) % 5, d = (seg + 2) % 5;
        position = catmull(p[a], p[b], p[c], p[d], local);
        forward_ = normalized(catmull(q[a], q[b], q[c], q[d], local) - position);
        rebuild_basis();
        collision_clamp(bodies);
        return;
    }

    if (mode_ == CameraMode::Orbit && orbit_target_ < bodies.size()) {
        const auto& body = bodies[orbit_target_];
        orbit_zoom_ = std::max(body.radius * 1.08, orbit_zoom_ - input.move_forward * speed * dt);
        orbit_zoom_ = std::max(body.radius * 1.08, orbit_zoom_);
        orbit_yaw_ += input.mouse_dx * 0.0025;
        orbit_pitch_ = clampd(orbit_pitch_ - input.mouse_dy * 0.0025, -1.45, 1.45);
        const double cp = std::cos(orbit_pitch_);
        const Vec3d from{orbit_zoom_ * cp * std::sin(orbit_yaw_), orbit_zoom_ * std::sin(orbit_pitch_),
                         orbit_zoom_ * cp * std::cos(orbit_yaw_)};
        position = body.position + from;
        forward_ = normalized(body.position - position);
    } else {
        const double yaw = input.mouse_dx * 0.0025;
        const double pitch = clampd(-input.mouse_dy * 0.0025, -1.5, 1.5);
        forward_ = normalized(rotate_y(forward_, yaw));
        forward_.y = clampd(forward_.y + pitch, -0.995, 0.995);
        forward_ = normalized(forward_);
        rebuild_basis();
        position = position +
                   (forward_ * input.move_forward + right_ * input.move_right + up_ * input.move_up) * (speed * dt);
    }
    rebuild_basis();
    collision_clamp(bodies);
}

void Camera::set_bookmark(std::size_t index, const std::vector<BodyState>& bodies) {
    if (index >= bookmarks_.size())
        bookmarks_.resize(index + 1);
    if (bookmarks_[index].valid && index < bodies.size()) {
        position = bodies[index].position + bookmarks_[index].offset;
        forward_ = normalized(bodies[index].position - position);
        rebuild_basis();
        return;
    }
    if (index < bodies.size()) {
        const auto& b = bodies[index];
        Vec3d local;
        if (index == 0)
            local = {0.0, b.radius * 0.45, b.radius * 2.1};
        else if (index == 1)
            local = {-b.radius * 0.6, b.radius * 0.25, b.radius * 2.2};
        else
            local = {b.radius, b.radius * 0.4, b.radius * 2.0};
        position = b.position + local;
        forward_ = normalized(b.position - position);
        rebuild_basis();
        bookmarks_[index] = {position - b.position, forward_, true};
    } else if (index == 3 && !bodies.empty()) {
        const auto& b = bodies[0];
        position = b.position + Vec3d{4.8, -0.2, 0.8};
        forward_ = normalized(b.position - position);
        rebuild_basis();
        bookmarks_[index] = {position - b.position, forward_, true};
    } else if (index == 4 && bodies.size() > 1) {
        const auto& b = bodies[1];
        position = b.position + Vec3d{45.0, -8.0, 22.0};
        forward_ = normalized(b.position - position);
        rebuild_basis();
        bookmarks_[index] = {position - b.position, forward_, true};
    }
}

void Camera::toggle_tour() {
    if (mode_ == CameraMode::Tour) {
        mode_ = CameraMode::Free;
        tour_clock_valid_ = false;
        tour_override_ = false;
    } else {
        mode_ = CameraMode::Tour;
        tour_clock_valid_ = false;
        tour_override_ = false;
    }
}

void Camera::look_at(Vec3d eye, Vec3d target) {
    position = eye;
    forward_ = normalized(target - eye);
    rebuild_basis();
}

void Camera::set_tour_time(double seconds) {
    if (!std::isfinite(seconds))
        seconds = 0.0;
    tour_override_time_ = seconds;
    tour_override_ = true;
    tour_clock_valid_ = true;
    mode_ = CameraMode::Tour;
}

void Camera::set_orbit_target(std::size_t index, double zoom) {
    orbit_target_ = index;
    if (zoom > 0.0)
        orbit_zoom_ = zoom;
    mode_ = CameraMode::Orbit;
    orbit_yaw_ = 0.0;
    orbit_pitch_ = 0.35;
}
void Camera::set_mode(CameraMode mode) {
    mode_ = mode;
    if (mode != CameraMode::Tour)
        tour_clock_valid_ = false;
}
} // namespace space
