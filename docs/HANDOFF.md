# Implementation continuation handoff

## Current intent

Implementation is authorized. Continue the native C++20/MSVC/CMake NoGraphicsAPI demo, prioritizing convincing planetary scale/materials/lighting and stable distant asteroids. RTX 4080 is the intended primary target; the available test device is GTX 1080 Ti. The user permits Sol/Luna delegation and prefers small dependencies and sourced realistic material sets over simplistic generated textures. Latest requests are integrated: 35,000 baseline / 65,000 high asteroids (10x increase), a sparse larger-body tail, plane-cut flat-shaded polyhedral rocks, mapped roughness variation and specular filtering, coarse spatial-cluster frustum/planet-occlusion pruning feeding instanced draws. The local Claude CLI exists and its help recognizes fable. Read-only proposal attempts with --model fable, including safe mode, returned no output after several minutes and were cancelled; no Claude contribution or project transfer completed. Do not stop or modify other existing Claude processes.

## Start here

- Root [README](../README.md): build, launch and controls.
- [VALIDATION.md](VALIDATION.md): observed results and outstanding acceptance checks.
- [FOUNDATION.md](FOUNDATION.md): compatibility backend and shader ABI.
- [ASSETS.md](ASSETS.md): sourced maps and license provenance.
- [STABILITY.md](STABILITY.md): capture/lifecycle test evidence and limits.
- [PRD.md](PRD.md), [TASKS.md](TASKS.md), [WORKSTREAMS.md](WORKSTREAMS.md): original acceptance requirements and assignments, not blanket completion claims.

## Implemented state

The app builds and renders on GTX 1080 Ti. NoGraphicsAPI has an in-tree conventional Vulkan descriptor/pipeline fallback because upstream experimental requirements are unavailable on this GPU. The demo forces that path on all devices for a consistent shader ABI. Upstream revision and deviations are documented in FOUNDATION.md.

Default shaders are now .slang, compiled offline to SPIR-V by pinned Slang. Shared CPU/shader data layout is shaders/scene_shared.h. Sourced Earth day/night/cloud/normal/specular, Jupiter, Moon and rock maps are stored as PNG in assets/materials, decoded with Wuffs and mip-generated on the CPU.

Rendering includes camera-relative coordinates, sphere/rock LODs, shadow map, material lighting, separate clouds, Rayleigh/Mie atmosphere approximation, floating-point HDR, exposure, bloom/solar flare, ACES fit, correct sRGB output, and reprojection-based temporal filtering. Tiny asteroids use filtered area-weighted billboards rather than subpixel triangles. Planet sampling corrects longitude-wrap derivatives. Recent display-space static dither replaces noisy linear-space dither.

## Remaining priorities

1. Core/synchronization validation and lifecycle smoke passed; final dense/faceted/high-belt Debug recheck and 5/5 Debug tests also passed. See FOUNDATION.md. Keep these checks when changing backend/resource lifetimes.
2. Continue distant-asteroid temporal stability work under camera motion. The final dense/faceted fixed-phase check measured mean max-channel delta .3264, maximum182, 2.6046% pixels changed >1 code value; this is not flicker-free acceptance. Earlier identical fixed captures only proved reproducibility.
3. Visually inspect seam fixes at multiple planet rotations and poles, and updated dawn bookmark. Existing captures under captures/ are developer evidence, not approved final art.
4. An installed-build 90-second 1080p tour and 1440p high belt benchmarks passed; see VALIDATION.md. Run a longer soak and obtain RTX 4080 measurements before final performance sign-off.
5. Continue visual polish against the user's critique. Current scales are a deliberately compressed fictional system, not an astronomical simulator. Scattering/clouds remain approximations, TAA has no per-object motion vectors, and subpixel billboards omit individual shadows. Windows HDR display output is not implemented (internal HDR is).

## Workspace and coordination

Use tools/build.ps1; it finds MSVC through vswhere or `ORBITAL_VCVARS`. Machine-specific CMake overrides (for example `Vulkan_LIBRARY`) belong in the ignored CMakeUserPresets.json. Local Vulkan headers are under .tools; the loader import library currently comes from an existing external installation, so a fresh machine needs Vulkan SDK or explicit CMake paths. Do not advertise this as a fully self-contained toolchain yet.

The repository is tracked in git on `main`. Preserve all work. Child agents share files: assign exclusive ownership. Parent owns renderer, root build, app integration and primary docs; foundation owns dependency backend/validation tooling; geometry owns camera tests/stability tooling. Inspect actual diffs/results rather than relying solely on agent completion messages.

## Suggested skills

- handoff: maintain this concise continuation record at the user's requested docs/ destination.
- imagegen: only if deliberately creating bitmap material assets; current sourced maps do not require it.
- tdd: only if the user explicitly requests test-first work.

No OpenAI-product, web UI or Cloudflare work is needed.
