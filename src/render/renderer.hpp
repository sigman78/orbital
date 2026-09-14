#pragma once
#include "render/camera_view.hpp"
#include "render/settings.hpp"
#include <filesystem>
#include <memory>
#include <span>

struct ImDrawData;
namespace space {
struct BodyState;
struct SystemDescription;
} // namespace space
namespace space::assets {
class ImageView;
}

namespace space::render {

struct ExposureStats {
    float luminance = 0, peak_luminance = 0;
    float target = 1, adapted = 1, applied = 1;
    bool ready = false, has_samples = false, limited = false, automatic = false;
};

struct Stats {
    ExposureStats exposure;
    float frame_ms = 0, gpu_ms = 0, shadow_ms = 0, surface_ms = 0, atmosphere_ms = 0,
          post_ms = 0; // shadow_ms includes the belt culling passes
    float cull_ms = 0, body_shadow_ms = 0, belt_light_ms = 0, belt_disc_ms = 0;
    float prepare_ms = 0; // CPU work between acquiring the swapchain image and submitting
    unsigned visible_asteroids = 0, triangles = 0,
             rock_triangles = 0;    // rock figures are from the previous frame's culling
    unsigned draw_calls = 0;        // API draw calls submitted this frame (an indirect multi-draw counts once)
    unsigned rock_groups_drawn = 0; // non-empty rock groups inside the multi-draw, from the previous frame
    float belt_lod = 0;             // far-belt blend weight this frame: 0 full detail, 1 baked disc
};

// Camera/settings are copied values; body and UI storage is borrowed for draw().
struct FrameInput {
    CameraView camera;
    std::span<const BodyState> bodies; // exactly one state per configured body ID; any order
    double time = 0;                   // simulation seconds; drives belt spin and rock rotation
    bool high_quality = false;
    bool overlay = true;
    const ImDrawData* ui = nullptr; // drawn over the presented frame
    ToneSettings tone;
    AntiAliasingSettings aa;
    BeltSettings belt;
    BeltDustSettings belt_dust;
    EarthSettings earth;
    SunSettings sun;
    PostSettings post;
    SkySettings sky;
    GasSettings gas;
};

// Startup choices that are not part of the scene description.
struct RendererConfig {
    unsigned belt_count = 0; // rocks generated and drawn regardless of quality tier; 0 keeps the tiers
};

class Renderer {
public:
    // HUD pixels are copied/uploaded during construction; their storage is not retained.
    Renderer(void* window, const SystemDescription& system, const std::filesystem::path& directory,
             assets::ImageView hud, const RendererConfig& config = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // False when there is no drawable surface (minimized) or no swapchain image.
    bool draw(const FrameInput& input);
    // Saves the last rendered frame as PNG; false (after logging) if it could not be written.
    bool capture(const std::filesystem::path& path);
    // FIFO presentation on, or unsynchronized (mailbox where available) off; measurements want it off.
    void set_vsync(bool vsync);
    Stats stats() const;
    // The Dear ImGui font atlas, RGBA8; uploaded once, before the first frame that draws the overlay.
    void set_ui_font(assets::ImageView image);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace space::render
