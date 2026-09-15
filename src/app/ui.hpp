#pragma once
#include "app/frame_history.hpp"
#include "app/stats_smoothing.hpp"

#include <cstdint>
#include <span>

struct ImDrawData;

namespace space::platform {
class Window;
}

namespace space::render {
struct Stats;
}

namespace space::app {
struct AppState;

// Dear ImGui over the platform window: input arrives through the window's
// message hook, the draw lists go to the renderer with the frame.
class Ui {
public:
    explicit Ui(platform::Window& window);
    ~Ui();
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    struct FontAtlas {
        const std::uint8_t* rgba = nullptr; // width * height * 4 bytes, owned by the context
        unsigned width = 0, height = 0;
    };
    FontAtlas font_atlas() const;

    void begin_frame();
    const ImDrawData* end_frame(); // empty when nothing was drawn this frame
    bool wants_keyboard() const;   // a text field has focus: flight keys stay idle
    bool wants_mouse() const;

private:
    platform::Window& window_;
};

// The control panel, docked to the right edge: frame statistics and every toggle the hotkeys reach.
// stats is the renderer's (exposure, memory, output); the frame readings shown are the smoothed ones.
void draw_panel(AppState& app, const render::Stats& stats, const SmoothedStats& smoothed, const FrameHistory& history);

// While Freeze culling holds the cull camera, its frustum drawn as lines over the
// scene through the live camera, so the cut the freeze holds is visible from outside.
void draw_cull_frustum(const AppState& app, const render::Stats& stats);

} // namespace space::app
