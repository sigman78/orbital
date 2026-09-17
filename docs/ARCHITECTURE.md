# Architecture

How the demo is put together as it stands. The reasoning behind each choice, with the measurements that
drove it, is in [DECISIONS.md](DECISIONS.md); the graphics backend and shader ABI are in
[FOUNDATION.md](FOUNDATION.md); the layering rules are in [CODE_STYLE.md](CODE_STYLE.md).

## Layers

```
core       logging, panic, file I/O, math, small containers
scene      seeded system description, orbital evaluation, sphere and rock geometry
assets     PNG I/O, SIMD pixel kernels, material mip chains
platform   window, input, process, text overlay, Dear ImGui overlay input   (win32/ and linux/ implement them)
render     the NoGraphicsAPI renderer: resources, materials, per-frame passes
app        options, camera, HUD, control panel, frame loop
```

The complete demo builds with MSVC on Windows and GCC on Linux. The Linux backend uses SDL2 for
Wayland/X11 windows and input, and Fontconfig/FreeType for HUD text. The renderer never sees an OS type; the application never sees a Vulkan type.

## Scene

`scene::generate_system` builds a deterministic description from a seed: a star, an Earth-like world, a gas
giant with an asteroid belt, a rocky moon, a desert world with two tidally locked moonlets, and a small
airless minor planet whose surface is generated rather than mapped. Bodies carry
stable ids and independent derived seeds; orbits and rotations are evaluated directly at a time, so a fixed
time reproduces a snapshot. Distances are deliberately compressed so several bodies share a frame.

Positions are double precision and converted to camera-relative floats each frame. Spheres come in four subdivision levels selected by projected size, and the same size sets a per-draw detail weight the body shaders fade their detail terms by (the Earth's normal map and cloud swirl, the gas giant's deck parallax and relief, the Moon's and Mars's normal map and height trace), and whether the Earth's cloud shell is drawn or folded into the ground pass. The belt draws from a library of 16 seeded rock shapes at 6
levels (20 to 20k triangles), packed into one pooled vertex and index range so every level of every shape is
a slice of the same buffers.

The renderer's CPU-only `Showcase` binding is stricter than generic scene validation: exactly one
terrestrial, gas giant, desert and rocky moon, optional moonlets within the eight-body limit, and one
positive-thickness belt whose parent ID is the gas giant. It resolves these roles once before GPU setup.
At draw entry, states are checked and copied by ID into description order using bounded stack storage;
missing, duplicate, unknown or non-finite states are rejected. Materials, meshes, atmosphere anchors and
belt placement therefore use the same body slots even when incoming states are reordered.

Some description fields remain metadata: `scale_policy` is not applied as a multiplier;
`atmosphere_scale` does not override the effect settings; star radius, temperature and intensity do not
control the artistic sun or lighting. Star position does drive lighting. Moonlet geometry consumes
`material_seed`; installed planetary maps remain assigned by body class. Giving the metadata rendering
semantics is a separate visual change. App bookmarks still target the generated showcase's ordering.

Frame preparation separates CPU geometry from shader packing. `frame_calculations` returns camera projection/history decisions, motion lattice coordinates, light boxes, belt LOD and sun visibility through typed values. `renderer_frame_data.cpp` packs these into the unchanged shader ABI alongside effect settings and resource availability. The calculation library has no backend or shader-header dependency and is tested in both MSVC and GCC CPU builds.

## Belt

