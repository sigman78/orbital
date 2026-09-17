#pragma once
#include "render/camera_view.hpp"
#include "render/gpu_pass.hpp"
#include "render/settings.hpp"
#include <cstdint>
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
    // The meter's last histogram: each bin's share of the weight, over log2 luminance
    // from stops_min across stops_range, for the panel's plot.
    static constexpr unsigned histogram_bins = 64;
    float histogram[histogram_bins] = {};
    float stops_min = -14.f, stops_range = 20.f;
};

// GPU memory by resource type, for the panel: the allocations the renderer owns,
// summed as their heaps report them (device memory for targets and materials, the
// host-visible heaps for the mapped data, and the readback heaps).
struct MemoryPool {
    std::uint64_t bytes = 0; // allocated
    std::uint64_t used = 0;  // the part in use where a heap is suballocated, else equal to bytes
    unsigned count = 0;      // allocations in the pool
};
struct MemoryStats {
    MemoryPool frame_targets;  // resized with the window
    MemoryPool fixed_targets;  // shadow, belt and disc maps
    MemoryPool materials;      // uploaded textures
    MemoryPool static_data;    // the device-only heap of meshes, rock records and sky tables
    MemoryPool mapped;         // the host-visible data heap: the per-frame region and the UI
    MemoryPool device_buffers; // culling scratch and the exposure histogram
    MemoryPool readback;       // CPU-visible copies and the descriptor heaps
    std::uint64_t total() const {
        return frame_targets.bytes + fixed_targets.bytes + materials.bytes + static_data.bytes + mapped.bytes +
               device_buffers.bytes + readback.bytes;
    }
};

// What one call to draw() cost and drew.
struct FrameStats {
    float draw_ms = 0;    // CPU time of draw(), including the wait for the previous frame
    float prepare_ms = 0; // CPU work between acquiring the swapchain image and submitting
    float belt_ms = 0;    // the rock sweep and frustum pass, done before the wait so it overlaps the GPU
    PassTimings gpu;      // the GPU passes of the last completed frame, by GpuPass
    unsigned visible_asteroids = 0, triangles = 0,
             rock_triangles = 0;    // rock figures are from the previous frame's culling
    unsigned rock_candidates = 0;   // rocks the CPU frustum pass handed this frame's GPU cull
    unsigned draw_calls = 0;        // API draw calls submitted this frame (an indirect multi-draw counts once)
    unsigned bodies_drawn = 0;      // bodies inside the view frustum this frame
    unsigned rock_groups_drawn = 0; // non-empty rock groups inside the multi-draw, from the previous frame
    float belt_lod = 0;             // far-belt blend weight this frame: 0 full detail, 1 baked disc
};

// What the swapchain presents, refreshed when the output mode changes.
struct OutputStatus {
    HdrOutput hdr_output = HdrOutput::Off; // what the swapchain presents
    bool hdr_unsupported = false;          // the requested HDR output is not offered by the surface
    bool hdr_metadata = false;             // the device can pass mastering metadata to the display
};

// The renderer's readings: the frame and the memory sums every draw, the
// exposure as the meter cycles, the output status as the mode changes.
struct Stats {
    FrameStats frame;
    ExposureStats exposure;
    MemoryStats memory;
    OutputStatus output;
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
    DisplaySettings display;
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
    const Stats& stats() const;
    // The Dear ImGui font atlas, RGBA8; uploaded once, before the first frame that draws the overlay.
    void set_ui_font(assets::ImageView image);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace space::render
