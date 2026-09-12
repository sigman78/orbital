#include "render/ui_scissor.hpp"

#include <cassert>

int main() {
    using space::render::ui_scissor;
    // A right-hand panel in a 960x540 logical window must remain visible at 2x Retina scale.
    const auto retina = ui_scissor({600, 0, 960, 540}, {0, 0}, {2, 2}, {1920, 1080});
    assert(retina && retina->x == 1200 && retina->y == 0 && retina->width == 720 && retina->height == 1080);
    const auto standard = ui_scissor({600, 0, 960, 540}, {0, 0}, {1, 1}, {960, 540});
    assert(standard && standard->x == 600 && standard->width == 360 && standard->height == 540);
    // Subtract the viewport origin before applying independent X/Y scales.
    const auto offset = ui_scissor({110, 220, 150, 260}, {100, 200}, {1.5f, 2}, {960, 540});
    assert(offset && offset->x == 15 && offset->y == 40 && offset->width == 60 && offset->height == 80);
    const auto clamped = ui_scissor({-10, -20, 1000, 600}, {0, 0}, {2, 2}, {1920, 1080});
    assert(clamped && clamped->x == 0 && clamped->y == 0 && clamped->width == 1920 && clamped->height == 1080);
    assert(!ui_scissor({970, 0, 1000, 20}, {0, 0}, {2, 2}, {1920, 1080}));
    assert(!ui_scissor({-10, -10, -1, -1}, {0, 0}, {2, 2}, {1920, 1080}));
    assert(!ui_scissor({10, 10, 10, 20}, {0, 0}, {2, 2}, {1920, 1080}));
    return 0;
}