The belt is a population rather than a mesh list: `geometry` places 280k (baseline) or 520k (high) rocks in
an annulus with rings, a gap, tapered edges and flared height, a power-law size distribution and three
composition classes. The CPU owns the rocks as two arrays in `scene/belt_motion.cpp`: the seeds that never
change (centre in the belt frame, unit tumble axis and rate, radius, band, shape) and the states it steps,
float4 (x, y, z, phase), the position after the band spin and the tumble phase. The GPU keeps a 48-byte
static record per rock written from the seeds once, and reads the state the CPU writes every frame; the
reader applies the fixed tilt and the phase along the record's axis (`belt/belt_frame.slang`). Every rock
turns about the belt's axis at its band's rate and tumbles about its own at its own, so a step is one
rotation per band and one phase increment per rock, applied to every rock in one plain loop. Time
advances in fixed steps of 1/120 s; a frame takes as many whole steps as it covers, a time jump or a gap
past 16 steps re-seeds exactly from the seeds, and a slice of the population is re-seeded every step so
repeated float rotations do not drift in radius. A standing time (fixed captures, benchmarks) writes
nothing after the first frame.

The sweep writes into one of two slots of a host-visible staging heap, chosen by frame parity, before
the frame waits for the previous frame's GPU work, so the write (1.45 ms at 280k rocks, 2.75 at 520k,
most of it the CPU-to-aperture path) overlaps that work instead of adding to the frame; the previous
frame may still be copying the other slot. The frame then copies the slot into one of two device slices
(the other keeps the previous frame's positions, for motion vectors), then a compute shader culls the
whole population in three passes (count, prefix, scatter): frustum,
planet occlusion, projected size, then a level and shape group, or a splat for rocks under the cut-off. The
scatter pass lights each splat once (Lambert sphere at its phase angle, belt shadowing) and writes indirect
draw arguments per group, so the meshes are one multi-draw and the splats one draw. Belt shadowing comes
from three sources: an analytic extinction through the belt's own density along the sun ray, a
transmittance map splatted by the largest rocks from the sun's direction, and the planets' shadows.
Between the rocks a half-resolution march through the same density field scatters sunlight as dust.

## Minor planet

The minor planet is a fifteenth of the Earth's radius, on its own orbit, and the one body whose surface is
made rather than mapped. `scene/terrain.hpp` is the single source of it: `MinorPlanetTerrain(seed)` gives a
height above the reference sphere and a linear-light albedo for any direction from the centre, from
gradient-noise fBm (`core/noise.hpp`), a ridged term and a seeded crater population (a flat floor, a wall
to a raised rim, ejecta, a central peak on the large ones; the fresh ones dark inside and bright around,
a few with a bright facula), within a stated height range, after Ceres and Pluto. At start-up
`bake_terrain_maps` samples it across the cores (`core/parallel.hpp`) into an equirectangular
normal+height map and an albedo map in the layout the Moon and Mars use, so the body draws through the
airless shader (`KIND_MINOR_PLANET`) with the same terrain shadowing and draw tiers, and the atmosphere
pass gives it a faint blue haze after Pluto's, tripled from the physical optical depth to read.

Close in, above a projected radius where the finest sphere level runs out, the body draws as its near
tier instead: a cube sphere of six faces, each a quadtree of 16 by 16 quad patches with skirts
(`scene/terrain_patch.hpp`), split while a patch's projected edge is over a threshold with hysteresis
and foreshortened toward the limb, collapsed out of view. `render/terrain_tier.hpp` keeps the tree and a
cache of 512 vertex slots by patch (least recently used out, the six faces pinned), lists the patches to
draw and asks for up to eight new ones a frame; a patch whose visible children are not all resident draws
itself, so the surface is complete every frame and refines over the following ones. The renderer
(`renderer_terrain.cpp`) generates the requested patches on the CPU before the previous frame's wait
into a staging slot chosen by frame parity, copies them into a device pool at the start of the command
buffer, and draws the listed slots through the surface vertex shader with one shared index buffer, in the
depth pre-pass, the scene pass and the motion pass; the shadow map keeps the sphere. Patch vertices are
the terrain's positions in radii with the sphere's normals, so the maps shade them exactly as the far
tier and the switch is invisible; procedural detail past the maps is the step after.

## Frame

1. Cull passes write the instance list and draw arguments into the dynamic half of the heap.
2. Shadow map (2048, depth only) for the bodies; belt transmittance map (2048, four coverage slices) and its blur.
3. The bodies' depth pre-pass, then the scene pass into the HDR target: the pooled rock multi-draw, bodies, the background and stars depth-tested behind them, then the blended splats and clouds. Bodies outside the view frustum (tested at the cull camera, with their atmosphere shell) skip all of it and their atmosphere pass.
4. Atmospheres, composited per body against depth; then the belt dust, marched at half resolution and
   composited with a depth-aware upsample.
5. Splats write fractional coverage and coverage-weighted depth into metadata; scene depth remains opaque-only.
   TAA uses the reconstructed splat depth for reprojection and currently keeps splat history unclipped.
6. Motion vectors: after the scene pass the rocks and the bodies are drawn again at equal depth (a bias of a
   few units toward the camera, since the pass has its own vertex shader) into an RG16F target, each vertex
   placed where it is and where it was a frame ago from the instance's previous centre and rotation (the
   cull writes a rock's step from the previous state slice as halves, the CPU writes a body's from last
   frame's state) and the previous camera; a triangle at the far plane fills the sky with the camera's
   step. The temporal pass reads the vector at its pixel (not between pixels: a rock's step and the sky's
   behind it differ by the parallax) and reprojects with it; splats keep the camera-only reprojection.
6. Temporal anti-aliasing into a history target, its neighbourhood clip tightening with motion and
   splat history clipped loosely rather than not at all; transient motion streaks into reused HDR storage, added before
   area-prefiltered bloom with adjacent-texel separable blur at quarter resolution;
   one shared 1×1 sun-visibility estimate for lens effects; the soft part of the lens flare stack
   (main ring, crescents, coloured ghosts, streak spindles) into a quarter-resolution target;
   exposure metering as a luminance histogram: a compute pass bins one sixteenth of a four pixel
   tap grid each frame (with the sun's glare added as the composite draws it) into 64 log bins
   accumulated over the cycle, zeroed at its start and read back at its end, so no frame carries
   the whole meter; the CPU takes the weighted mean below the 99.5th percentile plus a tenth of the
   mean of the brightest thousandth and maps the meter key to 1x, scaled in stops by the adaptation
   strength, within -2 to +3 stops; the target moves only when the request differs by a third of a
   stop, and the exposure eases toward it over five seconds up and one down.
7. The composite adds the sun glare, the aperture starburst and the thin axis streak at full
   resolution and samples the flare stack target, all in HDR using the shared sun visibility. The
   stack lies along the axis through the image centre and the projected sun; its major elements
   fade as the sun leaves the frame while the ghosts brighten near the edge and persist while the
   sun is just outside it. Each group has a strength in Sun & lens, and a Lens flare switch skips the
   pass and every element while keeping the sun's glare. Dirty glass (Post FX, off by
   default) blurs the view behind a baked film of dust, wipe residue and smears on a convex pane and
   brightens the film where the sun's direction grazes the pane.
   Tone mapping (PBR Neutral, AgX or ACES filmic) with vignette, chromatic fringe and grain into an
   intermediate, then the spatial pass (SMAA or FXAA) into the final image.
8. Present, with the HUD and the Dear ImGui panel drawn last into the swapchain. The swapchain is
   8-bit sRGB by default; with Output set to scRGB or HDR10 (Tone panel, `--hdr`) it is 16-bit float
   extended linear sRGB or 10-bit PQ, the intermediates become 16-bit float holding the composite's
   linear display value over the headroom (so the spatial pass, grain and dither see SDR values),
   the neutral curve's shoulder extends to the headroom, and the present and overlay passes decode and
   scale by the paper white and encode for the colour space. Captures clip at the curve's white.
   The platform layer reports the display's HDR state, luminance range and SDR white level (DXGI
   and the display configuration on Windows; nothing on Linux), polled once a second: they seed the
   paper white and peak defaults at startup, feed the ST 2086 metadata the swapchain carries, and
   show in the Tone panel.

