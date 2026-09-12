#pragma once
#include "app/camera.hpp"
#include "scene/system.hpp"
#include <filesystem>
#include <memory>
#include <span>

namespace space::render {

struct Stats {
    float frame_ms = 0, gpu_ms = 0, shadow_ms = 0, surface_ms = 0, atmosphere_ms = 0,
          post_ms = 0;    // shadow_ms includes the belt culling passes
    float prepare_ms = 0; // CPU work between acquiring the swapchain image and submitting
    unsigned visible_asteroids = 0, triangles = 0,
             rock_triangles = 0;    // rock figures are from the previous frame's culling
    unsigned draw_calls = 0;        // API draw calls submitted this frame (an indirect multi-draw counts once)
    unsigned rock_groups_drawn = 0; // non-empty rock groups inside the multi-draw, from the previous frame
};

// Everything the renderer needs for one frame; owned by the caller.
struct FrameInput {
    const Camera& camera;
    std::span<const BodyState> bodies; // at least the three major bodies, in system order
    double time = 0;                   // simulation seconds; drives belt spin and rock rotation
    float exposure = 1;                // manual exposure multiplier
    bool high_quality = false;
    bool overlay = true;
    bool auto_exposure = true;
    bool belt_light_map = true;  // rock-on-rock transmittance map (F3)
    bool belt_extinction = true; // analytic belt dust extinction (F4)
    unsigned anti_aliasing = 2;  // 0 off, 1 temporal, 2 temporal plus FXAA (F5)
};

// Startup choices that are not part of the scene description.
struct RendererConfig {
    unsigned belt_count = 0; // rocks generated and drawn regardless of quality tier; 0 keeps the tiers
};

class Renderer {
public:
    Renderer(void* window, const SystemDescription& system, const std::filesystem::path& directory,
             const RendererConfig& config = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // False when there is no drawable surface (minimized) or no swapchain image.
    bool draw(const FrameInput& input);
    // Saves the last rendered frame as PNG; false (after logging) if it could not be written.
    bool capture(const std::filesystem::path& path);
    Stats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace space::render
