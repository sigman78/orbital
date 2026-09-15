#include "app/camera.hpp"
#include "scene/system.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace space {
namespace {

namespace settings {
inline constexpr double base_speed = 8.0;               // scene units per second at speed_scale 1
inline constexpr Range<double> speed_scale{0.0, 100.0}; // guards against a runaway scale from input
inline constexpr double max_step_seconds = 0.25;        // a long stall must not teleport the camera
inline constexpr double mouse_sensitivity = 0.0025;     // radians per pixel, divided by the zoom
namespace telescope {
inline constexpr double magnification = 5.0; // the cap, reached while the middle button is held
inline constexpr double in_seconds = 0.105,
                        out_seconds = 0.17; // from 1x to the cap, and from the cap back to 1x, at steady rates
} // namespace telescope
inline constexpr double collision_margin = 1.08; // camera stays outside body radius * margin
inline constexpr double basis_epsilon = 1e-8;
namespace orbit {
inline constexpr Range<double> pitch{-1.45, 1.45}; // radians, keeps the orbit camera off the poles
inline constexpr double initial_pitch = 0.35;
} // namespace orbit
namespace free {
inline constexpr Range<double> pitch_step{-1.5, 1.5};    // per-step pitch change
inline constexpr Range<double> forward_y{-0.995, 0.995}; // avoids a degenerate basis when looking straight up
} // namespace free
namespace tour {
inline constexpr double period_seconds = 90.0;
} // namespace tour
} // namespace settings

// A view anchored to a body: body position + radius-scaled offset + fixed offset.
struct ViewDefinition {
    std::size_t body;
    Vec3d radius_scale;
    Vec3d offset;
};

constexpr ViewDefinition bookmark_views[bookmark_count] = {
    {.body = 0, .radius_scale = {0.0, 0.45, 2.1}, .offset = {}},
    {.body = 1, .radius_scale = {-0.6, 0.25, 2.2}, .offset = {}},
    {.body = 2, .radius_scale = {1.0, 0.4, 2.0}, .offset = {}},
    {.body = 3, .radius_scale = {0.9, 0.35, 2.4}, .offset = {}},
    {.body = 0, .radius_scale = {}, .offset = {4.8, -0.2, 0.8}},
    {.body = 1, .radius_scale = {}, .offset = {45.0, -8.0, 22.0}},
};

// Catmull-Rom waypoints of the tour, visited in order over one period.
constexpr ViewDefinition tour_views[] = {
    {.body = 0, .radius_scale = {0.0, 0.45, 2.1}, .offset = {}},
    {.body = 0, .radius_scale = {}, .offset = {4.4, -1.0, -1.8}},
    {.body = 1, .radius_scale = {-0.6, 0.25, 2.2}, .offset = {}},
    {.body = 2, .radius_scale = {1.0, 0.4, 2.0}, .offset = {}},
    {.body = 3, .radius_scale = {0.9, 0.35, 2.4}, .offset = {}},
    {.body = 1, .radius_scale = {}, .offset = {45.0, -8.0, 22.0}},
};
constexpr std::size_t tour_count = std::size(tour_views);

// Body index clamped so a short body list still yields a usable view.
const BodyState& view_body(const ViewDefinition& view, std::span<const BodyState> bodies) {
    return bodies[std::min(view.body, bodies.size() - 1)];
}

Vec3d view_position(const ViewDefinition& view, const BodyState& body) {
    const double r = body.radius;
    return body.position + Vec3d{view.radius_scale.x * r, view.radius_scale.y * r, view.radius_scale.z * r} +
           view.offset;
}

Vec3d rotate_y(Vec3d p, double a) {
    const double c = std::cos(a), s = std::sin(a);
    return {c * p.x - s * p.z, p.y, s * p.x + c * p.z};
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

void Camera::aim(Vec3d target, double x, double y, double aspect) {
    const Vec3d direction = normalized(target - position);
    const double tan_half = std::tan(vertical_fov * .5);
    // The direction's camera-space coordinates (right, up, forward) once aimed.
    const Vec3d wanted = normalized(Vec3d{x * tan_half * aspect, -y * tan_half, 1.0});
    ++cut_serial_;
    // The basis keeps world up, which rolls the frame a little after each turn,
    // so iterate: every pass turns the forward axis by the remaining error.
    for (int pass = 0; pass < 8; ++pass) {
        const Vec3d current{dot(direction, right_), dot(direction, up_), dot(direction, forward_)};
        const Vec3d axis = cross(wanted, current);
        const double angle = std::acos(std::clamp(dot(wanted, current), -1.0, 1.0));
        if (length(axis) < 1e-12) {
            if (angle < 1e-9)
                break;
            forward_ = forward_ * -1.0; // the direction is exactly behind: turn around first
            rebuild_basis();
            continue;
        }
        // The turn taking `wanted` to `current`, applied to the forward axis, puts the direction at `wanted`.
        const Vec3d turned = rotate_about(Vec3d{0, 0, 1}, axis, angle);
        forward_ = right_ * turned.x + up_ * turned.y + forward_ * turned.z;
        rebuild_basis();
    }
}

void Camera::rebuild_basis() {
    forward_ = normalized(forward_);
    // Keep world Y as the stable up direction except when looking nearly up.
    const Vec3d world_up{0, 1, 0};
    right_ = normalized(cross(forward_, world_up));
    if (length(right_) < settings::basis_epsilon)
        right_ = {1, 0, 0};
    up_ = normalized(cross(right_, forward_));
}

void Camera::collision_clamp(std::span<const BodyState> bodies) {
    for (const auto& body : bodies) {
        const Vec3d delta = position - body.position;
        const double distance = length(delta), minimum = std::max(0.0, body.radius * settings::collision_margin);
        if (minimum > 0.0 && distance < minimum) {
            const Vec3d outward = distance > 1e-9 ? delta * (1.0 / distance) : Vec3d{0, 1, 0};
            position = body.position + outward * minimum;
        }
    }
}

void Camera::step(double dt, double time, const Input& input, std::span<const BodyState> bodies) {
    if (!std::isfinite(dt) || dt <= 0.0)
        return;
    dt = std::min(dt, settings::max_step_seconds);
    // The telescope zooms at a steady rate in log space toward its cap while the
    // button is held, and back to 1x at its own rate once it is released.
    const double log_cap = std::log(settings::telescope::magnification);
    const double step = input.telescope ? log_cap / settings::telescope::in_seconds * dt
                                        : -log_cap / settings::telescope::out_seconds * dt;
    zoom_ = std::exp(std::clamp(std::log(zoom_) + step, 0.0, log_cap));
    const double scale = std::isfinite(input.speed_scale) ? input.speed_scale : 1.0;
    const double speed = settings::base_speed * settings::speed_scale.clamp(scale);
    if (mode_ == CameraMode::Tour && !bodies.empty()) {
        step_tour(time, bodies);
        return;
    }
    if (mode_ == CameraMode::Orbit && orbit_target_ < bodies.size())
        step_orbit(dt, speed, input, bodies[orbit_target_]);
    else
        step_free(dt, speed, input);
    rebuild_basis();
    collision_clamp(bodies);
}

void Camera::step_tour(double time, std::span<const BodyState> bodies) {
    if (!tour_clock_valid_) {
        tour_start_ = time;
        tour_clock_valid_ = true;
    }
    const double period = settings::tour::period_seconds;
    const double tour_time = tour_override_ ? tour_override_time_ : (time - tour_start_);
    const double phase = std::fmod(std::max(0.0, tour_time), period) / period;
    Vec3d eyes[tour_count], targets[tour_count];
    for (std::size_t i = 0; i < tour_count; ++i) {
        const auto& body = view_body(tour_views[i], bodies);
        eyes[i] = view_position(tour_views[i], body);
        targets[i] = body.position;
    }
    const double scaled = phase * double(tour_count), segment_floor = std::floor(scaled);
    const std::size_t segment = static_cast<std::size_t>(segment_floor) % tour_count;
    const double local = scaled - segment_floor;
    const std::size_t a = (segment + tour_count - 1) % tour_count, b = segment, c = (segment + 1) % tour_count,
                      d = (segment + 2) % tour_count;
    position = catmull(eyes[a], eyes[b], eyes[c], eyes[d], local);
    forward_ = normalized(catmull(targets[a], targets[b], targets[c], targets[d], local) - position);
    rebuild_basis();
    collision_clamp(bodies);
}

void Camera::step_orbit(double dt, double speed, const Input& input, const BodyState& body) {
    const double minimum_zoom = body.radius * settings::collision_margin;
    orbit_zoom_ = std::max(minimum_zoom, orbit_zoom_ - input.move_forward * speed * dt);
    orbit_yaw_ += input.mouse_dx * settings::mouse_sensitivity / zoom_;
    orbit_pitch_ = settings::orbit::pitch.clamp(orbit_pitch_ - input.mouse_dy * settings::mouse_sensitivity / zoom_);
    const double cp = std::cos(orbit_pitch_);
    const Vec3d from{orbit_zoom_ * cp * std::sin(orbit_yaw_), orbit_zoom_ * std::sin(orbit_pitch_),
                     orbit_zoom_ * cp * std::cos(orbit_yaw_)};
    position = body.position + from;
    forward_ = normalized(body.position - position);
}

void Camera::step_free(double dt, double speed, const Input& input) {
    const double yaw = input.mouse_dx * settings::mouse_sensitivity / zoom_;
    const double pitch = settings::free::pitch_step.clamp(-input.mouse_dy * settings::mouse_sensitivity / zoom_);
    forward_ = normalized(rotate_y(forward_, yaw));
    forward_.y = settings::free::forward_y.clamp(forward_.y + pitch);
    forward_ = normalized(forward_);
    rebuild_basis();
    position += (forward_ * double(input.move_forward) + right_ * double(input.move_right) +
                 up_ * double(input.move_up)) *
                (speed * dt);
}

std::size_t Camera::bookmark_body(std::size_t index) {
    return index < bookmark_count ? bookmark_views[index].body : 0;
}

void Camera::set_bookmark(std::size_t index, std::span<const BodyState> bodies) {
    if (index >= bookmark_count || bodies.empty())
        return;
    const auto& view = bookmark_views[index];
    if (view.body >= bodies.size())
        return;
    ++cut_serial_;
    const auto& body = bodies[view.body];
    if (bookmarks_[index].valid) {
        // Re-selecting a bookmark keeps the body-relative offset from the first visit.
        position = body.position + bookmarks_[index].offset;
    } else {
        position = view_position(view, body);
    }
    forward_ = normalized(body.position - position);
    rebuild_basis();
    bookmarks_[index] = {.offset = position - body.position, .forward = forward_, .valid = true};
}

void Camera::toggle_tour() {
    ++cut_serial_;
    mode_ = mode_ == CameraMode::Tour ? CameraMode::Free : CameraMode::Tour;
    tour_clock_valid_ = false;
    tour_override_ = false;
}

void Camera::look_at(Vec3d eye, Vec3d target) {
    ++cut_serial_;
    position = eye;
    forward_ = normalized(target - eye);
    rebuild_basis();
}

void Camera::set_tour_time(double seconds) {
    ++cut_serial_;
    if (!std::isfinite(seconds))
        seconds = 0.0;
    tour_override_time_ = seconds;
    tour_override_ = true;
    tour_clock_valid_ = true;
    mode_ = CameraMode::Tour;
}

void Camera::set_orbit_target(std::size_t index, double zoom) {
    ++cut_serial_;
    orbit_target_ = index;
    if (zoom > 0.0)
        orbit_zoom_ = zoom;
    mode_ = CameraMode::Orbit;
    orbit_yaw_ = 0.0;
    orbit_pitch_ = settings::orbit::initial_pitch;
}

void Camera::set_mode(CameraMode mode) {
    if (mode != mode_)
        ++cut_serial_;
    mode_ = mode;
    if (mode != CameraMode::Tour)
        tour_clock_valid_ = false;
}

} // namespace space