Scoped GPU timings separately bracket compute culling, body shadows, belt light maps, far-belt bakes, the scene,
atmospheres with dust, and post-processing. The original cull/shadow aggregate is retained in the panel and CSV.

## Image properties

The renders are held to these properties, settled on 2026-09-14 after the contrast and
night-side investigations (see DECISIONS.md for the measurements). Each names where it is
enforced and the control that scales it.

- **Black is black.** With no light source in frame, space and the night sides of bodies
  render at the tone curve's black; nothing adds a constant. The Earth night map's blue
  floor is subtracted and gated in `surface_earth.slang`; the starlight fill on shadow
  sides (surfaces, the cloud layer, the gas giant) follows Sun & lens > Ambient fill,
  default 0.25 of the former constants, 0 for none. City lights and faint cloud cover are
  what a night side shows.
- **The lens is additive.** The sun glare, starburst, streak and the flare stack are light
  the lens adds and never darken the scene. They follow the manual exposure but, by default,
  not the auto exposure (Sun & lens > Follows adaptation), so a sun in frame stopping the
  scene down leaves the flare at its tuned level. The composite passes them to the tone map
  apart from the scene, so the neutral curve's black offset is taken from the scene alone
  (ACES and AgX just sum); the glare's 1/d² tail is cut with exp(-1.5 d) so it does not
  veil the frame once nothing subtracts it. The stack's veils are intended and scale with
  their Sun & lens strengths; the Lens flare switch removes the stack and keeps the glare.
