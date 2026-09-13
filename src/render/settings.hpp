#pragma once

#include <algorithm>
#include <array>

namespace space::render {

// Shared defaults: the application owns these values and passes them to each frame.

// Tone and spatial-AA values match the shader modes and numeric CLI options.
enum class ToneCurve : unsigned { ACES = 0, AgX = 1, PbrNeutral = 2, Count };
enum class SpatialAA : unsigned { Off = 0, FXAA = 1, SMAA = 2, Count };
enum class SplatMode : unsigned { Off = 0, Pixels1_2 = 1, Pixels2_5 = 2, Pixels4 = 3, Count };
enum class GalaxyResolution : unsigned { Full = 1, Half = 2, Quarter = 4 };

struct ToneSettings {
    float exposure = 1; // manual exposure multiplier
    bool auto_exposure = true;
    ToneCurve tone_curve = ToneCurve::PbrNeutral; // 0 ACES filmic, 1 AgX, 2 Khronos PBR Neutral (F8)
};

struct AntiAliasingSettings {
    bool temporal_aa = true;                // temporal anti-aliasing (F5)
    SpatialAA spatial_aa = SpatialAA::SMAA; // spatial pass over the tone-mapped image: 0 off, 1 FXAA, 2 SMAA (F9)
};

struct BeltSettings {
    bool light_map = true;  // rock-on-rock transmittance map (F3)
    bool extinction = true; // analytic belt dust extinction (F4)
    bool disc = true;       // fade the belt to its baked disc at a distance; off keeps full detail everywhere
    float lod_scale = 1;    // multiplies the distance at which the belt fades to its baked disc
    static constexpr std::array<float, 4> splat_radii{0.f, 1.2f, 2.5f, 4.f};
    SplatMode splat_mode = SplatMode::Pixels2_5; // index into splat_radii; shared by F6 and the panel
    bool splat_light_twice = false;              // light splats in the count pass too, for comparison (F7)

    constexpr float billboard_radius() const {
        return splat_radii[std::min<std::size_t>(unsigned(splat_mode), splat_radii.size() - 1)];
    }
};

struct BeltDustSettings {
    bool enabled = true; // volumetric belt dust scattering (F11)
    float density = 1, brightness = 1,
          far = 1;             // multipliers on the dust shader's extinction, albedo and far-view scale
    float saturation = 1;      // 0 grey, 1 the tinted colour, above exaggerates it
    float tint[3] = {1, 1, 1}; // multiplies the dust colour
};

struct EarthSettings {
    float ocean_roughness = .18f;       // GGX roughness of the sea; Cox-Munk moderate wind
    float glint_intensity = 1.f;        // multiplies the sea's specular
    float sea_patchiness = .5f;         // wind-field modulation of the roughness, 0 even
    float cloud_shadow = .6f;           // how dark clouds shade the ground
    float cloud_shadow_softness = 1.5f; // mips of extra blur on the shadow
    float cloud_opacity = .88f;         // the cloud layer's peak alpha
};

struct SunSettings {
    float glare_intensity = 1.f;    // the glow around the sun
    float ghost_strength = 1.f;     // ghost images down the lens axis
    float starburst_strength = 1.f; // aperture streaks through the sun
    int starburst_blades = 6;       // streak count, 0 for none
    float sun_disc_radius = .007f;  // angular radius of the solar disc, radians
    float sun_limb_darkening = .6f; // edge darkening of the disc, 0 flat
};

struct PostSettings {
    bool bloom = true;            // disabling bloom retains its intensity for re-enabling
    float bloom_intensity = .24f; // halo added back, 0 skips the bloom passes
    float bloom_threshold = .85f; // linear brightness where the halo starts
    float bloom_knee = .5f;       // width of the soft threshold band
    float aberration = 1.f;       // chromatic aberration scale
    float vignette = .17f;        // corner darkening
    float grain = .010f;          // film grain amplitude in display space
    float black_offset = .3f;     // the neutral tone curve's flare subtraction, 1 as published, 0 none
    bool motion_streaks = true;   // dust motes streaking past the moving camera
    float motion_streak_intensity = 1.f;
};

enum class GalaxyMode { Splats, TextureLayers, OriginalTexture, Count };

struct SkySettings {
    GalaxyMode galaxy_mode = GalaxyMode::Splats;
    float galaxy_low = 1.f, galaxy_clouds = 1.f, galaxy_filaments = 1.f;
    bool catalogue_stars = true;      // the Bright Star Catalogue as points; off keeps the procedural sky
    float star_brightness = 2.f;      // gain on the catalogue fluxes; the display cannot hold the eye's range
    float star_saturation = .5f;      // share of the blackbody chroma shown; the eye sees stars nearly white
    bool milky_way = true;            // the fitted galactic band; off keeps the procedural band
    float milky_way_brightness = .1f; // the band's radiance against the stars; 0.1 on review with the Gaia fit
    float milky_way_contrast =
        1.f; // exponent on the fit's radiance about its unit: above 1 the halo stays faint and the core comes up
    int milky_way_splats =
        4096; // splats drawn, the first n of the cloud (the bake orders them by energy); clamped to the file
    GalaxyResolution galaxy_resolution = GalaxyResolution::Quarter; // the splat pass at the frame over this (1, 2 or
                                                                    // 4); 4 resolves a 0.6 degree splat at 1080p
    float dust_amplitude = 2.f;  // the dust lanes' fBm modulation: amplitude (0 none), ...
    float dust_scale = 1.4f;     // ... base feature size in degrees, ...
    float dust_lacunarity = 3.f; // ... frequency step per octave, ...
    float dust_gain = .7f;       // ... amplitude step per octave; three octaves
};

struct GasSettings {
    bool flow = true;           // off holds the cloud deck still
    float time_scale = 1500.f;  // wind speed, times real
    float cycle = 12.f;         // seconds per advection phase
    float turbulence = 1.f;     // fine roiling detail
    float haze = .08f;          // optical depth of the limb haze at normal incidence
    float terminator = .08f;    // wrap of the deck's lighting past the terminator (cosine units)
    float relief = 6.f;         // exaggeration of the cloud-top slopes
    float lightning_rate = 4.f; // night-side lightning, flashes per second over the planet
    float lightning = 1.f;      // lightning brightness
    bool polar = true;          // baked polar cyclone caps over the map's smeared poles
    float cap_size = 1.5f;      // scale of the caps on the sphere; 1 is Juno's measured size
    float cap_blend = .5f;      // crossfade into the map, as a fraction of the cap's radius
    float cap_opacity = 1.f;    // how fully the cap replaces the map
    bool layers = true;         // the zones' upper deck drawn with parallax over the belts
    float layer_lift = .0015f;  // parallax lift of the zones' upper deck, texture units at a 45 degree view
    float layer_shadow = .3f;   // how much the upper deck shades the belts below it
    bool streaks = true;        // per-pixel wind streaks where the filament map runs out of texels
    float streak_strength = 1.f;
};

} // namespace space::render
