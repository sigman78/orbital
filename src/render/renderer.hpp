#pragma once
#include "app/camera.hpp"
#include "scene/system.hpp"
#include <filesystem>
#include <memory>
#include <span>

struct ImDrawData;

namespace space::render {

struct Stats {
    float frame_ms = 0, gpu_ms = 0, shadow_ms = 0, surface_ms = 0, atmosphere_ms = 0,
          post_ms = 0;    // shadow_ms includes the belt culling passes
    float prepare_ms = 0; // CPU work between acquiring the swapchain image and submitting
    unsigned visible_asteroids = 0, triangles = 0,
             rock_triangles = 0;    // rock figures are from the previous frame's culling
    unsigned draw_calls = 0;        // API draw calls submitted this frame (an indirect multi-draw counts once)
    unsigned rock_groups_drawn = 0; // non-empty rock groups inside the multi-draw, from the previous frame
    float belt_lod = 0;             // far-belt blend weight this frame: 0 full detail, 1 baked disc
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
    bool belt_dust = true;       // volumetric belt dust scattering (F11)
    float dust_density = 1, dust_brightness = 1,
          dust_far = 1;             // multipliers on the dust shader's extinction, albedo and far-view scale
    float dust_saturation = 1;      // 0 grey, 1 the tinted colour, above exaggerates it
    float dust_tint[3] = {1, 1, 1}; // multiplies the dust colour
    bool belt_disc = true;          // fade the belt to its baked disc at a distance; off keeps full detail everywhere
    float belt_lod_scale = 1;       // multiplies the distance at which the belt fades to its baked disc
    bool temporal_aa = true;        // temporal anti-aliasing (F5)
    unsigned spatial_aa = 2;        // spatial pass over the tone-mapped image: 0 off, 1 FXAA, 2 SMAA (F9)
    float billboard_radius = 2.5f;  // rocks below this projected radius in pixels draw as disc splats; 0 never (F6)
    bool splat_light_twice = false; // light splats in the count pass too, for comparison (F7)
    unsigned tone_curve = 2;        // 0 ACES filmic, 1 AgX, 2 Khronos PBR Neutral (F8)
    const ImDrawData* ui = nullptr; // Dear ImGui draw lists, drawn over the presented frame
    // Earth look (panel): sea state and cloud shadows.
    float ocean_roughness = .18f;       // GGX roughness of the sea; Cox-Munk moderate wind
    float glint_intensity = 1.f;        // multiplies the sea's specular
    float sea_patchiness = .5f;         // wind-field modulation of the roughness, 0 even
    float cloud_shadow = .6f;           // how dark clouds shade the ground
    float cloud_shadow_softness = 1.5f; // mips of extra blur on the shadow
    float cloud_opacity = .88f;         // the cloud layer's peak alpha
    // Sun and lens (panel).
    float glare_intensity = 1.f;    // the glow around the sun
    float ghost_strength = 1.f;     // ghost images down the lens axis
    float starburst_strength = 1.f; // aperture streaks through the sun
    unsigned starburst_blades = 6;  // streak count, 0 for none
    float sun_disc_radius = .007f;  // angular radius of the solar disc, radians
    float sun_limb_darkening = .6f; // edge darkening of the disc, 0 flat
    // Post effects (panel).
    float bloom_intensity = .24f; // halo added back, 0 skips the bloom passes
    float bloom_threshold = .85f; // linear brightness where the halo starts
    float bloom_knee = .5f;       // width of the soft threshold band
    float aberration = 1.f;       // chromatic aberration scale
    float vignette = .17f;        // corner darkening
    float grain = .010f;          // film grain amplitude in display space
    // Gas giant (panel): flow-map advection of the cloud deck.
    bool gas_flow = true;          // off falls back to the noise warp
    float gas_time_scale = 1500.f; // wind speed, times real
    float gas_cycle = 12.f;        // seconds per advection phase
    float gas_turbulence = 1.f;    // fine roiling detail
    float gas_haze = .08f;         // optical depth of the limb haze at normal incidence
    float gas_terminator = .08f;   // wrap of the deck's lighting past the terminator (cosine units)
    float gas_relief = 6.f;        // exaggeration of the cloud-top slopes
    float gas_lightning_rate = 4.f; // night-side lightning, flashes per second over the planet
    float gas_lightning = 1.f;      // lightning brightness
    bool gas_polar = true;          // baked polar cyclone caps over the map's smeared poles
    float gas_cap_size = 1.5f;      // scale of the caps on the sphere; 1 is Juno's measured size
    float gas_cap_blend = .5f;      // crossfade into the map, as a fraction of the cap's radius
    float gas_cap_opacity = 1.f;    // how fully the cap replaces the map
    bool gas_layers = false;        // the zones' upper deck drawn with parallax over the belts
    float gas_layer_lift = .0015f;  // parallax lift of the zones' upper deck, texture units at a 45 degree view
    float gas_layer_shadow = .3f;   // how much the upper deck shades the belts below it
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
    // FIFO presentation on, or unsynchronized (mailbox where available) off; measurements want it off.
    void set_vsync(bool vsync);
    Stats stats() const;
    // The Dear ImGui font atlas, RGBA8; uploaded once, before the first frame that draws the overlay.
    void set_ui_font(const std::uint8_t* rgba, unsigned width, unsigned height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace space::render