- **The curve is the published PBR Neutral.** Black offset 1.0 by default (Post FX slider
  scales the subtraction); the per-curve exposure trims, the meter key and the adaptation
  range are Tone panel controls. On an HDR output the image below the curve's white is the
  SDR image exactly; only the shoulder above it reaches for the display's peak. The published subtraction also removes faint coloured
  light such as the Milky Way band: the remedy is the auto exposure, which brings a sky-only
  view up three stops and a sun in frame down one to two, never a lifted black.
- **Contrast reference.** The Sep 11 screenshots (`docs/images` at 7c9535b) are the target:
  at 1920x1080 with `--time 0`, the belt view's median is 7 display codes and the Dawn
  night side's 8; a change that moves either by more than a few codes is a regression
  unless it is the point of the change.
- **Review method.** Fixed-time captures per bookmark (`--time 0 --frames 90 --no-hud`),
  the sun placed with `--sun-at` for lens review, and percentile statistics of the
  captures rather than eyeballing, with a stretched crop for the dark regions.

## Renderer implementation

`Renderer::Impl` owns the device, heaps, images and pipeline lifetime registry. The implementation
is split by responsibility. Move-only `GpuImage` values own their texture allocation and
attachment view; pass files borrow handles from frame-sized and fixed-size target groups.
Pass ordering and synchronization remain explicit.

| File in `src/render/` | Responsibility |
| --- | --- |
| `renderer_frame.cpp` | Acquire, prepare, ordered pass calls, timestamps, submit and capture |
| `renderer_frame_data.cpp` | Camera/light transforms, frame constants, body instances and culling inputs |
| `renderer_pipelines.cpp` | Shader loading and pipeline creation, grouped into scene, belt, post and overlay |
| `renderer_assets.cpp` | Mesh packing, material decode batches and sky catalogues |
| `renderer_resources.cpp` | Device/heaps, bounded uploads and target-group reconciliation |
| `gpu_image.hpp` / `gpu_image.cpp` | Move-only image ownership and frame/fixed target groups |
| `renderer_belt.cpp` | Rock population, GPU culling, indirect rock batch, light/disc maps, splat mask and dust |
| `renderer_scene.cpp` | Shadow, galaxy, bodies/clouds, atmospheres and motion streaks |
| `renderer_terrain.cpp` | The minor planet's near tier: patch generation, staging, pool copies and draws |
| `renderer_post.cpp` | Temporal resolve, bloom, tone mapping, spatial AA, metering and presentation |
| `renderer_overlay.cpp` | ImGui font upload and draw-list packing |

