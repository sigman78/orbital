# Architecture

How the demo is put together as it stands. The reasoning behind each choice, with the measurements that
drove it, is in [DECISIONS.md](DECISIONS.md); the graphics backend and shader ABI are in
[FOUNDATION.md](FOUNDATION.md); the layering rules are in [CODE_STYLE.md](CODE_STYLE.md).

## Layers

```
core       logging, panic, file I/O, math, small containers
scene      seeded system description, orbital evaluation, sphere and rock geometry
assets     PNG I/O, SIMD pixel kernels, material mip chains
platform   window, input, process, text overlay, Dear ImGui overlay input   (win32/ implements them)
render     the NoGraphicsAPI renderer: resources, materials, per-frame passes
app        options, camera, HUD, control panel, frame loop
```

Everything below `platform` builds and tests with GCC on Linux in CI. Only `platform/win32` and the demo
executable need MSVC. The renderer never sees an OS type; the application never sees a Vulkan type.

## Scene

`scene::generate_system` builds a deterministic description from a seed: a star, an Earth-like world, a gas
giant with an asteroid belt, a rocky moon, a desert world with two tidally locked moonlets. Bodies carry
stable ids and independent derived seeds; orbits and rotations are evaluated directly at a time, so a fixed
time reproduces a snapshot. Distances are deliberately compressed so several bodies share a frame.

Positions are double precision and converted to camera-relative floats each frame. Spheres come in four
subdivision levels selected by projected size. The belt draws from a library of 16 seeded rock shapes at 6
levels (20 to 20k triangles), packed into one pooled vertex and index range so every level of every shape is
a slice of the same buffers.

## Belt

The belt is a population rather than a mesh list: `geometry` places 280k (baseline) or 520k (high) rocks in
an annulus with rings, a gap, tapered edges and flared height, a power-law size distribution and three
composition classes. Each rock is a 64-byte record on the GPU; nothing per rock lives on the CPU after
startup.

Every frame a compute shader culls the whole population in three passes (count, prefix, scatter): frustum,
planet occlusion, projected size, then a level and shape group, or a splat for rocks under the cut-off. The
scatter pass lights each splat once (Lambert sphere at its phase angle, belt shadowing) and writes indirect
draw arguments per group, so the meshes are one multi-draw and the splats one draw. Belt shadowing comes
from three sources: an analytic extinction through the belt's own density along the sun ray, a
transmittance map splatted by the largest rocks from the sun's direction, and the planets' shadows.
Between the rocks a half-resolution march through the same density field scatters sunlight as dust.

## Frame

1. Cull passes write the instance list and draw arguments into the dynamic half of the heap.
2. Shadow map (2048, depth only) for the bodies; belt transmittance map (2048, four coverage slices) and its blur.
3. Scene pass into the HDR target: background, bodies, the pooled rock multi-draw, splats, clouds.
4. Atmospheres, composited per body against depth; then the belt dust, marched at half resolution and
   composited with a depth-aware upsample.
5. The splats write their depth and a mask, so the temporal pass reprojects them exactly and keeps their
   history unclipped.
6. Temporal anti-aliasing into a history target; bloom at quarter resolution; exposure metering every
   sixteenth frame from a 16x16 log-luminance image.
7. Tone mapping (PBR Neutral, AgX or ACES filmic) with vignette, chromatic fringe and grain into an
   intermediate, then the spatial pass (SMAA or FXAA) into the final image.
8. Present, with the HUD and the Dear ImGui panel drawn last into the swapchain.

GPU timestamps bracket the cull and shadow passes, the scene, the atmospheres with the dust, and the post
passes; the title bar and the panel show them.

## Resources

One 128 MiB host-visible heap holds everything: meshes and rock records appended from the front at startup,
per-frame constants, cull scratch, the instance list and the overlay vertices in the back half. A separate
64 MiB staging heap uploads textures. Descriptors are a fixed table of 40 sampled images and 4 samplers;
`Slot` in `renderer_impl.hpp` names every entry. Push constants carry a 32-byte root per pipeline: the frame
pointer and vertex or instance pointers for surfaces, the rock data and scratch pointers for culling, a
vertex pointer and pixel scale for the overlay.

Materials are PNG files decoded with Wuffs, mip-generated on the CPU with SIMD kernels and uploaded once;
`tools/import-assets.ps1` records how each was derived from its upstream source. Frame targets are recreated
on resize; history is invalidated for one frame.

## Application

`app/main.cpp` parses options, owns the frame loop and turns key presses into `AppState`. The camera has
free flight, orbit and a scripted tour, with body-relative bookmarks for the six views. The control panel
(`app/ui.cpp`) edits the same `AppState` the hotkeys do, so the two never disagree. Captures and benchmarks
run the same loop with a frozen time and a frame limit, which is what every measurement in DECISIONS.md
comes from.

## Tools and tests

Seven CTest suites cover the CPU layers: core, system, geometry, camera, image, kernels and materials. They
use assertions as executable invariants and also run under GCC. `tools/check-gcc.ps1` runs that build
locally; `tools/smoke-window.ps1` drives the window through resize, minimize and key transitions;
`tools/check-stability.ps1` compares two fixed-time captures pixel by pixel.
