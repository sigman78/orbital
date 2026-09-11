# Proposed implementation architecture

This design is provisional until the foundation spike passes. Do not treat candidate folder names or implementation mechanisms as already existing code.

## Graphics foundation and compatibility gate

Pin a reviewed NoGraphicsAPI commit and preserve its license/notices. Prefer a project-contained, clearly documented fork or reproducible patch series for necessary compatibility changes. Do not silently replace the requested foundation with another rendering library.

Current upstream documents Vulkan 1.4 plus `VK_EXT_descriptor_heap`, `VK_KHR_device_address_commands`, `VK_KHR_shader_untyped_pointers`, and `VK_EXT_mesh_shader`. It also documents Vulkan SDK 1.4.357+, Slang 2026.14.1+, and SPIRV-Tools 2026.3+ for examples. Recheck requirements at the selected revision: [upstream README](https://github.com/sebbbi/NoGraphicsAPI#build-and-run).

The proposed compatibility path retains the public API where feasible and implements supported operations using conventional Vulkan mechanisms. This is an investigation, not a promise of a trivial backend switch. Audit descriptor heap shader syntax and reflection, root/push-data layout, buffer addressing, address-based commands, feature validation, and resource synchronization before committing to a mapping. Ordinary descriptor sets and push constants are candidate mechanisms; shader ABI translation and device limits must be demonstrated. Do not advertise mesh support on hardware without it: make optional features explicit if upstream currently requires them unconditionally.

Foundation acceptance requires a real rendered frame, shader-accessed data, sampled texture, depth attachment and floating-point offscreen target followed by presentation on GTX 1080 Ti. An upstream sample alone on a different GPU does not prove compatibility. Query actual RTX 4080 driver capabilities before selecting its path. If preserving the API proves impractical, present concrete findings and alternatives to the user before substituting another foundation.

## Code organization

| Candidate path | Responsibility |
| --- | --- |
| `src/app/` | Window, input, configuration, main loop, diagnostics, camera tour |
| `src/scene/` | Seeded body descriptions, transforms, cameras, asteroid instances |
| `src/render/` | GPU ownership, uploads, frame resources, passes, quality settings |
| `src/core/` | Logging, panic, file I/O, vector/matrix math |
| `src/assets/` | PNG image I/O, SIMD pixel kernels and material mip-chain generation |
| `shaders/` | Shared ABI, generation, opaque shading, atmosphere/clouds, post-processing |
| `cmake/` | Dependency and shader compilation integration |
| `tests/` | Focused non-GPU tests and GPU smoke entry points |
| `third_party/` | Pinned dependency/fork metadata and notices |

Keep scene descriptions independent of GPU allocation types. Keep graphics backend changes isolated from scene code. Avoid building a general-purpose engine, plugin framework, ECS or complex render graph for this demo.

Use named physical/rendering units; establish handedness, up axis, matrix layout, depth range and shader ABI in a shared contract before scene integration. Recommended starting point: right-handed world, Y-up, explicit simulation-to-render scale, camera-relative float rendering with double-precision scene positions where needed. Choose near/far planes and reversed-Z after checking API support; test projection and depth behavior. Collision/landing support is out of scope, so constrain camera approaches to prevent surface penetration.

## Resources and frame lifetime

Use RAII and explicit ownership for device, window, swapchain, heaps, views, pipelines and synchronization objects. Suballocations and descriptor slots must outlive all GPU references. Start with a small fixed number of frames in flight, recycle only after timeline completion, and retire resized targets and replaced assets safely. Idle the CPU while minimized. Report allocation and shader failures with context.

Keep generated planet maps resident; regenerate when seed/parameters change, not every frame. Use seamless sphere-domain noise, coherent octave filtering and mip chains. Prefer cube maps or another explicitly seam-managed representation. Budget generation time and memory, and avoid blocking normal interaction on expensive regeneration.

## Rendering sequence

1. Update camera and simulation; prepare visible asteroid instances and frame parameters.
2. Render bounded local shadow maps for nearby opaque bodies/asteroids. Use analytic body intersections for planetary eclipses and large-scale occlusion.
3. Render stars/background and opaque bodies into a linear floating-point HDR color target with depth.
4. Integrate atmospheres and clouds with scene depth and sun transmittance; composite in a documented order and avoid lighting the night side incorrectly.
5. Add visible solar emission and occlusion-aware optical effects in HDR.
6. Compute exposure from log luminance, apply temporally smoothed adaptation, produce a downsampled bloom pyramid, then tone map for SDR presentation.
7. Composite readable controls/diagnostics after tone mapping.

Each pass declares inputs, outputs and synchronization hazards. Correctness comes before asynchronous compute. Choose anti-aliasing during the renderer milestone: begin with a modest spatial solution, then add temporal accumulation only if needed and accompanied by motion/reprojection data and disocclusion handling.

### Planets

Use tessellated spheres with sufficient silhouette detail and distance-based mesh selection. Bake seeded material maps using compute when the compatibility path supports the necessary contract. Terrestrial generation combines broad continental structure, finer elevation, biome masks, ice, ocean masks and roughness. Derive bounded normal variation from height without noisy silhouettes. Clouds use their own density field and rotation; start with a shell and meaningful self/body shadowing, then increase volumetric sampling at high quality if the budget permits.

Gas-giant bands use latitude-dependent flow, domain warping and localized storm fields with a controlled palette. Animate rotation/flow coherently. Atmospheres use Rayleigh/Mie-inspired scattering and optical depth with shared sun geometry; numerical approximations are acceptable and should be documented.

### Asteroids and shadows

Generate several deterministic irregular rocky meshes, instance them in an annular distribution with thickness and gaps, and vary scale, orientation and rotation. Use CPU frustum culling initially; add GPU culling only when profiling justifies it. Use LODs or distant representations to limit geometry cost. Shadow work should be bounded to visible neighborhoods; do not attempt a single solar-system-wide shadow map.

### Sun, HDR and flares

Use consistent exposure-space radiance values and an explicit color-management contract. Bound exposure adaptation and offer a fixed exposure for captures. Occlude the finite solar disc against bodies/depth and use fractional visibility for smooth flare transitions. Atmosphere transmittance should affect sunlight. Tone mapping is required; native HDR swapchain output is not part of initial acceptance.

## Build and verification conventions

- C++20 and target-scoped CMake properties; avoid global compiler/linker flag mutations.
- Checked-in configure/build/test presets for MSVC Debug and Release; keep user-specific SDK/tool locations in untracked user presets or environment configuration.
- Pin dependencies by immutable revision/version; support local dependency overrides. No network fetch at application launch.
- Compile shaders through custom commands with include dependencies; surface compiler errors during the build. Install/copy runtime shader assets beside the executable and locate them relative to it, not the working directory.
- Enable strong warnings for project code; avoid forcing project warnings onto upstream sources. No fast-math default until numerical behavior has been evaluated.
- Add meaningful tests for seeded generation invariants, transforms and allocation/lifetime utilities where implemented. GPU smoke checks cover rendering, presentation and resize. Do not write tests that simply duplicate implementation.
- Record validation status, fixed-view captures and benchmark settings. Keep generated build artifacts out of source control.
