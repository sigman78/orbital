#pragma once
#include "core/math.hpp"
#include <cstddef>

namespace space::render {

// A value snapshot for one frame. Navigation, bookmarks and orbit state stay in app.
// The basis is supplied unchanged; rendering does not rebuild or normalize it.
struct CameraView {
    Vec3d position{};
    Vec3d forward{0, 0, -1}, right{1, 0, 0}, up{0, 1, 0};
    double vertical_fov = pi<double> / 3;
    std::size_t cut_serial = 0; // explicit discontinuities invalidate temporal history
};

} // namespace space::render