The frame function retains pass order and timestamp boundaries. Pass functions retain the barriers
needed by their consumers; the splat-mask depth transitions remain visible in the frame sequence
because that pass must follow atmospheres and precede temporal resolve. `draw_rock_batch` records
inside the scene pass, and `record_ui` records inside the presentation pass. Helpers taking `Root&`
explicitly preserve the push-constant updates used by subsequent draws.

## Resources

`FrameTargets` owns the images replaced on resize; `FixedTargets` owns window-independent
maps and metering targets. Material/font images have one owning vector. Each `GpuImage`
releases its attachment view before its texture and allocation; copying is forbidden,
and moving transfers ownership while clearing the source. Handle accessors are borrowed.
The renderer waits for GPU completion before resetting target groups and clears all
image owners before destroying the device. RAII does not perform implicit GPU waits.

Render passes use `RenderPassScope`; the closing brace ends rendering before subsequent
barriers. `SubmissionTimeline` owns the semaphore and increments completion values only
on explicit submit/present calls. The upload flush helper explicitly submits and waits before
reusing staging bytes. Move-only heap owners form one resettable buffer group; pipeline owners
are held in one vector, while pass-family handles remain borrowed. Shutdown waits idle, clears
all resource groups, resets the timeline, and then destroys the device.

`gpu_sync.hpp` names recurring stage/access pairs. These are global execution/memory
dependencies, not resource-state tracking. Unusual compute/combined-stage barriers remain
explicit, and no pass destructor inserts a barrier.


Static meshes, rock records and sky tables fill an 80 MiB device-only heap once at start-up, through
a bounded staging heap released afterwards, so the records the GPU reads every frame never depend on
the host-visible aperture (256 MiB on a GTX 1080 Ti, shared by every process; when it is full the
driver backs host-visible heaps with system memory and the belt cull runs 40x slower). A host-visible
heap of about 4 MiB holds frame constants, staged culling parameters/body instances and the overlay
space. GPU-written culling scratch, indirect
commands and generated instances use a separate device-only heap sized for the configured maximum
rock count (about 16 MiB by default); the two rock-state slices are another 16 MiB of device memory with
a 16 MiB host-visible staging heap in two slots. A small readback heap returns completed culling statistics; the
per-frame uploads are the parameters, the body instances and the rock state of the active tier. Rock-count overrides are checked against
the supported instance capacity before allocation. Generated asteroids use 32-byte records,
while the body prefix retains 48-byte instances. Mesh material data is reconstructed from
packed identity; billboards store half-precision RGB/rim and full-precision coverage/ambient.
Static asteroid records retain full-precision rotation and spin rates.
A separate 64 MiB staging heap uploads textures. Descriptors are a fixed table of 40 sampled images and 4 samplers;
`Slot` in `renderer_impl.hpp` names every entry. Push constants carry a 32-byte root per pipeline: the frame
pointer and vertex or instance pointers for surfaces, the rock data and scratch pointers for culling, a
vertex pointer and pixel scale for the overlay.

Materials are PNG files decoded with Wuffs into owning `Rgba8Image` values, mip-generated on the CPU with SIMD kernels and uploaded once;
`tools/import-assets.ps1` records how each was derived from its upstream source. Frame targets are recreated
on resize; history is invalidated for one frame.

## Application

The app/render adapter copies pose, basis, vertical FOV and cut serial into a `CameraView`
for each frame. Navigation and bookmark/orbit state remain in app; rendering uses the
supplied basis without recomputing it. Bodies and UI remain borrowed for the draw call.
App startup builds the HUD and supplies an image view to the renderer, which copies and
uploads it during construction. HUD and font uploads share the RGBA view-to-texture path.
The app releases its HUD pixels after construction. Renderer files include no app headers.
`orbital_app_cpu` builds navigation separately from `orbital_scene`; only the executable
and camera/app tests link it.


