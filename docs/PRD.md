# Product requirements: procedural space demo

## Objective

Deliver an impressive native Windows desktop 3D demo built on Sebastian Aaltonen's [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI). The demo presents convincing procedural planets and smaller celestial bodies through cinematic lighting and an explorable camera. Use modern C++, CMake, and MSVC.

The primary target is RTX 4080. GTX 1080 Ti is the baseline compatibility target. Both must render the same core scene; quality settings may reduce cost on the baseline GPU.

## Experience

Launch into a deliberately composed view of an Earth-like planet, with a gas giant and asteroid belt contributing depth and scale. An optional repeatable camera tour demonstrates atmospheric sunrise, the terrestrial terminator, ocean reflections, gas-giant storms, and the belt. Users can interrupt the tour, fly freely, orbit a selected planet, and return to bookmarked views.

The scene should feel physically plausible, while artistic distances and body sizes make several bodies visible together. It is not an astronomically scaled orbital simulator. Avoid excessive star brightness, neon atmospheres, uniformly noisy terrain, and lens effects that overwhelm the planets.

## Required scope

| Area | Required behavior |
| --- | --- |
| Earth-like planet | Seeded continents, oceans, terrain variation, polar ice, material roughness, ocean sun glints, atmosphere, moving clouds and cloud shadows |
| Gas giant | Seeded coherent bands, turbulence and localized storms, rotation, convincing terminator and atmospheric limb |
| Smaller bodies | At least one rocky moon and an asteroid belt containing thousands of instanced bodies with shape and rotation variation |
| Lighting | Shared sun direction/position, physically based opaque shading, eclipses, local asteroid shadows and stable day/night transitions |
| HDR | Floating-point lighting, exposure control/adaptation, bloom and tone mapping; SDR display output required |
| Sun and optics | Solar disc, restrained glare and flares with body occlusion; flares vanish when the sun is hidden |
| Space background | Procedural stars and a restrained distant galactic background |
| Interaction | Tour, free-flight, planet orbit, bookmarks, pause, exposure and quality controls, help, performance overlay |
| Desktop behavior | Resizable window, correct minimize/restore, clean exit, actionable startup errors |
| Reproducibility | Fixed default seed, explicit alternate seed, repeatable camera/timing mode for captures |
| System generation | Seeded body hierarchy, composition, orbital/rotation parameters and belt distributions with stable identities and validity checks |
| Asset workflow | Repeatable generation/caching, explicit material-map contracts, and provenance/licenses for any acquired supporting assets |
| Geometry and LOD | Procedural sphere/rock meshes, distance-appropriate detail, conservative bounds and stable transitions |
| Shader workflow | Reproducible stage/variant compilation, include dependency tracking, validation and packaged runtime outputs |

Native HDR monitor output, hardware ray tracing, planetary landings, terrain streaming, orbital physics, multiplayer, an editor, and cross-platform support are outside the initial release. Native HDR output can be proposed after SDR presentation is complete.

## Quality and performance

Provisional budgets, to be confirmed through measurement:

| Tier | Hardware | Resolution | Frame target |
| --- | --- | --- | --- |
| High | RTX 4080 | 2560 x 1440 | 60 FPS / 16.7 ms |
| Baseline | GTX 1080 Ti | 1920 x 1080 | 30 FPS minimum target / 33.3 ms; aim for 60 FPS |

Measure Release builds after shader/resource warmup across the fixed tour and bookmarks. Report median and 95th-percentile CPU/GPU frame times, render resolution, settings, driver, and peak observed GPU memory use. Targets are not verified performance claims. If RTX 4080 hardware is unavailable, explicitly leave that tier unverified.

Scale cloud and atmosphere samples, shadow resolution, asteroid density/detail, bloom resolution, and render scale. Preserve both planets, their defining material features, eclipses and sun occlusion on every tier. Do not depend on hardware ray tracing or mesh shaders for the baseline scene.

## Acceptance

1. A fresh checkout configures and builds using documented CMake presets and the supported MSVC toolchain, with pinned dependencies and shader build steps.
2. A supported execution path exists for both hardware tiers. Unsupported extension/toolchain cases report the exact missing capabilities; there is no silent blank window or assertion-only failure.
3. The full scene runs interactively with all required scope represented. Planet textures are generated from seeds, not substituted stock Earth/Jupiter maps.
4. Captures at fixed bookmarks show detailed lit surfaces, a readable terminator, atmospheric limbs, shadowed clouds/bodies, belt depth and an occluded sun. Inspect seams, poles, aliasing, halos and banding.
5. Exposure and bloom retain planetary detail. Linear/sRGB conversions are applied exactly once at the appropriate boundaries.
6. Minimize/restore, repeated resizing, quality changes, camera switching and a ten-minute tour complete without application crashes or unbounded resource growth.
7. Validation errors attributable to the application are resolved on an environment with the validation layer available. Record missing validation support separately.
8. Handoff includes launch instructions, controls, capability requirements, measured performance, known limitations and representative captures.