`app/main.cpp` parses options, owns the frame loop and turns key presses into `AppState`. The camera has
free flight, orbit and a scripted tour, with body-relative bookmarks for the six views. The control panel
(`app/ui.cpp`) edits the same `AppState` the hotkeys do, so the two never disagree.
Render controls use the concern-specific types in `render/settings.hpp`: tone, anti-aliasing, belt,
belt dust, Earth, sun, post effects, sky and gas giant. `AppState` owns the groups and `FrameInput`
copies them as complete values. Defaults, including panel resets and CLI defaults, come from those
types; adding a field within a group needs no new frame-loop assignment. Bloom enablement retains
its intensity, and the renderer derives the effective intensity and splat radius at use sites.

Captures and benchmarks run the same loop with a frozen time and a frame limit, which is what every measurement in DECISIONS.md
comes from.

## Shader implementation

Shader entry points include focused helpers instead of a common umbrella. `shaders/lib` contains
resource-independent geometry, noise, sampling, color, projection, scattering and tone curves.
`shaders/scene` declares the bindings and frame-aware view/visibility helpers; `shaders/surface`
holds shared surface contracts, BRDFs and lighting. Feature folders group planetary materials
and atmosphere (`planets`), all anti-aliasing (`aa`), belt rendering (`belt`), sky (`sky`),
post-processing (`post`) and UI (`overlay`), with entry points beside their helpers. Descriptor slot numbers are shared with C++ through
`resource_slots.h`. See [shader organization](../shaders/README.md) for dependency rules and checks.

Post-processing uses dedicated bloom, composite, sun-visibility, metering,
presentation and FXAA fragment entry points. Only bloom selects its internal
stage through a shared C++/shader mode contract. The composite keeps its finishing
effects together; splitting sources does not add passes or intermediate images.

Slang-generated depfiles track transitive includes. `tools/check-shaders.py` compiles each
helper on its own to catch accidental include-order dependencies, and every entry point with SPIR-V validation.

## Tools and tests

Ten CTest suites cover core, system, geometry, camera, image, kernels, materials, SMAA, texture and app boundaries. They
use assertions as executable invariants and also run under GCC. `tools/check-gcc.ps1` runs that build
locally; `tools/smoke-window.ps1` drives the window through resize, minimize and key transitions (opt-in,
`check-renderer.py --window`; the validation run itself is hidden);
`tools/check-stability.ps1` compares two fixed-time captures pixel by pixel.

The optional Gaia texture-layer sky path samples a global colour map, a cropped
cloud map and a scalar darkening map directly in the background shader. It skips
the galaxy splat pass. The three textures use the existing optional cache loader;
all three must be available before the mode activates. See [galaxy layers](GALAXY_LAYERS.md).
An independent uncompressed original-map path supports full-resolution comparison.
The Sky selector and `--galaxy` choose splats, layers or original; all assets load
at startup, while only the selected path renders. Texture modes share the splat
sky's brightness/contrast but bypass its procedural dust. Splats remain the default.

Checked borrowed `ImageView` values carry pixel layout, row stride and byte span across PNG output and font upload boundaries. Views support constant evaluation. Image types are separate from PNG I/O declarations in `assets/image_io.hpp`; shared asset validation permits 1 through 16,384 pixels on each axis.
Optional desktop GPU correctness tests use the pinned local Vulkan layer; see [RENDER_VALIDATION.md](RENDER_VALIDATION.md).


### App controls and options

`ui.cpp` manages the ImGui context and backend lifecycle; `ui_panel.cpp` assembles sections whose controls take only the relevant settings (plus stats where shown). Section order and ImGui IDs remain stable. Camera navigation and capture requests share actions with keyboard/startup handlers; simple setting assignments remain direct. Orbit requests reject an absent selected body.

`options.cpp` owns CLI parsing and validation in the CPU-only app target, separate from the window/render loop. Bounded enum/boolean choices share one parser. Numeric syntax, missing values, range checks and last-occurrence precedence are covered by CPU tests; non-finite floating-point values are rejected, including pan and LOD scale.
